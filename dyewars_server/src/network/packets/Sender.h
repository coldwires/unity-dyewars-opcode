#pragma once
#include "server/ClientConnection.h"
#include "network/packets/OpCodes.h"
namespace Protocol
{
    /// Cleanly d/c the client. Reason: Server shutdown \n |reason: 1 byte|
    inline void Send_ServerShutdown(std::shared_ptr<ClientConnection> client, uint8_t reason = 0x01)
    {
        Protocol::Packet pkt;
        Protocol::PacketWriter::WriteByte(pkt.payload, Protocol::Opcode::Server::Connection::S_ServerShutdown.op);
        Protocol::PacketWriter::WriteUInt(pkt.payload, reason);
        pkt.size = pkt.payload.size();
        client->SendPacket(pkt);
    }
}