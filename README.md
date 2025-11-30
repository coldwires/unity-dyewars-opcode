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
- [Client Architecture](#client-architecture-unityc)
- [Packet Protocol](#packet-protocol)
- [Lua Scripting](#lua-scripting-system)
- [Building & Running](#building--running)
- [Project Structure](#project-structure)

---

## Server Architecture (C++)

### Threading Model

| Thread | Responsibility |
|--------|----------------|
| **Main (asio)** | Accepts connections, async read/write packets |
| **Game Loop** | 20 ticks/sec, batches updates, broadcasts |
| **Console** | Reads stdin for commands (`r` = reload, `q` = quit) |
| **File Watcher** | Monitors Lua scripts for hot-reload |

<details>
<summary><strong>Connection Flow (click to expand)</strong></summary>

When a client connects:

```cpp
// 1. GameServer::StartAccept() is listening
acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
    // 2. Assign unique ID
    uint32_t id = next_player_id_++;
    
    // 3. Create GameSession for this connection
    auto session = std::make_shared<GameSession>(std::move(socket), lua_engine_, this, id);
    
    // 4. Store in sessions map (mutex protected)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        sessions_[id] = session;
    }
    
    // 5. Start session
    session->Start();
    
    // 6. Continue listening
    StartAccept();
});
```

```cpp
// GameSession::Start() sends initial data
void GameSession::Start() {
    SendPlayerID();           // "You are player #X"
    SendPosition();           // "You are at (0,0)"
    SendAllPlayers();         // "Here are other players"
    BroadcastPlayerJoined();  // Tell others about this player
    ReadPacketHeader();       // Begin async read loop
}
```

</details>

<details>
<summary><strong>Dirty Flag Pattern (click to expand)</strong></summary>

**Problem:** Broadcasting every movement immediately causes packet explosion.

**Solution:** Batch updates using dirty flags:

```cpp
// When player moves (network thread)
if (moved) {
    is_dirty_ = true;  // Mark for broadcast
    SendPosition();    // Only tell THIS player immediately
}

// Game loop (20 ticks/sec)
void GameServer::ProcessUpdates() {
    std::vector<PlayerData> moving_players;
    
    for (auto& pair : sessions_) {
        if (pair.second->IsDirty()) {
            moving_players.push_back(pair.second->GetPlayerData());
            pair.second->SetDirty(false);
        }
    }
    
    // Build ONE packet, send to ALL clients
}
```

**Why `std::atomic<bool>`?** The flag is written by network thread and read by game loop thread—atomic prevents data races.

</details>

### Class Responsibilities

| Class | Responsibility |
|-------|----------------|
| `GameServer` | Accepts connections, owns sessions, runs game loop |
| `GameSession` | Manages one client, handles packets, owns Player |
| `Player` | Position state, movement with collision |
| `GameMap` | Walkable/wall data |
| `LuaGameEngine` | Runs scripts, hot-reload support |
| `DatabaseManager` | SQLite wrapper (not yet integrated) |

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
public void SendMoveCommand(int direction) {
    // 1. Send to server
    SendMessage(new byte[] { 0x01, (byte)direction });
    
    // 2. Predict locally (immediate feedback)
    Vector2Int predicted = MyPosition;
    if (direction == 0) predicted.y += 1;
    // ...
    MyPosition = predicted;
    OnMyPositionUpdated?.Invoke(MyPosition);
}

// 3. Server correction (if prediction was wrong)
case 0x10:
    if (MyPosition.x != x || MyPosition.y != y) {
        MyPosition = new Vector2Int(x, y);  // Snap to server truth
    }
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

| Opcode | Direction | Name | Payload |
|--------|-----------|------|---------|
| `0x01` | C→S | Move | `[direction]` (0=up, 1=right, 2=down, 3=left) |
| `0x02` | C→S | Request Position | *(empty)* |
| `0x03` | C→S | Custom Message | `[data...]` |
| `0x10` | S→C | Your Position | `[x][y]` |
| `0x12` | S→C | Other Player Update | `[id:4][x][y]` |
| `0x13` | S→C | Your Player ID | `[id:4]` |
| `0x14` | S→C | Player Left | `[id:4]` |
| `0x20` | S→C | Batch Update | `[count][id,x,y]...` |

---

## Lua Scripting System

Game logic can be modified without recompiling:

```lua
-- scripts/main.lua
function process_move_command(x, y, direction)
    local new_x, new_y = x, y
    
    -- Custom rules: wall at (5,5)
    if new_x == 5 and new_y == 5 then
        return {x, y}  -- Block movement
    end
    
    return {new_x, new_y}
end

function on_player_moved(player_id, x, y)
    -- React to movement (traps, triggers, etc.)
end
```

**Hot Reload:** Type `r` in server console or save the file (auto-detected).

---

## Building & Running

### Server

**Dependencies (vcpkg):** `asio`, `nlohmann_json`, `spdlog`, `sol2`, `lua`, `sqlite3`

```bash
cd dyewars_server
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
cmake --build .
./DyeWarsServer
```

### Client

1. Open `dyewars/` in Unity
2. Set server IP in `NetworkManager` (default: `192.168.1.3`)
3. Press Play

---

## Project Structure

```
DyeWarsProject/
├── dyewars/                    # Unity client
│   └── Assets/code/
│       ├── NetworkManager.cs   # Networking, input, protocol
│       └── GridRenderer.cs     # Renders players on grid
│
├── dyewars_server/             # C++ server
│   ├── include/
│   │   ├── server/             # GameServer, GameSession, Player, GameMap
│   │   ├── lua/                # LuaEngine
│   │   └── database/           # DatabaseManager
│   ├── src/                    # Implementation files
│   └── scripts/main.lua        # Hot-reloadable game logic
│
└── README.md
```

---

## Key Concepts Demonstrated

| Concept | Description |
|---------|-------------|
| **Authoritative Server** | Client sends input, server decides state |
| **Client-Side Prediction** | Move locally, correct on server mismatch |
| **Packet Coalescing** | Batch updates into single packets |
| **Dirty Flag Pattern** | Track changes, sync only what changed |
| **Thread Safety** | Mutexes for maps, atomics for flags |
| **Hot Reloading** | Change Lua without restart |
| **Async I/O** | Non-blocking network with Boost.Asio |