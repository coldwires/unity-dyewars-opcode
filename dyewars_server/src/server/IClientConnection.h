/// =======================================
/// DyeWarsServer - IClientConnection
///
/// Interface for client connections (real and fake).
/// Allows the server to treat real TCP clients and stress test bots
/// uniformly for packet sending.
///
/// Created by Anonymous on Dec 11, 2025
/// =======================================
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include "network/Packets/Protocol.h"

/// Abstract interface for client connections.
/// Implemented by ClientConnection (real TCP) and FakeClientConnection (stress test).
class IClientConnection {
public:
    virtual ~IClientConnection() = default;

    // =========================================================================
    // PACKET I/O
    // =========================================================================

    /// Queue a framed packet for sending
    virtual void QueuePacket(const Protocol::Packet& pkt) = 0;

    /// Queue raw bytes for sending
    virtual void QueueRaw(std::shared_ptr<std::vector<uint8_t>> data) = 0;

    // =========================================================================
    // PING
    // =========================================================================

    virtual void SendPing() = 0;
    virtual uint32_t GetPing() const = 0;

    // =========================================================================
    // IDENTITY
    // =========================================================================

    virtual uint64_t GetClientID() const = 0;
    virtual const std::string& GetClientIP() const = 0;

    // =========================================================================
    // STATE
    // =========================================================================

    virtual bool IsHandshakeComplete() const = 0;

    /// Returns true if this is a fake connection (stress test bot)
    virtual bool IsFake() const { return false; }
};