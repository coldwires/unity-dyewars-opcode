#include "include/server/GameSession.h"
#include "include/server/GameServer.h" // Needed here to call Server methods

GameSession::GameSession(asio::ip::tcp::socket socket, std::shared_ptr<LuaGameEngine> engine,
                         GameServer* server, uint32_t player_id)
        : socket_(std::move(socket)), lua_engine_(engine), server_(server){

    // Create the player object starting at 0,0
    player_ = std::make_unique<Player>(player_id, 0, 0);
}

void GameSession::Start() {
    std::cout << "Player " << player_->GetID() << " connected!" << std::endl;
    SendPlayerID();
    SendPosition();
    SendAllPlayers();
    BroadcastPlayerJoined();
    ReadPacketHeader();
}

void GameSession::ReadPacketHeader() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_buffer_, 4),
                     [this, self](std::error_code ec, std::size_t) {
                         if (ec) {
                             std::cout << "Player " << player_->GetID() << " disconnected" << std::endl;
                             BroadcastPlayerLeft();
                             return;
                         }
                         if (header_buffer_[0] == 0x11 && header_buffer_[1] == 0x68) {
                             uint16_t size = (header_buffer_[2] << 8) | header_buffer_[3];
                             if (size > 0 && size < 4096) ReadPacketPayload(size);
                             else ReadPacketHeader();
                         } else ReadPacketHeader();
                     });
}

void GameSession::ReadPacketPayload(uint16_t size) {
    auto self(shared_from_this());
    auto buffer = std::make_shared<std::vector<uint8_t>>(size);
    asio::async_read(socket_, asio::buffer(*buffer),
                     [this, self, buffer](std::error_code ec, std::size_t) {
                         if (!ec) HandlePacket(*buffer);
                         ReadPacketHeader();
                     });
}

void GameSession::HandlePacket(const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    uint8_t msg_type = data[0];

    switch (msg_type) {
        case 0x01: // Move
            if (data.size() >= 2) {

                uint8_t direction = data[1];

                bool moved = player_->AttemptMove(direction, server_->GetMap());

                if(moved){
                    // 2. OPTIONAL: Tell Lua "Event: Player Moved"
                    // (Only do this if you need scripts to trigger, e.g. stepping on a trap)
                    is_dirty_ = true;
                    SendPosition();

                    lua_engine_->OnPlayerMoved(player_->GetID(), player_->GetX(), player_->GetY());

                } else {
                    // We hit a wall. Send position anyway to "Rubber Band" the client back to reality.
                    SendPosition();
                }
            }
            break;
        case 0x02: // Request Pos
            SendPosition();
            break;
        case 0x03: // Custom
        {
            std::vector<uint8_t> custom(data.begin() + 1, data.end());
            auto resp = lua_engine_->ProcessCustomMessage(custom);
            if (!resp.empty()) SendCustomMessage(resp);
        }
            break;
    }
}

// --- SENDING FUNCTIONS ---

void GameSession::SendPacket(const Packet& pkt) {
    auto self(shared_from_this());
    auto data = std::make_shared<std::vector<uint8_t>>(pkt.ToBytes());
    asio::async_write(socket_, asio::buffer(*data),
                      [this, self, data](std::error_code ec, std::size_t) {});
}

void GameSession::SendPlayerID() {
    Packet pkt; pkt.payload = {0x13, (uint8_t)(player_->GetID()>>24), (uint8_t)(player_->GetID()>>16), (uint8_t)(player_->GetID()>>8), (uint8_t)player_->GetID()};
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

void GameSession::SendPosition() {
    Packet pkt; pkt.payload = {0x10, (uint8_t)player_->GetX(), (uint8_t)player_->GetY()};
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

void GameSession::SendPlayerUpdate(uint32_t id, int x, int y) {
    Packet pkt;
    pkt.payload = {0x12, (uint8_t)(id>>24), (uint8_t)(id>>16), (uint8_t)(id>>8), (uint8_t)id, (uint8_t)x, (uint8_t)y};
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

void GameSession::SendPlayerLeft(uint32_t id) {
    Packet pkt;
    pkt.payload = {0x14, (uint8_t)(id>>24), (uint8_t)(id>>16), (uint8_t)(id>>8), (uint8_t)id};
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

void GameSession::SendCustomMessage(const std::vector<uint8_t>& data) {
    Packet pkt; pkt.payload = {0x11};
    pkt.payload.insert(pkt.payload.end(), data.begin(), data.end());
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

// --- BROADCAST IMPLEMENTATIONS ---
void GameSession::SendAllPlayers() {
    auto players = server_->GetAllPlayers();
    for (const auto& p : players) if (p.player_id != player_->GetID()) SendPlayerUpdate(p.player_id, p.x, p.y);
}
void GameSession::BroadcastPlayerJoined() {
    server_->BroadcastToOthers(player_->GetID(), [this](auto s){ s->SendPlayerUpdate(player_->GetID(), player_->GetX(), player_->GetY()); });
}
/*
void GameSession::BroadcastPlayerMoved() {
    server_->BroadcastToOthers(player_id_, [this](auto s){ s->SendPlayerUpdate(player_id_, player_x_, player_y_); });
}*/

void GameSession::BroadcastPlayerLeft() {
    server_->BroadcastToAll([this](auto s){ s->SendPlayerLeft(player_->GetID()); });
    server_->RemoveSession(player_->GetID());
}

void GameSession::RawSend(std::shared_ptr<std::vector<uint8_t>> data) {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(*data),
                      [this, self, data](std::error_code ec, std::size_t) {
                          // Error handling
                      });
}