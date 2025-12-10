// OpCodes.h
// Centralized definition of all network packet opcodes.
// This is the single source of truth for packet types used by client and server.
//
// Usage:
//   Protocol::Opcode::Connection::Server::S_HandshakeAccepted
//   Protocol::Opcode::Connection::Client::C_Handshake_Request
//   Protocol::Opcode::Movement::Client::C_Move_Request
//   Protocol::Opcode::LocalPlayer::Server::S_Welcome
//   Protocol::Opcode::Batch::Server::S_Player_Spatial
//
// Naming Convention:
//   C_ = Client -> Server
//   S_ = Server -> Client

#pragma once

#include <cstdint>

struct OpCodeInfo {
    uint8_t op;
    const char *desc;
    const char *name;
    uint8_t payloadSize;

    /// Payload size constant for variable-length packets
    static constexpr uint8_t VARIABLE_SIZE = 0;
};

namespace Protocol::Opcode {

    // ========================================================================
    // CONNECTION & HANDSHAKE - 0xF0-0xFF, 0x00
    // ========================================================================
    namespace Connection {
        namespace Server {
            // Server accepts handshake.
            // Payload: [serverVersion:2][serverMagic:4]
            constexpr OpCodeInfo S_HandshakeAccepted = {
                    0xF0,
                    "Server accepts client handshake",
                    "S_HandshakeAccepted",
                    7  // opcode(1) + version(2) + magic(4)
            };

            // Server rejects handshake.
            // Payload: [reasonCode:1][reasonLength:1][reason:variable]
            constexpr OpCodeInfo S_Handshake_Rejected = {
                    0xF1,
                    "Server rejects client handshake",
                    "S_Handshake_Rejected",
                    OpCodeInfo::VARIABLE_SIZE
            };

            // Server cleanly disconnects client due to shutdown.
            // Payload: [reason:1]
            constexpr OpCodeInfo S_ServerShutdown = {
                    0xF2,
                    "Server shutting down",
                    "S_ServerShutdown",
                    2  // opcode(1) + reason(1)
            };

            // Server acknowledges disconnect.
            // Payload: (none)
            constexpr OpCodeInfo S_Disconnect_Acknowledged = {
                    0xFF,
                    "Server acknowledges client disconnect",
                    "S_Disconnect_Acknowledged",
                    1  // opcode only
            };

            // Server ping request.
            // Payload: [timestamp:4]
            constexpr OpCodeInfo S_Ping_Request = {
                    0xF8,
                    "Server requests ping from client",
                    "S_Ping_Request",
                    5  // opcode(1) + timestamp(4)
            };

            // Server heartbeat acknowledgment.
            // Payload: (none)
            constexpr OpCodeInfo S_Heartbeat_Response = {
                    0xFB,
                    "Server acknowledges client heartbeat",
                    "S_Heartbeat_Response",
                    1  // opcode only
            };
        }

        namespace Client {
            // Initial connection handshake from client.
            // Payload: [version:2][clientMagic:4]
            constexpr OpCodeInfo C_Handshake_Request = {
                    0x00,
                    "Client sends handshake to server",
                    "C_Handshake_Request",
                    7  // opcode(1) + version(2) + magic(4)
            };

            // Client requests graceful disconnect.
            // Payload: (none)
            constexpr OpCodeInfo C_Disconnect_Request = {
                    0xFE,
                    "Client requests disconnect",
                    "C_Disconnect_Request",
                    1  // opcode only
            };

            // Client pong response.
            // Payload: [timestamp:4]
            constexpr OpCodeInfo C_Pong_Response = {
                    0xF9,
                    "Client responds to server ping",
                    "C_Pong_Response",
                    5  // opcode(1) + timestamp(4)
            };

            // Client heartbeat.
            // Payload: (none)
            constexpr OpCodeInfo C_Heartbeat_Request = {
                    0xFA,
                    "Client sends heartbeat",
                    "C_Heartbeat_Request",
                    1  // opcode only
            };
        }
    }

    // ========================================================================
    // MOVEMENT & ACTIONS - 0x01-0x04
    // ========================================================================
    namespace Movement {
        namespace Client {
            // Player requests to move.
            // Payload: [direction:1][facing:1]
            constexpr OpCodeInfo C_Move_Request = {
                    0x01,
                    "Client requests movement",
                    "C_Move_Request",
                    3  // opcode(1) + direction(1) + facing(1)
            };

            // Player requests to turn.
            // Payload: [direction:1]
            constexpr OpCodeInfo C_Turn_Request = {
                    0x02,
                    "Client requests turn",
                    "C_Turn_Request",
                    2  // opcode(1) + direction(1)
            };

            // Player requests to warp/teleport.
            // Payload: [targetMapId:2][targetX:2][targetY:2]
            constexpr OpCodeInfo C_Warp_Request = {
                    0x03,
                    "Client requests warp",
                    "C_Warp_Request",
                    7  // opcode(1) + mapId(2) + x(2) + y(2)
            };

            // Player interacts with something.
            // Payload: (none)
            constexpr OpCodeInfo C_Interact_Request = {
                    0x04,
                    "Client requests interaction",
                    "C_Interact_Request",
                    1  // opcode only
            };
        }
    }

    // ========================================================================
    // LOCAL PLAYER - 0x10-0x12
    // ========================================================================
    namespace LocalPlayer {
        namespace Server {
            // Welcome packet with player ID and initial state.
            // Payload: [playerId:8][x:2][y:2][facing:1]
            constexpr OpCodeInfo S_Welcome = {
                    0x10,
                    "Server sends welcome with player state",
                    "S_Welcome",
                    14  // opcode(1) + playerId(8) + x(2) + y(2) + facing(1)
            };

            // Position correction (rubber-banding).
            // Payload: [x:2][y:2][facing:1]
            constexpr OpCodeInfo S_Position_Correction = {
                    0x11,
                    "Server corrects client position",
                    "S_Position_Correction",
                    6  // opcode(1) + x(2) + y(2) + facing(1)
            };

            // Facing correction.
            // Payload: [facing:1]
            constexpr OpCodeInfo S_Facing_Correction = {
                    0x12,
                    "Server corrects client facing",
                    "S_Facing_Correction",
                    2  // opcode(1) + facing(1)
            };
        }
    }

    // ========================================================================
    // REMOTE PLAYERS - 0x26
    // ========================================================================
    namespace RemotePlayer {
        namespace Server {
            // Player left the game.
            // Payload: [playerId:8]
            constexpr OpCodeInfo S_Left_Game = {
                    0x26,
                    "Remote player left the game",
                    "S_Left_Game",
                    9  // opcode(1) + playerId(8)
            };
        }
    }

    // ========================================================================
    // BATCH UPDATES - 0x25
    // ========================================================================
    namespace Batch {
        namespace Server {
            // Batch player spatial update (position + facing).
            // Creates player on client if doesn't exist, otherwise updates.
            // Payload: [count:1][[playerId:8][x:2][y:2][facing:1]]... (13 bytes per player)
            constexpr OpCodeInfo S_Player_Spatial = {
                    0x25,
                    "Batch player position/facing update",
                    "S_Player_Spatial",
                    OpCodeInfo::VARIABLE_SIZE  // 2 + (13 * count)
            };
        }
    }

    // ========================================================================
    // COMBAT - 0x40
    // ========================================================================
    namespace Combat {
        namespace Client {
            // Player attacks.
            // Payload: (none, uses facing)
            constexpr OpCodeInfo C_Attack_Request = {
                    0x40,
                    "Client requests attack",
                    "C_Attack_Request",
                    1  // opcode only
            };
        }
    }

} // namespace Protocol::Opcode

// See UnusedOpCodes.h for planned but not yet implemented opcodes
#include "UnusedOpCodes.h"
