// PacketOpcodes.cs
// Centralized definition of all network packet opcodes.
// This is the single source of truth for packet types used by client and server.
//
// Opcode Ranges (convention):
//   0x01 - 0x0F : Client -> Server : Player Actions (move, turn, interact)
//   0x10 - 0x1F : Server -> Client : Local Player Updates (position, stats)
//   0x20 - 0x2F : Server -> Client : World Updates (other players, batch)
//   0x30 - 0x3F : Server -> Client : Combat & Effects
//   0x40 - 0x4F : Client -> Server : Combat Actions
//   0x50 - 0x5F : Bidirectional    : Chat & Social
//   0x60 - 0x6F : Client -> Server : Inventory & Items
//   0x70 - 0x7F : Server -> Client : Inventory Updates
//   0x80 - 0x8F : Client -> Server : Trade & Economy
//   0x90 - 0x9F : Server -> Client : Trade Updates
//   0xA0 - 0xAF : Client -> Server : Skills & Abilities
//   0xB0 - 0xBF : Server -> Client : Skill Updates
//   0xC0 - 0xCF : Reserved for future use
//   0xD0 - 0xDF : Reserved for future use
//   0xE0 - 0xEF : Admin / Debug commands
//   0xF0 - 0xFF : System (ping, handshake, disconnect)

namespace DyeWars.Network.Protocol
{
    public static class Opcode
    {
        // ====================================================================
        // CLIENT -> SERVER: Player Actions (0x01 - 0x0F)
        // ====================================================================

        /// <summary>
        /// Player requests to move in a direction.
        /// Payload: [direction:1][facing:1]
        /// </summary>
        public const byte C_Move = 0x01;

        /// <summary>
        /// Player requests to turn (change facing without moving).
        /// Payload: [direction:1]
        /// </summary>
        public const byte C_Turn = 0x04;

        /// <summary>
        /// Player interacts with something in front of them.
        /// Payload: (none)
        /// </summary>
        public const byte C_Interact = 0x05;

        // ====================================================================
        // SERVER -> CLIENT: Local Player Updates (0x10 - 0x1F)
        // ====================================================================

        /// <summary>
        /// Server confirms/corrects local player's position.
        /// Payload: [x:2][y:2][facing:1]
        /// </summary>
        public const byte S_MyPosition = 0x10;

        /// <summary>
        /// Custom server response (for testing/debug).
        /// Payload: (variable)
        /// </summary>
        public const byte S_CustomResponse = 0x11;

        /// <summary>
        /// Server assigns player ID to the client.
        /// Payload: [playerId:4]
        /// </summary>
        public const byte S_PlayerIdAssignment = 0x13;

        /// <summary>
        /// Server confirms/corrects local player's facing.
        /// Payload: [facing:1]
        /// </summary>
        public const byte S_MyFacing = 0x15;

        // ====================================================================
        // SERVER -> CLIENT: World Updates (0x20 - 0x2F)
        // ====================================================================

        /// <summary>
        /// Update for another player's position/facing.
        /// Payload: [playerId:4][x:2][y:2][facing:1]
        /// </summary>
        public const byte S_OtherPlayerUpdate = 0x12;

        /// <summary>
        /// Another player has left the game.
        /// Payload: [playerId:4]
        /// </summary>
        public const byte S_PlayerLeft = 0x14;

        /// <summary>
        /// Batch update for multiple players (efficiency optimization).
        /// Payload: [count:1][playerId:4, x:2, y:2, facing:1]...
        /// </summary>
        public const byte S_BatchUpdate = 0x20;

        /// <summary>
        /// A new player has joined (includes initial state).
        /// Payload: [playerId:4][x:2][y:2][facing:1][nameLength:1][name:variable]
        /// </summary>
        public const byte S_PlayerJoined = 0x21;

        // ====================================================================
        // SERVER -> CLIENT: Combat & Effects (0x30 - 0x3F)
        // ====================================================================

        /// <summary>
        /// Play a visual effect at a location.
        /// Payload: [effectId:2][x:2][y:2][param:1]
        /// </summary>
        public const byte S_PlayEffect = 0x30;

        /// <summary>
        /// Player took damage.
        /// Payload: [playerId:4][damage:2][currentHp:2][maxHp:2]
        /// </summary>
        public const byte S_Damage = 0x31;

        /// <summary>
        /// Player was healed.
        /// Payload: [playerId:4][amount:2][currentHp:2][maxHp:2]
        /// </summary>
        public const byte S_Heal = 0x32;

        /// <summary>
        /// Player died.
        /// Payload: [playerId:4]
        /// </summary>
        public const byte S_Death = 0x33;

        /// <summary>
        /// Player respawned.
        /// Payload: [playerId:4][x:2][y:2]
        /// </summary>
        public const byte S_Respawn = 0x34;

        // ====================================================================
        // CLIENT -> SERVER: Combat Actions (0x40 - 0x4F)
        // ====================================================================

        /// <summary>
        /// Player attacks in their facing direction.
        /// Payload: (none, direction determined by facing)
        /// </summary>
        public const byte C_Attack = 0x40;

        /// <summary>
        /// Player uses a skill/ability.
        /// Payload: [skillId:2][targetX:2][targetY:2]
        /// </summary>
        public const byte C_UseSkill = 0x41;

        // ====================================================================
        // BIDIRECTIONAL: Chat & Social (0x50 - 0x5F)
        // ====================================================================

        /// <summary>
        /// Chat message (client sends, server broadcasts).
        /// Payload: [channelId:1][messageLength:2][message:variable]
        /// </summary>
        public const byte ChatMessage = 0x50;

        /// <summary>
        /// Emote/gesture.
        /// Payload: [emoteId:1]
        /// </summary>
        public const byte Emote = 0x51;

        // ====================================================================
        // SYSTEM (0xF0 - 0xFF)
        // ====================================================================

        /// <summary>
        /// Ping request (measure latency).
        /// Payload: [timestamp:4]
        /// </summary>
        public const byte Ping = 0xF0;

        /// <summary>
        /// Pong response to ping.
        /// Payload: [timestamp:4]
        /// </summary>
        public const byte Pong = 0xF1;

        /// <summary>
        /// Server is kicking the player.
        /// Payload: [reasonLength:1][reason:variable]
        /// </summary>
        public const byte S_Kick = 0xFE;

        /// <summary>
        /// Graceful disconnect notification.
        /// Payload: (none)
        /// </summary>
        public const byte Disconnect = 0xFF;
    }

    // ========================================================================
    // HELPER: Payload sizes for validation
    // ========================================================================

    public static class PayloadSize
    {
        // Minimum payload sizes (including opcode)
        public const int C_Move = 3;                    // opcode + direction + facing
        public const int C_Turn = 2;                    // opcode + direction
        public const int S_MyPosition = 6;              // opcode + x(2) + y(2) + facing
        public const int S_OtherPlayerUpdate = 10;      // opcode + id(4) + x(2) + y(2) + facing
        public const int S_PlayerIdAssignment = 5;      // opcode + id(4)
        public const int S_PlayerLeft = 5;              // opcode + id(4)
        public const int S_MyFacing = 2;                // opcode + facing
        public const int S_BatchUpdateHeader = 2;       // opcode + count
        public const int S_BatchUpdatePerPlayer = 9;    // id(4) + x(2) + y(2) + facing
    }
}