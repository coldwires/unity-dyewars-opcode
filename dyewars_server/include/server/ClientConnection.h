#pragma once
#include <asio.hpp>
#include <memory>
#include <atomic>
#include <iostream>
#include "include/server/Common.h"
#include "include/lua/LuaEngine.h"
#include "include/server/Player.h"

// Forward declaration to avoid circular dependency
class GameServer;

class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(asio::ip::tcp::socket socket, std::shared_ptr<LuaGameEngine> engine,
                     GameServer* server, uint32_t player_id);

    void Start();
    uint32_t GetPlayerID() const { return player_->GetID(); }

    PlayerData GetPlayerData() const {
        return {player_->GetID(), player_->GetX(), player_->GetY(), player_->GetFacing()};
    }

    // --- NEW: Dirty Flag Management ---
    // Atomic ensures we don't crash if the Tick thread and Network thread touch this at the same time
    bool IsDirty() const { return is_dirty_; }
    void SetDirty(bool val) { is_dirty_ = val; }

    void SendPlayerUpdate(uint32_t id, int x, int y, uint8_t facing);
    void SendPlayerLeft(uint32_t id);
    void RawSend(std::shared_ptr<std::vector<uint8_t>> data);



    void SendFacingUpdate(uint8_t facing);
    std::string GetClientIP() const {return client_ip_; }
    std::string GetClientHostname() const {return client_hostname_;}

private:
    // Handshake methods
    void StartHandshakeTimeout();
    void OnHandshakeTimeout(const std::error_code& ec);
    void HandleHandshakePacket(const std::vector<uint8_t>& data);
    void CompleteHandshake();
    void FailHandshake(const std::string& reason);
    void LogFailedConnection(const std::string& reason);

    void SendPlayerID();
    void SendAllPlayers();
    void BroadcastPlayerJoined();
    //void BroadcastPlayerMoved();
    void BroadcastPlayerLeft();

    void LogPacketReceived(const std::vector<uint8_t>& payload, uint16_t size);
    void ReadPacketHeader();
    void ReadPacketPayload(uint16_t size);
    void HandlePacket(const std::vector<uint8_t>& data);
    void SendPosition();
    void SendCustomMessage(const std::vector<uint8_t>& data);
    void SendPacket(const Packet& pkt);

    void Disconnect(const std::string& reason);

    asio::ip::tcp::socket socket_;
    std::shared_ptr<LuaGameEngine> lua_engine_;
    GameServer* server_;

    uint32_t player_id_;                //stored until handshake completes
    std::unique_ptr<Player> player_;    //created after handshake
    uint8_t header_buffer_[4];
    std::atomic<bool> is_dirty_{false};

    //Handshake state
    bool handshake_complete_ = false;
    asio::steady_timer handshake_timer_;
    std::string client_ip_;
    std::string client_hostname_;
};