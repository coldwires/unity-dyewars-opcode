# DyeWarsServer Test Suite

Unit tests for thread safety, memory cleanup, and destructor scenarios.

## Building & Running

```bash
# Configure with tests enabled
cmake -B build -S . -DBUILD_TESTS=ON

# Build
cmake --build build

# Run tests
./build/DyeWarsTests      # Linux/Mac
.\build\Debug\DyeWarsTests.exe  # Windows
```

### CLion Setup

Add to CMake options in **Settings > Build, Execution, Deployment > CMake**:
```
-DBUILD_TESTS=ON
```

Then select **DyeWarsTests** from the run configuration dropdown.

---

## Test Categories

### DatabaseManager Tests

Tests for SQLite database lifecycle and async write queue thread cleanup.

| Test | Description |
|------|-------------|
| `database_manager_throws_on_invalid_path` | Verifies constructor throws `std::runtime_error` when database path is invalid (e.g., non-existent directory) |
| `database_manager_creates_and_destroys_cleanly` | Verifies destructor properly stops `write_thread_` and closes SQLite handle. Confirms file can be reopened after destruction. |
| `database_manager_multiple_instances_sequential` | Creates/destroys DatabaseManager 3 times sequentially. Catches thread leaks from improper `write_thread_` cleanup. |
| `database_manager_destructor_stops_write_thread` | Verifies destructor calls `StopWriteQueue()` which signals thread, notifies condition variable, and joins. Test hangs if broken. |

**Key Components Tested:**
- `std::thread write_thread_` - Background thread for async DB writes
- `std::atomic<bool> stop_queue_` - Shutdown signal
- `std::condition_variable queue_cv_` - Wake thread for new work
- `std::mutex queue_mutex_` - Protects write queue

---

### LuaGameEngine Tests

Tests for Lua scripting bridge, file watcher thread, and hot-reload safety.

| Test | Description |
|------|-------------|
| `lua_engine_creates_and_destroys_cleanly` | Verifies destructor sets `stop_watching_ = true` and joins `file_watcher_thread_`. Test hangs if thread isn't properly stopped. |
| `lua_engine_multiple_instances_sequential` | Creates/destroys LuaGameEngine 3 times. Catches leaked file watcher threads. |
| `lua_engine_reload_doesnt_leak` | Calls `ReloadScripts()` 5 times. Verifies Lua garbage collection runs and no memory accumulates. |
| `lua_engine_concurrent_reload_safe` | Spawns 3 threads all calling `ReloadScripts()` simultaneously. Verifies `lua_mutex_` prevents crashes from concurrent Lua state access. |

**Key Components Tested:**
- `std::thread file_watcher_thread_` - Monitors script file for changes
- `std::atomic<bool> stop_watching_` - Shutdown signal
- `std::mutex lua_mutex_` - Protects `sol::state` (Lua is NOT thread-safe)

---

### BandwidthMonitor Tests

Tests for lock-free atomic statistics tracking across IO and game threads.

| Test | Description |
|------|-------------|
| `bandwidth_monitor_singleton_returns_same_instance` | Verifies `Instance()` returns same pointer on multiple calls |
| `bandwidth_monitor_record_outgoing_increments` | Verifies `fetch_add` atomically increments `total_bytes_out_` |
| `bandwidth_monitor_record_incoming_increments` | Verifies `fetch_add` atomically increments `total_bytes_in_` |
| `bandwidth_monitor_concurrent_recording` | 4 threads each call `RecordOutgoing()` 1000 times. Verifies final count equals expected (no lost updates from torn reads/writes). |
| `bandwidth_monitor_tick_resets_per_second_counters` | Verifies `exchange()` atomically reads and resets `bytes_this_second_` |
| `bandwidth_monitor_format_bytes_various_sizes` | Unit test for B/KB/MB/GB string formatting |
| `bandwidth_monitor_get_stats_thread_safe` | One writer thread + two reader threads running concurrently. Verifies no crashes from simultaneous atomic access. |

**Key Components Tested:**
- `std::atomic<uint64_t>` counters with `memory_order_relaxed`
- `fetch_add()` for concurrent increments
- `exchange()` for atomic read-and-reset
- `std::atomic<double>` for rolling average
- `std::atomic<time_point>` for timestamps

---

### ConnectionLimiter Tests

Tests for rate limiting, connection tracking, and ban system with mutex protection.

| Test | Description |
|------|-------------|
| `connection_limiter_can_connect_under_limit` | Verifies `MAX_CONNECTIONS_PER_IP` (5) is enforced. 6th connection rejected. |
| `connection_limiter_add_remove_connection` | Tests `AddConnection()`/`RemoveConnection()` counting logic |
| `connection_limiter_ban_after_failures` | Verifies `MAX_FAILURES_BEFORE_BAN` (5) triggers automatic ban |
| `connection_limiter_unban` | Verifies `Unban()` removes IP from ban list |
| `connection_limiter_concurrent_access` | 4 threads rapidly adding/removing connections. Verifies mutex prevents data corruption. |
| `connection_limiter_rate_limit_check` | Verifies `MAX_ATTEMPTS_PER_WINDOW` (10 per 60s) is enforced |

**Key Components Tested:**
- `std::mutex mutex_` - Protects all internal maps
- `std::unordered_map<string, int> connections_` - Per-IP connection count
- `std::unordered_set<string> banned_` - Ban list

---

### ClientManager Tests

Tests for client connection tracking with shared pointer safety.

| Test | Description |
|------|-------------|
| `client_manager_add_remove_client` | Basic API test - `GetClient()` returns nullptr for unknown ID |
| `client_manager_count_thread_safe` | 4 threads calling `Count()` concurrently. Verifies mutex prevents crashes. |

**Key Components Tested:**
- `std::mutex mutex_` - Protects client map
- `std::shared_ptr<ClientConnection>` - Safe cross-thread references

**Note:** Full ClientConnection lifecycle tests require a running GameServer with ASIO context, which is complex to set up in isolation.

---

### PingTracker Tests

Tests for rolling average calculation with atomic read access.

| Test | Description |
|------|-------------|
| `ping_tracker_initial_value_zero` | Default `average_` is 0 |
| `ping_tracker_single_sample` | First sample becomes the average |
| `ping_tracker_rolling_average` | After `MAX_SAMPLES` (5), oldest sample drops off. Verifies math: `[100,100,100,100,200] = 120 avg` |
| `ping_tracker_get_is_thread_safe` | Writer thread calls `Record()`, reader thread calls `Get()` concurrently. Verifies atomic average is safe to read anytime. |

**Key Components Tested:**
- `std::atomic<uint32_t> average_` - Lock-free read from any thread
- `std::deque<uint32_t> samples_` - Rolling window (IO thread only)
- `ThreadOwner writer_thread_` - Debug assertion for single-threaded writes

---

## Threading Model Reference

```
+----------------+     QueueAction()      +------------------+
|   IO Thread    | ---------------------> |   Game Thread    |
| (ASIO events)  |                        | (20 TPS loop)    |
+----------------+                        +------------------+
       |                                          |
       | RecordOutgoing()                         | Tick()
       v                                          v
+------------------+                    +------------------+
| BandwidthMonitor |<---GetStats()------| BandwidthMonitor |
| (atomic counters)|                    | (compute stats)  |
+------------------+                    +------------------+
```

**Thread Ownership:**
- **IO Thread**: Socket reads/writes, ClientConnection callbacks, PingTracker.Record()
- **Game Thread**: World state, Player movement, action queue processing
- **File Watcher Thread**: LuaGameEngine hot-reload monitoring
- **DB Write Thread**: DatabaseManager async writes

---

## Adding New Tests

1. Define test function:
```cpp
TEST(my_new_test) {
    // Arrange
    MyClass obj;

    // Act
    obj.DoSomething();

    // Assert
    ASSERT_TRUE(obj.IsValid());
    ASSERT_EQ(obj.GetValue(), 42);
}
```

2. Register in `main()`:
```cpp
std::cout << "\nMyClass Tests:\n";
RUN_TEST(my_new_test);
```

### Available Assertions

| Macro | Description |
|-------|-------------|
| `ASSERT_TRUE(cond)` | Fails if condition is false |
| `ASSERT_FALSE(cond)` | Fails if condition is true |
| `ASSERT_EQ(a, b)` | Fails if a != b |
| `ASSERT_GE(a, b)` | Fails if a < b |
| `ASSERT_LE(a, b)` | Fails if a > b |
| `ASSERT_THROWS(expr, type)` | Fails if expr doesn't throw specified exception type |

---

## Running with Sanitizers

For deeper thread safety analysis on Linux/macOS:

```bash
# Thread Sanitizer (detects data races)
cmake -B build -DBUILD_TESTS=ON -DENABLE_TSAN=ON
cmake --build build
./build/DyeWarsTests

# Address Sanitizer (detects memory errors)
cmake -B build -DBUILD_TESTS=ON -DENABLE_ASAN=ON
cmake --build build
./build/DyeWarsTests
```

**Note:** TSAN and ASAN cannot be enabled simultaneously.
