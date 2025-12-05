/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "PacketHandler.h"
#include "server/ClientConnection.h"
#include "server/GameServer.h"
#include "network/Packets/Protocol.h"
#include "network/packets/Opcodes.h"
#include "game/Actions.h"
#include "core/Log.h"

namespace PacketHandler{
    void Handle(
            std::shared_ptr<ClientConnection> client,
            const vector<uint8_t> &data,
            GameServer *server
            )
    {
        if(data.empty()) return;

        // Validate packet size
        assert(data.size() <= Protocol::MAX_PAYLOAD_SIZE && "Packethandler handle data is over max protocol bytes");

        uint64_t client_id = client->GetClientID();

        // Check if this client has a player
        uint64_t player_id = server->Players().GetPlayerIDForClient(client_id);
        if (player_id == 0) {
            Log::Warn("Packet from client {} with no player", client_id);
            return;
        }

        size_t offset = 0;
        uint8_t opcode = Protocol::PacketReader::ReadByte(data, offset);

        switch (opcode) {
            case Protocol::Opcode::Movement::C_Move_Request: {
                if (data.size() < 3) {
                    Log::Warn("Move packet too small from client {}", client_id);
                    return;
                }

                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                uint8_t facing = Protocol::PacketReader::ReadByte(data, offset);

                // Validate direction
                if (direction > 3 || facing > 3) {
                    Log::Warn("Invalid direction/facing from client {}", client_id);
                    return;
                }
                // Queue the action - will be processed in game loop
                server->Players().QueueAction(Actions::MoveCommand{
                    player_id,
                    direction,
                    facing
                }, client_id);
                break;
            }

            case Protocol::Opcode::Movement::C_Turn_Request: {
                if (data.size() < 2) {
                    Log::Warn("Turn packet too small from client {}", client_id);
                    return;
                }

                uint8_t direction = Protocol::PacketReader::ReadByte(data, offset);
                if (direction > 3) {
                    Log::Warn("Invalid turn direction from client {}", client_id);
                    return;
                }

                server->Players().QueueAction(Actions::TurnCommand{
                        player_id,
                        direction
                }, client_id);
                break;
            }

            case Protocol::Opcode::Movement::C_Interact_Request: {
                // TODO: Queue interact action
                Log::Debug("Interact request from player {}", player_id);
                break;
            }

            case Protocol::Opcode::Combat::C_Attack_Request: {
                // TODO: Queue attack action
                Log::Debug("Attack request from player {}", player_id);
                break;
            }

            default:
                Log::Warn("Unknown opcode 0x{:02X} from client {}", opcode, client_id);
                break;
        }

        // Pass commands to systems. Then in gameserver loop -> calls sysstems tick which calls processcommands. does command on game loop thread.
    }
}

/*
 *

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

                case Protocol::Opcode::C_Custom: // Custom
            {
                std::vector<uint8_t> custom(data.begin() + 1, data.end());
                auto resp = lua_engine_->ProcessCustomMessage(custom);
                if (!resp.empty()) SendCustomMessage(resp);
            }
                break;

*/