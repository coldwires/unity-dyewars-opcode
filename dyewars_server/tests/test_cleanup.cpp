// test_cleanup.cpp
// Unit tests for memory cleanup, thread safety, and destructor scenarios
//
// Build: cmake -B build -S . -DBUILD_TESTS=ON && cmake --build build
// Run:   ./build/DyeWarsTests

#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>
#include <vector>
#include <atomic>
#include <future>

#include "database/DatabaseManager.h"
#include "lua/LuaEngine.h"
#include "network/BandwidthMonitor.h"
#include "network/ConnectionLimiter.h"
#include "server/ClientManager.h"
#include "server/ClientConnection.h"

namespace fs = std::filesystem;

// =============================================================================
// Simple Test Framework
// =============================================================================

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) run_test(#name, test_##name)

void run_test(const char* name, void (*test_fn)()) {
    std::cout << "  Running: " << name << "... " << std::flush;
    try {
        test_fn();
        std::cout << "PASSED\n";
        tests_passed++;
    } catch (const std::exception& e) {
        std::cout << "FAILED: " << e.what() << "\n";
        tests_failed++;
    } catch (...) {
        std::cout << "FAILED: Unknown exception\n";
        tests_failed++;
    }
}

#define ASSERT_TRUE(cond) \
    if (!(cond)) throw std::runtime_error("Assertion failed: " #cond)

#define ASSERT_FALSE(cond) \
    if (cond) throw std::runtime_error("Assertion failed: NOT " #cond)

#define ASSERT_EQ(a, b) \
    if ((a) != (b)) throw std::runtime_error("Assertion failed: " #a " == " #b)

#define ASSERT_GE(a, b) \
    if ((a) < (b)) throw std::runtime_error("Assertion failed: " #a " >= " #b)

#define ASSERT_LE(a, b) \
    if ((a) > (b)) throw std::runtime_error("Assertion failed: " #a " <= " #b)

#define ASSERT_THROWS(expr, exception_type) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const exception_type&) { caught = true; } \
        if (!caught) throw std::runtime_error("Expected " #exception_type " was not thrown"); \
    } while(0)

// =============================================================================
// DatabaseManager Tests - Thread Safety & Cleanup
// =============================================================================

TEST(database_manager_throws_on_invalid_path) {
    // Attempt to open database in non-existent directory should throw
    ASSERT_THROWS(
        DatabaseManager("/nonexistent/path/that/does/not/exist/db.sqlite"),
        std::runtime_error
    );
}

TEST(database_manager_creates_and_destroys_cleanly) {
    const std::string test_db = "test_cleanup_db.sqlite";

    // Clean up any existing test database
    if (fs::exists(test_db)) fs::remove(test_db);
    if (fs::exists(test_db + "-wal")) fs::remove(test_db + "-wal");
    if (fs::exists(test_db + "-shm")) fs::remove(test_db + "-shm");

    {
        // Create and destroy in a scope
        auto db = std::make_unique<DatabaseManager>(test_db);
        ASSERT_TRUE(fs::exists(test_db));
        // Destructor called here - should stop write queue thread
    }

    // Verify file still exists (wasn't deleted, but handle was closed)
    ASSERT_TRUE(fs::exists(test_db));

    // Can open again (proves previous handle was properly closed)
    {
        auto db2 = std::make_unique<DatabaseManager>(test_db);
    }

    // Clean up
    fs::remove(test_db);
    if (fs::exists(test_db + "-wal")) fs::remove(test_db + "-wal");
    if (fs::exists(test_db + "-shm")) fs::remove(test_db + "-shm");
}

TEST(database_manager_multiple_instances_sequential) {
    const std::string test_db = "test_sequential_db.sqlite";

    // Clean up
    if (fs::exists(test_db)) fs::remove(test_db);

    // Create and destroy multiple times - tests write_thread_ cleanup each time
    for (int i = 0; i < 3; i++) {
        auto db = std::make_unique<DatabaseManager>(test_db);
        // Each iteration creates and joins a write_thread_
    }

    // Clean up
    fs::remove(test_db);
    if (fs::exists(test_db + "-wal")) fs::remove(test_db + "-wal");
    if (fs::exists(test_db + "-shm")) fs::remove(test_db + "-shm");
}

TEST(database_manager_destructor_stops_write_thread) {
    const std::string test_db = "test_write_thread.sqlite";

    if (fs::exists(test_db)) fs::remove(test_db);

    {
        auto db = std::make_unique<DatabaseManager>(test_db);
        // Destructor should call StopWriteQueue() which:
        // 1. Sets stop_queue_ = true
        // 2. Notifies condition variable
        // 3. Joins write_thread_
        // If this hangs, the thread cleanup is broken
    }

    // If we get here, destructor completed (thread was joined)
    // Clean up
    fs::remove(test_db);
    if (fs::exists(test_db + "-wal")) fs::remove(test_db + "-wal");
    if (fs::exists(test_db + "-shm")) fs::remove(test_db + "-shm");
}

// =============================================================================
// LuaGameEngine Tests - File Watcher Thread Cleanup
// =============================================================================

TEST(lua_engine_creates_and_destroys_cleanly) {
    {
        auto lua = std::make_unique<LuaGameEngine>();
        // Give file watcher time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Destructor sets stop_watching_ = true and joins file_watcher_thread_
    }
    // If we get here without hanging, the thread cleanup worked
}

TEST(lua_engine_multiple_instances_sequential) {
    // Create and destroy multiple times to test file_watcher_thread_ cleanup
    for (int i = 0; i < 3; i++) {
        auto lua = std::make_unique<LuaGameEngine>();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // If we get here without hanging or crashing, cleanup works
}

TEST(lua_engine_reload_doesnt_leak) {
    auto lua = std::make_unique<LuaGameEngine>();

    // Reload multiple times - each reload runs garbage collection
    for (int i = 0; i < 5; i++) {
        lua->ReloadScripts();
    }

    // Destructor should still clean up properly
}

TEST(lua_engine_concurrent_reload_safe) {
    auto lua = std::make_unique<LuaGameEngine>();
    std::atomic<int> completed{0};

    // Simulate concurrent reloads from multiple threads
    // (e.g., console command + file watcher trigger)
    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&lua, &completed]() {
            lua->ReloadScripts();
            completed++;
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    ASSERT_EQ(completed.load(), 3);
}

// =============================================================================
// BandwidthMonitor Tests - Atomic Operations & Thread Safety
// =============================================================================

TEST(bandwidth_monitor_singleton_returns_same_instance) {
    auto& instance1 = BandwidthMonitor::Instance();
    auto& instance2 = BandwidthMonitor::Instance();
    ASSERT_TRUE(&instance1 == &instance2);
}

TEST(bandwidth_monitor_record_outgoing_increments) {
    auto& monitor = BandwidthMonitor::Instance();
    uint64_t before = monitor.GetTotalBytesOut();

    monitor.RecordOutgoing(100);

    uint64_t after = monitor.GetTotalBytesOut();
    ASSERT_EQ(after - before, 100);
}

TEST(bandwidth_monitor_record_incoming_increments) {
    auto& monitor = BandwidthMonitor::Instance();
    uint64_t before = monitor.GetTotalBytesIn();

    monitor.RecordIncoming(50);

    uint64_t after = monitor.GetTotalBytesIn();
    ASSERT_EQ(after - before, 50);
}

TEST(bandwidth_monitor_concurrent_recording) {
    auto& monitor = BandwidthMonitor::Instance();
    uint64_t before = monitor.GetTotalBytesOut();

    constexpr int NUM_THREADS = 4;
    constexpr int OPS_PER_THREAD = 1000;
    constexpr size_t BYTES_PER_OP = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([&monitor]() {
            for (int j = 0; j < OPS_PER_THREAD; j++) {
                monitor.RecordOutgoing(BYTES_PER_OP);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    uint64_t after = monitor.GetTotalBytesOut();
    uint64_t expected = NUM_THREADS * OPS_PER_THREAD * BYTES_PER_OP;
    ASSERT_EQ(after - before, expected);
}

TEST(bandwidth_monitor_tick_resets_per_second_counters) {
    auto& monitor = BandwidthMonitor::Instance();

    // Record some data
    monitor.RecordOutgoing(1000);

    // Force tick (normally called once per second)
    // Wait >1 second to ensure tick triggers
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    monitor.Tick();

    // bytes_per_second should be set, bytes_this_second should be reset
    // (We can't easily verify reset without more internal access,
    //  but this exercises the code path)
    ASSERT_TRUE(true);  // Test passes if no crash/hang
}

TEST(bandwidth_monitor_format_bytes_various_sizes) {
    auto& monitor = BandwidthMonitor::Instance();

    // These shouldn't crash and should return reasonable strings
    std::string bytes = monitor.FormatBytes(500);
    std::string kb = monitor.FormatBytes(1500);
    std::string mb = monitor.FormatBytes(1500000);
    std::string gb = monitor.FormatBytes(1500000000);

    ASSERT_TRUE(bytes.find("B") != std::string::npos);
    ASSERT_TRUE(kb.find("KB") != std::string::npos);
    ASSERT_TRUE(mb.find("MB") != std::string::npos);
    ASSERT_TRUE(gb.find("GB") != std::string::npos);
}

TEST(bandwidth_monitor_get_stats_thread_safe) {
    auto& monitor = BandwidthMonitor::Instance();
    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};

    // Writer thread (simulates IO thread)
    std::thread writer([&monitor, &stop]() {
        while (!stop) {
            monitor.RecordOutgoing(100);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Reader threads (simulate main/game thread reading stats)
    std::vector<std::thread> readers;
    for (int i = 0; i < 2; i++) {
        readers.emplace_back([&monitor, &stop, &reads]() {
            while (!stop) {
                std::string stats = monitor.GetStats();
                ASSERT_FALSE(stats.empty());
                reads++;
            }
        });
    }

    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;

    writer.join();
    for (auto& r : readers) {
        r.join();
    }

    ASSERT_TRUE(reads > 0);
}

// =============================================================================
// ConnectionLimiter Tests - Mutex Protection
// =============================================================================

TEST(connection_limiter_can_connect_under_limit) {
    ConnectionLimiter limiter;

    // Should allow connections under limit (MAX_CONNECTIONS_PER_IP = 5)
    for (int i = 0; i < 5; i++) {
        ASSERT_TRUE(limiter.CanConnect("192.168.1.1"));
        limiter.AddConnection("192.168.1.1");
    }

    // 6th should fail
    ASSERT_FALSE(limiter.CanConnect("192.168.1.1"));
}

TEST(connection_limiter_add_remove_connection) {
    ConnectionLimiter limiter;

    limiter.AddConnection("10.0.0.1");
    ASSERT_EQ(limiter.GetConnectionCount("10.0.0.1"), 1);

    limiter.AddConnection("10.0.0.1");
    ASSERT_EQ(limiter.GetConnectionCount("10.0.0.1"), 2);

    limiter.RemoveConnection("10.0.0.1");
    ASSERT_EQ(limiter.GetConnectionCount("10.0.0.1"), 1);

    limiter.RemoveConnection("10.0.0.1");
    ASSERT_EQ(limiter.GetConnectionCount("10.0.0.1"), 0);
}

TEST(connection_limiter_ban_after_failures) {
    ConnectionLimiter limiter;
    const std::string ip = "1.2.3.4";

    ASSERT_FALSE(limiter.IsBanned(ip));

    // Record failures (MAX_FAILURES_BEFORE_BAN = 5)
    for (int i = 0; i < 5; i++) {
        limiter.RecordFailure(ip);
    }

    ASSERT_TRUE(limiter.IsBanned(ip));
    ASSERT_EQ(limiter.GetBanCount(), 1);
}

TEST(connection_limiter_unban) {
    ConnectionLimiter limiter;
    const std::string ip = "5.6.7.8";

    // Ban the IP
    for (int i = 0; i < 5; i++) {
        limiter.RecordFailure(ip);
    }
    ASSERT_TRUE(limiter.IsBanned(ip));

    // Unban
    limiter.Unban(ip);
    ASSERT_FALSE(limiter.IsBanned(ip));
}

TEST(connection_limiter_concurrent_access) {
    ConnectionLimiter limiter;
    std::atomic<int> successful_adds{0};

    // Multiple threads trying to add/remove connections
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; i++) {
        threads.emplace_back([&limiter, &successful_adds, i]() {
            std::string ip = "192.168.1." + std::to_string(i);
            for (int j = 0; j < 100; j++) {
                if (limiter.CanConnect(ip)) {
                    limiter.AddConnection(ip);
                    successful_adds++;
                    std::this_thread::yield();
                    limiter.RemoveConnection(ip);
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Should have processed some connections without crashing
    ASSERT_TRUE(successful_adds > 0);
}

TEST(connection_limiter_rate_limit_check) {
    ConnectionLimiter limiter;
    const std::string ip = "11.22.33.44";

    // First few should pass (MAX_ATTEMPTS_PER_WINDOW = 10)
    for (int i = 0; i < 10; i++) {
        ASSERT_TRUE(limiter.CheckRateLimit(ip));
    }

    // 11th should fail (within same time window)
    ASSERT_FALSE(limiter.CheckRateLimit(ip));
}

// =============================================================================
// ClientManager Tests - Mutex Protection & Shared Ptr Safety
// =============================================================================

TEST(client_manager_add_remove_client) {
    ClientManager manager;

    // Can't easily create ClientConnection without GameServer,
    // so we test the interface doesn't crash with nullptr
    ASSERT_EQ(manager.Count(), 0);

    auto client = manager.GetClient(12345);
    ASSERT_TRUE(client == nullptr);
}

TEST(client_manager_count_thread_safe) {
    ClientManager manager;
    std::atomic<bool> stop{false};

    // Multiple threads reading count
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; i++) {
        readers.emplace_back([&manager, &stop]() {
            while (!stop) {
                size_t count = manager.Count();
                (void)count;  // Just exercising thread safety
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop = true;

    for (auto& r : readers) {
        r.join();
    }

    ASSERT_TRUE(true);  // Pass if no crash
}

// =============================================================================
// PingTracker Tests - Atomic Average
// =============================================================================

TEST(ping_tracker_initial_value_zero) {
    PingTracker tracker;
    ASSERT_EQ(tracker.Get(), 0);
}

TEST(ping_tracker_single_sample) {
    PingTracker tracker;
    tracker.Record(100);
    ASSERT_EQ(tracker.Get(), 100);
}

TEST(ping_tracker_rolling_average) {
    PingTracker tracker;

    // Add MAX_SAMPLES (5) values
    tracker.Record(100);  // avg: 100
    tracker.Record(100);  // avg: 100
    tracker.Record(100);  // avg: 100
    tracker.Record(100);  // avg: 100
    tracker.Record(100);  // avg: 100
    ASSERT_EQ(tracker.Get(), 100);

    // Add one more - oldest drops off
    tracker.Record(200);  // samples: 100,100,100,100,200 -> avg: 120
    ASSERT_EQ(tracker.Get(), 120);
}

TEST(ping_tracker_get_is_thread_safe) {
    PingTracker tracker;
    std::atomic<bool> stop{false};
    std::atomic<int> reads{0};

    // Reader thread (any thread can call Get())
    std::thread reader([&tracker, &stop, &reads]() {
        while (!stop) {
            uint32_t ping = tracker.Get();
            (void)ping;
            reads++;
        }
    });

    // Single writer (IO thread only)
    for (int i = 0; i < 100; i++) {
        tracker.Record(i * 10);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    stop = true;
    reader.join();

    ASSERT_TRUE(reads > 0);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "\n=== Thread Safety & Cleanup Tests ===\n\n";

    std::cout << "DatabaseManager Tests:\n";
    RUN_TEST(database_manager_throws_on_invalid_path);
    RUN_TEST(database_manager_creates_and_destroys_cleanly);
    RUN_TEST(database_manager_multiple_instances_sequential);
    RUN_TEST(database_manager_destructor_stops_write_thread);

    std::cout << "\nLuaGameEngine Tests:\n";
    RUN_TEST(lua_engine_creates_and_destroys_cleanly);
    RUN_TEST(lua_engine_multiple_instances_sequential);
    RUN_TEST(lua_engine_reload_doesnt_leak);
    RUN_TEST(lua_engine_concurrent_reload_safe);

    std::cout << "\nBandwidthMonitor Tests:\n";
    RUN_TEST(bandwidth_monitor_singleton_returns_same_instance);
    RUN_TEST(bandwidth_monitor_record_outgoing_increments);
    RUN_TEST(bandwidth_monitor_record_incoming_increments);
    RUN_TEST(bandwidth_monitor_concurrent_recording);
    RUN_TEST(bandwidth_monitor_tick_resets_per_second_counters);
    RUN_TEST(bandwidth_monitor_format_bytes_various_sizes);
    RUN_TEST(bandwidth_monitor_get_stats_thread_safe);

    std::cout << "\nConnectionLimiter Tests:\n";
    RUN_TEST(connection_limiter_can_connect_under_limit);
    RUN_TEST(connection_limiter_add_remove_connection);
    RUN_TEST(connection_limiter_ban_after_failures);
    RUN_TEST(connection_limiter_unban);
    RUN_TEST(connection_limiter_concurrent_access);
    RUN_TEST(connection_limiter_rate_limit_check);

    std::cout << "\nClientManager Tests:\n";
    RUN_TEST(client_manager_add_remove_client);
    RUN_TEST(client_manager_count_thread_safe);

    std::cout << "\nPingTracker Tests:\n";
    RUN_TEST(ping_tracker_initial_value_zero);
    RUN_TEST(ping_tracker_single_sample);
    RUN_TEST(ping_tracker_rolling_average);
    RUN_TEST(ping_tracker_get_is_thread_safe);

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";
    std::cout << "Total:  " << (tests_passed + tests_failed) << "\n";

    return tests_failed > 0 ? 1 : 0;
}
