/// =======================================
/// DyeWarsServer - BandwidthMonitor
///
/// Thread-safe bandwidth tracking for network statistics.
///
/// THREAD SAFETY ANALYSIS:
/// -----------------------
/// This class is accessed from MULTIPLE threads:
///   - IO thread: calls RecordOutgoing/RecordIncoming during packet send/receive
///   - Game thread: calls Tick() once per second, reads stats
///   - Main thread: calls GetStats() for console display
///
/// SOLUTION: All shared state is now atomic.
///
/// WHY ATOMICS INSTEAD OF MUTEX:
/// - Statistics are simple integers, perfect for atomics
/// - Atomics are lock-free on most platforms (no thread blocking)
/// - Mutex would be overkill and add unnecessary contention
///
/// ATOMIC OPERATIONS USED:
/// - fetch_add: Atomically add and return old value (for counters)
/// - exchange: Atomically swap and return old value (for reset)
/// - store/load: Atomically write/read (for computed values)
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <array>

class BandwidthMonitor {
public:
    static BandwidthMonitor &Instance() {
        static BandwidthMonitor instance;
        return instance;
    }

    /// Record outgoing bytes - called from IO thread during async_write
    ///
    /// WHY RELAXED MEMORY ORDER:
    /// -------------------------
    /// memory_order_relaxed means "just make this operation atomic, no ordering".
    ///
    /// Stronger orderings (acquire/release/seq_cst) add synchronization that
    /// ensures other memory operations are visible in a certain order.
    /// We don't need that here because:
    /// 1. Each counter is independent (no "happens-before" relationship needed)
    /// 2. We only care about the final sum, not the order of additions
    /// 3. Relaxed is the fastest atomic operation
    ///
    /// When would you need stronger ordering?
    /// - If you're implementing a lock
    /// - If one atomic guards access to non-atomic data
    /// - If order of operations matters across threads
    ///
    void RecordOutgoing(size_t bytes) {
        total_bytes_out_.fetch_add(bytes, std::memory_order_relaxed);
        bytes_this_second_.fetch_add(bytes, std::memory_order_relaxed);
        // BUG FIX: Was adding `bytes` instead of 1 - packet counter was growing by megabytes!
        packets_this_second_.fetch_add(1, std::memory_order_relaxed);
    }

    /// Record incoming bytes - called from IO thread during async_read
    ///
    /// fetch_add is an ATOMIC READ-MODIFY-WRITE operation:
    /// 1. Read current value
    /// 2. Add the amount
    /// 3. Write back new value
    /// ALL AS ONE INDIVISIBLE OPERATION.
    ///
    /// Without atomics, this would be:
    ///   temp = total_bytes_in_;    // Read
    ///   temp = temp + bytes;       // Modify
    ///   total_bytes_in_ = temp;    // Write
    ///
    /// Two threads could interleave:
    ///   Thread A: Read 100
    ///   Thread B: Read 100      (sees same value!)
    ///   Thread A: Write 150     (added 50)
    ///   Thread B: Write 130     (added 30, OVERWRITES A's update!)
    ///   Result: 130 instead of 180. Lost 50 bytes!
    ///
    void RecordIncoming(size_t bytes) {
        total_bytes_in_.fetch_add(bytes, std::memory_order_relaxed);
    }

    /// Called once per second from game loop to compute statistics
    ///
    /// This is safe because:
    /// 1. Tick() is only called from game thread (single-threaded update)
    /// 2. The atomics it reads (bytes_this_second_) are written from IO thread
    /// 3. exchange() atomically gets the value AND resets to 0
    ///
    void Tick() {
        // Tick timing uses a local variable, only accessed from game thread
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_tick_.load(std::memory_order_relaxed)
        ).count();

        if (elapsed >= 1000) {
            // exchange() atomically swaps the value with 0 and returns the old value
            // This is perfect for "read and reset" operations
            // Without exchange(), we'd need: read, then write 0 (two operations, race window)
            uint64_t bytes = bytes_this_second_.exchange(0, std::memory_order_relaxed);
            uint64_t packets = packets_this_second_.exchange(0, std::memory_order_relaxed);

            bytes_per_second_out_.store(bytes, std::memory_order_relaxed);
            packets_per_second_.store(packets, std::memory_order_relaxed);
            last_tick_.store(now, std::memory_order_relaxed);

            // Rolling average (simple exponential smoothing)
            // New average = 80% old + 20% new
            // This smooths out spikes and gives a stable "trend" value
            double current_avg = avg_bytes_per_second_.load(std::memory_order_relaxed);
            double new_avg;
            if (current_avg == 0) {
                new_avg = static_cast<double>(bytes);
            } else {
                new_avg = (current_avg * 0.8) + (static_cast<double>(bytes) * 0.2);
            }
            avg_bytes_per_second_.store(new_avg, std::memory_order_relaxed);
        }
    }

    // =========================================================================
    // GETTERS - Safe to call from any thread
    //
    // These all use atomic loads, so they're always consistent.
    // Note: The values might be slightly stale (from before most recent Tick),
    // but they'll never be torn or corrupted.
    // =========================================================================

    uint64_t GetTotalBytesOut() const {
        return total_bytes_out_.load(std::memory_order_relaxed);
    }

    uint64_t GetTotalBytesIn() const {
        return total_bytes_in_.load(std::memory_order_relaxed);
    }

    uint64_t GetBytesPerSecond() const {
        return bytes_per_second_out_.load(std::memory_order_relaxed);
    }

    uint64_t GetAvgBytesPerSecond() const {
        return static_cast<uint64_t>(avg_bytes_per_second_.load(std::memory_order_relaxed));
    }

    uint64_t GetPacketsPerSecond() const {
        return packets_per_second_.load(std::memory_order_relaxed);
    }

    std::string FormatBytes(uint64_t bytes) const {
        char buf[64];

        if (bytes < 1024) {
            snprintf(buf, sizeof(buf), "%llu B", (unsigned long long) bytes);
        } else if (bytes < 1024 * 1024) {
            snprintf(buf, sizeof(buf), "%.2f KB", bytes / 1024.0);
        } else if (bytes < 1024 * 1024 * 1024) {
            snprintf(buf, sizeof(buf), "%.2f MB", bytes / (1024.0 * 1024.0));
        } else {
            snprintf(buf, sizeof(buf), "%.2f GB", bytes / (1024.0 * 1024.0 * 1024.0));
        }

        return std::string(buf);
    }

    std::string GetStats() const {
        // Load all values atomically (each load is atomic, but the group isn't)
        // This is fine for display - we might show bytes from tick N and packets from tick N+1
        // but that's acceptable for human-readable stats
        uint64_t bps = bytes_per_second_out_.load(std::memory_order_relaxed);
        uint64_t avg = static_cast<uint64_t>(avg_bytes_per_second_.load(std::memory_order_relaxed));
        uint64_t pps = packets_per_second_.load(std::memory_order_relaxed);
        uint64_t total = total_bytes_out_.load(std::memory_order_relaxed);

        char buf[256];
        snprintf(buf, sizeof(buf),
                 "OUT: %s/s (avg: %s) | %llu pkt/s | Total: %s",
                 FormatBytes(bps).c_str(),
                 FormatBytes(avg).c_str(),
                 (unsigned long long) pps,
                 FormatBytes(total).c_str()
        );
        return std::string(buf);
    }


private:
    BandwidthMonitor() {
        last_tick_.store(std::chrono::steady_clock::now(), std::memory_order_relaxed);
    }

    // =========================================================================
    // ATOMIC COUNTERS - Written by IO thread, read by game/main thread
    //
    // WHY std::atomic<uint64_t> INSTEAD OF uint64_t:
    // ----------------------------------------------
    // On 32-bit systems, uint64_t is 8 bytes but the CPU can only do 4-byte
    // atomic operations. Reading/writing a plain uint64_t would be TWO
    // operations, allowing another thread to see half-updated value ("torn read").
    //
    // std::atomic<uint64_t> guarantees the entire 8 bytes are read/written
    // atomically, even on 32-bit systems (using locks if necessary).
    //
    // On 64-bit systems, this is usually lock-free (single instruction).
    // =========================================================================

    /// Total bytes sent since server start
    std::atomic<uint64_t> total_bytes_out_{0};

    /// Total bytes received since server start
    std::atomic<uint64_t> total_bytes_in_{0};

    /// Bytes sent in current measurement window (reset every second)
    std::atomic<uint64_t> bytes_this_second_{0};

    /// Packets sent in current measurement window (reset every second)
    std::atomic<uint64_t> packets_this_second_{0};

    // =========================================================================
    // COMPUTED STATISTICS - Written by game thread in Tick(), read anywhere
    //
    // These are computed once per second from the counters above.
    // Now atomic to allow safe reading from any thread.
    // =========================================================================

    /// Bytes per second (computed each tick)
    std::atomic<uint64_t> bytes_per_second_out_{0};

    /// Packets per second (computed each tick)
    std::atomic<uint64_t> packets_per_second_{0};

    /// Rolling average of bytes per second (smoothed)
    /// WHY std::atomic<double>:
    /// C++20 added full support for atomic<double>. On most platforms this is
    /// lock-free. If not, std::atomic provides its own lock internally.
    std::atomic<double> avg_bytes_per_second_{0.0};

    /// Timestamp of last Tick() - used to measure elapsed time
    /// WHY atomic<time_point>:
    /// time_point is typically 8 bytes (nanoseconds since epoch).
    /// Same torn-read risk as uint64_t, so we make it atomic.
    std::atomic<std::chrono::steady_clock::time_point> last_tick_{};
};