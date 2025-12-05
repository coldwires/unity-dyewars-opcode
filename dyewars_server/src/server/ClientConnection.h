#pragma once
#include <asio.hpp>
#include <memory>
#include <vector>
#include "network/Packets/Protocol.h"

// Forward declaration to avoid circular dependency
class GameServer;
class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(
        asio::ip::tcp::socket socket,
        GameServer* server,
        uint64_t client_id);
    ~ClientConnection();

    void CloseSocket();
    void Start();
    void Disconnect(const std::string& reason);

    // Packet Sending
    void SendPacket(const Protocol::Packet& pkt);
    void RawSend(std::shared_ptr<std::vector<uint8_t>> data);

    // Identity
    uint64_t GetClientID() const { return client_id_; }
    std::string GetClientIP() const {return client_ip_; }
    std::string GetClientHostname() const {return client_hostname_;}

    // State
    bool IsHandshakeComplete() const {return handshake_complete_;}

private:
    // Packet Reading
    void ReadPacketHeader();
    void ReadPacketPayload(uint16_t size);
    void HandlePacket(const std::vector<uint8_t>& data);

    // Handshake
    void StartHandshakeTimeout();
    void OnHandshakeTimeout(const std::error_code& ec);
    void HandleHandshakePacket(const std::vector<uint8_t>& data);
    void CompleteHandshake();
    void FailHandshake(const std::string& reason);

    // Logging
    void LogFailedConnection(const std::string& reason);
    void LogPacketReceived(const std::vector<uint8_t>& payload, uint16_t size);

    /*
    void SendPlayerID();
    void SendAllPlayers();
    void BroadcastPlayerJoined();
    //void BroadcastPlayerMoved();
    void BroadcastPlayerLeft();
    void SendPosition();
    void SendCustomMessage(const std::vector<uint8_t>& data);
    */

    // Network
    asio::ip::tcp::socket socket_;
    asio::steady_timer handshake_timer_;
    uint8_t header_buffer_[4];
    
    // Identity
    uint64_t client_id_;
    std::string client_ip_;
    std::string client_hostname_;

    // State
    bool handshake_complete_ = false;

    // Back Reference (non owning)
    GameServer* server_;
};