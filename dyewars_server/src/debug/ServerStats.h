/// =======================================
/// DyeWarsServer - Server Stats
///
/// Thread-safe stats collection for the debug dashboard.
/// Updated by GameServer each tick, read by DebugHttpServer.
///
/// Created by Anonymous on Dec 11, 2025
/// =======================================
#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <deque>

/// Collects and provides server statistics
class ServerStats {
public:
    // =========================================================================
    // TICK STATS (updated every tick)
    // =========================================================================

    void RecordTick(double tick_ms) {
        std::lock_guard<std::mutex> lock(mutex_);

        tick_count_++;
        tick_total_ms_ += tick_ms;
        last_tick_ms_ = tick_ms;

        if (tick_ms > tick_max_ms_) tick_max_ms_ = tick_ms;

        // Rolling average over last 100 ticks
        tick_history_.push_back(tick_ms);
        if (tick_history_.size() > 100) {
            tick_total_ms_ -= tick_history_.front();
            tick_history_.pop_front();
            tick_count_ = 100;
        }
    }

    void RecordBotMovement(double spatial_ms, double visibility_ms, double departure_ms) {
        // No mutex needed - these are independent atomic stores
        spatial_time_ms_.store(spatial_ms, std::memory_order_relaxed);
        visibility_time_ms_.store(visibility_ms, std::memory_order_relaxed);
        departure_time_ms_.store(departure_ms, std::memory_order_relaxed);
    }

    void RecordBroadcast(double broadcast_ms) {
        broadcast_time_ms_.store(broadcast_ms, std::memory_order_relaxed);
    }

    void RecordBroadcastBreakdown(double viewer_ms, double lookup_ms, double send_ms,
                                  size_t viewer_count, size_t dirty_count) {
        broadcast_viewer_ms_.store(viewer_ms, std::memory_order_relaxed);
        broadcast_lookup_ms_.store(lookup_ms, std::memory_order_relaxed);
        broadcast_send_ms_.store(send_ms, std::memory_order_relaxed);
        broadcast_viewer_count_.store(viewer_count, std::memory_order_relaxed);
        broadcast_dirty_count_.store(dirty_count, std::memory_order_relaxed);
    }

    void RecordViewerQueryBreakdown(double spatial_ms, double addknown_ms, size_t nearby_count) {
        vq_spatial_ms_.store(spatial_ms, std::memory_order_relaxed);
        vq_addknown_ms_.store(addknown_ms, std::memory_order_relaxed);
        vq_nearby_count_.store(nearby_count, std::memory_order_relaxed);
    }

    void SetDirtyPlayerCount(size_t count) {
        dirty_players_last_.store(count, std::memory_order_relaxed);
    }

    // =========================================================================
    // CONNECTION STATS
    // =========================================================================

    void SetConnectionCounts(size_t real, size_t fake, size_t players) {
        real_clients_.store(real, std::memory_order_relaxed);
        fake_clients_.store(fake, std::memory_order_relaxed);
        total_players_.store(players, std::memory_order_relaxed);
    }

    void SetVisibilityCount(size_t count) {
        visibility_tracked_.store(count, std::memory_order_relaxed);
    }

    // =========================================================================
    // BANDWIDTH STATS
    // =========================================================================

    void SetBandwidth(uint64_t bytes_out_per_sec, uint64_t bytes_out_avg,
                      uint64_t bytes_out_total, uint64_t packets_out_per_sec) {
        bytes_out_per_sec_.store(bytes_out_per_sec, std::memory_order_relaxed);
        bytes_out_avg_.store(bytes_out_avg, std::memory_order_relaxed);
        bytes_out_total_.store(bytes_out_total, std::memory_order_relaxed);
        packets_out_per_sec_.store(packets_out_per_sec, std::memory_order_relaxed);
    }

    // =========================================================================
    // JSON OUTPUT
    // =========================================================================

    std::string ToJson() const {
        std::lock_guard<std::mutex> lock(mutex_);

        double avg_ms = tick_count_ > 0 ? tick_total_ms_ / tick_count_ : 0;
        double tps = avg_ms > 0 ? 1000.0 / avg_ms : 20.0;

        // Build JSON manually to avoid dependency
        std::string json = "{";
        json += "\"tick_avg_ms\":" + std::to_string(avg_ms) + ",";
        json += "\"tick_max_ms\":" + std::to_string(tick_max_ms_) + ",";
        json += "\"tick_last_ms\":" + std::to_string(last_tick_ms_) + ",";
        json += "\"tps\":" + std::to_string(tps) + ",";
        json += "\"dirty_players\":" + std::to_string(dirty_players_last_.load(std::memory_order_relaxed)) + ",";

        json += "\"spatial_time_ms\":" + std::to_string(spatial_time_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"visibility_time_ms\":" + std::to_string(visibility_time_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"departure_time_ms\":" + std::to_string(departure_time_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"broadcast_time_ms\":" + std::to_string(broadcast_time_ms_.load(std::memory_order_relaxed)) + ",";

        json += "\"real_clients\":" + std::to_string(real_clients_.load(std::memory_order_relaxed)) + ",";
        json += "\"fake_clients\":" + std::to_string(fake_clients_.load(std::memory_order_relaxed)) + ",";
        json += "\"total_players\":" + std::to_string(total_players_.load(std::memory_order_relaxed)) + ",";
        json += "\"visibility_tracked\":" + std::to_string(visibility_tracked_.load(std::memory_order_relaxed)) + ",";

        json += "\"bytes_out_per_sec\":" + std::to_string(bytes_out_per_sec_.load(std::memory_order_relaxed)) + ",";
        json += "\"bytes_out_avg\":" + std::to_string(bytes_out_avg_.load(std::memory_order_relaxed)) + ",";
        json += "\"bytes_out_total\":" + std::to_string(bytes_out_total_.load(std::memory_order_relaxed)) + ",";
        json += "\"packets_out_per_sec\":" + std::to_string(packets_out_per_sec_.load(std::memory_order_relaxed)) + ",";

        // Broadcast breakdown
        json += "\"broadcast_viewer_ms\":" + std::to_string(broadcast_viewer_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"broadcast_lookup_ms\":" + std::to_string(broadcast_lookup_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"broadcast_send_ms\":" + std::to_string(broadcast_send_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"broadcast_viewer_count\":" + std::to_string(broadcast_viewer_count_.load(std::memory_order_relaxed))
                + ",";
        json += "\"broadcast_dirty_count\":" + std::to_string(broadcast_dirty_count_.load(std::memory_order_relaxed)) +
                ",";

        // Viewer query sub-breakdown
        json += "\"vq_spatial_ms\":" + std::to_string(vq_spatial_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"vq_addknown_ms\":" + std::to_string(vq_addknown_ms_.load(std::memory_order_relaxed)) + ",";
        json += "\"vq_nearby_count\":" + std::to_string(vq_nearby_count_.load(std::memory_order_relaxed));
        json += "}";

        return json;
    }

    /// Reset max values (call periodically)
    void ResetMaxValues() {
        std::lock_guard<std::mutex> lock(mutex_);
        tick_max_ms_ = 0;
    }

private:
    mutable std::mutex mutex_;

    // Tick timing
    std::deque<double> tick_history_;
    double tick_total_ms_ = 0;
    double tick_max_ms_ = 0;
    double last_tick_ms_ = 0;
    size_t tick_count_ = 0;

    // Dirty player count (atomic - set from ProcessTick, read from JSON)
    std::atomic<size_t> dirty_players_last_{0};

    // Bot movement breakdown (atomic - written by game thread, read by IO thread)
    std::atomic<double> spatial_time_ms_{0.0};
    std::atomic<double> visibility_time_ms_{0.0};
    std::atomic<double> departure_time_ms_{0.0};
    std::atomic<double> broadcast_time_ms_{0.0};

    // Broadcast breakdown
    std::atomic<double> broadcast_viewer_ms_{0.0};
    std::atomic<double> broadcast_lookup_ms_{0.0};
    std::atomic<double> broadcast_send_ms_{0.0};
    std::atomic<size_t> broadcast_viewer_count_{0};
    std::atomic<size_t> broadcast_dirty_count_{0};

    // Viewer query sub-breakdown (within broadcast_viewer_ms)
    std::atomic<double> vq_spatial_ms_{0.0};
    std::atomic<double> vq_addknown_ms_{0.0};
    std::atomic<size_t> vq_nearby_count_{0};

    // Connection counts (atomic for lock-free reads)
    std::atomic<size_t> real_clients_{0};
    std::atomic<size_t> fake_clients_{0};
    std::atomic<size_t> total_players_{0};
    std::atomic<size_t> visibility_tracked_{0};

    // Bandwidth
    std::atomic<uint64_t> bytes_out_per_sec_{0};
    std::atomic<uint64_t> bytes_out_avg_{0};
    std::atomic<uint64_t> bytes_out_total_{0};
    std::atomic<uint64_t> packets_out_per_sec_{0};
};
