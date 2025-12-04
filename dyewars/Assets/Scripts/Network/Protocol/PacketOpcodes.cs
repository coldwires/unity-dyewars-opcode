// PacketOpcodes.cs
// Centralized definition of all network packet opcodes.
// This is the single source of truth for packet types used by client and server.
//
// Usage:
//   Opcode.LocalPlayer.S_Died
//   Opcode.RemotePlayer.S_Entered_Range
//   Opcode.Batch.S_RemotePlayer_Update
//   Opcode.Map.S_Tile_Data
//
// Naming Convention:
//   C_ = Client -> Server
//   S_ = Server -> Client
//
// Opcode Ranges:
//   0x00 - 0x0F : Client -> Server : Connection & Movement
//   0x10 - 0x1F : Server -> Client : Local Player & Map
//   0x20 - 0x2F : Server -> Client : Remote Players & Entities
//   0x30 - 0x3F : Server -> Client : Combat & Effects
//   0x40 - 0x4F : Client -> Server : Combat Actions
//   0x50 - 0x5F : Bidirectional    : Chat & Social
//   0x60 - 0x6F : Client -> Server : Inventory & Items
//   0x70 - 0x7F : Server -> Client : Inventory Updates
//   0x80 - 0x8F : Client -> Server : Trade & Economy
//   0x90 - 0x9F : Server -> Client : Trade Updates
//   0xA0 - 0xAF : Client -> Server : Skills & Abilities
//   0xB0 - 0xBF : Server -> Client : Skill Updates
//   0xC0 - 0xCF : Reserved
//   0xD0 - 0xDF : Reserved
//   0xE0 - 0xEF : Admin / Debug
//   0xF0 - 0xFF : System (handshake response, ping, disconnect)

namespace DyeWars.Network.Protocol
{
    public static class Opcode
    {
        // ====================================================================
        // CONNECTION & HANDSHAKE
        // ====================================================================
        public static class Connection
        {
            /// <summary>
            /// Initial connection handshake from client.
            /// Payload: [version:2][clientMagic:4]
            /// </summary>
            public const byte C_Handshake_Request = 0x00;

            /// <summary>
            /// Server accepts handshake.
            /// Payload: [serverVersion:2][serverMagic:4]
            /// </summary>
            public const byte S_Handshake_Accepted = 0xF0;

            /// <summary>
            /// Server rejects handshake.
            /// Payload: [reasonCode:1][reasonLength:1][reason:variable]
            /// </summary>
            public const byte S_Handshake_Rejected = 0xF1;

            /// <summary>
            /// Client requests graceful disconnect.
            /// Payload: (none)
            /// </summary>
            public const byte C_Disconnect_Request = 0xFE;

            /// <summary>
            /// Server acknowledges disconnect.
            /// Payload: (none)
            /// </summary>
            public const byte S_Disconnect_Acknowledged = 0xFF;

            /// <summary>
            /// Client ping request.
            /// Payload: [timestamp:4]
            /// </summary>
            public const byte C_Ping_Request = 0xF6;

            /// <summary>
            /// Server pong response.
            /// Payload: [timestamp:4]
            /// </summary>
            public const byte S_Pong_Response = 0xF7;

            /// <summary>
            /// Server ping request.
            /// Payload: [timestamp:4]
            /// </summary>
            public const byte S_Ping_Request = 0xF8;

            /// <summary>
            /// Client pong response.
            /// Payload: [timestamp:4]
            /// </summary>
            public const byte C_Pong_Response = 0xF9;

            /// <summary>
            /// Client heartbeat.
            /// Payload: (none)
            /// </summary>
            public const byte C_Heartbeat_Request = 0xFA;

            /// <summary>
            /// Server heartbeat acknowledgment.
            /// Payload: (none)
            /// </summary>
            public const byte S_Heartbeat_Response = 0xFB;
        }

        // ====================================================================
        // CLIENT MOVEMENT & ACTIONS
        // ====================================================================
        public static class Movement
        {
            /// <summary>
            /// Player requests to move.
            /// Payload: [direction:1][facing:1]
            /// </summary>
            public const byte C_Move_Request = 0x01;

            /// <summary>
            /// Player requests to turn.
            /// Payload: [direction:1]
            /// </summary>
            public const byte C_Turn_Request = 0x02;

            /// <summary>
            /// Player requests to warp/teleport.
            /// Payload: [targetMapId:2][targetX:2][targetY:2]
            /// </summary>
            public const byte C_Warp_Request = 0x03;

            /// <summary>
            /// Player interacts with something.
            /// Payload: (none)
            /// </summary>
            public const byte C_Interact_Request = 0x04;
        }

        // ====================================================================
        // LOCAL PLAYER (Server -> Client)
        // ====================================================================
        public static class LocalPlayer
        {
            /// <summary>
            /// Welcome packet with player ID and initial state.
            /// Payload: [playerId:4][x:2][y:2][facing:1]
            /// </summary>
            public const byte S_Welcome = 0x10;

            /// <summary>
            /// Position correction.
            /// Payload: [x:2][y:2][facing:1]
            /// </summary>
            public const byte S_Position_Correction = 0x11;

            /// <summary>
            /// Facing correction.
            /// Payload: [facing:1]
            /// </summary>
            public const byte S_Facing_Correction = 0x12;

            /// <summary>
            /// Stats update (HP, MP, etc).
            /// Payload: [hp:2][maxHp:2][mp:2][maxMp:2]
            /// </summary>
            public const byte S_Stats_Update = 0x13;

            /// <summary>
            /// Experience gained.
            /// Payload: [expGained:4][totalExp:4]
            /// </summary>
            public const byte S_Exp_Gained = 0x14;

            /// <summary>
            /// Level up.
            /// Payload: [newLevel:1]
            /// </summary>
            public const byte S_Level_Up = 0x15;

            /// <summary>
            /// Player died.
            /// Payload: (none)
            /// </summary>
            public const byte S_Died = 0x16;

            /// <summary>
            /// Player respawned.
            /// Payload: [x:2][y:2]
            /// </summary>
            public const byte S_Respawned = 0x17;

            /// <summary>
            /// Appearance changed.
            /// Payload: [appearanceData:variable]
            /// </summary>
            public const byte S_Appearance_Changed = 0x18;

            /// <summary>
            /// Warped to new location.
            /// Payload: [mapId:2][x:2][y:2]
            /// </summary>
            public const byte S_Warped = 0x19;
        }

        // ====================================================================
        // MAP DATA (Server -> Client)
        // ====================================================================
        public static class Map
        {
            /// <summary>
            /// Full tile data for visible region + buffer.
            /// Payload: [mapId:2][originX:2][originY:2][width:1][height:1][tileData:width*height*2]
            /// Each tile: [tileId:2]
            /// </summary>
            public const byte S_Tile_Data = 0x1A;

            /// <summary>
            /// Partial tile update (for streaming/delta).
            /// Payload: [count:1][[x:2][y:2][tileId:2]]...
            /// </summary>
            public const byte S_Tile_Update = 0x1B;

            /// <summary>
            /// Map metadata (name, dimensions, flags).
            /// Payload: [mapId:2][width:2][height:2][flags:1][nameLength:1][name:variable]
            /// </summary>
            public const byte S_Map_Info = 0x1C;

            /// <summary>
            /// Object layer data (trees, rocks, etc).
            /// Payload: [originX:2][originY:2][width:1][height:1][objectData:variable]
            /// </summary>
            public const byte S_Object_Data = 0x1D;

            /// <summary>
            /// Collision/passability data.
            /// Payload: [originX:2][originY:2][width:1][height:1][collisionBitmap:variable]
            /// </summary>
            public const byte S_Collision_Data = 0x1E;
        }

        // ====================================================================
        // REMOTE PLAYERS (Server -> Client)
        // ====================================================================
        public static class RemotePlayer
        {
            /// <summary>
            /// Player entered visible range.
            /// Payload: [playerId:4][x:2][y:2][facing:1][appearanceData:variable]
            /// </summary>
            public const byte S_Entered_Range = 0x20;

            /// <summary>
            /// Player left visible range.
            /// Payload: [playerId:4]
            /// </summary>
            public const byte S_Left_Range = 0x21;

            /// <summary>
            /// Player appearance changed.
            /// Payload: [playerId:4][appearanceData:variable]
            /// </summary>
            public const byte S_Appearance_Changed = 0x22;

            /// <summary>
            /// Player died.
            /// Payload: [playerId:4]
            /// </summary>
            public const byte S_Died = 0x23;

            /// <summary>
            /// Player respawned.
            /// Payload: [playerId:4][x:2][y:2]
            /// </summary>
            public const byte S_Respawned = 0x24;

            /// <summary>
            /// Player joined the game.
            /// Payload: [playerId:4][x:2][y:2][facing:1][nameLength:1][name:variable]
            /// </summary>
            public const byte S_Joined_Game = 0x25;

            /// <summary>
            /// Player left the game.
            /// Payload: [playerId:4]
            /// </summary>
            public const byte S_Left_Game = 0x26;
        }

        // ====================================================================
        // ENTITIES - NPCs/Monsters (Server -> Client)
        // ====================================================================
        public static class Entity
        {
            /// <summary>
            /// Entity entered visible range.
            /// Payload: [entityId:4][entityType:1][x:2][y:2][facing:1][appearanceId:2]
            /// </summary>
            public const byte S_Entered_Range = 0x28;

            /// <summary>
            /// Entity left visible range.
            /// Payload: [entityId:4]
            /// </summary>
            public const byte S_Left_Range = 0x29;

            /// <summary>
            /// Entity position/facing update.
            /// Payload: [entityId:4][x:2][y:2][facing:1]
            /// </summary>
            public const byte S_Position_Update = 0x2A;

            /// <summary>
            /// Entity state changed (aggro, idle, etc).
            /// Payload: [entityId:4][state:1]
            /// </summary>
            public const byte S_State_Changed = 0x2B;

            /// <summary>
            /// Entity target changed.
            /// Payload: [entityId:4][targetType:1][targetId:4]
            /// </summary>
            public const byte S_Target_Changed = 0x2C;

            /// <summary>
            /// Entity died.
            /// Payload: [entityId:4]
            /// </summary>
            public const byte S_Died = 0x2D;

            /// <summary>
            /// Entity respawned.
            /// Payload: [entityId:4][x:2][y:2]
            /// </summary>
            public const byte S_Respawned = 0x2E;
        }

        // ====================================================================
        // BATCH UPDATES (Server -> Client)
        // ====================================================================
        public static class Batch
        {
            /// <summary>
            /// Batch remote player positions.
            /// Payload: [count:1][[playerId:4][x:2][y:2][facing:1]]...
            /// </summary>
            public const byte S_RemotePlayer_Update = 0x27;

            /// <summary>
            /// Batch entity positions.
            /// Payload: [count:1][[entityId:4][x:2][y:2][facing:1]]...
            /// </summary>
            public const byte S_Entity_Update = 0x2F;
        }

        // ====================================================================
        // COMBAT & EFFECTS
        // ====================================================================
        public static class Combat
        {
            // --- Server -> Client ---

            /// <summary>
            /// Play visual effect at location.
            /// Payload: [effectId:2][x:2][y:2][param:1]
            /// </summary>
            public const byte S_Effect_Play = 0x30;

            /// <summary>
            /// Entity took damage.
            /// Payload: [entityId:4][damage:2][currentHp:2][maxHp:2]
            /// </summary>
            public const byte S_Damage = 0x31;

            /// <summary>
            /// Entity was healed.
            /// Payload: [entityId:4][amount:2][currentHp:2][maxHp:2]
            /// </summary>
            public const byte S_Heal = 0x32;

            /// <summary>
            /// Skill/spell was cast.
            /// Payload: [casterId:4][skillId:2][targetX:2][targetY:2][facing:1]
            /// </summary>
            public const byte S_Skill_Cast = 0x33;

            /// <summary>
            /// Buff applied.
            /// Payload: [entityId:4][buffId:2][duration:2]
            /// </summary>
            public const byte S_Buff_Applied = 0x34;

            /// <summary>
            /// Buff removed.
            /// Payload: [entityId:4][buffId:2]
            /// </summary>
            public const byte S_Buff_Removed = 0x35;

            /// <summary>
            /// Miss/dodge notification.
            /// Payload: [attackerId:4][targetId:4][missType:1]
            /// </summary>
            public const byte S_Miss = 0x36;

            /// <summary>
            /// Critical hit notification.
            /// Payload: [attackerId:4][targetId:4][damage:2]
            /// </summary>
            public const byte S_Critical = 0x37;

            // --- Client -> Server ---

            /// <summary>
            /// Player attacks.
            /// Payload: (none, uses facing)
            /// </summary>
            public const byte C_Attack_Request = 0x40;

            /// <summary>
            /// Player uses skill.
            /// Payload: [skillId:2][targetX:2][targetY:2]
            /// </summary>
            public const byte C_Skill_Use_Request = 0x41;

            /// <summary>
            /// Player targets entity.
            /// Payload: [entityId:4]
            /// </summary>
            public const byte C_Target_Request = 0x42;

            /// <summary>
            /// Player clears target.
            /// Payload: (none)
            /// </summary>
            public const byte C_Target_Clear_Request = 0x43;
        }

        // ====================================================================
        // CHAT & SOCIAL
        // ====================================================================
        public static class Chat
        {
            // --- Client -> Server ---

            /// <summary>
            /// Send chat message.
            /// Payload: [channelId:1][messageLength:2][message:variable]
            /// </summary>
            public const byte C_Message_Send = 0x50;

            /// <summary>
            /// Send emote.
            /// Payload: [emoteId:1]
            /// </summary>
            public const byte C_Emote_Send = 0x51;

            /// <summary>
            /// Send whisper.
            /// Payload: [targetNameLength:1][targetName:variable][messageLength:2][message:variable]
            /// </summary>
            public const byte C_Whisper_Send = 0x52;

            // --- Server -> Client ---

            /// <summary>
            /// Broadcast chat message.
            /// Payload: [senderId:4][channelId:1][messageLength:2][message:variable]
            /// </summary>
            public const byte S_Message_Broadcast = 0x58;

            /// <summary>
            /// Broadcast emote.
            /// Payload: [senderId:4][emoteId:1]
            /// </summary>
            public const byte S_Emote_Broadcast = 0x59;

            /// <summary>
            /// Whisper received.
            /// Payload: [senderNameLength:1][senderName:variable][messageLength:2][message:variable]
            /// </summary>
            public const byte S_Whisper_Received = 0x5A;

            /// <summary>
            /// Whisper sent confirmation.
            /// Payload: [targetNameLength:1][targetName:variable]
            /// </summary>
            public const byte S_Whisper_Sent_Confirm = 0x5B;

            /// <summary>
            /// System message.
            /// Payload: [messageType:1][messageLength:2][message:variable]
            /// </summary>
            public const byte S_System_Message = 0x5C;
        }

        // ====================================================================
        // INVENTORY & ITEMS
        // ====================================================================
        public static class Inventory
        {
            // --- Client -> Server ---

            /// <summary>
            /// Use item.
            /// Payload: [slot:1]
            /// </summary>
            public const byte C_Item_Use_Request = 0x60;

            /// <summary>
            /// Drop item.
            /// Payload: [slot:1][quantity:2]
            /// </summary>
            public const byte C_Item_Drop_Request = 0x61;

            /// <summary>
            /// Pick up item.
            /// Payload: [groundItemId:4]
            /// </summary>
            public const byte C_Item_Pickup_Request = 0x62;

            /// <summary>
            /// Move item in inventory.
            /// Payload: [fromSlot:1][toSlot:1]
            /// </summary>
            public const byte C_Item_Move_Request = 0x63;

            // --- Server -> Client ---

            /// <summary>
            /// Full inventory update.
            /// Payload: [slotCount:1][[slot:1][itemId:2][quantity:2][flags:1]]...
            /// </summary>
            public const byte S_Full_Update = 0x70;

            /// <summary>
            /// Single slot update.
            /// Payload: [slot:1][itemId:2][quantity:2][flags:1]
            /// </summary>
            public const byte S_Slot_Update = 0x71;

            /// <summary>
            /// Item spawned on ground.
            /// Payload: [groundItemId:4][itemId:2][x:2][y:2][quantity:2]
            /// </summary>
            public const byte S_GroundItem_Spawned = 0x72;

            /// <summary>
            /// Item removed from ground.
            /// Payload: [groundItemId:4]
            /// </summary>
            public const byte S_GroundItem_Removed = 0x73;
        }

        // ====================================================================
        // DEBUG & ADMIN
        // ====================================================================
        public static class Debug
        {
            /// <summary>
            /// Custom debug response.
            /// Payload: [dataLength:2][data:variable]
            /// </summary>
            public const byte S_Custom_Response = 0xE0;

            /// <summary>
            /// Request server state.
            /// Payload: [debugFlags:1]
            /// </summary>
            public const byte C_Request_State = 0xE1;

            /// <summary>
            /// Server state response.
            /// Payload: [dataLength:2][data:variable]
            /// </summary>
            public const byte S_State_Response = 0xE2;
        }

        // ====================================================================
        // SYSTEM NOTIFICATIONS
        // ====================================================================
        public static class System
        {
            /// <summary>
            /// Kick notification.
            /// Payload: [reasonLength:1][reason:variable]
            /// </summary>
            public const byte S_Kick_Notification = 0xF2;

            /// <summary>
            /// Server shutdown warning.
            /// Payload: [secondsRemaining:2][reasonLength:1][reason:variable]
            /// </summary>
            public const byte S_Shutdown_Warning = 0xF3;

            /// <summary>
            /// Server time sync.
            /// Payload: [serverTime:4]
            /// </summary>
            public const byte S_Time_Sync = 0xF4;
        }
    }

    // ========================================================================
    // PAYLOAD SIZES (names match opcodes exactly)
    // ========================================================================
    public static class PayloadSize
    {
        // Connection
        public const int C_Connection_Handshake_Request = 7;        // opcode + version(2) + magic(4)
        public const int S_Connection_Handshake_Accepted = 7;       // opcode + version(2) + magic(4)
        public const int C_Connection_Ping_Request = 5;             // opcode + timestamp(4)
        public const int S_Connection_Pong_Response = 5;            // opcode + timestamp(4)
        public const int S_Connection_Ping_Request = 5;             // opcode + timestamp(4)
        public const int C_Connection_Pong_Response = 5;            // opcode + timestamp(4)
        public const int C_Connection_Heartbeat_Request = 1;        // opcode only
        public const int S_Connection_Heartbeat_Response = 1;       // opcode only
        public const int C_Connection_Disconnect_Request = 1;       // opcode only
        public const int S_Connection_Disconnect_Acknowledged = 1;  // opcode only

        // Movement
        public const int C_Movement_Move_Request = 3;               // opcode + direction + facing
        public const int C_Movement_Turn_Request = 2;               // opcode + direction
        public const int C_Movement_Warp_Request = 7;               // opcode + mapId(2) + x(2) + y(2)
        public const int C_Movement_Interact_Request = 1;           // opcode only

        // LocalPlayer
        public const int S_LocalPlayer_Welcome = 10;                // opcode + id(4) + x(2) + y(2) + facing
        public const int S_LocalPlayer_Position_Correction = 6;     // opcode + x(2) + y(2) + facing
        public const int S_LocalPlayer_Facing_Correction = 2;       // opcode + facing
        public const int S_LocalPlayer_Stats_Update = 9;            // opcode + hp(2) + maxHp(2) + mp(2) + maxMp(2)
        public const int S_LocalPlayer_Exp_Gained = 9;              // opcode + expGained(4) + totalExp(4)
        public const int S_LocalPlayer_Level_Up = 2;                // opcode + level
        public const int S_LocalPlayer_Died = 1;                    // opcode only
        public const int S_LocalPlayer_Respawned = 5;               // opcode + x(2) + y(2)
        public const int S_LocalPlayer_Warped = 7;                  // opcode + mapId(2) + x(2) + y(2)

        // Map (header sizes, tile data is variable)
        public const int S_Map_Tile_Data_Header = 9;                // opcode + mapId(2) + originX(2) + originY(2) + width + height
        public const int S_Map_Tile_Update_Header = 2;              // opcode + count
        public const int S_Map_Tile_Update_PerTile = 6;             // x(2) + y(2) + tileId(2)
        public const int S_Map_Map_Info_Header = 8;                 // opcode + mapId(2) + width(2) + height(2) + flags
        public const int S_Map_Object_Data_Header = 7;              // opcode + originX(2) + originY(2) + width + height
        public const int S_Map_Collision_Data_Header = 7;           // opcode + originX(2) + originY(2) + width + height

        // RemotePlayer
        public const int S_RemotePlayer_Left_Range = 5;             // opcode + id(4)
        public const int S_RemotePlayer_Died = 5;                   // opcode + id(4)
        public const int S_RemotePlayer_Respawned = 9;              // opcode + id(4) + x(2) + y(2)
        public const int S_RemotePlayer_Left_Game = 5;              // opcode + id(4)

        // Entity
        public const int S_Entity_Entered_Range = 13;               // opcode + id(4) + type + x(2) + y(2) + facing + appearanceId(2)
        public const int S_Entity_Left_Range = 5;                   // opcode + id(4)
        public const int S_Entity_Position_Update = 10;             // opcode + id(4) + x(2) + y(2) + facing
        public const int S_Entity_State_Changed = 6;                // opcode + id(4) + state
        public const int S_Entity_Target_Changed = 10;              // opcode + id(4) + targetType + targetId(4)
        public const int S_Entity_Died = 5;                         // opcode + id(4)
        public const int S_Entity_Respawned = 9;                    // opcode + id(4) + x(2) + y(2)

        // Batch
        public const int S_Batch_RemotePlayer_Update_Header = 2;    // opcode + count
        public const int S_Batch_RemotePlayer_Update_PerPlayer = 9; // id(4) + x(2) + y(2) + facing
        public const int S_Batch_Entity_Update_Header = 2;          // opcode + count
        public const int S_Batch_Entity_Update_PerEntity = 9;       // id(4) + x(2) + y(2) + facing

        // Combat
        public const int S_Combat_Effect_Play = 8;                  // opcode + effectId(2) + x(2) + y(2) + param
        public const int S_Combat_Damage = 11;                      // opcode + id(4) + damage(2) + hp(2) + maxHp(2)
        public const int S_Combat_Heal = 11;                        // opcode + id(4) + amount(2) + hp(2) + maxHp(2)
        public const int S_Combat_Skill_Cast = 12;                  // opcode + casterId(4) + skillId(2) + x(2) + y(2) + facing
        public const int S_Combat_Buff_Applied = 9;                 // opcode + id(4) + buffId(2) + duration(2)
        public const int S_Combat_Buff_Removed = 7;                 // opcode + id(4) + buffId(2)
        public const int S_Combat_Miss = 10;                        // opcode + attackerId(4) + targetId(4) + missType
        public const int S_Combat_Critical = 11;                    // opcode + attackerId(4) + targetId(4) + damage(2)
        public const int C_Combat_Attack_Request = 1;               // opcode only
        public const int C_Combat_Skill_Use_Request = 7;            // opcode + skillId(2) + x(2) + y(2)
        public const int C_Combat_Target_Request = 5;               // opcode + entityId(4)
        public const int C_Combat_Target_Clear_Request = 1;         // opcode only

        // Chat
        public const int C_Chat_Message_Send_Header = 4;            // opcode + channelId + msgLen(2)
        public const int C_Chat_Emote_Send = 2;                     // opcode + emoteId
        public const int S_Chat_Emote_Broadcast = 6;                // opcode + senderId(4) + emoteId
        public const int S_Chat_System_Message_Header = 4;          // opcode + msgType + msgLen(2)

        // Inventory
        public const int C_Inventory_Item_Use_Request = 2;          // opcode + slot
        public const int C_Inventory_Item_Drop_Request = 4;         // opcode + slot + quantity(2)
        public const int C_Inventory_Item_Pickup_Request = 5;       // opcode + groundId(4)
        public const int C_Inventory_Item_Move_Request = 3;         // opcode + fromSlot + toSlot
        public const int S_Inventory_Slot_Update = 7;               // opcode + slot + itemId(2) + quantity(2) + flags
        public const int S_Inventory_GroundItem_Spawned = 13;       // opcode + groundId(4) + itemId(2) + x(2) + y(2) + qty(2)
        public const int S_Inventory_GroundItem_Removed = 5;        // opcode + groundId(4)

        // System
        public const int S_System_Kick_Notification_Header = 2;     // opcode + reasonLen
        public const int S_System_Shutdown_Warning_Header = 4;      // opcode + seconds(2) + reasonLen
        public const int S_System_Time_Sync = 5;                    // opcode + serverTime(4)
    }

    // ========================================================================
    // OPCODE UTILITIES
    // ========================================================================
    public static class OpcodeUtil
    {
        public static string GetName(byte opcode) => opcode switch
        {
            // Connection
            Opcode.Connection.C_Handshake_Request => "Connection.C_Handshake_Request",
            Opcode.Connection.S_Handshake_Accepted => "Connection.S_Handshake_Accepted",
            Opcode.Connection.S_Handshake_Rejected => "Connection.S_Handshake_Rejected",
            Opcode.Connection.C_Disconnect_Request => "Connection.C_Disconnect_Request",
            Opcode.Connection.S_Disconnect_Acknowledged => "Connection.S_Disconnect_Acknowledged",
            Opcode.Connection.C_Ping_Request => "Connection.C_Ping_Request",
            Opcode.Connection.S_Pong_Response => "Connection.S_Pong_Response",
            Opcode.Connection.S_Ping_Request => "Connection.S_Ping_Request",
            Opcode.Connection.C_Pong_Response => "Connection.C_Pong_Response",
            Opcode.Connection.C_Heartbeat_Request => "Connection.C_Heartbeat_Request",
            Opcode.Connection.S_Heartbeat_Response => "Connection.S_Heartbeat_Response",

            // Movement
            Opcode.Movement.C_Move_Request => "Movement.C_Move_Request",
            Opcode.Movement.C_Turn_Request => "Movement.C_Turn_Request",
            Opcode.Movement.C_Warp_Request => "Movement.C_Warp_Request",
            Opcode.Movement.C_Interact_Request => "Movement.C_Interact_Request",

            // LocalPlayer
            Opcode.LocalPlayer.S_Welcome => "LocalPlayer.S_Welcome",
            Opcode.LocalPlayer.S_Position_Correction => "LocalPlayer.S_Position_Correction",
            Opcode.LocalPlayer.S_Facing_Correction => "LocalPlayer.S_Facing_Correction",
            Opcode.LocalPlayer.S_Stats_Update => "LocalPlayer.S_Stats_Update",
            Opcode.LocalPlayer.S_Exp_Gained => "LocalPlayer.S_Exp_Gained",
            Opcode.LocalPlayer.S_Level_Up => "LocalPlayer.S_Level_Up",
            Opcode.LocalPlayer.S_Died => "LocalPlayer.S_Died",
            Opcode.LocalPlayer.S_Respawned => "LocalPlayer.S_Respawned",
            Opcode.LocalPlayer.S_Appearance_Changed => "LocalPlayer.S_Appearance_Changed",
            Opcode.LocalPlayer.S_Warped => "LocalPlayer.S_Warped",

            // Map
            Opcode.Map.S_Tile_Data => "Map.S_Tile_Data",
            Opcode.Map.S_Tile_Update => "Map.S_Tile_Update",
            Opcode.Map.S_Map_Info => "Map.S_Map_Info",
            Opcode.Map.S_Object_Data => "Map.S_Object_Data",
            Opcode.Map.S_Collision_Data => "Map.S_Collision_Data",

            // RemotePlayer
            Opcode.RemotePlayer.S_Entered_Range => "RemotePlayer.S_Entered_Range",
            Opcode.RemotePlayer.S_Left_Range => "RemotePlayer.S_Left_Range",
            Opcode.RemotePlayer.S_Appearance_Changed => "RemotePlayer.S_Appearance_Changed",
            Opcode.RemotePlayer.S_Died => "RemotePlayer.S_Died",
            Opcode.RemotePlayer.S_Respawned => "RemotePlayer.S_Respawned",
            Opcode.RemotePlayer.S_Joined_Game => "RemotePlayer.S_Joined_Game",
            Opcode.RemotePlayer.S_Left_Game => "RemotePlayer.S_Left_Game",

            // Entity
            Opcode.Entity.S_Entered_Range => "Entity.S_Entered_Range",
            Opcode.Entity.S_Left_Range => "Entity.S_Left_Range",
            Opcode.Entity.S_Position_Update => "Entity.S_Position_Update",
            Opcode.Entity.S_State_Changed => "Entity.S_State_Changed",
            Opcode.Entity.S_Target_Changed => "Entity.S_Target_Changed",
            Opcode.Entity.S_Died => "Entity.S_Died",
            Opcode.Entity.S_Respawned => "Entity.S_Respawned",

            // Batch
            Opcode.Batch.S_RemotePlayer_Update => "Batch.S_RemotePlayer_Update",
            Opcode.Batch.S_Entity_Update => "Batch.S_Entity_Update",

            // Combat
            Opcode.Combat.S_Effect_Play => "Combat.S_Effect_Play",
            Opcode.Combat.S_Damage => "Combat.S_Damage",
            Opcode.Combat.S_Heal => "Combat.S_Heal",
            Opcode.Combat.S_Skill_Cast => "Combat.S_Skill_Cast",
            Opcode.Combat.S_Buff_Applied => "Combat.S_Buff_Applied",
            Opcode.Combat.S_Buff_Removed => "Combat.S_Buff_Removed",
            Opcode.Combat.S_Miss => "Combat.S_Miss",
            Opcode.Combat.S_Critical => "Combat.S_Critical",
            Opcode.Combat.C_Attack_Request => "Combat.C_Attack_Request",
            Opcode.Combat.C_Skill_Use_Request => "Combat.C_Skill_Use_Request",
            Opcode.Combat.C_Target_Request => "Combat.C_Target_Request",
            Opcode.Combat.C_Target_Clear_Request => "Combat.C_Target_Clear_Request",

            // Chat
            Opcode.Chat.C_Message_Send => "Chat.C_Message_Send",
            Opcode.Chat.C_Emote_Send => "Chat.C_Emote_Send",
            Opcode.Chat.C_Whisper_Send => "Chat.C_Whisper_Send",
            Opcode.Chat.S_Message_Broadcast => "Chat.S_Message_Broadcast",
            Opcode.Chat.S_Emote_Broadcast => "Chat.S_Emote_Broadcast",
            Opcode.Chat.S_Whisper_Received => "Chat.S_Whisper_Received",
            Opcode.Chat.S_Whisper_Sent_Confirm => "Chat.S_Whisper_Sent_Confirm",
            Opcode.Chat.S_System_Message => "Chat.S_System_Message",

            // Inventory
            Opcode.Inventory.C_Item_Use_Request => "Inventory.C_Item_Use_Request",
            Opcode.Inventory.C_Item_Drop_Request => "Inventory.C_Item_Drop_Request",
            Opcode.Inventory.C_Item_Pickup_Request => "Inventory.C_Item_Pickup_Request",
            Opcode.Inventory.C_Item_Move_Request => "Inventory.C_Item_Move_Request",
            Opcode.Inventory.S_Full_Update => "Inventory.S_Full_Update",
            Opcode.Inventory.S_Slot_Update => "Inventory.S_Slot_Update",
            Opcode.Inventory.S_GroundItem_Spawned => "Inventory.S_GroundItem_Spawned",
            Opcode.Inventory.S_GroundItem_Removed => "Inventory.S_GroundItem_Removed",

            // Debug
            Opcode.Debug.S_Custom_Response => "Debug.S_Custom_Response",
            Opcode.Debug.C_Request_State => "Debug.C_Request_State",
            Opcode.Debug.S_State_Response => "Debug.S_State_Response",

            // System
            Opcode.System.S_Kick_Notification => "System.S_Kick_Notification",
            Opcode.System.S_Shutdown_Warning => "System.S_Shutdown_Warning",
            Opcode.System.S_Time_Sync => "System.S_Time_Sync",

            _ => $"Unknown(0x{opcode:X2})"
        };

        public static bool IsClientToServer(byte opcode) =>
            (opcode >= 0x00 && opcode <= 0x0F) ||
            (opcode >= 0x40 && opcode <= 0x4F) ||
            (opcode >= 0x50 && opcode <= 0x57) ||
            (opcode >= 0x60 && opcode <= 0x6F) ||
            (opcode >= 0x80 && opcode <= 0x8F) ||
            (opcode >= 0xA0 && opcode <= 0xAF) ||
            opcode == Opcode.Connection.C_Ping_Request ||
            opcode == Opcode.Connection.C_Pong_Response ||
            opcode == Opcode.Connection.C_Heartbeat_Request ||
            opcode == Opcode.Connection.C_Disconnect_Request ||
            opcode == Opcode.Debug.C_Request_State;

        public static bool IsServerToClient(byte opcode) => !IsClientToServer(opcode);

        public static string GetCategory(byte opcode) => opcode switch
        {
            0x00 => "Connection",
            >= 0x01 and <= 0x0F => "Movement",
            >= 0x10 and <= 0x19 => "LocalPlayer",
            >= 0x1A and <= 0x1F => "Map",
            >= 0x20 and <= 0x26 => "RemotePlayer",
            0x27 => "Batch",
            >= 0x28 and <= 0x2E => "Entity",
            0x2F => "Batch",
            >= 0x30 and <= 0x3F => "Combat",
            >= 0x40 and <= 0x4F => "Combat",
            >= 0x50 and <= 0x5F => "Chat",
            >= 0x60 and <= 0x6F => "Inventory",
            >= 0x70 and <= 0x7F => "Inventory",
            >= 0xE0 and <= 0xEF => "Debug",
            >= 0xF0 and <= 0xFF => "System/Connection",
            _ => "Unknown"
        };
    }
}