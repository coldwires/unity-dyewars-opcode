#pragma once
#include <asio.hpp>
#include <memory>
#include <vector>
#include "core/Common.h"
#include "lua/LuaEngine.h"
#include "game/Player.h"
#include "network/Packets/Protocol.h"

// Forward declaration to avoid circular dependency
class GameServer;
class Player;

class ClientConnection : public std::enable_shared_from_this<ClientConnection> {
public:
    ClientConnection(
        asio::ip::tcp::socket socket, 
        std::shared_ptr<LuaGameEngine> engine,
        GameServer* server,
        uint64_t client_id);
    ~ClientConnection();

    void Start();
    void Disconnect(const std::string& reason);

    void SendPacket(const Protocol::Packet& pkt);
    void RawSend(std::shared_ptr<std::vector<uint8_t>> data);

    uint64_t GetClientID() const { return client_id_; }

    //Player link
    bool HasPlayer() const;
    std::shared_ptr<Player> GetPlayer() const;
    void SetPlayer(std::weak_ptr<Player> player);
    void ClearPlayer();

    std::string GetClientIP() const {return client_ip_; }
    std::string GetClientHostname() const {return client_hostname_;}

    // --- NEW: Dirty Flag Management ---
    // Atomic ensures we don't crash if the Tick thread and Network thread touch this at the same time
    bool IsDirty() const { return is_dirty_; }
    void SetDirty(bool val) { is_dirty_ = val; }
    
    ///  TODO Remove below
    /*
    uint32_t GetPlayerID() const { return player_->GetID(); }
    PlayerData GetPlayerData() const {
        return {player_->GetID(), player_->GetX(), player_->GetY(), player_->GetFacing()};
    }

    void SendPlayerUpdate(uint32_t id, int x, int y, uint8_t facing);
    void SendPlayerLeft(uint32_t id);
    void SendFacingUpdate(uint8_t facing);
    */


private:
    void ReadPacketHeader();
    void ReadPacketPayload(uint16_t size);
    void HandlePacket(const std::vector<uint8_t>& data);

    // Handshake methods
    void StartHandshakeTimeout();
    void OnHandshakeTimeout(const std::error_code& ec);
    void HandleHandshakePacket(const std::vector<uint8_t>& data);
    void CompleteHandshake();
    void FailHandshake(const std::string& reason);
    
    void LogFailedConnection(const std::string& reason);

    /*
    void SendPlayerID();
    void SendAllPlayers();
    void BroadcastPlayerJoined();
    //void BroadcastPlayerMoved();
    void BroadcastPlayerLeft();
    void SendPosition();
    void SendCustomMessage(const std::vector<uint8_t>& data);
    */
    void LogPacketReceived(const std::vector<uint8_t>& payload, uint16_t size);
    void CloseSocket();

    // Network
    asio::ip::tcp::socket socket_;
    asio::steady_timer handshake_timer_;
    uint8_t header_buffer_[4];
    
    // Identity
    uint64_t client_id_;
    std::string client_ip_;
    std::string client_hostname_;

    // State
    std::atomic<bool> is_dirty_{false};
    bool handshake_complete_ = false;

    // References
    GameServer* server_;
    std::weak_ptr<Player> player_;
    uint64_t player_id_;                //stored until handshake completes
    std::shared_ptr<LuaGameEngine> lua_engine_;
};