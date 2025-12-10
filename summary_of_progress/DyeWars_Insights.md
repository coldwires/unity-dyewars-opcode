# DyeWars Server - C++ Insights & Breakthroughs

A compilation of key learning moments from building the DyeWars multiplayer game server.

---

## Memory & Ownership

### Insight: References Can't Point to Temporaries

**The "aha!" moment:**
```cpp
const bool& Boop() { return false; }  // BROKEN - dangling reference!
```

When you return by reference, you're returning "here's where the thing lives." A temporary like `false` dies after the function returns — the reference points to garbage.

**The rule:**
| Return type | When to use |
|-------------|-------------|
| `T` (value) | Returning locals, temporaries, computed results |
| `const T&` | Returning a member that outlives the call |
| `T&` | Returning a member the caller can modify |

```cpp
std::string MakeName() { return "Bob"; }           // By value ✓
const std::string& GetName() { return name_; }     // Reference to member ✓
const std::string& Bad() { return "Bob"; }         // Dangling! ✗
```

---

### Insight: Iterators Point to Pairs in Maps

**The confusion:**
```cpp
auto player_it = players_.find(player_id);
dirty_players_.erase(player_it);  // Wrong! player_it is for players_, not dirty_players_
```

**The clarity:**
```cpp
auto player_it = players_.find(player_id);

player_it->first   // The KEY: uint64_t (player_id)
player_it->second  // The VALUE: std::shared_ptr<Player>
```

Iterators to `std::unordered_map<K, V>` point to `std::pair<const K, V>`. Use `.first` for key, `.second` for value.

---

### Insight: You Only Delete What You Own

**The question:** "If my Player has a pointer to a target, do I delete it in the destructor?"

**The answer:** No. Raw pointers don't imply ownership.

```cpp
class Player {
    Entity* target_ = nullptr;  // Just pointing at something World owns
    
    ~Player() {
        // DON'T delete target_!
        // We're just looking at it, we don't own it
    }
};
```

**Ownership cheat sheet:**
| Type | Owns? | Delete in destructor? |
|------|-------|----------------------|
| `T*` (raw pointer) | No | No |
| `std::unique_ptr<T>` | Yes | Automatic |
| `std::shared_ptr<T>` | Shared | Automatic when refcount = 0 |

---

## shared_ptr & Atomics

### Insight: The "Intuitive" Way is the Worst

**The surprise:**
```cpp
// Intuitive but WORST (2 atomic ops)
void Add(shared_ptr<T> obj);
Add(obj);

// Slightly weird but BEST for keeping copy (1 atomic op)
void Add(const shared_ptr<T>& obj);
Add(obj);

// Explicit transfer, BEST when giving up (0 atomic ops)
void Add(shared_ptr<T> obj);
Add(std::move(obj));
```

**Why?** Every `shared_ptr` copy does an atomic refcount increment (~20-50ns). Passing by const reference avoids the copy. Moving transfers ownership with zero atomics.

**The rule:** Default to `const shared_ptr<T>&`. Only use by-value when the function name signals ownership transfer and callers `std::move`.

---

### Insight: Atomics Protect Values, Mutexes Protect Containers

**The confusion:** "If `shared_ptr` refcount is always atomic, why do I need a mutex?"

**The clarity:**
```cpp
std::unordered_map<uint64_t, shared_ptr<Player>> players_;

// Thread 1: Writing
players_[1] = playerA;  // Might trigger rehash!

// Thread 2: Reading (same time)
auto it = players_.find(1);  // UNDEFINED BEHAVIOR
```

The `shared_ptr` refcount is atomic, but the **map's internal structure** isn't. During rehash, the map reallocates buckets — reading mid-rehash reads freed memory.

**Summary:**
| Thing | Thread-safe? |
|-------|--------------|
| `shared_ptr` refcount | Yes (atomic) |
| `std::map` / `unordered_map` | No (needs mutex) |
| `std::vector` | No (needs mutex) |
| `std::atomic<T>` | Yes |

---

### Insight: Even Plain Int Needs Protection

```cpp
int x = 0;

// Thread 1
x = 5;

// Thread 2
y = x;  // Might read partial write, garbage, or stale cache
```

If two threads access the same variable and at least one writes, you need synchronization — even for `int`.

---

## Thread Architecture

### Insight: Game Loop = Race Car, Worker Thread = Delivery Truck

**The principle:**
```
GAME LOOP (hot path, 33ms budget):
├── Must complete on time or players feel lag
├── Use atomics, lock-free structures
├── Never wait on anything
└── If you MUST lock, hold for microseconds max

WORKER THREAD (no time budget):
├── Can block for milliseconds
├── Mutex locks are fine
├── Database calls, file I/O, HTTP requests
└── Slow analysis, logging
```

**Bad — database in game loop:**
```cpp
void GameLoop::ProcessUpdates() {
    std::lock_guard<std::mutex> lock(db_mutex_);  // Blocks game loop!
    database_.Save(player);  // 5-50ms, catastrophic
}
```

**Good — queue work for worker:**
```cpp
void GameLoop::ProcessUpdates() {
    worker_.Push({SAVE_PLAYER, player.id, player.Serialize()});  // ~10ns
}

void Worker::ProcessJob(const Job& job) {
    std::lock_guard<std::mutex> lock(db_mutex_);  // Fine here
    database_.Save(job.player_id, job.data);       // Take your time
}
```

---

### Insight: Single-Threaded Access = No Locks Needed

If everything goes through `QueueAction()` and executes on the game thread, you have single-threaded access to game state — no locks required.

```
Network Thread                     Game Thread
     │                                  │
     │  server->QueueAction([&] {       │
     │      players_.CreatePlayer()  ───┼──► Runs here
     │  });                             │
```

Locks are only needed when multiple threads access the same data.

---

## Architecture Patterns

### Insight: Separation of Concerns — Static vs Dynamic Data

**The realization:** TileMap (static terrain) and SpatialHash (dynamic entities) are different concerns.

| | TileMap | SpatialHash |
|---|---------|-------------|
| Data | Tiles, walls | Player positions |
| Changes | Rarely | Every tick |
| Query | "Can I walk here?" | "Who's near here?" |

World owns both and coordinates queries. Clean separation means adding NPCs later doesn't bloat TileMap.

---

### Insight: Big O Matters for Multiplayer

**The problem:**
```cpp
// Naive: check all players for every viewer
for (auto& viewer : all_players) {           // N viewers
    for (auto& player : dirty_players) {     // M dirty
        if (InView(viewer, player)) ...      // N × M checks
    }
}
// 500 viewers × 50 dirty = 25,000 checks per tick
```

**The solution:** Spatial hash gives O(K) lookups where K = players in nearby cells (~20), not O(N) where N = all players (500).

```cpp
// For each dirty player, find nearby viewers
for (auto& dirty : dirty_players) {
    auto viewers = spatial_hash.GetNearby(dirty.x, dirty.y);  // O(K)
    // ...
}
// 50 dirty × 20 nearby = 1,000 checks per tick
```

25x fewer checks.

---

## Protocol & Serialization

### Insight: Sign Bit Corruption in Packet Reading

**The bug:**
```cpp
uint16_t ReadShort(const std::vector<uint8_t>& buffer, size_t offset) {
    return (buffer[offset] << 8) | buffer[offset + 1];  // BUG!
}
```

`uint8_t` promotes to **signed** `int` when shifting. If `buffer[offset] >= 0x80`, the sign bit gets set, corrupting the result.

**The fix:**
```cpp
uint16_t ReadShort(const std::vector<uint8_t>& buffer, size_t offset) {
    return (static_cast<uint16_t>(buffer[offset]) << 8) | 
            static_cast<uint16_t>(buffer[offset + 1]);
}
```

Cast to target type **before** shifting to ensure unsigned arithmetic.

---

## Performance Wisdom

### Insight: "Premature Optimization" Has Limits

Counted atomic operations in `SendPacket`: 7 atomics per packet.

**The analysis:** 7 × 50ns = 350ns per packet. Network I/O takes 100+ microseconds. The atomics are ~0.3% of total send time.

**The conclusion:** Not worth optimizing. Build the game first, profile later.

---

### Insight: Return Value Optimization (RVO) Makes Copies Free

```cpp
std::vector<Player> GetPlayers() {
    std::vector<Player> result;
    // ... fill it ...
    return result;  // No copy! Compiler optimizes this.
}
```

Modern compilers eliminate the copy via RVO/NRVO. Returning by value is often free.

---

### Insight: Bandwidth Math for Multiplayer

```
Naive broadcast (500 players, 50 dirty):
500 × (6 header + 50×13 bytes) = 328 KB per tick

View-based (10 visible average):
500 × (6 header + 10×13 bytes) = 68 KB per tick

5x bandwidth reduction!
```

View-based broadcasting isn't optional at scale — it's mandatory.

---

## Debugging Wisdom

### Insight: Log Outside the Lock

```cpp
// Bad: I/O while holding lock
void RemovePlayer(uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_.erase(id);
    Log::Info("Removed {}", id);  // Slow I/O blocks other threads!
}

// Good: Log after releasing
void RemovePlayer(uint64_t id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        players_.erase(id);
    }
    Log::Info("Removed {}", id);  // No lock held
}
```

---

### Insight: `std::unordered_set::erase()` is Safe on Missing Elements

```cpp
dirty_players_.erase(player);  // Fine even if player isn't in the set
```

Returns count removed (0 or 1). No need to check `contains()` first.

---

## Quick Reference

### When to Lock
- Multiple threads access same data AND at least one writes → Lock
- Single thread only → No lock needed
- Read-only after initialization → No lock needed
- `std::atomic<T>` → No lock needed (but only for simple types)

### Passing shared_ptr
- Reading: `const shared_ptr<T>&`
- Storing a copy: `const shared_ptr<T>&` then copy inside
- Taking ownership: `shared_ptr<T>` by value, caller `std::move`s

### Returning from Functions
- New/computed values: Return by value
- Existing members: Return by reference (if always valid) or pointer (if might be null)

### Container Thread Safety
- All STL containers need external synchronization
- Only `std::atomic<T>` is inherently thread-safe
