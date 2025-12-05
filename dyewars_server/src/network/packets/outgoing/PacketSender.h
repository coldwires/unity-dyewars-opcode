/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#pragma once
#include <memory>
#include "network/packets/Protocol.h"
#include "network/packets/OpCodes.h"
#include "server/ClientConnection.h"

namespace Packets::PacketSender {

    inline void Welcome(
            const std::shared_ptr<ClientConnection> &client,
            const std::shared_ptr<Player> &player
            ) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::S_Welcome);
        Protocol::PacketWriter::WriteUInt(pkt.payload, player->GetID());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetX());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetY());
        Protocol::PacketWriter::WriteByte(pkt.payload, player->GetFacing());
        // Can add more later: level, name, appearance...
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
        }

/// Send player position update
/// Payload: [playerId:4][x:2][y:2][facing:1]
    inline void PlayerUpdate(
            const std::shared_ptr<ClientConnection> &client,
            uint64_t player_id,
            int16_t x,
            int16_t y,
            uint8_t facing
    ) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Batch::S_RemotePlayer_Update);
        Protocol::PacketWriter::WriteByte(pkt.payload, 1);  // Count = 1 for single update
        Protocol::PacketWriter::WriteUInt(pkt.payload, player_id);
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

/// Send position correction to local player
/// Payload: [x:2][y:2][facing:1]
    inline void PositionCorrection(
            const std::shared_ptr<ClientConnection> &client,
            int16_t x,
            int16_t y,
            uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::S_Position_Correction);
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

/// Send facing correction
/// Payload: [facing:1]
    inline void FacingCorrection(
            const std::shared_ptr<ClientConnection> &client,
            uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::S_Facing_Correction);
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

/// Notify that a player left
/// Payload: [playerId:4]
    inline void PlayerLeft(
            const std::shared_ptr<ClientConnection> &client,
            uint32_t player_id) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::RemotePlayer::S_Left_Game);
        Protocol::PacketWriter::WriteUInt(pkt.payload, player_id);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

/// Notify that a player joined
/// Payload: [playerId:4][x:2][y:2][facing:1]
    inline void PlayerJoined(
            const std::shared_ptr<ClientConnection> &client,
            uint32_t player_id,
            int16_t x,
            int16_t y,
            uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::RemotePlayer::S_Joined_Game);
        Protocol::PacketWriter::WriteUInt(pkt.payload, player_id);
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
        Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

/// Server shutdown notification
/// Payload: [reason:1]
    inline void ServerShutdown(
            const std::shared_ptr<ClientConnection> &client,
            uint8_t reason = 0x01) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Server::Connection::S_ServerShutdown.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, reason);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    inline void GivePlayerID(const std::shared_ptr<ClientConnection> &client) {
        Protocol::Packet pkt;
        // TODO NOT the right opcode, just experimenting
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Server::Connection::S_HandshakeAccepted.op);
        pkt.size = pkt.payload.size();
        client->SendPacket(pkt);
    }
}
