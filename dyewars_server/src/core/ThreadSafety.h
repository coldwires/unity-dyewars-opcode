/// =======================================
/// DyeWarsServer - Thread Safety Utilities
///
/// Debug assertions and utilities for enforcing
/// thread safety contracts at runtime.
///
/// WHY THIS FILE EXISTS:
/// --------------------
/// Thread safety bugs are the WORST kind of bugs because:
/// 1. They're non-deterministic - might work 99% of the time, crash 1%
/// 2. They're hard to reproduce - debugger changes timing, hides the bug
/// 3. They corrupt data silently - you don't know until much later
///
/// This file provides tools to CATCH these bugs early in development
/// by asserting "this code should only run on thread X".
///
/// In release builds, all of this compiles away to ZERO overhead.
/// In debug builds, it catches violations immediately with clear errors.
///
/// Created Dec 10, 2025
/// =======================================
#pragma once

#include <thread>
#include <atomic>
#include <cassert>

/// ============================================================================
/// THREAD OWNER TRACKER
///
/// THE PROBLEM:
/// ------------
/// Your server has multiple threads:
///   - Game thread: runs game logic, modifies Player/World state
///   - IO thread: handles network I/O, reads/writes packets
///
/// If both threads access the same data without synchronization = DATA RACE.
/// Data races cause:
///   - Torn reads (reading half-updated values)
///   - Lost updates (two threads write, one wins, one loses)
///   - Memory corruption (half-written pointers = crash)
///
/// THE SOLUTION:
/// -------------
/// For game state (Player, World, SpatialHash, VisibilityTracker):
/// We enforce a contract: "This data is ONLY accessed from game thread."
///
/// ThreadOwner helps enforce this contract at runtime:
/// 1. At startup, call SetOwner() from the game thread
/// 2. In every method, call AssertOwner() to verify we're still on that thread
/// 3. If called from wrong thread → immediate assertion failure → easy to debug
///
/// ALTERNATIVE SOLUTIONS:
/// ----------------------
/// 1. Mutex: Lock data before access. Slower, but allows multi-thread access.
/// 2. Atomic: For simple values (int, pointer). No locking needed.
/// 3. Lock-free queues: Pass messages between threads instead of sharing data.
///
/// We chose "single-thread ownership" because:
/// - Game state is complex (maps, vectors, pointers)
/// - Locking everything would be slow and error-prone
/// - Game logic naturally runs on one thread anyway
///
/// HOW std::thread::id WORKS:
/// --------------------------
/// Every thread has a unique ID. std::this_thread::get_id() returns it.
/// We store the "owner" thread ID, then compare against current thread.
/// If they don't match → wrong thread → assertion failure.
///
/// USAGE:
/// ------
///   class MyClass {
///       ThreadOwner owner_;  // Declare in class
///   public:
///       void Init() {
///           owner_.SetOwner();  // Call once from owning thread
///       }
///       void DoWork() {
///           owner_.AssertOwner();  // Call at start of methods
///           // ... work ...
///       }
///   };
/// ============================================================================
class ThreadOwner {
public:
    /// ========================================================================
    /// CONDITIONAL COMPILATION: NDEBUG
    ///
    /// NDEBUG is defined by the compiler in release builds.
    /// When defined, assert() becomes a no-op.
    ///
    /// We use the same pattern here:
    /// - Debug build: Full thread checking, catches bugs early
    /// - Release build: Empty functions, zero runtime cost
    ///
    /// This is called "zero-cost abstraction" - you get safety in development
    /// without paying for it in production.
    /// ========================================================================
#ifdef NDEBUG
    // ==========================================================================
    // RELEASE BUILD: All methods are empty (compiler optimizes them away)
    //
    // WHY [[maybe_unused]]:
    // The 'context' parameter isn't used in release builds.
    // [[maybe_unused]] tells the compiler "I know, it's intentional, don't warn."
    // ==========================================================================
    void SetOwner() {}
    void AssertOwner([[maybe_unused]] const char* context = nullptr) const {}
    void ClearOwner() {}
    bool IsOwnerSet() const { return true; }
#else
    // ==========================================================================
    // DEBUG BUILD: Full implementation with runtime checks
    // ==========================================================================

    /// Set the current thread as the owner.
    /// Call this ONCE from the thread that should own this resource.
    ///
    /// WHEN TO CALL:
    /// - In GameServer constructor (from main thread, which becomes game thread)
    /// - Or in the first game loop iteration
    ///
    /// WHY memory_order_relaxed:
    /// -------------------------
    /// Atomics have "memory ordering" that controls how operations are seen
    /// by other threads. Options are (from weakest to strongest):
    ///
    /// - relaxed: No ordering guarantees, fastest
    /// - acquire/release: Synchronizes "before/after" relationships
    /// - seq_cst (sequential consistency): Strongest, slowest, global order
    ///
    /// We use relaxed because:
    /// 1. SetOwner() is called once at startup, before any contention
    /// 2. AssertOwner() just reads and compares, doesn't need ordering
    /// 3. If there's a race in setting owner, we have bigger problems
    ///
    void SetOwner() {
        owner_thread_id_.store(std::this_thread::get_id(), std::memory_order_relaxed);
    }

    /// Assert that current thread is the owner.
    /// Call at the START of any method that modifies state.
    ///
    /// @param context Optional string describing what operation is being checked.
    ///                Makes assertion failures easier to debug.
    ///
    /// HOW ASSERT WORKS:
    /// -----------------
    /// assert(condition) does nothing if condition is true.
    /// If condition is false, it:
    /// 1. Prints the condition, file, and line number
    /// 2. Calls abort() to terminate the program
    ///
    /// This is INTENTIONAL - we WANT to crash immediately on thread violations.
    /// A crash with a clear message is infinitely better than silent corruption.
    ///
    void AssertOwner([[maybe_unused]] const char* context = nullptr) const {
        // Load the owner ID that was set during initialization
        auto owner = owner_thread_id_.load(std::memory_order_relaxed);

        // Get the current thread's ID
        auto current = std::this_thread::get_id();

        // If no owner set yet (default-constructed thread::id), allow access.
        // This handles the case where AssertOwner is called before SetOwner.
        // It's a grace period for initialization.
        if (owner == std::thread::id{}) {
            return;
        }

        // THE ACTUAL CHECK: Are we on the right thread?
        // If not, this assertion will fire and crash the program.
        // The message tells you exactly what went wrong.
        assert(owner == current && "Thread safety violation: accessed from wrong thread");
    }

    /// Clear ownership (for transfer or shutdown).
    ///
    /// WHEN TO USE:
    /// - If you need to transfer ownership to another thread
    /// - During shutdown when threads are joining
    /// - Usually not needed in normal operation
    ///
    void ClearOwner() {
        owner_thread_id_.store(std::thread::id{}, std::memory_order_relaxed);
    }

    /// Check if owner has been set.
    /// Useful for conditional initialization.
    ///
    bool IsOwnerSet() const {
        return owner_thread_id_.load(std::memory_order_relaxed) != std::thread::id{};
    }

private:
    /// The thread ID of the owning thread.
    ///
    /// WHY ATOMIC:
    /// -----------
    /// Even though we expect single-threaded access, the SET and CHECK
    /// might happen from different threads during initialization.
    /// Making it atomic prevents torn reads of the thread ID itself.
    ///
    /// std::thread::id is typically 4-8 bytes (platform-dependent).
    /// std::atomic<std::thread::id> ensures reads/writes are indivisible.
    ///
    /// DEFAULT VALUE:
    /// std::thread::id{} is a "null" thread ID that compares unequal
    /// to any real thread. We use this to detect "owner not set yet".
    ///
    std::atomic<std::thread::id> owner_thread_id_{};
#endif
};

/// ============================================================================
/// THREAD SAFETY ASSERTION MACROS
///
/// Convenience macros for common patterns.
///
/// WHY MACROS INSTEAD OF FUNCTIONS:
/// --------------------------------
/// Macros have a unique advantage: they can compile to NOTHING.
/// A function call, even if empty, might not be fully optimized away.
/// ((void)0) is the standard "do nothing" expression in C/C++.
///
/// MACRO NAMING CONVENTION:
/// - ALL_CAPS indicates it's a macro
/// - Macros don't respect namespaces, so be careful with names
/// ============================================================================

#ifdef NDEBUG
    // Release build: compile to nothing
    // ((void)0) is a valid expression that does nothing
    // It's used instead of empty {} to work in all contexts (e.g., ternary)
    #define ASSERT_GAME_THREAD(owner) ((void)0)
    #define ASSERT_IO_THREAD(owner) ((void)0)
    #define ASSERT_SINGLE_THREADED(owner) ((void)0)
#else
    // Debug build: call AssertOwner with descriptive context
    #define ASSERT_GAME_THREAD(owner) (owner).AssertOwner("Expected game thread")
    #define ASSERT_IO_THREAD(owner) (owner).AssertOwner("Expected IO thread")
    #define ASSERT_SINGLE_THREADED(owner) (owner).AssertOwner("Expected single-threaded access")
#endif

/// ============================================================================
/// FURTHER READING ON THREAD SAFETY
/// ============================================================================
///
/// DATA RACES vs RACE CONDITIONS:
/// ------------------------------
/// - Data race: Two threads access same memory, at least one writes, no sync.
///   → Undefined behavior. Compiler can do ANYTHING. Nasal demons.
/// - Race condition: Logic bug where outcome depends on thread timing.
///   → Defined behavior, but wrong results. Still bad, but debuggable.
///
/// C++ MEMORY MODEL (C++11 and later):
/// -----------------------------------
/// C++ has a formal memory model that defines:
/// - What operations are atomic (indivisible)
/// - How threads see each other's writes
/// - What ordering guarantees exist
///
/// Without atomics/mutexes, the compiler and CPU can reorder operations
/// in ways that break your assumptions. This is why "it works on my machine"
/// but crashes in production - different CPUs reorder differently.
///
/// TOOLS FOR FINDING THREAD BUGS:
/// ------------------------------
/// 1. ThreadSanitizer (TSAN): Compile with -fsanitize=thread
///    → Detects data races at runtime with minimal overhead
/// 2. Helgrind (Valgrind tool): Slower but catches more issues
/// 3. Static analysis: Clang-tidy has thread-safety annotations
///
/// PATTERNS FOR THREAD-SAFE CODE:
/// ------------------------------
/// 1. Immutability: If data never changes, it's always safe to read
/// 2. Thread-local: Each thread has its own copy (std::thread_local)
/// 3. Confinement: Only one thread ever accesses the data (what we use)
/// 4. Synchronization: Mutexes, atomics, condition variables
/// 5. Message passing: Threads communicate via queues, not shared state
///
/// Our server uses a combination:
/// - Game state: Confined to game thread (this file enforces it)
/// - Statistics: Atomics (can be read from any thread)
/// - Action queue: Message passing (IO thread → game thread)
/// - Client connections: Each connection has its own strand/socket
///
/// ============================================================================
