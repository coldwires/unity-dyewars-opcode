/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "PacketHandler.h"
#include "server/ClientConnection.h"
#include "server/GameServer.h"
#include "network/Packets/Protocol.h"
#include "network/packets/Opcodes.h"
#include "game/actions/Actions.h"
#include "core/Log.h"

namespace PacketHandler {
    void Handle(
            const std::shared_ptr<ClientConnection> &client,
            const std::vector<uint8_t> &data,
            GameServer *server
    ) {
        if (data.empty()) return;

        // Validate packet size
        assert(data.size() <= Protocol::MAX_PAYLOAD_SIZE && "Packethandler handle data is over max protocol bytes");


        //
        // // Check if this client has a player
        // uint64_t player_id = server->Players().GetPlayerIDForClient(client_id);
        // if (player_id == 0) {
        //     Log::Warn("Packet from client {} with no player", client_id);
        //     return;
        // }

        uint8_t opcode = data[0];

        uint64_t client_id = client->GetClientID();
        size_t offset = 0;
        switch (opcode)
            //uint8_t opcode = Protocol::PacketReader::ReadByte(data, offset))
        {
            case Protocol::Opcode::Movement::Client::C_Move_Request.op: {
                if (data.size() != Protocol::Opcode::Movement::Client::C_Move_Request.payloadSize) {
                    Log::Warn("Move packet size mismatch from client {} (got {}, expected {})",
                              client_id, data.size(), Protocol::Opcode::Movement::Client::C_Move_Request.payloadSize);
                    return;
                }
                const uint8_t direction = data[1];
                const uint8_t facing = data[2];

                // Queue the action - will be processed in game loop
                Actions::Movement::Move(server, client->GetClientID(), direction, facing);
                break;
            }

            case Protocol::Opcode::Movement::Client::C_Turn_Request.op: {
                if (data.size() != Protocol::Opcode::Movement::Client::C_Turn_Request.payloadSize) {
                    Log::Warn("Turn packet size mismatch from client {} (got {}, expected {})",
                              client_id, data.size(), Protocol::Opcode::Movement::Client::C_Turn_Request.payloadSize);
                    return;
                }
                uint8_t facing = data[1];
                Actions::Movement::Turn(server, client->GetClientID(), facing);
                break;
            }

            case Protocol::Opcode::Movement::Client::C_Interact_Request.op: {
                // TODO: Queue interact action
                Log::Debug("Interact request from player {}", client_id);
                break;
            }

            case Protocol::Opcode::Combat::Client::C_Attack_Request.op: {
                // TODO: Queue attack action
                Log::Debug("Attack request from player {}", client_id);
                break;
            }

            case Protocol::Opcode::Connection::Client::C_Pong_Response.op: {
                // Client responded to our ping - calculate RTT (Round Trip Time)
                //
                // RTT = now - time_when_ping_was_sent
                //
                // We use the atomic getter because:
                // - Game thread writes ping_sent_time_ (via SendPingToAllClients → SendPing)
                // - We're reading it here (IO thread)
                // - Without atomics, we could get a torn read (half-updated timestamp)
                //
                auto now = std::chrono::steady_clock::now();
                auto ping_sent = client->GetPingSentTime();
                auto rtt = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - ping_sent
                ).count();

                // Clamp to reasonable values
                // Negative RTT could happen if clock skew or ping_sent wasn't set properly
                // Very high RTT (>5s) is likely a stale ping response after reconnect
                if (rtt < 0) rtt = 0;
                if (rtt > 5000) rtt = 5000;

                client->RecordPing(static_cast<uint32_t>(rtt));
                Log::Trace("Client {} ping: {}ms (avg: {}ms)", client_id, rtt, client->GetPing());
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