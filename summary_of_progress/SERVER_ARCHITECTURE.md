# DyeWars Server Architecture

## Overview

The DyeWars server is a C++ application built on **asio** for asynchronous networking. It handles multiple client connections concurrently, validates player actions against authoritative game state, and broadcasts updates to all connected players. The architecture emphasizes **security** (handshake validation, bounds checking), **performance** (packet batching, efficient broadcasting), and **extensibility** (Lua scripting for game logic).

This document explains every component, the flow of data through the system, and provides step-by-step guides for adding new features.

---

## Core Architecture Components

### GameServer

The central orchestrator that:
- Accepts incoming TCP connections
- Manages the collection of active sessions
- Runs the game loop on a dedicated thread
- Broadcasts updates to all players
- Owns the game map

**Key members:**

| Member | Purpose |
|--------|---------|
| `acceptor_` | Listens for incoming connections |
| `sessions_` | Map of player ID → ClientConnection |
| `sessions_mutex_` | Thread-safe access to sessions |
| `lua_engine_` | Shared Lua scripting engine |
| `game_map_` | Authoritative map data (walls, bounds) |
| `game_loop_thread_` | Dedicated thread for tick processing |

### ClientConnection (formerly GameSession)

Represents a single client connection and manages:
- TCP socket communication
- Handshake validation
- Packet reading and writing
- Player state (via owned `Player` object)
- Dirty flag for broadcast optimization

**Connection Lifecycle:**

```
TCP Accept
    │
    ▼
Create ClientConnection (not in sessions_ map yet)
    │
    ▼
Start() - Log connection, start 5-second handshake timer
    │
    ▼
ReadPacketHeader() - Begin async read chain
    │
    ▼
Handshake packet arrives (opcode 0x00)
    │
    ├── Valid? ──────────────────────────────────────┐
    │                                                 │
    ▼                                                 ▼
FailHandshake()                              CompleteHandshake()
  └── Log reason                                └── Cancel timer
  └── Write to failed_connections.log           └── Create Player object
  └── Close socket                              └── RegisterSession() with server
  └── Session destroyed (no refs)               └── Send player ID, position
                                                └── Send all other players
                                                └── Broadcast join to others
                                                └── Normal packet processing begins
```

### Player

Pure data class representing a player's authoritative state:

```cpp
class Player {
    uint32_t id_;
    int x_, y_;
    uint8_t facing_;
    
    bool AttemptMove(uint8_t direction, const GameMap& map);
    void SetFacing(uint8_t direction);
};
```

The `AttemptMove` method validates movement against the map before updating position. This is the authoritative check—clients predict, but the server decides.

### GameMap

Stores walkability data for the game world:

```cpp
class GameMap {
    int width_, height_;
    std::vector<bool> walls_;
    
    bool IsWalkable(int x, int y) const;
    void SetWall(int x, int y, bool is_wall);
};
```

Currently uses a simple 1D vector for wall data (faster than 2D). Can be extended for tiles, elevation, etc.

### LuaGameEngine

Embedded scripting for game logic customization:

- Hot-reloadable without server restart
- Thread-safe via mutex
- Exposes callbacks: `on_player_moved`, `process_custom_message`
- File watcher automatically reloads on script changes

### BandwidthMonitor

Singleton that tracks network statistics:

- Bytes sent per second
- Packets per second
- Rolling average bandwidth
- Total bytes sent/received

---

## Threading Model

The server uses multiple threads:

**Main Thread (asio io_context):**
- Handles all async operations (accept, read, write)
- Runs connection callbacks
- Processes individual packets

**Game Loop Thread:**
- Runs at fixed tick rate (20 ticks/second = 50ms)
- Collects dirty players
- Calls Lua callbacks
- Broadcasts batch updates

**Lua File Watcher Thread:**
- Monitors script file for changes
- Triggers hot-reload when modified

**Console Thread:**
- Reads stdin for admin commands
- Detached thread

### Thread Safety

The `sessions_` map is protected by `sessions_mutex_`. Every access must lock:

```cpp
// Reading sessions
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& pair : sessions_) {
        // ... access session ...
    }
}

// Modifying sessions
{
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_[id] = session;  // or sessions_.erase(id)
}
```

The `is_dirty_` flag on each session is `std::atomic<bool>` so the network thread can set it and the game loop thread can read/clear it without locks.

---

## Protocol

### Packet Structure

Every packet has a 4-byte header followed by payload:

```
[0x11][0x68][size_high][size_low][payload...]
  │     │       │          │         │
  └─────┴── magic bytes    │         └── variable length
                           └── payload size (big-endian)
```

### Protocol Constants

Centralized in `Common.h` under the `Protocol` namespace:

```cpp
namespace Protocol {
    constexpr uint8_t MAGIC_1 = 0x11;
    constexpr uint8_t MAGIC_2 = 0x68;
    constexpr uint16_t VERSION = 0x0001;
    constexpr uint32_t CLIENT_MAGIC = 0x44594557;  // "DYEW"
    constexpr int HANDSHAKE_TIMEOUT_SECONDS = 5;
    
    namespace Opcode {
        constexpr uint8_t C_Handshake = 0x00;
        constexpr uint8_t C_Move = 0x01;
        // ... etc
    }
}
```

### Handshake Protocol

Before a client can play, they must complete the handshake:

**Client sends (7 bytes):**
```
[opcode: 0x00][version: 2 bytes][magic: 4 bytes]
    │              │                  │
    └── C_Handshake │                  └── "DYEW" (0x44594557)
                   └── 0x0001
```

**Server validates:**
1. Packet size is exactly 7 bytes
2. Opcode is 0x00
3. Protocol version matches server's VERSION
4. Client magic matches "DYEW"

**On success:** Server creates Player, registers session, sends initial state.

**On failure:** Server logs reason, writes to `failed_connections.log`, closes socket.

### Opcodes

| Opcode | Direction | Name | Payload |
|--------|-----------|------|---------|
| `0x00` | C→S | Handshake | version(2) + magic(4) |
| `0x01` | C→S | Move | direction(1) + facing(1) |
| `0x04` | C→S | Turn | direction(1) |
| `0x10` | S→C | MyPosition | x(2) + y(2) + facing(1) |
| `0x12` | S→C | OtherPlayerUpdate | id(4) + x(2) + y(2) + facing(1) |
| `0x13` | S→C | PlayerIdAssignment | id(4) |
| `0x14` | S→C | PlayerLeft | id(4) |
| `0x15` | S→C | MyFacing | facing(1) |
| `0x20` | S→C | BatchUpdate | count(1) + [id(4)+x(2)+y(2)+facing(1)]... |

---

## Packet Flow

### Receiving Packets

```
1. ReadPacketHeader() - async_read 4 bytes
   │
   ▼
2. Validate magic bytes (0x11 0x68)
   │
   ├── Invalid? Log error, close connection
   │
   ▼
3. Extract payload size from bytes 2-3
   │
   ├── Invalid size (0 or >4096)? Skip, read next header
   │
   ▼
4. ReadPacketPayload(size) - async_read payload bytes
   │
   ▼
5. Handshake complete?
   │
   ├── No → HandleHandshakePacket()
   │         ├── Valid? → CompleteHandshake()
   │         └── Invalid? → FailHandshake()
   │
   └── Yes → HandlePacket()
             │
             ▼
6. Switch on opcode, process accordingly
   │
   ▼
7. ReadPacketHeader() - loop back to step 1
```

### Sending Packets

```cpp
// 1. Build packet
Packet pkt;
PacketWriter::WriteByte(pkt.payload, opcode);
PacketWriter::WriteUInt(pkt.payload, player_id);
// ... more data ...
pkt.size = pkt.payload.size();

// 2. Serialize to bytes
auto bytes = pkt.ToBytes();  // Adds header

// 3. Send asynchronously
asio::async_write(socket_, asio::buffer(bytes), ...);
```

### Broadcasting

For efficiency, the server batches updates. Every tick (50ms):

```
1. Lock sessions_mutex_
2. Iterate all sessions
   └── If session.IsDirty():
       └── Add to moving_players list
       └── Clear dirty flag
3. Unlock mutex

4. Build batch packet:
   [0x20][count][player1_data][player2_data]...

5. Serialize ONCE

6. Send same bytes to ALL connected clients
```

This is dramatically more efficient than sending individual updates to each client.

---

## Game Loop

The game loop runs on a dedicated thread at 20 ticks per second:

```cpp
void GameServer::RunGameLoop() {
    const auto TICK_RATE = std::chrono::milliseconds(50);
    
    while (server_running_) {
        auto start = std::chrono::steady_clock::now();
        
        // 1. Process game logic
        ProcessUpdates();
        
        // 2. Update bandwidth stats
        BandwidthMonitor::Instance().Tick();
        
        // 3. Sleep until next tick
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed < TICK_RATE) {
            std::this_thread::sleep_for(TICK_RATE - elapsed);
        }
    }
}
```

### ProcessUpdates() Detail

```cpp
void GameServer::ProcessUpdates() {
    std::vector<PlayerData> moving_players;
    std::vector<std::shared_ptr<ClientConnection>> all_receivers;

    // PHASE 1: Collect data (hold lock briefly)
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : sessions_) {
            all_receivers.push_back(session);
            
            if (session->IsDirty()) {
                moving_players.push_back(session->GetPlayerData());
                session->SetDirty(false);
            }
        }
    }  // Lock released!

    if (moving_players.empty()) return;

    // PHASE 2: Call Lua (outside lock!)
    for (const auto& data : moving_players) {
        lua_engine_->OnPlayerMoved(data.player_id, data.x, data.y, data.facing);
    }

    // PHASE 3: Build batch packet
    Packet batch;
    PacketWriter::WriteByte(batch.payload, 0x20);
    PacketWriter::WriteByte(batch.payload, moving_players.size());
    
    for (const auto& p : moving_players) {
        PacketWriter::WriteUInt(batch.payload, p.player_id);
        PacketWriter::WriteShort(batch.payload, p.x);
        PacketWriter::WriteShort(batch.payload, p.y);
        PacketWriter::WriteByte(batch.payload, p.facing);
    }
    batch.size = batch.payload.size();

    // PHASE 4: Serialize once, send to all
    auto bytes = std::make_shared<std::vector<uint8_t>>(batch.ToBytes());
    
    for (auto& session : all_receivers) {
        session->RawSend(bytes);
    }
}
```

---

## Movement Validation

When a move packet arrives:

```cpp
case Protocol::Opcode::C_Move:
    if (data.size() >= 3) {
        uint8_t direction = data[1];
        uint8_t facing = data[2];
        
        // Only move if direction matches facing
        if (direction == facing && direction == player_->GetFacing()) {
            bool moved = player_->AttemptMove(direction, server_->GetMap());
            
            if (moved) {
                is_dirty_ = true;  // Broadcast on next tick
                SendPosition();    // Immediate confirmation to this client
            } else {
                SendPosition();    // Rubber-band client back
            }
        } else {
            // Mismatch: correct client's facing
            player_->SetFacing(direction);
            SendFacingUpdate(player_->GetFacing());
        }
    }
    break;
```

### Why Check direction == facing?

This prevents cheating where a client sends "move right" while facing up. The client must turn first, then move. The server enforces this by checking that the requested move direction matches the player's current facing.

---

## Security Features

### Handshake Validation

- **Timeout:** 5 seconds to complete handshake
- **Protocol version:** Must match server
- **Client magic:** Must be "DYEW" (identifies legitimate clients)
- **Logging:** Failed attempts written to `failed_connections.log`

### Resource Protection

- **Player creation deferred:** `Player` object only created after handshake
- **Session registration deferred:** Session only added to `sessions_` map after handshake
- **This prevents:** Memory exhaustion from malicious connections that never complete handshake

### Movement Validation

- **Bounds checking:** Map validates all positions
- **Wall checking:** Map tracks walkable tiles
- **Direction validation:** Can only move in facing direction
- **Server authoritative:** Client predictions are just predictions

---

## Lua Scripting

### Available Callbacks

```lua
-- Called when a player moves (after server validation)
function on_player_moved(player_id, x, y, facing)
    -- Trigger traps, update AI, etc.
end

-- Called for custom message packets (opcode 0x03)
function process_custom_message(data)
    -- Process and return response
    return modified_data
end
```

### Hot Reloading

The file watcher monitors `scripts/main.lua`. When the file changes:

1. Watcher detects modification time change
2. Calls `ReloadScripts()`
3. Lua state is reset and script reloaded
4. All callbacks become available with new logic

To manually reload, type `r` in the server console.

### Thread Safety

All Lua calls are wrapped with a mutex:

```cpp
void LuaGameEngine::OnPlayerMoved(...) {
    std::lock_guard<std::mutex> lock(lua_mutex_);
    // ... Lua call ...
}
```

---

## Adding a New Feature: Step-by-Step Guides

### Example 1: Adding Attack

**Step 1: Add opcodes to Common.h**

```cpp
namespace Protocol::Opcode {
    // ... existing opcodes ...
    
    // Client -> Server
    constexpr uint8_t C_Attack = 0x40;
    
    // Server -> Client
    constexpr uint8_t S_Damage = 0x31;
}
```

**Step 2: Add handler in ClientConnection.cpp**

In `HandlePacket()` switch:

```cpp
case Protocol::Opcode::C_Attack:
    HandleAttack();
    break;
```

Add the handler method:

```cpp
void ClientConnection::HandleAttack() {
    // Get target position (in front of player)
    int target_x = player_->GetX();
    int target_y = player_->GetY();
    
    switch (player_->GetFacing()) {
        case 0: target_y++; break;  // Up
        case 1: target_x++; break;  // Right
        case 2: target_y--; break;  // Down
        case 3: target_x--; break;  // Left
    }
    
    // Find player at target position
    auto targets = server_->GetPlayersAt(target_x, target_y);
    
    for (auto& target : targets) {
        // Deal damage
        int damage = 10;
        target->TakeDamage(damage);
        
        // Send damage packet to target
        SendDamagePacket(target->GetID(), damage, 
                         target->GetCurrentHP(), target->GetMaxHP());
        
        // Broadcast effect to nearby players
        BroadcastAttackEffect(target_x, target_y);
    }
}
```

**Step 3: Add helper to GameServer**

In `GameServer.h`:

```cpp
std::vector<std::shared_ptr<ClientConnection>> GetPlayersAt(int x, int y);
```

In `GameServer.cpp`:

```cpp
std::vector<std::shared_ptr<ClientConnection>> GameServer::GetPlayersAt(int x, int y) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<std::shared_ptr<ClientConnection>> result;
    
    for (auto& [id, session] : sessions_) {
        auto data = session->GetPlayerData();
        if (data.x == x && data.y == y) {
            result.push_back(session);
        }
    }
    
    return result;
}
```

**Step 4: Extend Player class**

In `Player.h`:

```cpp
class Player {
    // ... existing members ...
    int current_hp_ = 100;
    int max_hp_ = 100;
    
public:
    int GetCurrentHP() const { return current_hp_; }
    int GetMaxHP() const { return max_hp_; }
    void TakeDamage(int amount);
    bool IsDead() const { return current_hp_ <= 0; }
};
```

In `Player.cpp`:

```cpp
void Player::TakeDamage(int amount) {
    current_hp_ = std::max(0, current_hp_ - amount);
}
```

**Step 5: Add send method in ClientConnection**

```cpp
void ClientConnection::SendDamagePacket(uint32_t target_id, int damage, int hp, int max_hp) {
    Packet pkt;
    PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::S_Damage);
    PacketWriter::WriteUInt(pkt.payload, target_id);
    PacketWriter::WriteShort(pkt.payload, damage);
    PacketWriter::WriteShort(pkt.payload, hp);
    PacketWriter::WriteShort(pkt.payload, max_hp);
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}
```

### Example 2: Adding Chat

**Step 1: Add opcodes**

```cpp
namespace Protocol::Opcode {
    constexpr uint8_t ChatMessage = 0x50;  // Bidirectional
}
```

**Step 2: Handle incoming chat in ClientConnection**

```cpp
case Protocol::Opcode::ChatMessage:
    HandleChatMessage(data);
    break;

// ...

void ClientConnection::HandleChatMessage(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    
    size_t offset = 1;  // Skip opcode
    uint8_t channel = PacketReader::ReadByte(data, offset);
    uint16_t length = PacketReader::ReadShort(data, offset);
    
    if (offset + length > data.size()) return;
    
    std::string message(data.begin() + offset, data.begin() + offset + length);
    
    // Validate message (no profanity, length limits, etc.)
    if (message.length() > 200) {
        message = message.substr(0, 200);
    }
    
    // Broadcast to all players
    server_->BroadcastChatMessage(player_->GetID(), channel, message);
}
```

**Step 3: Add broadcast method to GameServer**

```cpp
void GameServer::BroadcastChatMessage(uint32_t sender_id, uint8_t channel, const std::string& message) {
    Packet pkt;
    PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::ChatMessage);
    PacketWriter::WriteUInt(pkt.payload, sender_id);
    PacketWriter::WriteByte(pkt.payload, channel);
    
    std::vector<uint8_t> msg_bytes(message.begin(), message.end());
    PacketWriter::WriteShort(pkt.payload, msg_bytes.size());
    pkt.payload.insert(pkt.payload.end(), msg_bytes.begin(), msg_bytes.end());
    
    pkt.size = pkt.payload.size();
    
    auto bytes = std::make_shared<std::vector<uint8_t>>(pkt.ToBytes());
    
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& [id, session] : sessions_) {
        session->RawSend(bytes);
    }
}
```

### Example 3: Adding Inventory

**Step 1: Create Item structures in Common.h**

```cpp
struct ItemData {
    uint32_t item_id;
    uint16_t item_type;
    uint8_t quantity;
    uint8_t slot;
};

namespace Protocol::Opcode {
    constexpr uint8_t C_UseItem = 0x60;
    constexpr uint8_t C_DropItem = 0x61;
    constexpr uint8_t C_PickupItem = 0x62;
    constexpr uint8_t S_InventoryUpdate = 0x70;
    constexpr uint8_t S_ItemDropped = 0x71;
}
```

**Step 2: Extend Player class**

```cpp
class Player {
    // ... existing ...
    std::array<ItemData, 20> inventory_;  // 20 slots
    
public:
    bool AddItem(const ItemData& item);
    bool RemoveItem(uint8_t slot);
    const ItemData* GetItem(uint8_t slot) const;
    std::vector<ItemData> GetInventory() const;
};
```

**Step 3: Create ItemManager class**

```cpp
// include/server/ItemManager.h
class ItemManager {
public:
    static ItemManager& Instance();
    
    // Ground items
    void DropItem(int x, int y, const ItemData& item);
    std::vector<ItemData> GetItemsAt(int x, int y);
    bool PickupItem(int x, int y, uint32_t item_id, ItemData& out_item);
    
private:
    std::map<std::pair<int,int>, std::vector<ItemData>> ground_items_;
    std::mutex mutex_;
};
```

**Step 4: Handle packets in ClientConnection**

```cpp
case Protocol::Opcode::C_UseItem:
    HandleUseItem(data);
    break;

case Protocol::Opcode::C_DropItem:
    HandleDropItem(data);
    break;

case Protocol::Opcode::C_PickupItem:
    HandlePickupItem(data);
    break;
```

---

## Database Integration

The `DatabaseManager` provides SQLite persistence:

```cpp
// Login or create account
auto account = db_manager_->LoginOrRegister("username");

// Save player position periodically
db_manager_->SavePlayerPosition(user_id, x, y);

// Update stats on level up
db_manager_->UpdatePlayerStats(user_id, level, experience);
```

### Integration Points

1. **On handshake:** Could add authentication packet before/after handshake
2. **On disconnect:** Save player state to database
3. **Periodically:** Auto-save player positions every N minutes

---

## Admin Console

Type commands in the server terminal:

| Command | Action |
|---------|--------|
| `r` | Reload Lua scripts |
| `bandwidth` | Show bandwidth statistics |
| `q` | Quit server |

### Adding Console Commands

In `GameServer::StartConsole()`:

```cpp
void GameServer::StartConsole() {
    std::thread([this]() {
        std::string cmd;
        while (true) {
            std::cout << "Server> ";
            std::getline(std::cin, cmd);
            
            if (cmd == "r") {
                lua_engine_->ReloadScripts();
            }
            else if (cmd == "bandwidth") {
                std::cout << BandwidthMonitor::Instance().GetStats() << std::endl;
            }
            else if (cmd == "players") {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                std::cout << "Connected players: " << sessions_.size() << std::endl;
                for (auto& [id, session] : sessions_) {
                    auto data = session->GetPlayerData();
                    std::cout << "  ID: " << id 
                              << " at (" << data.x << "," << data.y << ")"
                              << " IP: " << session->GetClientIP() << std::endl;
                }
            }
            else if (cmd == "q") {
                exit(0);
            }
        }
    }).detach();
}
```

---

## Performance Considerations

### Packet Batching

Instead of sending individual update packets:
- Collect all dirty players
- Build one batch packet
- Serialize once
- Send same bytes to all clients

This reduces CPU (serialization) and bandwidth (header overhead).

### Lock Minimization

The game loop pattern:
1. Lock briefly to copy data
2. Unlock
3. Process data
4. Lock briefly to send

This prevents the network thread from being blocked during processing.

### Async I/O

All socket operations are asynchronous via asio:
- `async_accept` for new connections
- `async_read` for receiving
- `async_write` for sending

This allows the server to handle many connections without threading per-connection.

### Memory Efficiency

- `shared_ptr` for sessions allows automatic cleanup
- Batch packets use `shared_ptr<vector<uint8_t>>` so one buffer serves all clients
- `unique_ptr` for Player prevents copying

---

## Debugging Tips

### Packet Logging

`ClientConnection::LogPacketReceived()` prints every incoming packet:

```
Packet Received: 11 68 00 07 00 00 01 44 59 45 57 (7 bytes)
                 │  │  │  │  └──────────────────── payload
                 │  │  └──┴── size (big-endian)
                 └──┴── magic bytes
```

### Failed Connections Log

Check `failed_connections.log` for handshake failures:

```
Mon Dec  2 15:23:47 2024
  IP: 192.168.1.50
  Hostname: 192.168.1.50
  Reason: failed to handshake within 5 seconds
---
```

### Bandwidth Monitoring

Type `bandwidth` in console:

```
OUT: 12.5 KB/s (avg: 10.2 KB/s) | 45 pkt/s | Total: 1.2 MB
```

### Common Issues

**"Access violation" crash:** Usually a null pointer. Check if `player_` is accessed before handshake completes. After our refactor, `player_` only exists after `CompleteHandshake()`.

**Clients not seeing each other:** Check that sessions are being added to `sessions_` map. Only happens after `RegisterSession()` is called.

**Movement not working:** Check `HandlePacket()` switch statement. Verify opcode matches. Check if direction/facing validation is failing.

---

## File Organization

```
dyewars_server/
├── CMakeLists.txt
├── include/
│   ├── database/
│   │   └── DatabaseManager.h      # SQLite integration
│   ├── lua/
│   │   └── LuaEngine.h            # Lua scripting engine
│   └── server/
│       ├── BandwidthMonitor.h     # Network statistics
│       ├── ClientConnection.h     # Per-client session
│       ├── Common.h               # Protocol, packets, utilities
│       ├── GameMap.h              # Map data
│       ├── GameServer.h           # Main server class
│       └── Player.h               # Player data
├── scripts/
│   └── main.lua                   # Game logic script
└── src/
    ├── lua/
    │   └── LuaEngine.cpp
    ├── server/
    │   ├── ClientConnection.cpp
    │   ├── GameMap.cpp
    │   ├── GameServer.cpp
    │   └── Player.cpp
    └── main.cpp                   # Entry point
```

---

## Quick Reference: Adding Features Checklist

1. **Add opcode(s)** in `Common.h` under `Protocol::Opcode`
2. **Add handler case** in `ClientConnection::HandlePacket()` switch
3. **Implement handler method** in `ClientConnection.cpp`
4. **Add send method** in `ClientConnection` for server→client packets
5. **Extend Player** if new player state needed
6. **Extend GameMap** if new world state needed
7. **Add broadcast helper** to `GameServer` if needed
8. **Add Lua callback** if game logic should be scriptable
9. **Test** with packet logging and console commands

---

## Security Checklist for New Features

When adding features, always consider:

1. **Input validation:** Check packet sizes before reading
2. **Bounds checking:** Validate coordinates against map
3. **Rate limiting:** Prevent spam attacks (e.g., chat flood)
4. **Authority:** Server decides outcomes, not client
5. **Logging:** Record suspicious activity
6. **Resource limits:** Cap arrays, strings, iterations
