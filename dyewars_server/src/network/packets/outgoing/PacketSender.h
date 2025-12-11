/// Outgoing packet builders. Each function builds and sends a specific packet type.
#pragma once

#include <memory>
#include "network/packets/Protocol.h"
#include "network/packets/OpCodes.h"
#include "server/ClientConnection.h"
#include "game/Player.h"

namespace Packets::PacketSender {

    inline void Welcome(const std::shared_ptr<ClientConnection>& client,
                        const std::shared_ptr<Player>& player) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Welcome.op);
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player->GetID());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetX());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetY());
        Protocol::PacketWriter::WriteByte(pkt.payload, player->GetFacing());
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void BatchPlayerSpatial(const std::shared_ptr<ClientConnection>& client,
                                   const std::vector<std::shared_ptr<Player>>& players) {
        if (players.empty()) return;

        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Batch::Server::S_Player_Spatial.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, 0);  // Count placeholder

        uint8_t count = 0;
        for (const auto& player : players) {
            Protocol::PacketWriter::WriteUInt64(pkt.payload, player->GetID());
            Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(player->GetX()));
            Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(player->GetY()));
            Protocol::PacketWriter::WriteByte(pkt.payload, player->GetFacing());
            if (++count == 255) break;
        }

        pkt.payload[1] = count;
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void PlayerSpatial(const std::shared_ptr<ClientConnection>& client,
                              uint64_t player_id, int16_t x, int16_t y, uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Batch::Server::S_Player_Spatial.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, 1);  // Count = 1
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player_id);
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void PositionCorrection(const std::shared_ptr<ClientConnection>& client,
                                   int16_t x, int16_t y, uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Position_Correction.op);
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void FacingCorrection(const std::shared_ptr<ClientConnection>& client, uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Facing_Correction.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void PlayerLeft(const std::shared_ptr<ClientConnection>& client, uint64_t player_id) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::RemotePlayer::Server::S_Left_Game.op);
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player_id);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void ServerShutdown(const std::shared_ptr<ClientConnection>& client, uint8_t reason = 0x01) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Connection::Server::S_ServerShutdown.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, reason);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }

    inline void GivePlayerID(const std::shared_ptr<ClientConnection>& client) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Connection::Server::S_HandshakeAccepted.op);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->QueuePacket(pkt);
    }
}
