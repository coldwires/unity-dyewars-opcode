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

    inline void GivePlayerID(const std::shared_ptr<ClientConnection>& client)
    {
        Protocol::Packet pkt;
        // TODO NOT the right opcode, just experimenting
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::ServerOpcode::Handshake_Accepted.op);
        pkt.size = pkt.payload.size();
        client->SendPacket(pkt);
    }
};
