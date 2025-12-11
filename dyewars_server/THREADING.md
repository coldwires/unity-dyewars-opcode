# Threading Model

This document describes the threading architecture of DyeWarsServer.

## Thread Overview

The server uses three threads:

| Thread | Purpose | Lifetime |
|--------|---------|----------|
| **Main** | Console command loop | Program start → exit |
| **IO** | ASIO network I/O | Server start → shutdown |
| **Game** | Game logic at 20 TPS | Server start → shutdown |

```
┌─────────────────────────────────────────────────────────────────┐
│ MAIN THREAD                                                     │
│ - Console commands (start/stop/stats/exit)                      │
│ - Spawns IO and Game threads                                    │
└──────────────────┬──────────────────────────────────────────────┘
                   │
        ┌──────────┴──────────┐
        │                     │
        ▼                     ▼
┌───────────────────┐  ┌─────────────────────────────┐
│ IO THREAD         │  │ GAME THREAD                 │
│ - ASIO event loop │  │ - 20 TPS fixed timestep     │
│ - Accept conns    │  │ - Owns all game state       │
│ - Receive packets │  │ - Processes action queue    │
│ - Send packets    │  │ - Broadcasts updates        │
└───────────────────┘  └─────────────────────────────┘
```

## Data Ownership

### Game Thread Owns (No Synchronization Needed)
- `World` - spatial authority
- `SpatialHash` - player positions
- `VisibilityTracker` - who sees who
- `PlayerRegistry` - player lifecycle
- `Player` - individual player state

These use `ThreadOwner` assertions to catch violations in debug builds.

### Shared Between Threads (Synchronization Required)

| Data | Sync Method | Writers | Readers |
|------|-------------|---------|---------|
| `action_queue_` | Mutex | IO | Game |
| `send_queue_` (per conn) | Mutex | Game | IO |
| `ClientManager::clients_` | Mutex | Game, IO | Game, IO |
| `ConnectionLimiter` | Mutex | IO | IO |
| `BandwidthMonitor` stats | Atomics | IO | All |
| `ping_sent_time_` | Atomic | Game | IO |
| `disconnecting_` | Atomic | Any | Any |
| `server_running_` | Atomic | Main | Game |

### Immutable After Construction (No Sync Needed)
- `client_id_`
- `client_ip_`
- `client_hostname_`

## Communication Patterns

### IO → Game: Action Queue
```cpp
// IO thread receives packet, queues work for game thread
server->QueueAction([=]() {
    // This runs on game thread
    auto player = players_.GetByClientID(client_id);
    player->AttemptMove(...);
});
```

### Game → IO: Send Queue
```cpp
// Game thread queues packet, IO thread sends
client->QueuePacket(pkt);  // Thread-safe, dispatches to IO
```

### Efficient Queue Processing
```cpp
// Game thread: swap-and-process pattern
void ProcessActionQueue() {
    std::queue<...> to_process;
    {
        std::lock_guard lock(action_mutex_);
        std::swap(to_process, action_queue_);  // O(1) swap
    } // Lock released immediately

    while (!to_process.empty()) {
        to_process.front()();  // Execute without lock
        to_process.pop();
    }
}
```

## Debug Assertions

Game-thread-only classes use `ThreadOwner` for debug enforcement:

```cpp
void SpatialHash::Add(...) {
    AssertGameThread();  // Crashes in debug if wrong thread
    // ... implementation
}
```

In release builds, these compile to nothing (zero overhead).

## Adding New Shared Data

When adding data accessed from multiple threads:

1. **Prefer thread confinement** - Can it live on just one thread?
2. **Use atomics for simple values** - counters, flags, timestamps
3. **Use mutex for complex data** - maps, vectors, structs
4. **Minimize lock scope** - lock only for data access, not computation
5. **Document the contract** - which threads read/write?

## Testing Thread Safety

### ThreadSanitizer (TSAN)
```bash
# Build with TSAN
cmake -B build -DENABLE_TSAN=ON
cmake --build build

# Run - TSAN reports data races at runtime
./build/DyeWarsServer
```

### Manual Testing
1. Run with multiple simultaneous connections
2. Rapid connect/disconnect cycles
3. High packet rate stress testing
4. Sudden shutdown during activity

## Common Pitfalls

### DON'T: Access game state from IO thread
```cpp
// BAD - Called from PacketHandler (IO thread)
void HandleMove(Client* client) {
    auto player = registry.GetByClientID(...);  // WRONG THREAD!
    player->SetPosition(...);
}
```

### DO: Queue work for game thread
```cpp
// GOOD - Queue action, execute on game thread
void HandleMove(Client* client) {
    server->QueueAction([=]() {
        auto player = registry.GetByClientID(...);  // Game thread - OK
        player->AttemptMove(...);
    });
}
```

### DON'T: Hold locks during I/O
```cpp
// BAD - Lock held during slow operation
void Broadcast() {
    std::lock_guard lock(mutex_);
    for (auto& client : clients_) {
        client->Send(...);  // Slow! Blocks other threads
    }
}
```

### DO: Snapshot and release
```cpp
// GOOD - Lock only for snapshot
void Broadcast() {
    std::vector<ClientPtr> snapshot;
    {
        std::lock_guard lock(mutex_);
        snapshot = {clients_.begin(), clients_.end()};
    } // Lock released

    for (auto& client : snapshot) {
        client->Send(...);  // No lock held
    }
}
```
