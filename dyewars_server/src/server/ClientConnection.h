#pragma once

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <vector>
#include <deque>
#include <queue>
#include <mutex>
#include <unordered_set>
#include "network/Packets/Protocol.h"
#include "core/ThreadSafety.h"

// Forward declaration to avoid circular dependency
class GameServer;

/// Rolling average of last N ping samples.
/// Record() from IO thread only, Get() from any thread.
struct PingTracker {
    static constexpr size_t MAX_SAMPLES = 5;

    void Record(uint32_t ping_ms) {
        ASSERT_SINGLE_THREADED(writer_thread_);
        if (!writer_thread_.IsOwnerSet()) writer_thread_.SetOwner();

        samples_.push_back(ping_ms);
        if (samples_.size() > MAX_SAMPLES) samples_.pop_front();

        uint32_t sum = 0;
        for (uint32_t s : samples_) sum += s;
        average_.store(sum / static_cast<uint32_t>(samples_.size()), std::memory_order_relaxed);
    }

    uint32_t Get() const { return average_.load(std::memory_order_relaxed); }

private:
    ThreadOwner writer_thread_;
    std::deque<uint32_t> samples_;
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
/// THREAD SAFETY:
/// --------------
/// This class is primarily used from the IO thread (ASIO callbacks).
/// However, some data is accessed from other threads:
///
///   - client_id_: Immutable after construction, safe to read anywhere
///   - client_ip_: Immutable after construction, safe to read anywhere
///   - ping_sent_time_: Atomic, written by SendPing(), read by PacketHandler
///   - ping_ (PingTracker): Thread-safe internally (atomic average)
///   - disconnecting_: Atomic, used for double-disconnect prevention
///
/// The shared_ptr prevents use-after-free when connection is closed while
/// game thread still holds a reference. Operations may fail (socket closed),
/// but they won't crash.
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
    // PACKET I/O (Thread-safe send queue)
    //
    // All sends are queued from any thread, then dispatched to IO thread.
    // This ensures async_write is only called from the IO thread.
    // =========================================================================

    /// Queue a framed packet for sending (thread-safe, called from game thread)
    void QueuePacket(const Protocol::Packet &pkt);

    /// Queue raw bytes for sending (thread-safe, caller must include framing)
    void QueueRaw(std::shared_ptr<std::vector<uint8_t>> data);

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

    /// Get timestamp when last ping was sent (for RTT calculation).
    /// Returns the time point atomically - safe to call from any thread.
    ///
    /// WHY THIS IS NEEDED:
    /// When the client sends a pong back, PacketHandler (on IO thread) needs
    /// to calculate RTT = now - ping_sent_time. If SendPing is setting
    /// ping_sent_time while PacketHandler is reading it, we'd get a torn read.
    ///
    /// ALTERNATIVE APPROACHES:
    /// 1. Include timestamp in ping packet, client echoes it back (no shared state)
    /// 2. Use a mutex (overkill for one value)
    /// 3. Make it atomic (what we do here)
    ///
    std::chrono::steady_clock::time_point GetPingSentTime() const {
        return ping_sent_time_.load(std::memory_order_relaxed);
    }

    /// Set the ping sent timestamp. Called by SendPing() right before sending.
    void SetPingSentTime(std::chrono::steady_clock::time_point time) {
        ping_sent_time_.store(time, std::memory_order_relaxed);
    }

    // =========================================================================
    // ACCESSORS
    // =========================================================================

    /// Get unique client ID. Immutable after construction.
    uint64_t GetClientID() const { return client_id_; }

    /// Get client IP address. Immutable after construction, safe from any thread.
    /// WHY const std::string&: Returns reference to avoid copy, const because
    /// the string itself never changes after construction.
    const std::string& GetClientIP() const { return client_ip_; }

    /// Get client hostname (currently same as IP, DNS lookup not implemented).
    const std::string& GetClientHostname() const { return client_hostname_; }

    /// Check if handshake completed successfully.
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
    // SEND QUEUE (IO thread processing)
    // =========================================================================

    /// Process next packet in send queue (IO thread only)
    void StartNextSend();

    /// Handle completion of async_write (IO thread only)
    void OnSendComplete(const std::error_code& ec);

    // =========================================================================
    // LOGGING
    // =========================================================================

    /// Log failed connection to file for analysis
    void LogFailedConnection(const std::string &reason) const;

    /// Debug: print packet bytes to console
    static void LogPacketReceived(const std::vector<uint8_t> &payload, uint16_t size);

    // =========================================================================
    // DATA
    //
    // THREAD SAFETY NOTES:
    // - IMMUTABLE: Safe to read from any thread without synchronization
    // - ATOMIC: Use atomic operations (load/store) for access
    // - IO_THREAD: Only accessed from ASIO IO thread callbacks
    // =========================================================================

    /// Back reference to server (non-owning, server outlives connections)
    /// IMMUTABLE: Set in constructor, never changes
    GameServer* const server_;

    // --- Network (IO_THREAD only) ---
    asio::ip::tcp::socket socket_;
    asio::steady_timer handshake_timer_;
    uint8_t header_buffer_[4]{};  // Reused buffer for reading packet headers

    // --- Identity (IMMUTABLE after construction) ---
    /// Unique identifier for this connection/client.
    /// WHY const: Set once in constructor, never changes. This makes it safe
    /// to read from any thread without synchronization.
    ///
    /// WHY uint64_t: 32-bit would overflow after ~4 billion connections.
    /// With 1000 connections/second, that's 50 days. uint64_t lasts forever.
    const uint64_t client_id_;

    /// Client IP address (e.g., "192.168.1.100").
    /// IMMUTABLE: Set in constructor from socket.remote_endpoint().
    ///
    /// WHY IMMUTABLE MATTERS FOR THREAD SAFETY:
    /// An immutable value is inherently thread-safe because:
    /// 1. No writes means no data races (races require at least one write)
    /// 2. No synchronization needed for reads
    /// 3. Can be safely cached in CPU cache without invalidation
    ///
    /// If we allowed changing IP (e.g., IP migration), we'd need a mutex.
    const std::string client_ip_;

    /// Client hostname (currently same as IP).
    /// TODO: DNS reverse lookup is slow, would need async worker thread
    const std::string client_hostname_;

    // --- State (IO_THREAD primarily, with atomic for cross-thread) ---

    /// True once handshake packet validated.
    /// Only written from IO thread, but adding atomic just to be explicit.
    bool handshake_complete_ = false;

    /// Prevents double-disconnect from concurrent calls.
    ///
    /// WHY ATOMIC BOOL WITH compare_exchange:
    /// Multiple code paths can trigger disconnect:
    /// - Network error during read
    /// - Handshake timeout
    /// - Protocol violation
    /// - Server shutdown
    ///
    /// Without atomics, two threads could both run disconnect logic,
    /// causing double-cleanup (crash or corruption).
    ///
    /// compare_exchange_strong atomically checks "is it false?" and
    /// sets it to true ONLY if it was false. Returns true if we won
    /// the race, false if someone else already set it.
    std::atomic<bool> disconnecting_{false};

    /// Strike count before kick (too many bad packets = ban).
    /// IO_THREAD only
    uint8_t protocol_violations_{0};

    // --- Ping (ATOMIC for cross-thread access) ---

    /// Timestamp when we last sent a ping request.
    /// ATOMIC because:
    ///   - Written by SendPing() (called from timer, could be any thread)
    ///   - Read by PacketHandler when pong arrives (IO thread)
    ///
    /// WHY std::atomic<time_point> WORKS:
    /// std::chrono::steady_clock::time_point is typically 8 bytes (int64 nanoseconds).
    /// C++20 std::atomic supports any trivially copyable type.
    /// On 64-bit systems, this is usually lock-free (single instruction).
    std::atomic<std::chrono::steady_clock::time_point> ping_sent_time_{};

    /// Rolling average of recent ping samples.
    /// Thread-safe internally (atomic average value).
    PingTracker ping_;

    // --- Send Queue (MUTEX protected, IO_THREAD processes) ---

    /// Queue of packets waiting to be sent.
    /// Protected by send_mutex_. Populated from any thread, drained by IO thread.
    std::queue<std::shared_ptr<std::vector<uint8_t>>> send_queue_;

    /// Mutex protecting send_queue_ access.
    std::mutex send_mutex_;

    /// True when an async_write is in progress (IO_THREAD only, no lock needed).
    /// Ensures only one async_write at a time per socket (ASIO requirement).
    bool write_in_progress_ = false;
};