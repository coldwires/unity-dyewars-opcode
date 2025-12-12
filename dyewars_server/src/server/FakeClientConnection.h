/// =======================================
/// DyeWarsServer - FakeClientConnection
///
/// A lightweight connection that simulates packet building/queueing
/// overhead without actual TCP sockets. Used for stress testing bots
/// to accurately measure server performance with "real" clients.
///
/// Created by Anonymous on Dec 11, 2025
/// =======================================
#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstdint>
#include "network/Packets/Protocol.h"

/// Simulates a ClientConnection for stress testing.
/// Does real packet building and queueing work, but doesn't use actual sockets.
/// This captures the overhead of:
/// - Packet serialization
/// - Memory allocation for packet buffers
/// - Mutex acquisition for send queue
/// - shared_ptr reference counting
class FakeClientConnection : public std::enable_shared_from_this<FakeClientConnection> {
public:
    explicit FakeClientConnection(uint64_t client_id)
        : client_id_(client_id) {}

    ~FakeClientConnection() = default;

    // =========================================================================
    // PACKET I/O - Simulates real packet queueing overhead
    // =========================================================================

    /// Queue a framed packet (same interface as ClientConnection)
    void QueuePacket(const Protocol::Packet& pkt) {
        auto data = std::make_shared<std::vector<uint8_t>>(pkt.ToBytes());
        QueueRaw(data);
    }

    /// Queue raw bytes (same interface as ClientConnection)
    void QueueRaw(std::shared_ptr<std::vector<uint8_t>> data) {
        std::lock_guard<std::mutex> lock(send_mutex_);
        // Count bytes for bandwidth tracking
        bytes_queued_.fetch_add(data->size(), std::memory_order_relaxed);
        packets_queued_.fetch_add(1, std::memory_order_relaxed);

        // Actually queue the packet (simulates memory pressure)
        send_queue_.push(std::move(data));

        // Drain queue periodically to prevent unbounded growth
        // Real connections drain via async_write completion
        if (send_queue_.size() > 100) {
            while (send_queue_.size() > 50) {
                send_queue_.pop();
            }
        }
    }

    // =========================================================================
    // PING - Simulated (instant response)
    // =========================================================================

    void SendPing() {
        // No-op for fake connections
    }

    void RecordPing(uint32_t) {}

    uint32_t GetPing() const { return 0; }

    // =========================================================================
    // ACCESSORS
    // =========================================================================

    uint64_t GetClientID() const { return client_id_; }

    const std::string& GetClientIP() const {
        static const std::string fake_ip = "127.0.0.1";
        return fake_ip;
    }

    bool IsHandshakeComplete() const { return true; }

    bool IsFake() const { return true; }

    // =========================================================================
    // STATS
    // =========================================================================

    uint64_t GetBytesQueued() const {
        return bytes_queued_.load(std::memory_order_relaxed);
    }

    uint64_t GetPacketsQueued() const {
        return packets_queued_.load(std::memory_order_relaxed);
    }

    void ResetStats() {
        bytes_queued_.store(0, std::memory_order_relaxed);
        packets_queued_.store(0, std::memory_order_relaxed);
    }

private:
    const uint64_t client_id_;

    // Send queue - simulates real packet queueing
    std::queue<std::shared_ptr<std::vector<uint8_t>>> send_queue_;
    std::mutex send_mutex_;

    // Stats tracking
    std::atomic<uint64_t> bytes_queued_{0};
    std::atomic<uint64_t> packets_queued_{0};
};