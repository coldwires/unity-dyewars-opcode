# Performance Optimizations - DyeWars Server

This document details the performance investigation and optimizations made to reduce tick time with 2000+ concurrent players.

## Problem Statement

With 2000 spread bots, tick time was exceeding the 50ms budget (20 TPS target), reaching ~78ms. The debug dashboard was created to identify bottlenecks.

---

## Debug Infrastructure Created

### Debug HTTP Server (`DebugHttpServer.h/cpp`)

A lightweight HTTP server on port 8082 serving:
- `/` - Real-time HTML dashboard with auto-refresh
- `/stats` or `/stats.json` - JSON API for stats

### ServerStats (`ServerStats.h`)

Thread-safe stats collection using:
- `std::mutex` for tick history (rolling average)
- `std::atomic<double>` for timing breakdowns (lock-free cross-thread reads)
- `std::atomic<size_t>` for counters

**Key metrics tracked:**
- Tick time (avg, max, last)
- Connection counts (real, fake, total)
- Bandwidth (current, average, total bytes out)
- Bot movement breakdown (spatial, visibility, departure time)
- Broadcast breakdown (viewer query, client lookup, packet send)
- Viewer query sub-breakdown (pure spatial hash, AddKnown time)

---

## Optimizations Implemented

### 1. Flat Grid for Spatial Hash

**Problem:** `std::unordered_map` lookups have O(1) average but with hash computation overhead and cache-unfriendly memory access patterns.

**Solution:** Pre-allocate a flat 2D array (`std::vector<std::vector<shared_ptr<Player>>>`) indexed by cell coordinates.

```cpp
// Before: Hash map lookup
auto cell_it = cell_entities_.find(key);

// After: Direct array indexing
size_t idx = cy * grid_width_ + cx;
auto& cell = flat_grid_[idx];
```

**Location:** `SpatialHash.h`
- `InitFlatGrid(world_width, world_height)` - Called from World constructor
- Modified `Add()`, `Remove()`, `Update()` to maintain flat grid
- Modified `GetNearbyEntities()` and `ForEachNearby()` to use flat grid

**Impact:** Reduced spatial query time from ~20ms to ~6-8ms

### 2. Zero-Copy Iteration (`ForEachNearby`)

**Problem:** `GetNearbyEntities()` returns `std::vector<shared_ptr<Player>>`, causing:
- Vector allocation
- shared_ptr reference count increments (atomic operations)

**Solution:** Template callback function that iterates without copying.

```cpp
template<typename Func>
void ForEachNearby(int16_t x, int16_t y, int16_t range, Func&& func) const {
    // ... iterate cells ...
    for (const auto& entity : flat_grid_[idx]) {
        if (entity) func(entity);  // No copy, just reference
    }
}
```

**Location:** `SpatialHash.h`, used via `World::ForEachPlayerInRange()`

**Usage in hot path:** `GameServer::BroadcastDirtyPlayers()`

### 3. Fixed Stale Position Bug in SpatialHash

**Problem:** `Update()` and `Remove()` were using `player->GetX()/GetY()` to find the old cell, but the player's position was already updated before these calls.

```cpp
// BotStressTest.cpp call order:
bot->SetPosition(new_x, new_y);           // Position updated HERE
world.UpdatePlayerPosition(bot_id, ...);  // SpatialHash called AFTER
```

In `SpatialHash::Update()`:
```cpp
// BUG: GetX() returns NEW position, not old!
int32_t old_cx = ptr_it->second->GetX() / CELL_SIZE;

// FIX: Derive from stored key
int32_t old_cx = static_cast<int32_t>(old_key >> 32);
int32_t old_cy = static_cast<int32_t>(old_key & 0xFFFFFFFF);
```

**Impact:** Without this fix, entities accumulated in wrong cells, causing queries to iterate over ghost entries and degrade over time.

**Location:** `SpatialHash.h` - `Update()` and `Remove()` methods

### 4. Accurate Timing Measurements

**Problem:** The "spatial hash time" metric included AddKnown() calls inside the iteration lambda.

```cpp
// spatial_us measured EVERYTHING including visibility updates
world_.ForEachPlayerInRange(px, py, [&](const auto& viewer) {
    // ... work ...
    world_.Visibility().AddKnown(...);  // This was included in spatial_us!
});
```

**Solution:** Subtract visibility time from total to get pure spatial time.

```cpp
int64_t pure_spatial_us = spatial_us - visibility_us;
stats_.RecordViewerQueryBreakdown(pure_spatial_us / 1000.0, ...);
```

**Location:** `GameServer.cpp` - `BroadcastDirtyPlayers()`

### 5. Batch Client Lookups

**Problem:** Looking up each client connection individually inside the broadcast loop caused repeated mutex acquisitions.

**Solution:** Single batch lookup before the send loop.

```cpp
// One lock acquisition for all lookups
auto connections = clients_.GetAnyClientsForIDs(viewer_updates);

// Then iterate without locks
for (auto& [client_id, data] : viewer_updates) {
    auto conn_it = connections.find(client_id);  // No mutex here
    // ... send packet ...
}
```

**Location:** `GameServer.cpp` - `BroadcastDirtyPlayers()`

---

## Architecture Decisions

### Why `UpdatePlayerPosition()` is Called AFTER `SetPosition()`

The call order is intentional:

```cpp
// 1. Update player's internal state
bot->SetPosition(new_x, new_y);

// 2. Update spatial hash with new position
world.UpdatePlayerPosition(bot_id, new_x, new_y);
```

**Reasoning:**
- `Player::SetPosition()` updates the player's authoritative position
- `SpatialHash::Update()` needs the NEW coordinates to place the entity in the correct cell
- The spatial hash stores a cell key mapping (`entity_cells_[id] = key`), which it uses to find the OLD cell
- This separation allows the spatial hash to be agnostic of Player internals

**The bug was:** Using `GetX()/GetY()` to find the old cell when we should use the stored key.

### Why Atomic for Stats Instead of Mutex Everywhere

Stats are written by the game thread and read by the IO thread (HTTP server):

- **Tick history** uses mutex because it's a deque with complex operations
- **Timing values** use `std::atomic<double>` because:
  - Single writer (game thread)
  - Torn reads are acceptable (worst case: slightly stale value)
  - No allocation or complex operations
  - `memory_order_relaxed` is sufficient

### Why Both `cell_entities_` and `flat_grid_`

We maintain both for compatibility:
- `flat_grid_` - Fast path when world size is known (O(1) array access)
- `cell_entities_` - Fallback for dynamic worlds or out-of-bounds queries

The flat grid is only used when `use_flat_grid_ == true` and coordinates are in bounds.

---

## Current Performance (2000 spread bots)

| Metric | Before | After |
|--------|--------|-------|
| Spatial Hash Query | ~20ms | ~6-8ms |
| Total Tick Time | ~78ms | ~40-50ms |
| Broadcast Time | ~40ms | ~25-30ms |

---

## Further Optimization Opportunities

### 1. Object Pooling for Vectors

**Problem:** `GetPlayersInRange()` allocates a new vector every call.

**Solution:** Thread-local or per-call scratch buffers.

```cpp
// Instead of returning vector, use output parameter
void GetPlayersInRange(int16_t x, int16_t y, std::vector<Player*>& out) {
    out.clear();  // Keeps capacity
    // ... fill out ...
}
```

**Potential gain:** Reduce allocator pressure in hot paths.

### 2. Raw Pointers Instead of shared_ptr in Hot Paths

**Problem:** `shared_ptr` copies increment atomic reference counts.

**Solution:** Use raw pointers within single-tick operations where lifetime is guaranteed.

```cpp
// Players exist for entire tick, so raw pointer is safe
void ForEachNearby(..., Func&& func) {
    func(entity.get());  // Pass raw pointer
}
```

**Potential gain:** 10-20% on iteration-heavy operations.

### 3. SIMD Distance Checks

**Problem:** `IsInRange()` does scalar comparisons.

**Solution:** Batch distance checks using SSE/AVX.

```cpp
// Check 4 or 8 entities at once
__m128i dx = _mm_abs_epi16(_mm_sub_epi16(x1, x2));
__m128i in_range = _mm_cmplt_epi16(dx, range_vec);
```

**Potential gain:** 2-4x on fine-filter distance checks.

### 4. Spatial Hash Cell Size Tuning

**Current:** `CELL_SIZE = 11` (matches VIEW_RANGE * 2 + 1)

**Trade-off:**
- Larger cells = fewer cells to check, more entities per cell
- Smaller cells = more cells to check, fewer entities per cell

**Experiment:** Profile with different cell sizes (8, 16, 22) to find optimal.

### 5. Visibility Tracking Optimization

**Problem:** `AddKnown()` does hash set insertions for every viewer-dirty pair.

**Solution:** Batch visibility updates or use bloom filter for "probably known" fast path.

```cpp
// Skip AddKnown if already known (common case)
if (likely(IsKnown(viewer_id, dirty_id))) continue;
AddKnown(viewer_id, dirty_id);
```

### 6. Packet Batching Improvements

**Current:** One packet per viewer with all their visible updates.

**Alternative:** Delta compression - only send changed fields.

```cpp
// Instead of full position (13 bytes per player)
// Send: [id:8][dx:1][dy:1][facing:1] = 11 bytes for small moves
```

### 7. Parallel Processing

**Problem:** All game logic runs on single thread.

**Solution:** Parallelize independent operations:

```cpp
// Spatial queries are read-only, can parallelize
std::for_each(std::execution::par, dirty_players.begin(), dirty_players.end(),
    [&](const auto& player) {
        // Query is thread-safe, accumulate results per-thread
    });
```

**Caveat:** Requires careful synchronization for writes.

### 8. Reduce Visibility Tracker Overhead

**Current:** Two hash maps (`known_players_`, `known_by_`) with sets.

**Alternative:** Flat bitset for small player counts:

```cpp
// If player IDs are sequential 0-N
std::vector<std::bitset<MAX_PLAYERS>> known_matrix_;
```

### 9. Move to ECS Architecture

For larger scale, consider Entity Component System:
- Better cache locality (components stored contiguously)
- Easier parallelization
- Systems process components in bulk

---

## Profiling Recommendations

To identify the next bottleneck:

1. **CPU profiler** (VTune, perf) - Find hot functions
2. **Cache analysis** - Check L1/L2 miss rates
3. **Lock contention** - Verify mutex isn't blocking game thread
4. **Memory allocator** - Consider jemalloc/tcmalloc for reduced fragmentation

---

## Summary

The main wins came from:
1. **Eliminating hash lookups** with flat grid (biggest impact)
2. **Fixing data corruption bug** that caused accumulating ghost entries
3. **Accurate measurements** to identify real bottlenecks
4. **Batch operations** to reduce lock acquisitions

The server now handles 2000 spread bots within acceptable tick time. Further optimizations would require more invasive changes (ECS, parallelization) for diminishing returns.