#include "ClientConnection.h"
#include "GameServer.h" // Needed here to call Server methods
#include "network/BandwidthMonitor.h"
#include "network/Packets/OpCodes.h"
#include "network/packets/incoming/PacketHandler.h"
#include <core/Log.h>
#include <fstream>
#include <iomanip>
using namespace Protocol::Opcode;

ClientConnection::ClientConnection(asio::ip::tcp::socket socket,
                                   GameServer* server,
                                   uint64_t client_id)
        : socket_(std::move(socket)),
        server_(server),
        client_id_(client_id),
        handshake_timer_(socket_.get_executor())
        {
            try{
                client_ip_ = socket.remote_endpoint().address().to_string();
                // TODO DNS lookup is slow, move to worker thread
                client_hostname_ = client_ip_;
            } catch (...) {
                client_ip_ = "unknown";
                client_hostname_ = "unknown";
            }
        }

ClientConnection::~ClientConnection(){
    try {
        Disconnect("");
    } catch (...) {
        // Ignore errors during destruction
    }
}

void ClientConnection::Start() {
    Log::Info("IP: {} Hostname: {} starting client connection.",client_ip_, client_hostname_);

    //5 seconds to respond
    StartHandshakeTimeout();

    //Begin reading
    ReadPacketHeader();
}

void ClientConnection::Disconnect(const std::string& reason) {
    if (!reason.empty()) {
        Log::Debug("Client {} disconnecting: {}", client_id_, reason);
    }

    server_->Players().RemoveByClientID(client_id_);
    server_->Clients().RemoveClient(client_id_);
    server_->Limiter().RemoveConnection(client_ip_);
    CloseSocket();
    Log::Debug("Client {} IP: {} disconnected.", client_id_, client_ip_);
}

void ClientConnection::CloseSocket() {
    std::error_code ec;
    handshake_timer_.cancel();
    if (socket_.is_open()) {
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

// =====================
// PACKETS
// =====================
/// Send
void ClientConnection::SendPacket(const Protocol::Packet& pkt) {
    auto self(shared_from_this());
    auto data = std::make_shared<std::vector<uint8_t>>(pkt.ToBytes());

    // Track bandwidth
    BandwidthMonitor::Instance().RecordOutgoing(data->size());

    asio::async_write(socket_, asio::buffer(*data),
        [this, self, data](std::error_code ec, std::size_t) {});
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

/// Receive
void ClientConnection::ReadPacketHeader() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_buffer_, 4),
                     [this, self](std::error_code ec, std::size_t) {
                         if (ec) {
                             if (!handshake_complete_) {
                                 Disconnect("connection closed before handshake");
                             } else {
                                 Log::Info("Client {} disconnected.", client_id_);
                             }
                             return;
                         }

                         if (header_buffer_[0] == Protocol::MAGIC_1 &&
                             header_buffer_[1] == Protocol::MAGIC_2) {
                             uint16_t size = (header_buffer_[2] << 8) | header_buffer_[3];

                             if (size > 0 && size < Protocol::MAX_PAYLOAD_SIZE) {
                                 ReadPacketPayload(size);
                             } else {
                                 Log::Warn("Client {} sent invalid size: {}", client_id_, size);
                                 if(socket_.is_open()) ReadPacketHeader();
                             }
                         } else {
                             // Invalid magic bytes
                             if (!handshake_complete_) {
                                 Log::Debug("Invalid magic bytes from client: {} when expecting handshake. Got 0x{:02X} 0x{:02X}",
                                           client_id_, (int)header_buffer_[0],(int)header_buffer_[1]);
                                 FailHandshake("invalid header while waiting for handshake");
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
                     [this, self, buffer, size](std::error_code ec, std::size_t) {
                         if (ec) {
                             Disconnect("read error");
                             return;
                         }


                         BandwidthMonitor::Instance().RecordIncoming(size + 4);  // payload + header
                         LogPacketReceived(*buffer, size);
                         // If handshake not complete, this must be the handshake packet
                         if (!handshake_complete_) {
                             HandleHandshakePacket(*buffer);
                         } else {
                             HandlePacket(*buffer);
                         }
                         // Only continue reading if socket is still open
                         if (socket_.is_open()) {
                             ReadPacketHeader();
                         }
                     });
}

void ClientConnection::HandlePacket(const std::vector<uint8_t>& data) {
    if (data.empty()) return;
    // Forward to PacketHandler - we don't process game logic here
    PacketHandler::Handle(shared_from_this(), data, server_);
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
    /// \note
    /// Expected format:\n
    /// Byte 0: Opcode (0x00)\n
    /// Bytes 1-2: Protocol version (0x00 0x01)\n
    /// Bytes 3-6: Client magic ("DYEW" = 0x44 0x59 0x45 0x57)\n
    const auto& op = Protocol::Opcode::Client::Connection::C_Handshake_Request;

    if (data.size() != op.payloadSize) {
        return FailHandshake(std::format("invalid packet size (got {}, expected {})",
                                         data.size(), op.payloadSize));
    }

    size_t offset = 0;
    uint8_t opcode = Protocol::PacketReader::ReadByte(data, offset);
    uint16_t version = Protocol::PacketReader::ReadShort(data, offset);
    uint32_t magic = Protocol::PacketReader::ReadUInt(data, offset);

    if (opcode != op.op) {
        return FailHandshake(std::format("expected opcode 0x{:02X}, got 0x{:02X}", op.op, opcode));
    }

    if (version != Protocol::VERSION) {
        return FailHandshake(std::format("version mismatch (client: 0x{:04X}, server: 0x{:04X})",
                                         version, Protocol::VERSION));
    }

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

    server_->OnClientLogin(shared_from_this());
}

void ClientConnection::FailHandshake(const std::string& reason) {
    Log::Warn("IP: {} Hostname: {} handshake failed because: {}.", client_ip_, client_hostname_, reason);
    server_->Limiter().RecordFailure(client_ip_);
    LogFailedConnection(reason);
    Disconnect(reason);
}
/// =============================\n
/// LOGGING\n
/// =============================\n
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
/*


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

void GameSession::BroadcastPlayerMoved() {
    server_->BroadcastToOthers(player_id_, [this](auto s){ s->SendPlayerUpdate(player_id_, player_x_, player_y_); });
}

void ClientConnection::BroadcastPlayerLeft() {
    uint32_t my_id = player_->GetID();
    server_->RemoveSession(my_id);  // Remove first
    server_->BroadcastToAll([my_id](auto s){ s->SendPlayerLeft(my_id); });  // Then broadcast
}
*/
