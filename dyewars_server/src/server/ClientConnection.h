#pragma once

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <vector>
#include <unordered_set>
#include "network/Packets/Protocol.h"

// Forward declaration to avoid circular dependency
class GameServer;

/// ============================================================================
/// PING TRACKER
///
/// Maintains a rolling average of the last N ping samples.
/// Used to smooth out network jitter and get a stable RTT value.
///
/// Example with SAMPLE_COUNT = 5:
///   Record(50), Record(60), Record(55) -> Average = 55ms
///   Record(100) -> Average = 66ms (smoothed, not spiking to 100)
///
/// Thread Safety Contract:
///   - Record() MUST only be called from ONE thread (the IO thread)
///   - Get() can be called from any thread (reads atomic average_)
///   - The internal samples_/index_/filled_ are NOT atomic because they're
///     only ever written by Record() on the IO thread
///   - If you need to call Record() from multiple threads, add a mutex!
/// ============================================================================
struct PingTracker {
    static constexpr size_t SAMPLE_COUNT = 5;

    /// Add a new ping sample and recalculate average
    void Record(uint32_t ping_ms) {
        // Store in circular buffer at current index
        samples_[index_] = ping_ms;

        // Advance index, wrapping around when we hit the end
        index_ = (index_ + 1) % SAMPLE_COUNT;

        // Track how many samples we've collected (caps at SAMPLE_COUNT)
        if (filled_ < SAMPLE_COUNT) filled_++;

        // Recalculate average from all filled samples
        uint32_t sum = 0;
        for (size_t i = 0; i < filled_; i++) {
            sum += samples_[i];
        }
        average_.store(sum / static_cast<uint32_t>(filled_));
    }

    /// Get the current averaged ping in milliseconds
    uint32_t Get() const { return average_.load(); }

private:
    /// Circular buffer holding the last SAMPLE_COUNT ping values
    /// Example: [50, 60, 55, 0, 0] with index_=3 means next write goes to slot 3
    uint32_t samples_[SAMPLE_COUNT]{};

    /// Current write position in the circular buffer (0 to SAMPLE_COUNT-1)
    size_t index_{0};

    /// How many samples have been recorded so far (0 to SAMPLE_COUNT)
    /// Used to avoid averaging uninitialized zeros on startup
    size_t filled_{0};

    /// The calculated average, updated on each Record()
    /// Atomic because it's written on IO thread and read on game thread
    std::atomic<uint32_t> average_{0};
};

/// ============================================================================
/// CLIENT CONNECTION
///
/// Manages a single client's TCP connection to the server.
///
/// Responsibilities:
/// - Validate handshake (magic bytes, protocol version)
/// - Read/write packets asynchronously via ASIO
/// - Track ping for latency compensation
///
/// Threading:
/// - All socket I/O runs on the ASIO IO thread
/// - Ping is written on IO thread, read on game thread (hence atomic)
/// ============================================================================
class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    explicit ClientConnection(
            asio::ip::tcp::socket socket,
            GameServer *server,
            uint64_t client_id);

    ~ClientConnection();

    // =========================================================================
    // LIFECYCLE
    // =========================================================================

    /// Start reading packets from this connection
    void Start();

    /// Gracefully disconnect with a reason (logs and cleans up)
    void Disconnect(const std::string &reason);

    /// Force close the socket immediately
    void CloseSocket();

    // =========================================================================
    // PACKET I/O
    // =========================================================================

    /// Send a framed packet (adds magic bytes + size header)
    void SendPacket(const Protocol::Packet &pkt);

    /// Send raw bytes directly (caller must include framing)
    void RawSend(const std::shared_ptr<std::vector<uint8_t>> &data);

    // =========================================================================
    // PING
    // Server sends ping request, client echoes back, we measure RTT
    // =========================================================================

    /// Send a ping request to this client (0xF8)
    void SendPing();

    /// Record a ping sample and update rolling average
    void RecordPing(uint32_t ping_ms);

    /// Get the averaged ping in milliseconds
    uint32_t GetPing() const { return ping_.Get(); }

    /// Timestamp when last ping was sent (for RTT calculation)
    /// Public because PacketHandler needs it when pong arrives
    std::chrono::steady_clock::time_point ping_sent_time_{};

    // =========================================================================
    // ACCESSORS
    // =========================================================================

    uint64_t GetClientID() const { return client_id_; }
    std::string GetClientIP() const { return client_ip_; }
    std::string GetClientHostname() const { return client_hostname_; }
    bool IsHandshakeComplete() const { return handshake_complete_; }

private:
    // =========================================================================
    // PACKET READING (async chain: Header -> Payload -> Handle -> Header...)
    // =========================================================================

    /// Read 4-byte header: [magic:2][size:2]
    void ReadPacketHeader();

    /// Read payload of given size, then handle it
    void ReadPacketPayload(uint16_t size);

    /// Route packet to PacketHandler for processing
    void HandlePacket(const std::vector<uint8_t> &data);

    /// Handle invalid packets (strike system before disconnect)
    void HandleProtocolViolation();

    // =========================================================================
    // HANDSHAKE
    // Client must send valid handshake within timeout or get disconnected
    // =========================================================================

    void StartHandshakeTimeout();
    void OnHandshakeTimeout(const std::error_code &ec);
    void CheckIfHandshakePacket(const std::vector<uint8_t> &data);
    void CompleteHandshake();
    void FailHandshake(const std::string &reason);

    // =========================================================================
    // LOGGING
    // =========================================================================

    /// Log failed connection to file for analysis
    void LogFailedConnection(const std::string &reason) const;

    /// Debug: print packet bytes to console
    static void LogPacketReceived(const std::vector<uint8_t> &payload, uint16_t size);

    // =========================================================================
    // DATA
    // =========================================================================

    // Back reference to server (non-owning, server outlives connections)
    GameServer *server_;

    // --- Network ---
    asio::ip::tcp::socket socket_;
    asio::steady_timer handshake_timer_;
    uint8_t header_buffer_[4]{};  // Reused buffer for reading packet headers

    // --- Identity ---
    uint64_t client_id_;
    std::string client_ip_;
    std::string client_hostname_;  // TODO: DNS lookup is slow, currently same as IP

    // --- State ---
    bool handshake_complete_ = false;
    std::atomic<bool> disconnecting_{false};  // Prevents double-disconnect
    uint8_t protocol_violations_{0};          // Strike count before kick

    // --- Ping ---
    PingTracker ping_;
};