// UnusedOpCodes.h
// Opcodes that are planned but not yet implemented.
// These are kept separate from OpCodes.h to keep the active protocol clean.
// Move opcodes back to OpCodes.h when implementing them.
//
// All opcodes use OpCodeInfo struct defined in OpCodes.h

#pragma once

#include <cstdint>

// Forward declaration - OpCodeInfo is defined in OpCodes.h
// This file is included by OpCodes.h after the struct definition

namespace Protocol::Opcode::Unused {

    // ========================================================================
    // LOCAL PLAYER (Server -> Client) - 0x13-0x19
    // ========================================================================
    namespace LocalPlayer {
        // Stats update (HP, MP, etc).
        // Payload: [hp:2][maxHp:2][mp:2][maxMp:2]
        constexpr OpCodeInfo S_Stats_Update = {
            0x13,
            "Server updates player stats",
            "S_Stats_Update",
            9  // opcode(1) + hp(2) + maxHp(2) + mp(2) + maxMp(2)
        };

        // Experience gained.
        // Payload: [expGained:4][totalExp:4]
        constexpr OpCodeInfo S_Exp_Gained = {
            0x14,
            "Server notifies exp gained",
            "S_Exp_Gained",
            9  // opcode(1) + expGained(4) + totalExp(4)
        };

        // Level up.
        // Payload: [newLevel:1]
        constexpr OpCodeInfo S_Level_Up = {
            0x15,
            "Server notifies level up",
            "S_Level_Up",
            2  // opcode(1) + level(1)
        };

        // Player died.
        // Payload: (none)
        constexpr OpCodeInfo S_Died = {
            0x16,
            "Server notifies player death",
            "S_Died",
            1  // opcode only
        };

        // Player respawned.
        // Payload: [x:2][y:2]
        constexpr OpCodeInfo S_Respawned = {
            0x17,
            "Server notifies player respawn",
            "S_Respawned",
            5  // opcode(1) + x(2) + y(2)
        };

        // Appearance changed.
        // Payload: [appearanceData:variable]
        constexpr OpCodeInfo S_Appearance_Changed = {
            0x18,
            "Server notifies appearance change",
            "S_Appearance_Changed",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Warped to new location.
        // Payload: [mapId:2][x:2][y:2]
        constexpr OpCodeInfo S_Warped = {
            0x19,
            "Server notifies warp",
            "S_Warped",
            7  // opcode(1) + mapId(2) + x(2) + y(2)
        };
    }

    // ========================================================================
    // MAP DATA (Server -> Client) - 0x1A-0x1E
    // ========================================================================
    namespace Map {
        // Full tile data for visible region + buffer.
        // Payload: [mapId:2][originX:2][originY:2][width:1][height:1][tileData:width*height*2]
        constexpr OpCodeInfo S_Tile_Data = {
            0x1A,
            "Server sends tile data",
            "S_Tile_Data",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Partial tile update (for streaming/delta).
        // Payload: [count:1][[x:2][y:2][tileId:2]]...
        constexpr OpCodeInfo S_Tile_Update = {
            0x1B,
            "Server sends tile update",
            "S_Tile_Update",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Map metadata (name, dimensions, flags).
        // Payload: [mapId:2][width:2][height:2][flags:1][nameLength:1][name:variable]
        constexpr OpCodeInfo S_Map_Info = {
            0x1C,
            "Server sends map info",
            "S_Map_Info",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Object layer data (trees, rocks, etc).
        // Payload: [originX:2][originY:2][width:1][height:1][objectData:variable]
        constexpr OpCodeInfo S_Object_Data = {
            0x1D,
            "Server sends object data",
            "S_Object_Data",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Collision/passability data.
        // Payload: [originX:2][originY:2][width:1][height:1][collisionBitmap:variable]
        constexpr OpCodeInfo S_Collision_Data = {
            0x1E,
            "Server sends collision data",
            "S_Collision_Data",
            OpCodeInfo::VARIABLE_SIZE
        };
    }

    // ========================================================================
    // REMOTE PLAYERS (Server -> Client) - 0x20-0x24
    // Note: Player IDs are 8 bytes
    // ========================================================================
    namespace RemotePlayer {
        // Player entered visible range.
        // Payload: [playerId:8][x:2][y:2][facing:1][appearanceData:variable]
        constexpr OpCodeInfo S_Entered_Range = {
            0x20,
            "Remote player entered range",
            "S_Entered_Range",
            OpCodeInfo::VARIABLE_SIZE  // 14 + appearance
        };

        // Player left visible range.
        // Payload: [playerId:8]
        constexpr OpCodeInfo S_Left_Range = {
            0x21,
            "Remote player left range",
            "S_Left_Range",
            9  // opcode(1) + playerId(8)
        };

        // Player appearance changed.
        // Payload: [playerId:8][appearanceData:variable]
        constexpr OpCodeInfo S_Appearance_Changed = {
            0x22,
            "Remote player appearance changed",
            "S_Appearance_Changed",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Player died.
        // Payload: [playerId:8]
        constexpr OpCodeInfo S_Died = {
            0x23,
            "Remote player died",
            "S_Died",
            9  // opcode(1) + playerId(8)
        };

        // Player respawned.
        // Payload: [playerId:8][x:2][y:2]
        constexpr OpCodeInfo S_Respawned = {
            0x24,
            "Remote player respawned",
            "S_Respawned",
            13  // opcode(1) + playerId(8) + x(2) + y(2)
        };
    }

    // ========================================================================
    // ENTITIES - NPCs/Monsters (Server -> Client) - 0x28-0x2E
    // Note: Entity IDs are 4 bytes (unlike 8-byte Player IDs)
    // ========================================================================
    namespace Entity {
        // Entity entered visible range.
        // Payload: [entityId:4][entityType:1][x:2][y:2][facing:1][appearanceId:2]
        constexpr OpCodeInfo S_Entered_Range = {
            0x28,
            "Entity entered range",
            "S_Entered_Range",
            13  // opcode(1) + entityId(4) + type(1) + x(2) + y(2) + facing(1) + appearanceId(2)
        };

        // Entity left visible range.
        // Payload: [entityId:4]
        constexpr OpCodeInfo S_Left_Range = {
            0x29,
            "Entity left range",
            "S_Left_Range",
            5  // opcode(1) + entityId(4)
        };

        // Entity position/facing update.
        // Payload: [entityId:4][x:2][y:2][facing:1]
        constexpr OpCodeInfo S_Position_Update = {
            0x2A,
            "Entity position update",
            "S_Position_Update",
            10  // opcode(1) + entityId(4) + x(2) + y(2) + facing(1)
        };

        // Entity state changed (aggro, idle, etc).
        // Payload: [entityId:4][state:1]
        constexpr OpCodeInfo S_State_Changed = {
            0x2B,
            "Entity state changed",
            "S_State_Changed",
            6  // opcode(1) + entityId(4) + state(1)
        };

        // Entity target changed.
        // Payload: [entityId:4][targetType:1][targetId:4]
        constexpr OpCodeInfo S_Target_Changed = {
            0x2C,
            "Entity target changed",
            "S_Target_Changed",
            10  // opcode(1) + entityId(4) + targetType(1) + targetId(4)
        };

        // Entity died.
        // Payload: [entityId:4]
        constexpr OpCodeInfo S_Died = {
            0x2D,
            "Entity died",
            "S_Died",
            5  // opcode(1) + entityId(4)
        };

        // Entity respawned.
        // Payload: [entityId:4][x:2][y:2]
        constexpr OpCodeInfo S_Respawned = {
            0x2E,
            "Entity respawned",
            "S_Respawned",
            9  // opcode(1) + entityId(4) + x(2) + y(2)
        };
    }

    // ========================================================================
    // BATCH UPDATES (Server -> Client)
    // ========================================================================
    namespace Batch {
        // Batch entity positions.
        // Payload: [count:1][[entityId:4][x:2][y:2][facing:1]]... (9 bytes per entity)
        constexpr OpCodeInfo S_Entity_Update = {
            0x2F,
            "Batch entity position update",
            "S_Entity_Update",
            OpCodeInfo::VARIABLE_SIZE  // 2 + (9 * count)
        };
    }

    // ========================================================================
    // COMBAT & EFFECTS - 0x30-0x4F
    // ========================================================================
    namespace Combat {
        // --- Server -> Client ---

        // Play visual effect at location.
        // Payload: [effectId:2][x:2][y:2][param:1]
        constexpr OpCodeInfo S_Effect_Play = {
            0x30,
            "Play effect at location",
            "S_Effect_Play",
            8  // opcode(1) + effectId(2) + x(2) + y(2) + param(1)
        };

        // Entity took damage.
        // Payload: [entityId:4][damage:2][currentHp:2][maxHp:2]
        constexpr OpCodeInfo S_Damage = {
            0x31,
            "Entity took damage",
            "S_Damage",
            11  // opcode(1) + entityId(4) + damage(2) + hp(2) + maxHp(2)
        };

        // Entity was healed.
        // Payload: [entityId:4][amount:2][currentHp:2][maxHp:2]
        constexpr OpCodeInfo S_Heal = {
            0x32,
            "Entity was healed",
            "S_Heal",
            11  // opcode(1) + entityId(4) + amount(2) + hp(2) + maxHp(2)
        };

        // Skill/spell was cast.
        // Payload: [casterId:4][skillId:2][targetX:2][targetY:2][facing:1]
        constexpr OpCodeInfo S_Skill_Cast = {
            0x33,
            "Skill was cast",
            "S_Skill_Cast",
            12  // opcode(1) + casterId(4) + skillId(2) + x(2) + y(2) + facing(1)
        };

        // Buff applied.
        // Payload: [entityId:4][buffId:2][duration:2]
        constexpr OpCodeInfo S_Buff_Applied = {
            0x34,
            "Buff applied to entity",
            "S_Buff_Applied",
            9  // opcode(1) + entityId(4) + buffId(2) + duration(2)
        };

        // Buff removed.
        // Payload: [entityId:4][buffId:2]
        constexpr OpCodeInfo S_Buff_Removed = {
            0x35,
            "Buff removed from entity",
            "S_Buff_Removed",
            7  // opcode(1) + entityId(4) + buffId(2)
        };

        // Miss/dodge notification.
        // Payload: [attackerId:4][targetId:4][missType:1]
        constexpr OpCodeInfo S_Miss = {
            0x36,
            "Attack missed",
            "S_Miss",
            10  // opcode(1) + attackerId(4) + targetId(4) + missType(1)
        };

        // Critical hit notification.
        // Payload: [attackerId:4][targetId:4][damage:2]
        constexpr OpCodeInfo S_Critical = {
            0x37,
            "Critical hit",
            "S_Critical",
            11  // opcode(1) + attackerId(4) + targetId(4) + damage(2)
        };

        // --- Client -> Server ---

        // Player uses skill.
        // Payload: [skillId:2][targetX:2][targetY:2]
        constexpr OpCodeInfo C_Skill_Use_Request = {
            0x41,
            "Client requests skill use",
            "C_Skill_Use_Request",
            7  // opcode(1) + skillId(2) + x(2) + y(2)
        };

        // Player targets entity.
        // Payload: [entityId:4]
        constexpr OpCodeInfo C_Target_Request = {
            0x42,
            "Client requests target",
            "C_Target_Request",
            5  // opcode(1) + entityId(4)
        };

        // Player clears target.
        // Payload: (none)
        constexpr OpCodeInfo C_Target_Clear_Request = {
            0x43,
            "Client clears target",
            "C_Target_Clear_Request",
            1  // opcode only
        };
    }

    // ========================================================================
    // CHAT & SOCIAL - 0x50-0x5F
    // ========================================================================
    namespace Chat {
        // --- Client -> Server ---

        // Send chat message.
        // Payload: [channelId:1][messageLength:2][message:variable]
        constexpr OpCodeInfo C_Message_Send = {
            0x50,
            "Client sends chat message",
            "C_Message_Send",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Send emote.
        // Payload: [emoteId:1]
        constexpr OpCodeInfo C_Emote_Send = {
            0x51,
            "Client sends emote",
            "C_Emote_Send",
            2  // opcode(1) + emoteId(1)
        };

        // Send whisper.
        // Payload: [targetNameLength:1][targetName:variable][messageLength:2][message:variable]
        constexpr OpCodeInfo C_Whisper_Send = {
            0x52,
            "Client sends whisper",
            "C_Whisper_Send",
            OpCodeInfo::VARIABLE_SIZE
        };

        // --- Server -> Client ---

        // Broadcast chat message.
        // Payload: [senderId:4][channelId:1][messageLength:2][message:variable]
        constexpr OpCodeInfo S_Message_Broadcast = {
            0x58,
            "Server broadcasts chat message",
            "S_Message_Broadcast",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Broadcast emote.
        // Payload: [senderId:4][emoteId:1]
        constexpr OpCodeInfo S_Emote_Broadcast = {
            0x59,
            "Server broadcasts emote",
            "S_Emote_Broadcast",
            6  // opcode(1) + senderId(4) + emoteId(1)
        };

        // Whisper received.
        // Payload: [senderNameLength:1][senderName:variable][messageLength:2][message:variable]
        constexpr OpCodeInfo S_Whisper_Received = {
            0x5A,
            "Server delivers whisper",
            "S_Whisper_Received",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Whisper sent confirmation.
        // Payload: [targetNameLength:1][targetName:variable]
        constexpr OpCodeInfo S_Whisper_Sent_Confirm = {
            0x5B,
            "Server confirms whisper sent",
            "S_Whisper_Sent_Confirm",
            OpCodeInfo::VARIABLE_SIZE
        };

        // System message.
        // Payload: [messageType:1][messageLength:2][message:variable]
        constexpr OpCodeInfo S_System_Message = {
            0x5C,
            "Server sends system message",
            "S_System_Message",
            OpCodeInfo::VARIABLE_SIZE
        };
    }

    // ========================================================================
    // INVENTORY & ITEMS - 0x60-0x7F
    // ========================================================================
    namespace Inventory {
        // --- Client -> Server ---

        // Use item.
        // Payload: [slot:1]
        constexpr OpCodeInfo C_Item_Use_Request = {
            0x60,
            "Client uses item",
            "C_Item_Use_Request",
            2  // opcode(1) + slot(1)
        };

        // Drop item.
        // Payload: [slot:1][quantity:2]
        constexpr OpCodeInfo C_Item_Drop_Request = {
            0x61,
            "Client drops item",
            "C_Item_Drop_Request",
            4  // opcode(1) + slot(1) + quantity(2)
        };

        // Pick up item.
        // Payload: [groundItemId:4]
        constexpr OpCodeInfo C_Item_Pickup_Request = {
            0x62,
            "Client picks up item",
            "C_Item_Pickup_Request",
            5  // opcode(1) + groundItemId(4)
        };

        // Move item in inventory.
        // Payload: [fromSlot:1][toSlot:1]
        constexpr OpCodeInfo C_Item_Move_Request = {
            0x63,
            "Client moves item",
            "C_Item_Move_Request",
            3  // opcode(1) + fromSlot(1) + toSlot(1)
        };

        // --- Server -> Client ---

        // Full inventory update.
        // Payload: [slotCount:1][[slot:1][itemId:2][quantity:2][flags:1]]...
        constexpr OpCodeInfo S_Full_Update = {
            0x70,
            "Server sends full inventory",
            "S_Full_Update",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Single slot update.
        // Payload: [slot:1][itemId:2][quantity:2][flags:1]
        constexpr OpCodeInfo S_Slot_Update = {
            0x71,
            "Server updates inventory slot",
            "S_Slot_Update",
            7  // opcode(1) + slot(1) + itemId(2) + quantity(2) + flags(1)
        };

        // Item spawned on ground.
        // Payload: [groundItemId:4][itemId:2][x:2][y:2][quantity:2]
        constexpr OpCodeInfo S_GroundItem_Spawned = {
            0x72,
            "Item spawned on ground",
            "S_GroundItem_Spawned",
            13  // opcode(1) + groundId(4) + itemId(2) + x(2) + y(2) + qty(2)
        };

        // Item removed from ground.
        // Payload: [groundItemId:4]
        constexpr OpCodeInfo S_GroundItem_Removed = {
            0x73,
            "Item removed from ground",
            "S_GroundItem_Removed",
            5  // opcode(1) + groundId(4)
        };
    }

    // ========================================================================
    // DEBUG & ADMIN - 0xE0-0xEF
    // ========================================================================
    namespace Debug {
        // Custom debug response.
        // Payload: [dataLength:2][data:variable]
        constexpr OpCodeInfo S_Custom_Response = {
            0xE0,
            "Server sends debug response",
            "S_Custom_Response",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Request server state.
        // Payload: [debugFlags:1]
        constexpr OpCodeInfo C_Request_State = {
            0xE1,
            "Client requests debug state",
            "C_Request_State",
            2  // opcode(1) + debugFlags(1)
        };

        // Server state response.
        // Payload: [dataLength:2][data:variable]
        constexpr OpCodeInfo S_State_Response = {
            0xE2,
            "Server sends debug state",
            "S_State_Response",
            OpCodeInfo::VARIABLE_SIZE
        };
    }

    // ========================================================================
    // SYSTEM NOTIFICATIONS - 0xF3-0xF4
    // Note: 0xF2 is used by S_ServerShutdown in active opcodes
    // ========================================================================
    namespace System {
        // Kick notification.
        // Payload: [reasonLength:1][reason:variable]
        constexpr OpCodeInfo S_Kick_Notification = {
            0xF3,  // Changed from 0xF2 to avoid conflict with S_ServerShutdown
            "Server kicks client",
            "S_Kick_Notification",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Server shutdown warning.
        // Payload: [secondsRemaining:2][reasonLength:1][reason:variable]
        constexpr OpCodeInfo S_Shutdown_Warning = {
            0xF4,
            "Server shutdown warning",
            "S_Shutdown_Warning",
            OpCodeInfo::VARIABLE_SIZE
        };

        // Server time sync.
        // Payload: [serverTime:4]
        constexpr OpCodeInfo S_Time_Sync = {
            0xF5,  // Changed from 0xF4 to avoid conflict
            "Server time sync",
            "S_Time_Sync",
            5  // opcode(1) + serverTime(4)
        };
    }

} // namespace Protocol::Opcode::Unused
