# DyeWars Project

A multiplayer grid-based game with a C++ authoritative server and Unity thin client.

```
┌─────────────────────┐         TCP/IP          ┌─────────────────────┐
│   Unity Client      │◄───────────────────────►│   C++ Server        │
│   (Renderer Only)   │    Custom Protocol      │   (All Game Logic)  │
└─────────────────────┘                         └─────────────────────┘
```

**Key Design Decision:** The server is *authoritative*—it owns all game state. The Unity client is a "dumb terminal" that sends input and renders positions.

---

## Table of Contents

- [Server Architecture](#server-architecture-c)
- [Performance](#performance)
- [Debug Dashboard](#debug-dashboard)
- [Client Architecture](#client-architecture-unityc)
- [Packet Protocol](#packet-protocol)
- [Lua Scripting](#lua-scripting-system)
- [Building & Running](#building--running)
- [Console Commands](#console-commands)
- [Project Structure](#project-structure)

---

## Server Architecture (C++)

### Threading Model

| Thread | Responsibility |
|--------|----------------|
| **IO Thread (asio)** | Accepts connections, async read/write packets |
| **Game Loop Thread** | 20 ticks/sec, processes actions, broadcasts updates |
| **Console Thread** | Reads stdin for commands |

Communication between threads uses a thread-safe action queue (`GameServer::QueueAction`). Network handlers queue lambdas that execute on the game thread.

### Core Components

| Class | Responsibility |
|-------|----------------|
| `GameServer` | Central coordinator: accepts connections, runs game loop, broadcasts |
| `ClientConnection` | Per-client TCP handler, packet parsing, handshake |
| `ClientManager` | Tracks all connections (real + fake for stress testing) |
| `World` | Owns TileMap (static), SpatialHash (dynamic), VisibilityTracker |
| `SpatialHash` | O(1) spatial queries via flat grid + hash map fallback |
| `VisibilityTracker` | Bidirectional tracking for enter/leave view events |
| `PlayerRegistry` | Player lifecycle, dirty tracking for broadcasts |
| `Player` | Position, facing, movement validation with cooldowns |

<details>
<summary><strong>Connection Flow (click to expand)</strong></summary>

When a client connects:

```cpp
// 1. GameServer accepts connection
acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
    // 2. Create ClientConnection with unique ID
    auto client = std::make_shared<ClientConnection>(std::move(socket), this, next_id_++);

    // 3. Add to ClientManager (mutex protected)
    clients_.AddClient(client);

    // 4. Start async read loop
    client->Start();
});

// 5. On successful handshake, queue player creation on game thread
server->QueueAction([=]() {
    auto player = players_.CreatePlayer(client_id, spawn_x, spawn_y);
    world_.AddPlayer(player->GetID(), spawn_x, spawn_y, player);
    // Send welcome packet, nearby players, etc.
});
```

</details>

<details>
<summary><strong>Dirty Flag Pattern (click to expand)</strong></summary>

**Problem:** Broadcasting every movement immediately causes packet explosion.

**Solution:** Batch updates using dirty flags:

```cpp
// When player moves successfully
world.UpdatePlayerPosition(player_id, new_x, new_y);
players.MarkDirty(player);  // Flag for broadcast

// Game loop processes dirty players once per tick
void ProcessTick() {
    auto dirty = players_.ConsumeDirtyPlayers();
    BroadcastDirtyPlayers(dirty);  // One batch operation
}
```

</details>

<details>
<summary><strong>Spatial Hash Architecture (click to expand)</strong></summary>

The spatial hash divides the world into cells for efficient range queries:

```cpp
// Flat grid for O(1) cell access (no hash lookups)
std::vector<std::vector<std::shared_ptr<Player>>> flat_grid_;

// Query: Who's near position (x, y)?
void ForEachNearby(x, y, range, [](const auto& player) {
    // Zero-copy iteration, no vector allocation
});
```

**Critical ordering for position updates:**
```cpp
// 1. Update player's internal position FIRST
player->SetPosition(new_x, new_y);

// 2. THEN update spatial hash (derives OLD cell from stored key)
world.UpdatePlayerPosition(player_id, new_x, new_y);
```

The spatial hash stores a cell key per entity. On update, it derives the old cell from this key (not from `GetX()/GetY()` which already has the new position).

</details>

<details>
<summary><strong>Visibility Tracking (click to expand)</strong></summary>

**Problem:** When A moves toward stationary B, the dirty system tells B about A. But A never learns about B (B didn't move).

**Solution:** Track who each player "knows about":

```
A moves → Update(A, visible_players)
        → Diff against A's known set
        → entered: send S_Player_Spatial
        → left: send S_Left_Game
```

**Bidirectional maps for O(K) disconnect cleanup:**
```cpp
known_players_: A → {B, C}     // A knows about B and C
known_by_:      B → {A, D}     // B is known by A and D
```

When B disconnects, only update A and D (not all N players).

</details>

---

## Performance

The server handles **2000+ concurrent players** at 20 TPS with optimized spatial queries.

| Metric | Value |
|--------|-------|
| Target Tick Rate | 20 TPS (50ms budget) |
| Spatial Hash Query | ~6-8ms (2000 spread players) |
| Total Tick Time | ~40-50ms (2000 spread players) |

### Key Optimizations

1. **Flat Grid Spatial Hash** - O(1) array indexing instead of hash map lookups
2. **Zero-Copy Iteration** - `ForEachNearby()` template avoids vector allocation
3. **Batch Client Lookups** - Single mutex acquisition for multiple connection lookups
4. **Bidirectional Visibility** - O(K) disconnect cleanup instead of O(N)

See `src/debug/PERFORMANCE_OPTIMIZATIONS.md` for detailed documentation.

---

## Debug Dashboard

Access `http://localhost:8082` when server is running for real-time metrics:

- **Performance** - Tick time (avg/max), TPS, tick history chart
- **Connections** - Real clients, fake clients (bots), total players
- **World** - Visibility tracked, dirty players per tick
- **Bandwidth** - Current/average/total bytes out, packets/sec
- **Bot Movement** - Spatial query, visibility, departure time breakdown
- **Broadcast Breakdown** - Viewer query, client lookup, packet send time

---

## Client Architecture (Unity/C#)

### Thread-Safe Communication

Unity APIs only work on the main thread. Network thread queues actions:

```csharp
// Network thread: queue action
lock (queueLock) {
    mainThreadActions.Enqueue(() => {
        MyPosition = new Vector2Int(x, y);
        OnMyPositionUpdated?.Invoke(MyPosition);
    });
}

// Main thread: process queue in Update()
void Update() {
    lock (queueLock) {
        while (mainThreadActions.Count > 0) {
            mainThreadActions.Dequeue()?.Invoke();
        }
    }
}
```

### Client-Side Prediction

Move locally for responsiveness, correct if server disagrees:

```csharp
public void SendMoveCommand(int direction, int facing) {
    // 1. Send to server
    SendMessage(new byte[] { 0x01, (byte)direction, (byte)facing });

    // 2. Predict locally (immediate feedback)
    Vector2Int predicted = CalculateNewPosition(direction);
    MyPosition = predicted;
    OnMyPositionUpdated?.Invoke(MyPosition);
}

// 3. Server correction (if prediction was wrong)
case 0x11:  // Position correction
    MyPosition = new Vector2Int(x, y);  // Snap to server truth
    break;
```

---

## Packet Protocol

### Structure

```
┌─────────┬─────────┬─────────┬─────────┬─────────────────────┐
│  0x11   │  0x68   │ size_hi │ size_lo │      payload        │
└─────────┴─────────┴─────────┴─────────┴─────────────────────┘
   byte 0    byte 1    byte 2    byte 3      bytes 4+
```

**Magic header `0x11 0x68`:** Validates packet boundaries and detects malformed data.

### Message Types

**Client → Server:**

| Opcode | Name | Payload |
|--------|------|---------|
| `0x01` | Move Request | `[direction:1][facing:1]` |
| `0x02` | Turn Request | `[facing:1]` |
| `0x04` | Interact Request | *(empty)* |
| `0x40` | Attack Request | *(empty)* |

**Server → Client:**

| Opcode | Name | Payload |
|--------|------|---------|
| `0x10` | Welcome | `[player_id:8][x:2][y:2][facing:1]` |
| `0x11` | Position Correction | `[x:2][y:2][facing:1]` |
| `0x12` | Facing Correction | `[facing:1]` |
| `0x25` | Batch Player Spatial | `[count:1][[id:8][x:2][y:2][facing:1]]...` |
| `0x26` | Player Left | `[player_id:8]` |
| `0xF0` | Handshake Accepted | *(empty)* |
| `0xF2` | Server Shutdown | `[reason:1]` |

---

## Lua Scripting System

Game logic can be modified without recompiling:

```lua
-- scripts/main.lua
function on_player_moved(player_id, x, y)
    -- React to movement (traps, triggers, etc.)
    print("Player " .. player_id .. " moved to " .. x .. "," .. y)
end

function on_player_joined(player_id)
    print("Welcome player " .. player_id)
end
```

**Hot Reload:** Type `r` in server console to reload scripts.

**Note:** Player IDs are passed as strings because Lua's `double` can't represent all `uint64_t` values.

---

## Building & Running

### Server

**Requirements:** C++20, CMake, vcpkg

**Dependencies:** `asio`, `nlohmann_json`, `spdlog`, `sol2`, `lua`, `sqlite3`

```bash
cd dyewars_server
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build build
./build/DyeWarsServer.exe   # Windows
./build/DyeWarsServer       # Linux/Mac
```

### Client

1. Open `dyewars/` in Unity
2. Configure server IP in NetworkManager
3. Press Play

---

## Console Commands

| Command | Description |
|---------|-------------|
| `start` | Start the server |
| `stop` | Stop the server |
| `restart` | Restart the server |
| `r` | Reload Lua scripts |
| `stats` | Show bandwidth and player counts |
| `debug` | Enable trace logging |
| `bots <count> [spread\|clustered]` | Spawn stress test bots |
| `nobots` | Remove all bots |
| `exit` | Shutdown server |

---

## Project Structure

```
DyeWarsProject/
├── dyewars/                        # Unity client
│   └── Assets/code/
│       ├── NetworkManager.cs       # Networking, protocol, input
│       └── GridRenderer.cs         # Renders players on grid
│
├── dyewars_server/                 # C++ server
│   ├── src/
│   │   ├── main.cpp                # Entry point, console loop
│   │   ├── core/                   # Log, ThreadSafety
│   │   ├── server/                 # GameServer, ClientConnection, ClientManager
│   │   ├── game/                   # World, SpatialHash, Player, VisibilityTracker
│   │   │   └── actions/            # MoveActions, BotStressTest
│   │   ├── network/packets/        # Protocol, OpCodes, PacketHandler, PacketSender
│   │   ├── debug/                  # DebugHttpServer, ServerStats
│   │   ├── database/               # DatabaseManager (SQLite)
│   │   └── lua/                    # LuaEngine
│   ├── scripts/main.lua            # Hot-reloadable game logic
│   ├── tests/                      # Unit tests
│   └── CMakeLists.txt
│
└── README.md
```

---

## Key Concepts Demonstrated

| Concept | Description |
|---------|-------------|
| **Authoritative Server** | Client sends input, server decides state |
| **Client-Side Prediction** | Move locally, correct on server mismatch |
| **Spatial Partitioning** | Flat grid for O(1) range queries |
| **Visibility Tracking** | Bidirectional maps for enter/leave events |
| **Dirty Flag Pattern** | Track changes, batch broadcasts |
| **Thread Safety** | Action queue, mutexes, atomics |
| **Hot Reloading** | Change Lua without restart |
| **Async I/O** | Non-blocking network with Boost.Asio |
| **Performance Monitoring** | Real-time debug dashboard |