#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <array>

class BandwidthMonitor {
public:
    static BandwidthMonitor& Instance() {
        static BandwidthMonitor instance;
        return instance;
    }

    // Call this every send packet
    void RecordOutgoing(size_t bytes){
        total_bytes_out_.fetch_add(bytes, std::memory_order_relaxed);
        bytes_this_second_.fetch_add(bytes, std::memory_order_relaxed);
        packets_this_second_.fetch_add(bytes, std::memory_order_relaxed);
    }

    void RecordIncoming(size_t bytes){
        // We are adding bytes to total_bytes_in_ with fetch_add. Total_bytes_in is atomic, so this 'locks' the region
        // with an order of relaxed (no guarantees in order processed) so other threads can't access this
        // it reads/adds/and writes with one opcode in assembly versus
        // read
        // add      //multiple threads could hit this and add twice, while writing back the false value
        // write
        total_bytes_in_.fetch_add(bytes, std::memory_order_relaxed);
    }

    // Call this once/second from game loop
    void Tick()
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_).count();

        if(elapsed >= 1000) {
            bytes_per_second_out_ = bytes_this_second_.exchange(0);
            packets_per_second_ = packets_this_second_.exchange(0);
            last_tick_ = now;

            // Rolling average (simple exponential smoothing)
            if (avg_bytes_per_second_ == 0) {
                avg_bytes_per_second_ = bytes_per_second_out_;
            } else {
                avg_bytes_per_second_ = (avg_bytes_per_second_ * 0.8) + (bytes_per_second_out_ * 0.2);
            }
        }
    }

    // Getters
    uint64_t GetTotalBytesOut() const { return total_bytes_out_.load(); }
    uint64_t GetTotalBytesIn() const { return total_bytes_in_.load(); }
    uint64_t GetBytesPerSecond() const { return bytes_per_second_out_; }
    uint64_t GetAvgBytesPerSecond() const { return static_cast<uint64_t>(avg_bytes_per_second_); }
    uint64_t GetPacketsPerSecond() const { return packets_per_second_; }

    std::string GetStats() const {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "OUT: %llu B/s (avg: %llu) | %llu pkt/s | Total: %.2f MB",
                 (unsigned long long)bytes_per_second_out_,
                 (unsigned long long)avg_bytes_per_second_,
                 (unsigned long long)packets_per_second_,
                 total_bytes_out_.load() / (1024.0 * 1024.0)
        );
        return std::string(buf);
    }

private:
    BandwidthMonitor() : last_tick_(std::chrono::steady_clock::now()) {}

    std::atomic<uint64_t> total_bytes_out_{0};
    std::atomic<uint64_t> total_bytes_in_{0};
    std::atomic<uint64_t> bytes_this_second_{0};
    std::atomic<uint64_t> packets_this_second_{0};

    uint64_t bytes_per_second_out_{0};
    uint64_t packets_per_second_{0};
    double avg_bytes_per_second_{0};

    std::chrono::steady_clock::time_point last_tick_;
};