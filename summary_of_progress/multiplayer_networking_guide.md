# Multiplayer Game Networking Guide

A comprehensive guide covering real-time multiplayer game architecture, from basic TCP/UDP decisions to scaling to thousands of players.

---

## Table of Contents

1. [TCP vs UDP](#tcp-vs-udp)
2. [Client-Server Architecture](#client-server-architecture)
3. [Delta Compression](#delta-compression)
4. [Server Tick Rate & Interpolation](#server-tick-rate--interpolation)
5. [Projectile Syncing](#projectile-syncing)
6. [Lag Compensation & Server Rewind](#lag-compensation--server-rewind)
7. [Preventing Lag Switch Cheats](#preventing-lag-switch-cheats)
8. [Threading Architecture](#threading-architecture)
9. [Bandwidth Considerations](#bandwidth-considerations)
10. [Thread Safety: Atomics vs Mutexes](#thread-safety-atomics-vs-mutexes)
11. [Scaling to Many Players](#scaling-to-many-players)

---

## TCP vs UDP

### TCP (Transmission Control Protocol)
- Guarantees delivery and ordering
- If packet 5 is lost, packets 6, 7, 8 are blocked until retransmission
- Good for: login, chat, inventory changes, important events

### UDP (User Datagram Protocol)
- Fire and forget - no delivery guarantee
- No head-of-line blocking
- Good for: position updates, input streams, anything where latest-value-wins

### Hybrid Architecture (Recommended)

```
TCP channel (reliable, ordered):
  - Player joined/left
  - Chat messages
  - Inventory changes
  - Round start/end
  - Important game events

UDP channel (unreliable, fast):
  - Position snapshots (20-30/sec)
  - Player inputs
  - Fast-updating state
```

---

## Client-Server Architecture

### Authoritative Server Model

The server is the source of truth. Clients are "dumb terminals" that send inputs and render what the server tells them.

### The Flow (Example: Barrel Roll)

```
TICK 0: Player presses roll button

CLIENT (immediately):
  - Predicts: state = ROLLING, roll_tick = 0
  - Starts roll animation locally
  - Applies 2x velocity
  - Sends to server: { input: ROLL, tick: 0 }

SERVER (receives ~30-50ms later):
  - Validates: can this player roll?
  - If valid: state = ROLLING, apply velocity
  - Runs physics, checks collisions
  - Sends back state in next snapshot

CLIENT (receives snapshot):
  - Compares server state to predicted state
  - If close: smooth interpolation
  - If different: snap to server state
```

### Key Techniques

1. **Client-side prediction** - Run game logic locally for immediate feedback
2. **Server reconciliation** - When server state arrives, correct if needed
3. **Entity interpolation** - Render other players slightly in the past, interpolating between snapshots
4. **Input buffering** - Server buffers inputs to smooth out jitter

### Smoothing Corrections

```cpp
float error = distance(predicted_pos, server_pos);
if (error < 0.5f) {
    // Small error: blend smoothly
    player.pos = lerp(player.pos, server_pos, 0.2f);
} else {
    // Big desync: snap immediately
    player.pos = server_pos;
}
```

---

## Delta Compression

Instead of sending full world state every tick, send only what changed since the last acknowledged snapshot.

### Full Snapshot (Naive)

```
Tick 100: [Entity1: x=100, y=200, vel=5, hp=100, state=running]
          [Entity2: x=400, y=150, vel=0, hp=80, state=idle]
          [Entity3: x=50, y=300, vel=2, hp=100, state=jumping]
          ... repeat for 200 entities every tick
```

### Delta Compressed

```
Client: "I've acknowledged tick 97"

Server: "Here's tick 100, delta from 97"
        [Entity1: x=+15]              // only x changed
        [Entity3: x=+6, state=falling] // two fields changed
        // Entity2 not mentioned - unchanged
```

### Implementation

```cpp
struct EntityDelta {
    uint16_t id;
    uint8_t changed_mask;  // bitfield: which fields changed
    // only include fields where bit is set
};

// changed_mask bits:
// 0x01 = x changed
// 0x02 = y changed
// 0x04 = vel_x changed
// ...
```

### The Acknowledgment Loop

```
Server keeps: last N snapshots (ring buffer)
Client sends: "last snapshot I received was tick 97"
Server diffs: current state vs tick 97, sends delta
```

---

## Server Tick Rate & Interpolation

### Tick Rate vs Frame Rate

```
Server tick rate: 20-30 ticks/sec (sends snapshots)
Client frame rate: 60 fps (renders smoothly)

Server sends:    Tick 0 -------- Tick 1 -------- Tick 2
                 (33ms gap at 30 ticks/sec)

Client renders:  Frame 0, 1, 2, 3, 4, 5, 6, 7, 8, 9...
                 (interpolating between server snapshots)
```

### Bandwidth Math (30 players)

```
Per player snapshot: ~20 bytes
30 players:          600 bytes per snapshot
At 30 ticks/sec:     18 KB/sec per client
30 clients:          ~540 KB/sec total server outbound
```

### Optimization: Dirty Flag

Only include entities that changed:

```cpp
for each player:
    if player.state != IDLE or player.moved_this_tick:
        mark_dirty(player)

broadcast_dirty_entities()  // only send what changed
```

---

## Projectile Syncing

### Fast Projectiles (Hitscan - Bullets)

Don't sync the projectile. It's instant.

```
Client: "I fired at tick 50, direction = 0.7, 0.3"

Server:
  - Rewinds world to tick 50 (lag compensation)
  - Raycasts from player's position at tick 50
  - Checks hits against enemy positions at tick 50
  - If hit: apply damage, broadcast result

Client just draws muzzle flash and tracer locally.
```

### Slow Projectiles (Arrows, Rockets)

These need to be actual synced entities:

```
Client: "I fired arrow at tick 50"

Server:
  - Spawns projectile with ID, position, velocity
  - Broadcasts: { SPAWN_PROJECTILE, id, x, y, vel_x, vel_y }
  - Projectile lives in server simulation
  - Included in UDP snapshots

Clients:
  - Receive spawn event
  - Create local projectile
  - Interpolate toward server snapshots if drift occurs
```

### Hit Disagreement

Server is always authoritative on hits. Options:

1. **Server-only detection** - Server detects collision, broadcasts result
2. **Lag compensation (shooter-favored)** - If shooter saw the hit, it probably counts
3. **Favor the target** - Only count if both agree (feels bad for shooter)

---

## Lag Compensation & Server Rewind

### The Rewind Buffer

```cpp
#define HISTORY_SIZE 64  // ~2 seconds at 30 ticks/sec

struct WorldSnapshot {
    uint32_t tick;
    PlayerState players[MAX_PLAYERS];
};

class Server {
    WorldSnapshot history[HISTORY_SIZE];
    int current_index = 0;
    
    void tick() {
        // Save current state before simulating
        history[current_index] = capture_current_state();
        current_index = (current_index + 1) % HISTORY_SIZE;
        
        // Now simulate
        process_inputs();
        check_collisions();  // uses lag compensation
    }
    
    WorldSnapshot* get_state_at_tick(uint32_t tick) {
        for (int i = 0; i < HISTORY_SIZE; i++) {
            if (history[i].tick == tick) return &history[i];
        }
        return nullptr;
    }
};
```

### Lag Compensated Hit Detection

```cpp
void check_arrow_hit(Projectile* arrow) {
    // Arrow position NOW on server
    float arrow_x = arrow->x;
    float arrow_y = arrow->y;
    
    // Get shooter's latency
    Player* shooter = get_player(arrow->owner_id);
    uint32_t rewind_ticks = ms_to_ticks(shooter->latency_ms);
    
    // Rewind to where shooter SAW targets
    uint32_t check_tick = current_tick - rewind_ticks;
    WorldSnapshot* past = get_state_at_tick(check_tick);
    
    // Check collision against PAST player positions
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i == arrow->owner_id) continue;
        
        PlayerState* past_player = &past->players[i];
        
        if (collides(arrow, past_player)) {
            // HIT! Apply damage to CURRENT player
            apply_damage(i, arrow->damage);
            arrow->alive = false;
            broadcast_hit(arrow->id, i);
            return;
        }
    }
}
```

### Max Rewind Cap

```cpp
// Don't let high-ping players shoot too far into the past
uint32_t max_rewind = ms_to_ticks(150);  // cap at 150ms
uint32_t actual_rewind = min(shooter_latency_ticks, max_rewind);
```

---

## Preventing Lag Switch Cheats

### The Attack

Cheaters artificially inflate ping to get more rewind time:

```cpp
// Cheater's hooked client
void handle_ping_request(Packet* pkt) {
    sleep(150);  // fake delay
    send({ type: PING_RESPONSE, server_time: pkt->server_time });
}
```

### Server-Side Latency Measurement

Never trust client-reported timing. Server measures latency:

```cpp
void send_ping_request(Player* player) {
    player->ping_send_tick = current_tick;
    send_udp(player, { type: PING_REQUEST, server_time: get_time_ms() });
}

void handle_ping_response(Player* player, Packet* pkt) {
    uint32_t rtt = get_time_ms() - pkt->server_time;
    player->ping_ms = rolling_average(player->ping_samples, rtt);
    player->latency_ms = player->ping_ms / 2;
}
```

### Detection Methods

1. **Correlate packet timing with claimed latency**
   - High ping but inputs arrive consistently? Suspicious.

2. **Watch for selective lag**
   - Compare ping measurement vs observed input round-trip

3. **Ping stability check**
   - Real connections drift gradually
   - Lag switches cause sudden spikes (often right before shooting)

4. **Nuclear option**
   - Suspicious players get NO lag compensation

```cpp
if (shooter->lag_switch_score > THRESHOLD) {
    rewind_ms = 0;  // no lag comp for you
}
```

---

## Threading Architecture

### Thread Separation

```
┌─────────────────────────────────────────────────────────────┐
│  GAME THREAD (hot path, never block)                        │
│  - 30 tick/sec loop                                         │
│  - Process inputs from queue                                │
│  - Physics / collision                                      │
│  - Build and send snapshots                                 │
│  - Must complete in <33ms                                   │
└─────────────────────────────────────────────────────────────┘
        │                          ▲
        │ push work                │ results via queue
        ▼                          │
┌─────────────────────────────────────────────────────────────┐
│  WORKER THREAD(S) (slow stuff)                              │
│  - Anti-cheat analysis                                      │
│  - Database writes                                          │
│  - JSON parsing                                             │
│  - File I/O, logging                                        │
│  - Ban checks against external API                          │
└─────────────────────────────────────────────────────────────┘
        │
        ▼
┌─────────────────────────────────────────────────────────────┐
│  I/O THREAD(S) (network)                                    │
│  - Receive packets, push to game thread queue               │
│  - Send packets from outbound queue                         │
└─────────────────────────────────────────────────────────────┘
```

### Lock-Free Communication

```cpp
Queue<InputPacket> input_queue;      // IO -> Game
Queue<OutputPacket> output_queue;    // Game -> IO
Queue<Job> work_queue;               // Game -> Worker
Queue<Result> result_queue;          // Worker -> Game
```

### What Goes Where

**Game Loop (must be fast):**
- Input processing
- Physics simulation
- Snapshot building
- Atomic counter updates

**Worker Thread (can be slow):**
- Database operations
- File I/O
- Complex analysis
- JSON parsing
- Anything with mutex locks

---

## Bandwidth Considerations

### Typical Targets

```
Snapshot size:    < 1 KB per player
Tick rate:        20-30 for most games, 60-128 for competitive shooters
Per-player:       < 50 KB/sec outbound
Total server:     < 10 Mbps for 30 players
Game thread:      < 10ms per tick
```

### What Actually Slows Servers

1. **CPU per packet** - Don't parse JSON for every packet
2. **Allocations per packet** - Use object pools
3. **Syscall overhead** - Batch sends into one buffer

```cpp
// BAD - one send per entity
for (Entity& e : entities) {
    send(socket, &e, sizeof(e));  // syscall each time
}

// GOOD - batch into one send
Buffer buf;
for (Entity& e : entities) {
    buf.write(&e, sizeof(e));
}
send(socket, buf.data(), buf.size());  // one syscall
```

---

## Thread Safety: Atomics vs Mutexes

### When Two Threads Access Same Variable

```cpp
// Without protection - BROKEN
uint64_t counter = 0;

// Thread A and B both do counter++
// CPU actually does: READ -> ADD -> WRITE (3 steps)
// Can interleave, lose increments
```

### Mutex (Heavy)

```cpp
std::mutex mutex_;
int counter_ = 0;

void Increment() {
    std::lock_guard<std::mutex> lock(mutex_);  // 20-200ns, blocks others
    counter_++;
}
```

### Atomic (Light)

```cpp
std::atomic<uint64_t> counter_{0};

void Increment() {
    counter_.fetch_add(1, std::memory_order_relaxed);  // 3-10ns, never blocks
}
```

### When to Use Which

| Atomic | Mutex |
|--------|-------|
| Single variable | Multiple related variables |
| Simple operations (add, swap) | Complex logic |
| No invariants between values | Values must stay in sync |
| 3-10ns | 20-200ns+ (more with contention) |

### Memory Ordering

```cpp
// RELAXED - fastest, no ordering guarantees
counter_.fetch_add(1, std::memory_order_relaxed);
// Fine for counters where exact timing doesn't matter

// SEQ_CST - slowest, full ordering guarantees
counter_.fetch_add(1, std::memory_order_seq_cst);
// Use when operations must be seen in specific order
```

### Contention

When multiple threads fight over the same lock:

```
Thread A: lock() -> work -> unlock()
Thread B:      lock() BLOCKED........... -> finally got it -> work
Thread C:           lock() BLOCKED........................... -> work

// Threads wait in line, wasting time
```

Atomics never block - hardware handles conflicts at the cache line level.

---

## Scaling to Many Players

### The Network Math Problem

```
Naive: everyone sees everyone
10,000 players × 10,000 players × 10 bytes = 1 GB per tick
× 30 ticks/sec = 30 GB/sec outbound

That's 240 Gbps. Impossible.
```

### Solution 1: Interest Management

Only send nearby players:

```cpp
void BuildSnapshotForPlayer(Player& p) {
    std::vector<PlayerState> nearby;
    
    for (auto& other : all_players_) {
        if (distance(p.pos, other.pos) < VIEW_RADIUS) {
            nearby.push_back(other.GetState());
        }
    }
    
    SendSnapshot(p, nearby);  // 50-100 players, not 10,000
}
```

### Solution 2: Spatial Partitioning

```cpp
class SpatialHash {
    std::unordered_map<int, std::vector<Player*>> cells_;
    
    int CellKey(int x, int y) {
        return (x / CELL_SIZE) + (y / CELL_SIZE) * GRID_WIDTH;
    }
    
    std::vector<Player*> GetNearby(int x, int y) {
        std::vector<Player*> result;
        // Check this cell + 8 neighbors
        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                int key = CellKey(x + dx * CELL_SIZE, y + dy * CELL_SIZE);
                auto& cell = cells_[key];
                result.insert(result.end(), cell.begin(), cell.end());
            }
        }
        return result;
    }
};
```

### Solution 3: Zone Servers

```
┌─────────┬─────────┬─────────┐
│ Zone A  │ Zone B  │ Zone C  │
│ 500     │ 500     │ 500     │
│ players │ players │ players │
└─────────┴─────────┴─────────┘

Each zone only processes players in that zone.
Truly independent = no synchronization overhead.
```

### Solution 4: Multiple Servers

```
Level 1: Single executable, one machine
         Max ~1000-3000 players

Level 2: Multiple executables, one machine
         Different ports, share resources

Level 3: Multiple machines, one datacenter
         Each has own NIC: 10 machines × 10Gbps = 100Gbps

Level 4: Multiple datacenters worldwide
         US-East, US-West, EU, Asia
         Players connect to nearest
```

### Inter-Server Communication

```
┌────────────┐ ┌────────────┐ ┌────────────┐
│ Zone Srv 1 │ │ Zone Srv 2 │ │ Zone Srv 3 │
└─────┬──────┘ └─────┬──────┘ └─────┬──────┘
      │              │              │
      └──────────────┼──────────────┘
                     │
      ┌──────────────┼──────────────┐
      ▼              ▼              ▼
┌──────────┐  ┌──────────┐  ┌──────────┐
│   Chat   │  │  Guild   │  │ Database │
│  Server  │  │  Server  │  │  Cluster │
└──────────┘  └──────────┘  └──────────┘
```

Servers trade messages for:
- Player zone transfers
- Global chat
- Guild updates
- Auction/economy

### Realistic Scaling Progression

```
Phase 1: Single server, 100 players
Phase 2: Single server + spatial hashing, 500 players
Phase 3: Multiple zone processes, same machine, 2000 players
Phase 4: Multiple machines, one datacenter, 10,000 players
Phase 5: Multiple datacenters, 100,000+ players
```

---

## Quick Reference

### Protocol Choice
- Position updates: UDP
- Important events: TCP
- Hybrid is standard

### Tick Rates
- Server: 20-30 ticks/sec (60-128 for competitive)
- Client: 60fps, interpolate between snapshots

### Latency Budget
- Cap lag compensation at ~150ms
- Measure ping server-side, never trust client

### Thread Safety
- Game loop: atomics, lock-free, never block
- Worker thread: mutexes fine, do slow stuff here

### Scaling
- Interest management first
- Spatial partitioning second
- Zone separation third
- Multiple servers/datacenters for massive scale
