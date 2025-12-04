// PacketOpcodes.hpp
// Centralized definition of all network packet opcodes.
// This is the single source of truth for packet types used by client and server.
//
// Usage:
//   Protocol::Opcode::LocalPlayer::S_Died
//   Protocol::Opcode::RemotePlayer::S_Entered_Range
//   Protocol::Opcode::Batch::S_RemotePlayer_Update
//   Protocol::Opcode::Map::S_Tile_Data
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

#pragma once

#include <cstdint>
#include <string>

namespace Protocol::Opcode
{
    // ========================================================================
    // CONNECTION & HANDSHAKE
    // ========================================================================
    namespace Connection
    {
        // Initial connection handshake from client.
        // Payload: [version:2][clientMagic:4]
        constexpr uint8_t C_Handshake_Request = 0x00;

        // Server accepts handshake.
        // Payload: [serverVersion:2][serverMagic:4]
        constexpr uint8_t S_Handshake_Accepted = 0xF0;

        // Server rejects handshake.
        // Payload: [reasonCode:1][reasonLength:1][reason:variable]
        constexpr uint8_t S_Handshake_Rejected = 0xF1;

        // Client requests graceful disconnect.
        // Payload: (none)
        constexpr uint8_t C_Disconnect_Request = 0xFE;

        // Server acknowledges disconnect.
        // Payload: (none)
        constexpr uint8_t S_Disconnect_Acknowledged = 0xFF;

        // Client ping request.
        // Payload: [timestamp:4]
        constexpr uint8_t C_Ping_Request = 0xF6;

        // Server pong response.
        // Payload: [timestamp:4]
        constexpr uint8_t S_Pong_Response = 0xF7;

        // Server ping request.
        // Payload: [timestamp:4]
        constexpr uint8_t S_Ping_Request = 0xF8;

        // Client pong response.
        // Payload: [timestamp:4]
        constexpr uint8_t C_Pong_Response = 0xF9;

        // Client heartbeat.
        // Payload: (none)
        constexpr uint8_t C_Heartbeat_Request = 0xFA;

        // Server heartbeat acknowledgment.
        // Payload: (none)
        constexpr uint8_t S_Heartbeat_Response = 0xFB;
    }

    // ========================================================================
    // CLIENT MOVEMENT & ACTIONS
    // ========================================================================
    namespace Movement
    {
        // Player requests to move.
        // Payload: [direction:1][facing:1]
        constexpr uint8_t C_Move_Request = 0x01;

        // Player requests to turn.
        // Payload: [direction:1]
        constexpr uint8_t C_Turn_Request = 0x02;

        // Player requests to warp/teleport.
        // Payload: [targetMapId:2][targetX:2][targetY:2]
        constexpr uint8_t C_Warp_Request = 0x03;

        // Player interacts with something.
        // Payload: (none)
        constexpr uint8_t C_Interact_Request = 0x04;
    }

    // ========================================================================
    // LOCAL PLAYER (Server -> Client)
    // ========================================================================
    namespace LocalPlayer
    {
        // Welcome packet with player ID and initial state.
        // Payload: [playerId:4][x:2][y:2][facing:1]
        constexpr uint8_t S_Welcome = 0x10;

        // Position correction.
        // Payload: [x:2][y:2][facing:1]
        constexpr uint8_t S_Position_Correction = 0x11;

        // Facing correction.
        // Payload: [facing:1]
        constexpr uint8_t S_Facing_Correction = 0x12;

        // Stats update (HP, MP, etc).
        // Payload: [hp:2][maxHp:2][mp:2][maxMp:2]
        constexpr uint8_t S_Stats_Update = 0x13;

        // Experience gained.
        // Payload: [expGained:4][totalExp:4]
        constexpr uint8_t S_Exp_Gained = 0x14;

        // Level up.
        // Payload: [newLevel:1]
        constexpr uint8_t S_Level_Up = 0x15;

        // Player died.
        // Payload: (none)
        constexpr uint8_t S_Died = 0x16;

        // Player respawned.
        // Payload: [x:2][y:2]
        constexpr uint8_t S_Respawned = 0x17;

        // Appearance changed.
        // Payload: [appearanceData:variable]
        constexpr uint8_t S_Appearance_Changed = 0x18;

        // Warped to new location.
        // Payload: [mapId:2][x:2][y:2]
        constexpr uint8_t S_Warped = 0x19;
    }

    // ========================================================================
    // MAP DATA (Server -> Client)
    // ========================================================================
    namespace Map
    {
        // Full tile data for visible region + buffer.
        // Payload: [mapId:2][originX:2][originY:2][width:1][height:1][tileData:width*height*2]
        // Each tile: [tileId:2]
        constexpr uint8_t S_Tile_Data = 0x1A;

        // Partial tile update (for streaming/delta).
        // Payload: [count:1][[x:2][y:2][tileId:2]]...
        constexpr uint8_t S_Tile_Update = 0x1B;

        // Map metadata (name, dimensions, flags).
        // Payload: [mapId:2][width:2][height:2][flags:1][nameLength:1][name:variable]
        constexpr uint8_t S_Map_Info = 0x1C;

        // Object layer data (trees, rocks, etc).
        // Payload: [originX:2][originY:2][width:1][height:1][objectData:variable]
        constexpr uint8_t S_Object_Data = 0x1D;

        // Collision/passability data.
        // Payload: [originX:2][originY:2][width:1][height:1][collisionBitmap:variable]
        constexpr uint8_t S_Collision_Data = 0x1E;
    }

    // ========================================================================
    // REMOTE PLAYERS (Server -> Client)
    // ========================================================================
    namespace RemotePlayer
    {
        // Player entered visible range.
        // Payload: [playerId:4][x:2][y:2][facing:1][appearanceData:variable]
        constexpr uint8_t S_Entered_Range = 0x20;

        // Player left visible range.
        // Payload: [playerId:4]
        constexpr uint8_t S_Left_Range = 0x21;

        // Player appearance changed.
        // Payload: [playerId:4][appearanceData:variable]
        constexpr uint8_t S_Appearance_Changed = 0x22;

        // Player died.
        // Payload: [playerId:4]
        constexpr uint8_t S_Died = 0x23;

        // Player respawned.
        // Payload: [playerId:4][x:2][y:2]
        constexpr uint8_t S_Respawned = 0x24;

        // Player joined the game.
        // Payload: [playerId:4][x:2][y:2][facing:1][nameLength:1][name:variable]
        constexpr uint8_t S_Joined_Game = 0x25;

        // Player left the game.
        // Payload: [playerId:4]
        constexpr uint8_t S_Left_Game = 0x26;
    }

    // ========================================================================
    // ENTITIES - NPCs/Monsters (Server -> Client)
    // ========================================================================
    namespace Entity
    {
        // Entity entered visible range.
        // Payload: [entityId:4][entityType:1][x:2][y:2][facing:1][appearanceId:2]
        constexpr uint8_t S_Entered_Range = 0x28;

        // Entity left visible range.
        // Payload: [entityId:4]
        constexpr uint8_t S_Left_Range = 0x29;

        // Entity position/facing update.
        // Payload: [entityId:4][x:2][y:2][facing:1]
        constexpr uint8_t S_Position_Update = 0x2A;

        // Entity state changed (aggro, idle, etc).
        // Payload: [entityId:4][state:1]
        constexpr uint8_t S_State_Changed = 0x2B;

        // Entity target changed.
        // Payload: [entityId:4][targetType:1][targetId:4]
        constexpr uint8_t S_Target_Changed = 0x2C;

        // Entity died.
        // Payload: [entityId:4]
        constexpr uint8_t S_Died = 0x2D;

        // Entity respawned.
        // Payload: [entityId:4][x:2][y:2]
        constexpr uint8_t S_Respawned = 0x2E;
    }

    // ========================================================================
    // BATCH UPDATES (Server -> Client)
    // ========================================================================
    namespace Batch
    {
        // Batch remote player positions.
        // Payload: [count:1][[playerId:4][x:2][y:2][facing:1]]...
        constexpr uint8_t S_RemotePlayer_Update = 0x27;

        // Batch entity positions.
        // Payload: [count:1][[entityId:4][x:2][y:2][facing:1]]...
        constexpr uint8_t S_Entity_Update = 0x2F;
    }

    // ========================================================================
    // COMBAT & EFFECTS
    // ========================================================================
    namespace Combat
    {
        // --- Server -> Client ---

        // Play visual effect at location.
        // Payload: [effectId:2][x:2][y:2][param:1]
        constexpr uint8_t S_Effect_Play = 0x30;

        // Entity took damage.
        // Payload: [entityId:4][damage:2][currentHp:2][maxHp:2]
        constexpr uint8_t S_Damage = 0x31;

        // Entity was healed.
        // Payload: [entityId:4][amount:2][currentHp:2][maxHp:2]
        constexpr uint8_t S_Heal = 0x32;

        // Skill/spell was cast.
        // Payload: [casterId:4][skillId:2][targetX:2][targetY:2][facing:1]
        constexpr uint8_t S_Skill_Cast = 0x33;

        // Buff applied.
        // Payload: [entityId:4][buffId:2][duration:2]
        constexpr uint8_t S_Buff_Applied = 0x34;

        // Buff removed.
        // Payload: [entityId:4][buffId:2]
        constexpr uint8_t S_Buff_Removed = 0x35;

        // Miss/dodge notification.
        // Payload: [attackerId:4][targetId:4][missType:1]
        constexpr uint8_t S_Miss = 0x36;

        // Critical hit notification.
        // Payload: [attackerId:4][targetId:4][damage:2]
        constexpr uint8_t S_Critical = 0x37;

        // --- Client -> Server ---

        // Player attacks.
        // Payload: (none, uses facing)
        constexpr uint8_t C_Attack_Request = 0x40;

        // Player uses skill.
        // Payload: [skillId:2][targetX:2][targetY:2]
        constexpr uint8_t C_Skill_Use_Request = 0x41;

        // Player targets entity.
        // Payload: [entityId:4]
        constexpr uint8_t C_Target_Request = 0x42;

        // Player clears target.
        // Payload: (none)
        constexpr uint8_t C_Target_Clear_Request = 0x43;
    }

    // ========================================================================
    // CHAT & SOCIAL
    // ========================================================================
    namespace Chat
    {
        // --- Client -> Server ---

        // Send chat message.
        // Payload: [channelId:1][messageLength:2][message:variable]
        constexpr uint8_t C_Message_Send = 0x50;

        // Send emote.
        // Payload: [emoteId:1]
        constexpr uint8_t C_Emote_Send = 0x51;

        // Send whisper.
        // Payload: [targetNameLength:1][targetName:variable][messageLength:2][message:variable]
        constexpr uint8_t C_Whisper_Send = 0x52;

        // --- Server -> Client ---

        // Broadcast chat message.
        // Payload: [senderId:4][channelId:1][messageLength:2][message:variable]
        constexpr uint8_t S_Message_Broadcast = 0x58;

        // Broadcast emote.
        // Payload: [senderId:4][emoteId:1]
        constexpr uint8_t S_Emote_Broadcast = 0x59;

        // Whisper received.
        // Payload: [senderNameLength:1][senderName:variable][messageLength:2][message:variable]
        constexpr uint8_t S_Whisper_Received = 0x5A;

        // Whisper sent confirmation.
        // Payload: [targetNameLength:1][targetName:variable]
        constexpr uint8_t S_Whisper_Sent_Confirm = 0x5B;

        // System message.
        // Payload: [messageType:1][messageLength:2][message:variable]
        constexpr uint8_t S_System_Message = 0x5C;
    }

    // ========================================================================
    // INVENTORY & ITEMS
    // ========================================================================
    namespace Inventory
    {
        // --- Client -> Server ---

        // Use item.
        // Payload: [slot:1]
        constexpr uint8_t C_Item_Use_Request = 0x60;

        // Drop item.
        // Payload: [slot:1][quantity:2]
        constexpr uint8_t C_Item_Drop_Request = 0x61;

        // Pick up item.
        // Payload: [groundItemId:4]
        constexpr uint8_t C_Item_Pickup_Request = 0x62;

        // Move item in inventory.
        // Payload: [fromSlot:1][toSlot:1]
        constexpr uint8_t C_Item_Move_Request = 0x63;

        // --- Server -> Client ---

        // Full inventory update.
        // Payload: [slotCount:1][[slot:1][itemId:2][quantity:2][flags:1]]...
        constexpr uint8_t S_Full_Update = 0x70;

        // Single slot update.
        // Payload: [slot:1][itemId:2][quantity:2][flags:1]
        constexpr uint8_t S_Slot_Update = 0x71;

        // Item spawned on ground.
        // Payload: [groundItemId:4][itemId:2][x:2][y:2][quantity:2]
        constexpr uint8_t S_GroundItem_Spawned = 0x72;

        // Item removed from ground.
        // Payload: [groundItemId:4]
        constexpr uint8_t S_GroundItem_Removed = 0x73;
    }

    // ========================================================================
    // DEBUG & ADMIN
    // ========================================================================
    namespace Debug
    {
        // Custom debug response.
        // Payload: [dataLength:2][data:variable]
        constexpr uint8_t S_Custom_Response = 0xE0;

        // Request server state.
        // Payload: [debugFlags:1]
        constexpr uint8_t C_Request_State = 0xE1;

        // Server state response.
        // Payload: [dataLength:2][data:variable]
        constexpr uint8_t S_State_Response = 0xE2;
    }

    // ========================================================================
    // SYSTEM NOTIFICATIONS
    // ========================================================================
    namespace System
    {
        // Kick notification.
        // Payload: [reasonLength:1][reason:variable]
        constexpr uint8_t S_Kick_Notification = 0xF2;

        // Server shutdown warning.
        // Payload: [secondsRemaining:2][reasonLength:1][reason:variable]
        constexpr uint8_t S_Shutdown_Warning = 0xF3;

        // Server time sync.
        // Payload: [serverTime:4]
        constexpr uint8_t S_Time_Sync = 0xF4;
    }

} // namespace Protocol::Opcode

// ============================================================================
// PAYLOAD SIZES (names match opcodes exactly)
// ============================================================================
namespace Protocol::PayloadSize
{
    // Connection
    constexpr size_t C_Connection_Handshake_Request = 7;        // opcode + version(2) + magic(4)
    constexpr size_t S_Connection_Handshake_Accepted = 7;       // opcode + version(2) + magic(4)
    constexpr size_t C_Connection_Ping_Request = 5;             // opcode + timestamp(4)
    constexpr size_t S_Connection_Pong_Response = 5;            // opcode + timestamp(4)
    constexpr size_t S_Connection_Ping_Request = 5;             // opcode + timestamp(4)
    constexpr size_t C_Connection_Pong_Response = 5;            // opcode + timestamp(4)
    constexpr size_t C_Connection_Heartbeat_Request = 1;        // opcode only
    constexpr size_t S_Connection_Heartbeat_Response = 1;       // opcode only
    constexpr size_t C_Connection_Disconnect_Request = 1;       // opcode only
    constexpr size_t S_Connection_Disconnect_Acknowledged = 1;  // opcode only

    // Movement
    constexpr size_t C_Movement_Move_Request = 3;               // opcode + direction + facing
    constexpr size_t C_Movement_Turn_Request = 2;               // opcode + direction
    constexpr size_t C_Movement_Warp_Request = 7;               // opcode + mapId(2) + x(2) + y(2)
    constexpr size_t C_Movement_Interact_Request = 1;           // opcode only

    // LocalPlayer
    constexpr size_t S_LocalPlayer_Welcome = 10;                // opcode + id(4) + x(2) + y(2) + facing
    constexpr size_t S_LocalPlayer_Position_Correction = 6;     // opcode + x(2) + y(2) + facing
    constexpr size_t S_LocalPlayer_Facing_Correction = 2;       // opcode + facing
    constexpr size_t S_LocalPlayer_Stats_Update = 9;            // opcode + hp(2) + maxHp(2) + mp(2) + maxMp(2)
    constexpr size_t S_LocalPlayer_Exp_Gained = 9;              // opcode + expGained(4) + totalExp(4)
    constexpr size_t S_LocalPlayer_Level_Up = 2;                // opcode + level
    constexpr size_t S_LocalPlayer_Died = 1;                    // opcode only
    constexpr size_t S_LocalPlayer_Respawned = 5;               // opcode + x(2) + y(2)
    constexpr size_t S_LocalPlayer_Warped = 7;                  // opcode + mapId(2) + x(2) + y(2)

    // Map (header sizes, tile data is variable)
    constexpr size_t S_Map_Tile_Data_Header = 9;                // opcode + mapId(2) + originX(2) + originY(2) + width + height
    constexpr size_t S_Map_Tile_Update_Header = 2;              // opcode + count
    constexpr size_t S_Map_Tile_Update_PerTile = 6;             // x(2) + y(2) + tileId(2)
    constexpr size_t S_Map_Map_Info_Header = 8;                 // opcode + mapId(2) + width(2) + height(2) + flags
    constexpr size_t S_Map_Object_Data_Header = 7;              // opcode + originX(2) + originY(2) + width + height
    constexpr size_t S_Map_Collision_Data_Header = 7;           // opcode + originX(2) + originY(2) + width + height

    // RemotePlayer
    constexpr size_t S_RemotePlayer_Left_Range = 5;             // opcode + id(4)
    constexpr size_t S_RemotePlayer_Died = 5;                   // opcode + id(4)
    constexpr size_t S_RemotePlayer_Respawned = 9;              // opcode + id(4) + x(2) + y(2)
    constexpr size_t S_RemotePlayer_Left_Game = 5;              // opcode + id(4)

    // Entity
    constexpr size_t S_Entity_Entered_Range = 13;               // opcode + id(4) + type + x(2) + y(2) + facing + appearanceId(2)
    constexpr size_t S_Entity_Left_Range = 5;                   // opcode + id(4)
    constexpr size_t S_Entity_Position_Update = 10;             // opcode + id(4) + x(2) + y(2) + facing
    constexpr size_t S_Entity_State_Changed = 6;                // opcode + id(4) + state
    constexpr size_t S_Entity_Target_Changed = 10;              // opcode + id(4) + targetType + targetId(4)
    constexpr size_t S_Entity_Died = 5;                         // opcode + id(4)
    constexpr size_t S_Entity_Respawned = 9;                    // opcode + id(4) + x(2) + y(2)

    // Batch
    constexpr size_t S_Batch_RemotePlayer_Update_Header = 2;    // opcode + count
    constexpr size_t S_Batch_RemotePlayer_Update_PerPlayer = 9; // id(4) + x(2) + y(2) + facing
    constexpr size_t S_Batch_Entity_Update_Header = 2;          // opcode + count
    constexpr size_t S_Batch_Entity_Update_PerEntity = 9;       // id(4) + x(2) + y(2) + facing

    // Combat
    constexpr size_t S_Combat_Effect_Play = 8;                  // opcode + effectId(2) + x(2) + y(2) + param
    constexpr size_t S_Combat_Damage = 11;                      // opcode + id(4) + damage(2) + hp(2) + maxHp(2)
    constexpr size_t S_Combat_Heal = 11;                        // opcode + id(4) + amount(2) + hp(2) + maxHp(2)
    constexpr size_t S_Combat_Skill_Cast = 12;                  // opcode + casterId(4) + skillId(2) + x(2) + y(2) + facing
    constexpr size_t S_Combat_Buff_Applied = 9;                 // opcode + id(4) + buffId(2) + duration(2)
    constexpr size_t S_Combat_Buff_Removed = 7;                 // opcode + id(4) + buffId(2)
    constexpr size_t S_Combat_Miss = 10;                        // opcode + attackerId(4) + targetId(4) + missType
    constexpr size_t S_Combat_Critical = 11;                    // opcode + attackerId(4) + targetId(4) + damage(2)
    constexpr size_t C_Combat_Attack_Request = 1;               // opcode only
    constexpr size_t C_Combat_Skill_Use_Request = 7;            // opcode + skillId(2) + x(2) + y(2)
    constexpr size_t C_Combat_Target_Request = 5;               // opcode + entityId(4)
    constexpr size_t C_Combat_Target_Clear_Request = 1;         // opcode only

    // Chat
    constexpr size_t C_Chat_Message_Send_Header = 4;            // opcode + channelId + msgLen(2)
    constexpr size_t C_Chat_Emote_Send = 2;                     // opcode + emoteId
    constexpr size_t S_Chat_Emote_Broadcast = 6;                // opcode + senderId(4) + emoteId
    constexpr size_t S_Chat_System_Message_Header = 4;          // opcode + msgType + msgLen(2)

    // Inventory
    constexpr size_t C_Inventory_Item_Use_Request = 2;          // opcode + slot
    constexpr size_t C_Inventory_Item_Drop_Request = 4;         // opcode + slot + quantity(2)
    constexpr size_t C_Inventory_Item_Pickup_Request = 5;       // opcode + groundId(4)
    constexpr size_t C_Inventory_Item_Move_Request = 3;         // opcode + fromSlot + toSlot
    constexpr size_t S_Inventory_Slot_Update = 7;               // opcode + slot + itemId(2) + quantity(2) + flags
    constexpr size_t S_Inventory_GroundItem_Spawned = 13;       // opcode + groundId(4) + itemId(2) + x(2) + y(2) + qty(2)
    constexpr size_t S_Inventory_GroundItem_Removed = 5;        // opcode + groundId(4)

    // System
    constexpr size_t S_System_Kick_Notification_Header = 2;     // opcode + reasonLen
    constexpr size_t S_System_Shutdown_Warning_Header = 4;      // opcode + seconds(2) + reasonLen
    constexpr size_t S_System_Time_Sync = 5;                    // opcode + serverTime(4)

} // namespace Protocol::PayloadSize

// ============================================================================
// OPCODE UTILITIES
// ============================================================================
namespace Protocol::OpcodeUtil
{
    inline std::string GetName(uint8_t opcode)
    {
        switch (opcode)
        {
            // Connection
            case Opcode::Connection::C_Handshake_Request:       return "Connection::C_Handshake_Request";
            case Opcode::Connection::S_Handshake_Accepted:      return "Connection::S_Handshake_Accepted";
            case Opcode::Connection::S_Handshake_Rejected:      return "Connection::S_Handshake_Rejected";
            case Opcode::Connection::C_Disconnect_Request:      return "Connection::C_Disconnect_Request";
            case Opcode::Connection::S_Disconnect_Acknowledged: return "Connection::S_Disconnect_Acknowledged";
            case Opcode::Connection::C_Ping_Request:            return "Connection::C_Ping_Request";
            case Opcode::Connection::S_Pong_Response:           return "Connection::S_Pong_Response";
            case Opcode::Connection::S_Ping_Request:            return "Connection::S_Ping_Request";
            case Opcode::Connection::C_Pong_Response:           return "Connection::C_Pong_Response";
            case Opcode::Connection::C_Heartbeat_Request:       return "Connection::C_Heartbeat_Request";
            case Opcode::Connection::S_Heartbeat_Response:      return "Connection::S_Heartbeat_Response";

                // Movement
            case Opcode::Movement::C_Move_Request:              return "Movement::C_Move_Request";
            case Opcode::Movement::C_Turn_Request:              return "Movement::C_Turn_Request";
            case Opcode::Movement::C_Warp_Request:              return "Movement::C_Warp_Request";
            case Opcode::Movement::C_Interact_Request:          return "Movement::C_Interact_Request";

                // LocalPlayer
            case Opcode::LocalPlayer::S_Welcome:                return "LocalPlayer::S_Welcome";
            case Opcode::LocalPlayer::S_Position_Correction:    return "LocalPlayer::S_Position_Correction";
            case Opcode::LocalPlayer::S_Facing_Correction:      return "LocalPlayer::S_Facing_Correction";
            case Opcode::LocalPlayer::S_Stats_Update:           return "LocalPlayer::S_Stats_Update";
            case Opcode::LocalPlayer::S_Exp_Gained:             return "LocalPlayer::S_Exp_Gained";
            case Opcode::LocalPlayer::S_Level_Up:               return "LocalPlayer::S_Level_Up";
            case Opcode::LocalPlayer::S_Died:                   return "LocalPlayer::S_Died";
            case Opcode::LocalPlayer::S_Respawned:              return "LocalPlayer::S_Respawned";
            case Opcode::LocalPlayer::S_Appearance_Changed:     return "LocalPlayer::S_Appearance_Changed";
            case Opcode::LocalPlayer::S_Warped:                 return "LocalPlayer::S_Warped";

                // Map
            case Opcode::Map::S_Tile_Data:                      return "Map::S_Tile_Data";
            case Opcode::Map::S_Tile_Update:                    return "Map::S_Tile_Update";
            case Opcode::Map::S_Map_Info:                       return "Map::S_Map_Info";
            case Opcode::Map::S_Object_Data:                    return "Map::S_Object_Data";
            case Opcode::Map::S_Collision_Data:                 return "Map::S_Collision_Data";

                // RemotePlayer
            case Opcode::RemotePlayer::S_Entered_Range:         return "RemotePlayer::S_Entered_Range";
            case Opcode::RemotePlayer::S_Left_Range:            return "RemotePlayer::S_Left_Range";
            case Opcode::RemotePlayer::S_Appearance_Changed:    return "RemotePlayer::S_Appearance_Changed";
            case Opcode::RemotePlayer::S_Died:                  return "RemotePlayer::S_Died";
            case Opcode::RemotePlayer::S_Respawned:             return "RemotePlayer::S_Respawned";
            case Opcode::RemotePlayer::S_Joined_Game:           return "RemotePlayer::S_Joined_Game";
            case Opcode::RemotePlayer::S_Left_Game:             return "RemotePlayer::S_Left_Game";

                // Entity
            case Opcode::Entity::S_Entered_Range:               return "Entity::S_Entered_Range";
            case Opcode::Entity::S_Left_Range:                  return "Entity::S_Left_Range";
            case Opcode::Entity::S_Position_Update:             return "Entity::S_Position_Update";
            case Opcode::Entity::S_State_Changed:               return "Entity::S_State_Changed";
            case Opcode::Entity::S_Target_Changed:              return "Entity::S_Target_Changed";
            case Opcode::Entity::S_Died:                        return "Entity::S_Died";
            case Opcode::Entity::S_Respawned:                   return "Entity::S_Respawned";

                // Batch
            case Opcode::Batch::S_RemotePlayer_Update:          return "Batch::S_RemotePlayer_Update";
            case Opcode::Batch::S_Entity_Update:                return "Batch::S_Entity_Update";

                // Combat
            case Opcode::Combat::S_Effect_Play:                 return "Combat::S_Effect_Play";
            case Opcode::Combat::S_Damage:                      return "Combat::S_Damage";
            case Opcode::Combat::S_Heal:                        return "Combat::S_Heal";
            case Opcode::Combat::S_Skill_Cast:                  return "Combat::S_Skill_Cast";
            case Opcode::Combat::S_Buff_Applied:                return "Combat::S_Buff_Applied";
            case Opcode::Combat::S_Buff_Removed:                return "Combat::S_Buff_Removed";
            case Opcode::Combat::S_Miss:                        return "Combat::S_Miss";
            case Opcode::Combat::S_Critical:                    return "Combat::S_Critical";
            case Opcode::Combat::C_Attack_Request:              return "Combat::C_Attack_Request";
            case Opcode::Combat::C_Skill_Use_Request:           return "Combat::C_Skill_Use_Request";
            case Opcode::Combat::C_Target_Request:              return "Combat::C_Target_Request";
            case Opcode::Combat::C_Target_Clear_Request:        return "Combat::C_Target_Clear_Request";

                // Chat
            case Opcode::Chat::C_Message_Send:                  return "Chat::C_Message_Send";
            case Opcode::Chat::C_Emote_Send:                    return "Chat::C_Emote_Send";
            case Opcode::Chat::C_Whisper_Send:                  return "Chat::C_Whisper_Send";
            case Opcode::Chat::S_Message_Broadcast:             return "Chat::S_Message_Broadcast";
            case Opcode::Chat::S_Emote_Broadcast:               return "Chat::S_Emote_Broadcast";
            case Opcode::Chat::S_Whisper_Received:              return "Chat::S_Whisper_Received";
            case Opcode::Chat::S_Whisper_Sent_Confirm:          return "Chat::S_Whisper_Sent_Confirm";
            case Opcode::Chat::S_System_Message:                return "Chat::S_System_Message";

                // Inventory
            case Opcode::Inventory::C_Item_Use_Request:         return "Inventory::C_Item_Use_Request";
            case Opcode::Inventory::C_Item_Drop_Request:        return "Inventory::C_Item_Drop_Request";
            case Opcode::Inventory::C_Item_Pickup_Request:      return "Inventory::C_Item_Pickup_Request";
            case Opcode::Inventory::C_Item_Move_Request:        return "Inventory::C_Item_Move_Request";
            case Opcode::Inventory::S_Full_Update:              return "Inventory::S_Full_Update";
            case Opcode::Inventory::S_Slot_Update:              return "Inventory::S_Slot_Update";
            case Opcode::Inventory::S_GroundItem_Spawned:       return "Inventory::S_GroundItem_Spawned";
            case Opcode::Inventory::S_GroundItem_Removed:       return "Inventory::S_GroundItem_Removed";

                // Debug
            case Opcode::Debug::S_Custom_Response:              return "Debug::S_Custom_Response";
            case Opcode::Debug::C_Request_State:                return "Debug::C_Request_State";
            case Opcode::Debug::S_State_Response:               return "Debug::S_State_Response";

                // System
            case Opcode::System::S_Kick_Notification:           return "System::S_Kick_Notification";
            case Opcode::System::S_Shutdown_Warning:            return "System::S_Shutdown_Warning";
            case Opcode::System::S_Time_Sync:                   return "System::S_Time_Sync";

            default:
                return "Unknown(0x" + std::to_string(opcode) + ")";
        }
    }

    inline bool IsClientToServer(uint8_t opcode)
    {
        return (opcode >= 0x00 && opcode <= 0x0F) ||
               (opcode >= 0x40 && opcode <= 0x4F) ||
               (opcode >= 0x50 && opcode <= 0x57) ||
               (opcode >= 0x60 && opcode <= 0x6F) ||
               (opcode >= 0x80 && opcode <= 0x8F) ||
               (opcode >= 0xA0 && opcode <= 0xAF) ||
               opcode == Opcode::Connection::C_Ping_Request ||
               opcode == Opcode::Connection::C_Pong_Response ||
               opcode == Opcode::Connection::C_Heartbeat_Request ||
               opcode == Opcode::Connection::C_Disconnect_Request ||
               opcode == Opcode::Debug::C_Request_State;
    }

    inline bool IsServerToClient(uint8_t opcode)
    {
        return !IsClientToServer(opcode);
    }

    inline std::string GetCategory(uint8_t opcode)
    {
        if (opcode == 0x00) return "Connection";
        if (opcode >= 0x01 && opcode <= 0x0F) return "Movement";
        if (opcode >= 0x10 && opcode <= 0x19) return "LocalPlayer";
        if (opcode >= 0x1A && opcode <= 0x1F) return "Map";
        if (opcode >= 0x20 && opcode <= 0x26) return "RemotePlayer";
        if (opcode == 0x27) return "Batch";
        if (opcode >= 0x28 && opcode <= 0x2E) return "Entity";
        if (opcode == 0x2F) return "Batch";
        if (opcode >= 0x30 && opcode <= 0x4F) return "Combat";
        if (opcode >= 0x50 && opcode <= 0x5F) return "Chat";
        if (opcode >= 0x60 && opcode <= 0x7F) return "Inventory";
        if (opcode >= 0xE0 && opcode <= 0xEF) return "Debug";
        if (opcode >= 0xF0 && opcode <= 0xFF) return "System/Connection";
        return "Unknown";
    }

} // namespace Protocol::OpcodeUtil