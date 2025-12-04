#include "include/server/ClientConnection.h"
#include "include/server/GameServer.h" // Needed here to call Server methods
#include "include/server/BandwidthMonitor.h"
#include "include/server/Packets/OpCodes.h"
#include <iomanip>
#include <fstream>

ClientConnection::ClientConnection(asio::ip::tcp::socket socket,
                                   std::shared_ptr<LuaGameEngine> engine,
                                   GameServer* server,
                                   uint32_t player_id)
        : socket_(std::move(socket)),
        lua_engine_(engine),
        server_(server),
        player_id_(player_id),
        handshake_timer_(socket_.get_executor())
        {
            try{
                auto endpoint = socket_.remote_endpoint();
                client_ip_ = endpoint.address().to_string();
                //DNS lookup is slow, move to worker thread
                client_hostname_ = client_ip_;
            } catch (const std::exception& e) {
                client_ip_ = "unknown";
                client_hostname_ = "unknown";
            }
        }

void ClientConnection::Start() {
    std::cout << "IP:" << client_ip_ << " Hostname:" << client_hostname_
    << " connected to the server." << std::endl;

    //5 seconds to respond
    StartHandshakeTimeout();

    //Begin reading
    ReadPacketHeader();

//    std::cout << "Player " << player_->GetID() << " connected!" << std::endl;
//    SendPlayerID();
//    SendPosition();
//    SendAllPlayers();
//    BroadcastPlayerJoined();
//    ReadPacketHeader();
}
// ============================================================================
// HANDSHAKE LOGIC
// ============================================================================

void ClientConnection::StartHandshakeTimeout() {
    auto self(shared_from_this());

    handshake_timer_.expires_after(std::chrono::seconds(Protocol::HANDSHAKE_TIMEOUT_SECONDS));
    handshake_timer_.async_wait(
            [this, self](const std::error_code& ec) {
                OnHandshakeTimeout(ec);
            });
}

void ClientConnection::OnHandshakeTimeout(const std::error_code& ec) {
    // If timer was cancelled (handshake succeeded), do nothing
    if (ec == asio::error::operation_aborted) {
        return;
    }

    // Timer expired - no handshake received
    if (!handshake_complete_) {
        FailHandshake("failed to handshake within 5 seconds");
    }
}

void ClientConnection::HandleHandshakePacket(const std::vector<uint8_t>& data) {
    // Expected format:
    // Byte 0: Opcode (0x00)
    // Bytes 1-2: Protocol version (0x00 0x01)
    // Bytes 3-6: Client magic ("DYEW" = 0x44 0x59 0x45 0x57)

    if (data.size() != 7) {
        FailHandshake("handshake packet size doesn't match");
        return;
    }

    size_t offset = 0;
    uint8_t opcode = Protocol::PacketReader::ReadByte(data, offset);

    if (opcode != Protocol::Opcode::Connection::C_Handshake_Request) {
        FailHandshake("expected handshake opcode 0x00");
        return;
    }

    uint16_t version = Protocol::PacketReader::ReadShort(data, offset);
    if (version != Protocol::VERSION) {
        std::ostringstream oss;
        oss << "protocol version mismatch (client: 0x" << std::hex << version
            << ", server: 0x" << Protocol::VERSION << ")";
        FailHandshake(oss.str());
        return;
    }

    uint32_t magic = Protocol::PacketReader::ReadUInt(data, offset);
    if (magic != Protocol::CLIENT_MAGIC) {
        FailHandshake("invalid client identifier");
        return;
    }
    // Handshake successful!
    CompleteHandshake();
}

void ClientConnection::CompleteHandshake() {
    handshake_timer_.cancel();
    handshake_complete_ = true;

    player_ = std::make_unique<Player>(player_id_, 0, 0);
    server_->RegisterSession(shared_from_this());

    std::cout << "IP:" << client_ip_ << " Hostname:" << client_hostname_
              << " handshake successful. Player ID: " << player_->GetID() << std::endl;

    // Now send the player their info
    SendPlayerID();
    SendPosition();
    SendAllPlayers();
    BroadcastPlayerJoined();
}

void ClientConnection::FailHandshake(const std::string& reason) {
    std::cout << "IP:" << client_ip_ << " Hostname:" << client_hostname_
              << " " << reason << ". Adding to log." << std::endl;

    LogFailedConnection(reason);

    // Close the socket
    std::error_code ec;
    socket_.close(ec);
}
void ClientConnection::Disconnect(const std::string& reason){
    handshake_timer_.cancel();  // Cancel any pending timeout
    std::error_code ec;
    socket_.close(ec);
}

void ClientConnection::LogFailedConnection(const std::string& reason) {
    std::ofstream logfile("failed_connections.log", std::ios::app);
    if (logfile.is_open()) {
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        logfile << std::ctime(&time);  // Includes newline
        logfile << "  IP: " << client_ip_ << "\n";
        logfile << "  Hostname: " << client_hostname_ << "\n";
        logfile << "  Reason: " << reason << "\n";
        logfile << "---\n";
    }
}

void ClientConnection::ReadPacketHeader() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_buffer_, 4),
                     [this, self](std::error_code ec, std::size_t) {
                         if (ec) {
                             if (!handshake_complete_) {
                                 // Connection closed before handshake
                                 Disconnect("connection closed before handshake");
                             } else {
                                 std::cout << "Player " << player_->GetID() << " disconnected" << std::endl;
                                 BroadcastPlayerLeft();
                             }
                             return;
                         }

                         if (header_buffer_[0] == Protocol::MAGIC_1 && header_buffer_[1] == Protocol::MAGIC_2) {
                             uint16_t size = (header_buffer_[2] << 8) | header_buffer_[3];

                             if (size > 0 && size < 4096) {
                                 ReadPacketPayload(size);
                             } else {
                                 std::cout << "[DEBUG] Invalid size (over 4096)." << std::endl;
                                 ReadPacketHeader();
                             }
                         } else {
                             // Invalid magic bytes
                             if (!handshake_complete_) {
                                 std::ostringstream oss;
                                 oss << "invalid magic bytes (got 0x" << std::hex
                                     << (int)header_buffer_[0] << " 0x" << (int)header_buffer_[1] << ")";
                                 FailHandshake(oss.str());
                             } else {
                                 ReadPacketHeader();
                             }
                         }
                     });
}

void ClientConnection::ReadPacketPayload(uint16_t size) {
    auto self(shared_from_this());
    auto buffer = std::make_shared<std::vector<uint8_t>>(size);
    asio::async_read(socket_,
                     asio::buffer(*buffer),
                     [this, self, buffer, size](std::error_code ec, std::size_t)
                     {
                         if (!ec) {
                             LogPacketReceived(*buffer, size);
                             // If handshake not complete, this must be the handshake packet
                             if (!handshake_complete_) {
                                 HandleHandshakePacket(*buffer);
                             } else {
                                 HandlePacket(*buffer);
                             }
                         }
                         // Only continue reading if socket is still open
                         if (socket_.is_open()) {
                             ReadPacketHeader();
                         }
                     });
}

void ClientConnection::HandlePacket(const std::vector<uint8_t>& data) {
    if (data.empty()) return;

    size_t offset = 0;
    uint8_t msg_type = Protocol::PacketReader::ReadByte(data, offset);//data[0];

    switch (msg_type) {
        case Protocol::Opcode::Movement::C_Move_Request: // Move
            if (data.size() >= 3) {
                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                uint8_t facing = Protocol::PacketReader::ReadByte(data, offset);

                //only move if direction matches facing
                if(direction == facing && direction == player_->GetFacing()) {


                    bool moved = player_->AttemptMove(direction, server_->GetMap());

                    if (moved) {
                        is_dirty_ = true;
                        SendPosition();

                        //This would block the networking thread (even though lua should be fast and only calling events)
                        //lua_engine_->OnPlayerMoved(player_->GetID(), player_->GetX(), player_->GetY());

                    } else {
                        // We hit a wall. Send position anyway to "Rubber Band" the client back to reality.
                        SendPosition();
                    }
                } else {
                    //Mismatch: turn player to match and send correction
                    player_->SetFacing(direction);
                    SendFacingUpdate(player_->GetFacing());
                }
            }
            break;

        case Protocol::Opcode::Movement::C_Turn_Request: //Turn
            if(data.size() >= 2)
            {
                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                player_->SetFacing(direction);
                is_dirty_ = true;       //Broadcast facing change to others
                SendFacingUpdate(player_->GetFacing());
            }
            break;
//       " case Protocol::Opcode::Movement::  C_RequestPosition: // Request Pos
//            SendPosition();
//            break;"
        /*
            case Protocol::Opcode::C_Custom: // Custom
        {
            std::vector<uint8_t> custom(data.begin() + 1, data.end());
            auto resp = lua_engine_->ProcessCustomMessage(custom);
            if (!resp.empty()) SendCustomMessage(resp);
        }
            break;
         */
    }
}

void ClientConnection::LogPacketReceived(const std::vector<uint8_t>& payload, uint16_t size)
{
    std::cout<<"Packet Received: 11 68 ";

    // Print size as two hex bytes (big-endian)
    std::cout << std::hex << std::uppercase << std::setfill('0');
    std::cout << std::setw(2) << ((size >> 8) & 0xFF) << " ";
    std::cout << std::setw(2) << (size & 0xFF) << " ";

    // Print payload bytes
    for (size_t i = 0; i < payload.size() && i < 20; i++) {
        std::cout << std::setw(2) << static_cast<int>(payload[i]) << " ";
    }

    if (payload.size() > 20) {
        std::cout << "...";
    }

    std::cout << std::dec << " (" << size << " bytes)" << std::endl;
}

// --- SENDING FUNCTIONS ---

void ClientConnection::SendPacket(const Protocol::Packet& pkt) {
    auto self(shared_from_this());
    auto data = std::make_shared<std::vector<uint8_t>>(pkt.ToBytes());

    // Track bandwidth
    BandwidthMonitor::Instance().RecordOutgoing(data->size());

    asio::async_write(socket_, asio::buffer(*data),
                      [this, self, data](std::error_code ec, std::size_t) {});
}

void ClientConnection::SendPlayerID() {
    Protocol::Packet pkt;
    Protocol::PacketWriter::WriteByte(pkt.payload, 0x13);
    Protocol::PacketWriter::WriteUInt(pkt.payload, player_->GetID());
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}

void ClientConnection::SendPosition() {
    Protocol::Packet pkt;
    Protocol::PacketWriter::WriteByte(pkt.payload, 0x10);
    Protocol::PacketWriter::WriteShort(pkt.payload, player_->GetX());
    Protocol::PacketWriter::WriteShort(pkt.payload, player_->GetY());
    Protocol::PacketWriter::WriteByte(pkt.payload, player_->GetFacing());
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}

void ClientConnection::SendPlayerUpdate(uint32_t id, int x, int y, uint8_t facing) {
    Protocol::Packet pkt;
    Protocol::PacketWriter::WriteByte(pkt.payload, 0x12);
    Protocol::PacketWriter::WriteUInt(pkt.payload, id);
    Protocol::PacketWriter::WriteShort(pkt.payload, x);
    Protocol::PacketWriter::WriteShort(pkt.payload, y);
    Protocol::PacketWriter::WriteByte(pkt.payload, facing);
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}

void ClientConnection::SendFacingUpdate(uint8_t facing) {
    Protocol::Packet pkt;
    Protocol::PacketWriter::WriteByte(pkt.payload, 0x15);
    Protocol::PacketWriter::WriteByte(pkt.payload, facing);
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}

void ClientConnection::SendPlayerLeft(uint32_t id) {
    Protocol::Packet pkt;
    Protocol::PacketWriter::WriteByte(pkt.payload, 0x14);
    Protocol::PacketWriter::WriteUInt(pkt.payload, id);
    pkt.size = pkt.payload.size();
    SendPacket(pkt);
}

void ClientConnection::SendCustomMessage(const std::vector<uint8_t>& data) {
    Protocol::Packet pkt; pkt.payload = {0x11};
    pkt.payload.insert(pkt.payload.end(), data.begin(), data.end());
    pkt.size = pkt.payload.size(); SendPacket(pkt);
}

// --- BROADCAST IMPLEMENTATIONS ---
void ClientConnection::SendAllPlayers() {
    auto players = server_->GetAllPlayers();
    for (const auto& p : players)
        if (p.player_id != player_->GetID())
            SendPlayerUpdate(p.player_id, p.x, p.y, p.facing);
}
void ClientConnection::BroadcastPlayerJoined() {
    server_->BroadcastToOthers(player_->GetID(), [this](auto s){ s->SendPlayerUpdate(player_->GetID(), player_->GetX(), player_->GetY(), player_->GetFacing()); });
}
/*
void GameSession::BroadcastPlayerMoved() {
    server_->BroadcastToOthers(player_id_, [this](auto s){ s->SendPlayerUpdate(player_id_, player_x_, player_y_); });
}*/

void ClientConnection::BroadcastPlayerLeft() {
    uint32_t my_id = player_->GetID();
    server_->RemoveSession(my_id);  // Remove first
    server_->BroadcastToAll([my_id](auto s){ s->SendPlayerLeft(my_id); });  // Then broadcast
}

void ClientConnection::RawSend(std::shared_ptr<std::vector<uint8_t>> data) {
    // Track bandwidth
    BandwidthMonitor::Instance().RecordOutgoing(data->size());

    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(*data),
                      [this, self, data](std::error_code ec, std::size_t) {
                          // Error handling
                      });
}