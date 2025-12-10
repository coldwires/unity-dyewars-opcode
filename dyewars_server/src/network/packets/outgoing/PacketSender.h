/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#pragma once

#include <memory>
#include "network/packets/Protocol.h"
#include "network/packets/OpCodes.h"
#include "server/ClientConnection.h"
#include "game/Player.h"

namespace Packets::PacketSender {

    /// Send welcome packet to newly connected player
    /// Payload: [playerId:8][x:2][y:2][facing:1]
    inline void Welcome(
            const std::shared_ptr<ClientConnection> &client,
            const std::shared_ptr<Player> &player
    ) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Welcome.op);
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player->GetID());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetX());
        Protocol::PacketWriter::WriteShort(pkt.payload, player->GetY());
        Protocol::PacketWriter::WriteByte(pkt.payload, player->GetFacing());
        // Can add more later: level, name, appearance...
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    /// Batch player spatial update - sends position/facing for multiple players.
    /// Client creates player if it doesn't exist, otherwise updates position/facing.
    /// Payload: [count:1][[playerId:8][x:2][y:2][facing:1]]... (13 bytes per player)
    inline void BatchPlayerSpatial(
            const std::shared_ptr<ClientConnection> &client,
            const std::vector<std::shared_ptr<Player>> &players
    ) {
        if (players.empty()) return;

        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Batch::Server::S_Player_Spatial.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, 0); // Placeholder for count

        uint8_t count = 0;
        for (const auto &player : players) {
            Protocol::PacketWriter::WriteUInt64(pkt.payload, player->GetID());
            Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(player->GetX()));
            Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(player->GetY()));
            Protocol::PacketWriter::WriteByte(pkt.payload, player->GetFacing());
            count++;

            // Max 255 players per packet (count is uint8_t)
            if (count == 255) break;
        }

        pkt.payload[1] = count; // Patch in actual count
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    /// Single player spatial update - convenience wrapper for one player
    /// Payload: [count:1][[playerId:8][x:2][y:2][facing:1]]
    inline void PlayerSpatial(
            const std::shared_ptr<ClientConnection> &client,
            const uint64_t player_id,
            const int16_t x,
            const int16_t y,
            const uint8_t facing
    ) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Batch::Server::S_Player_Spatial.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, 1);  // Count = 1
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player_id);
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
            const int16_t x,
            const int16_t y,
            const uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Position_Correction.op);
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
            const uint8_t facing) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::LocalPlayer::Server::S_Facing_Correction.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, facing);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    /// Notify that a player left
    /// Payload: [playerId:8]
    inline void PlayerLeft(
            const std::shared_ptr<ClientConnection> &client,
            const uint64_t player_id) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::RemotePlayer::Server::S_Left_Game.op);
        Protocol::PacketWriter::WriteUInt64(pkt.payload, player_id);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    // DEPRECATED: Use BatchPlayerSpatial or PlayerSpatial instead
    // /// Notify that a player joined
    // /// Payload: [playerId:8][x:2][y:2][facing:1]
    // inline void PlayerJoined(
    //         const std::shared_ptr<ClientConnection> &client,
    //         const uint64_t player_id,
    //         const int16_t x,
    //         const int16_t y,
    //         const uint8_t facing) {
    //     Protocol::Packet pkt;
    //     Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::RemotePlayer::S_Joined_Game);
    //     Protocol::PacketWriter::WriteUInt64(pkt.payload, player_id);
    //     Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(x));
    //     Protocol::PacketWriter::WriteShort(pkt.payload, static_cast<uint16_t>(y));
    //     Protocol::PacketWriter::WriteByte(pkt.payload, facing);
    //     pkt.size = static_cast<uint16_t>(pkt.payload.size());
    //     client->SendPacket(pkt);
    // }

    /// Server shutdown notification
    /// Payload: [reason:1]
    inline void ServerShutdown(
            const std::shared_ptr<ClientConnection> &client,
            const uint8_t reason = 0x01) {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Connection::Server::S_ServerShutdown.op);
        Protocol::PacketWriter::WriteByte(pkt.payload, reason);
        pkt.size = static_cast<uint16_t>(pkt.payload.size());
        client->SendPacket(pkt);
    }

    inline void GivePlayerID(const std::shared_ptr<ClientConnection> &client) {
        Protocol::Packet pkt;
        // TODO NOT the right opcode, just experimenting
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Connection::Server::S_HandshakeAccepted.op);
        pkt.size = pkt.payload.size();
        client->SendPacket(pkt);
    }
}
