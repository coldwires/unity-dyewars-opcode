#pragma once
#include <asio.hpp>
#include <memory>
#include <atomic>
#include <iostream>
#include "include/server/Common.h"
#include "include/lua/LuaEngine.h"
#include "include/server/Player.h"

inline const uint8_t HEADERBYTE_1 = 0x11;
inline const uint8_t HEADERBYTE_2 = 0x68;
// Forward declaration to avoid circular dependency
class GameServer;

class GameSession : public std::enable_shared_from_this<GameSession> {
public:
    GameSession(asio::ip::tcp::socket socket, std::shared_ptr<LuaGameEngine> engine,
                GameServer* server, uint32_t player_id);

    void Start();
    uint32_t GetPlayerID() const { return player_->GetID(); }
    //PlayerData GetPlayerData() const { return {player_id_, player_x_, player_y_}; }

    // --- NEW: Dirty Flag Management ---
    // Atomic ensures we don't crash if the Tick thread and Network thread touch this at the same time
    bool IsDirty() const { return is_dirty_; }
    void SetDirty(bool val) { is_dirty_ = val; }

    void SendPlayerUpdate(uint32_t id, int x, int y, uint8_t facing);
    void SendPlayerLeft(uint32_t id);
    void RawSend(std::shared_ptr<std::vector<uint8_t>> data);

    // Wrapper: Ask Player object for data
    PlayerData GetPlayerData() const {
        return {player_->GetID(), player_->GetX(), player_->GetY(), player_->GetFacing()};
    }


    void SendFacingUpdate(uint8_t facing);

private:
    void SendPlayerID();
    void SendAllPlayers();
    void BroadcastPlayerJoined();
    //void BroadcastPlayerMoved();
    void BroadcastPlayerLeft();

    void ReadPacketHeader();
    void ReadPacketPayload(uint16_t size);
    void HandlePacket(const std::vector<uint8_t>& data);
    void SendPosition();
    void SendCustomMessage(const std::vector<uint8_t>& data);
    void SendPacket(const Packet& pkt);

    asio::ip::tcp::socket socket_;
    std::shared_ptr<LuaGameEngine> lua_engine_;
    GameServer* server_;

    std::unique_ptr<Player> player_;
    uint8_t header_buffer_[4];
    std::atomic<bool> is_dirty_{false};
};