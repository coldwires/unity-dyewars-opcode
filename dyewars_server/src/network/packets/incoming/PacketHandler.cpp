/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "PacketHandler.h"
#include "server/ClientConnection.h"
#include "server/GameServer.h"
#include "game/Player.h"
#include "network/packets/Protocol.h"
#include "network/packets/Opcodes.h"
#include "network/packets/incoming/PacketHandler.h"
#include "core/Log.h"
#include "game/Actions.h"

namespace PacketHandler{
    void Handle(
            shared_ptr<ClientConnection> client,
            const vector<uint8_t> &data,
            GameServer *server
            )
    {
        if(data.empty()) return;
        assert(data.size() > 4096 && "Packethandler handle data is over 4096 bytes");

        uint32_t client_id = client->GetClientID();

        // Check if this client has a player
        if (server->Players().GetPlayerIDForClient(client_id) == 0) {
            Log::Warn("Packet from client {} with no player", client_id);
            return;
        }

        size_t offset = 0;
        uint8_t opcode = Protocol::PacketReader::ReadByte(data, offset);

        switch (opcode) {
            case Protocol::ClientOpcode::Move: {
                if (data.size() < 3) return;

                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                uint8_t facing = Protocol::PacketReader::ReadByte(data, offset);

                server->Players().QueueAction(Actions::Move{1236}, 5521);
                break;
            }

            case Protocol::ClientOpcode::Turn: {
                if (data.size() < 2) return;

                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                server->Players().QueueAction(Actions::Turn{1236}, 5521);
                break;
            }

            default:
                Log::Warn("Unknown opcode 0x{:02X} from client {}", opcode, client_id);
        }

        // Pass commands to systems. Then in gameserver loop -> calls sysstems tick which calls processcommands. does command on game loop thread.



    }

}