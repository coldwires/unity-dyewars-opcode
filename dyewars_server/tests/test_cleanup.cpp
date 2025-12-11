// test_cleanup.cpp
// Unit tests for memory cleanup scenarios
//
// Build: cmake -B build -S . -DBUILD_TESTS=ON && cmake --build build
// Run:   ./build/DyeWarsTests

#include <iostream>
#include <cassert>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>

#include "database/DatabaseManager.h"
#include "lua/LuaEngine.h"

namespace fs = std::filesystem;

// Simple test framework
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) void test_##name()
#define RUN_TEST(name) run_test(#name, test_##name)

void run_test(const char* name, void (*test_fn)()) {
    std::cout << "  Running: " << name << "... ";
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

#define ASSERT_THROWS(expr, exception_type) \
    do { \
        bool caught = false; \
        try { expr; } \
        catch (const exception_type&) { caught = true; } \
        if (!caught) throw std::runtime_error("Expected " #exception_type " was not thrown"); \
    } while(0)

// =============================================================================
// DatabaseManager Tests
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
        // Destructor called here
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

    // Create and destroy multiple times
    for (int i = 0; i < 3; i++) {
        auto db = std::make_unique<DatabaseManager>(test_db);
    }

    // Clean up
    fs::remove(test_db);
    if (fs::exists(test_db + "-wal")) fs::remove(test_db + "-wal");
    if (fs::exists(test_db + "-shm")) fs::remove(test_db + "-shm");
}

// =============================================================================
// LuaGameEngine Tests
// =============================================================================

TEST(lua_engine_creates_and_destroys_cleanly) {
    {
        auto lua = std::make_unique<LuaGameEngine>();
        // Give file watcher time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Destructor called here - should cleanly join watcher thread
    }
    // If we get here without hanging, the test passed
}

TEST(lua_engine_multiple_instances_sequential) {
    // Create and destroy multiple times to test cleanup
    for (int i = 0; i < 3; i++) {
        auto lua = std::make_unique<LuaGameEngine>();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    // If we get here without hanging or crashing, cleanup works
}

TEST(lua_engine_reload_doesnt_leak) {
    auto lua = std::make_unique<LuaGameEngine>();

    // Reload multiple times
    for (int i = 0; i < 5; i++) {
        lua->ReloadScripts();
    }

    // Destructor should still clean up properly
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "\n=== Memory Cleanup Tests ===\n\n";

    std::cout << "DatabaseManager Tests:\n";
    RUN_TEST(database_manager_throws_on_invalid_path);
    RUN_TEST(database_manager_creates_and_destroys_cleanly);
    RUN_TEST(database_manager_multiple_instances_sequential);

    std::cout << "\nLuaGameEngine Tests:\n";
    RUN_TEST(lua_engine_creates_and_destroys_cleanly);
    RUN_TEST(lua_engine_multiple_instances_sequential);
    RUN_TEST(lua_engine_reload_doesnt_leak);

    std::cout << "\n=== Results ===\n";
    std::cout << "Passed: " << tests_passed << "\n";
    std::cout << "Failed: " << tests_failed << "\n";

    return tests_failed > 0 ? 1 : 0;
}
