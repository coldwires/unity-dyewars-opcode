// PacketHandler.cs
// All incoming packet processing in one place.
// When you add a new server->client packet, add it here.
//
// Single responsibility: parse incoming packets and publish events.
//
// This class does NOT contain game logic. It:
//   1. Parses the packet bytes
//   2. Publishes an event with the parsed data
//   3. Other systems subscribe to events and handle the game logic
//
// This keeps all "what did the server send us?" logic in one file.

using UnityEngine;
using DyeWars.Network.Protocol;

namespace DyeWars.Network.Inbound
{
    public class PacketHandler
    {
        // Track local player ID for filtering batch updates
        private uint localPlayerId = 0;

        public void SetLocalPlayerId(uint playerId)
        {
            localPlayerId = playerId;
        }

        /// <summary>
        /// Process an incoming packet payload. Called from main thread.
        /// </summary>
        public void ProcessPacket(byte[] payload)
        {
            if (payload == null || payload.Length < 1) return;

            byte opcode = payload[0];
            int offset = 1; // Skip opcode

            switch (opcode)
            {
                // ============================================================
                // LOCAL PLAYER UPDATES (0x10 - 0x1F)
                // ============================================================

                case Opcode.S_MyPosition:
                    HandleMyPosition(payload, offset);
                    break;

                case Opcode.S_PlayerIdAssignment:
                    HandlePlayerIdAssignment(payload, offset);
                    break;

                case Opcode.S_MyFacing:
                    HandleMyFacing(payload, offset);
                    break;

                // ============================================================
                // WORLD UPDATES (0x12, 0x14, 0x20 - 0x2F)
                // ============================================================

                case Opcode.S_OtherPlayerUpdate:
                    HandleOtherPlayerUpdate(payload, offset);
                    break;

                case Opcode.S_PlayerLeft:
                    HandlePlayerLeft(payload, offset);
                    break;

                case Opcode.S_BatchUpdate:
                    HandleBatchUpdate(payload, offset);
                    break;

                case Opcode.S_PlayerJoined:
                    HandlePlayerJoined(payload, offset);
                    break;

                // ============================================================
                // COMBAT & EFFECTS (0x30 - 0x3F)
                // ============================================================

                case Opcode.S_PlayEffect:
                    HandlePlayEffect(payload, offset);
                    break;

                case Opcode.S_Damage:
                    HandleDamage(payload, offset);
                    break;

                case Opcode.S_Heal:
                    HandleHeal(payload, offset);
                    break;

                case Opcode.S_Death:
                    HandleDeath(payload, offset);
                    break;

                case Opcode.S_Respawn:
                    HandleRespawn(payload, offset);
                    break;

                // ============================================================
                // SYSTEM (0xF0 - 0xFF)
                // ============================================================

                case Opcode.Pong:
                    HandlePong(payload, offset);
                    break;

                case Opcode.S_Kick:
                    HandleKick(payload, offset);
                    break;

                default:
                    Debug.LogWarning($"PacketHandler: Unknown opcode 0x{opcode:X2}");
                    break;
            }
        }

        // ========================================================================
        // LOCAL PLAYER HANDLERS
        // ========================================================================

        private void HandleMyPosition(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_MyPosition - 1)) return;

            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.PlayerPositionChangedEvent
            {
                PlayerId = localPlayerId,
                Position = new Vector2Int(x, y),
                IsLocalPlayer = true,
                IsCorrection = true
            });

            Core.EventBus.Publish(new Core.PlayerFacingChangedEvent
            {
                PlayerId = localPlayerId,
                Facing = facing,
                IsLocalPlayer = true
            });
        }

        private void HandlePlayerIdAssignment(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            localPlayerId = playerId;

            Debug.Log($"PacketHandler: Assigned player ID {playerId}");

            Core.EventBus.Publish(new Core.LocalPlayerIdAssignedEvent
            {
                PlayerId = playerId
            });
        }

        private void HandleMyFacing(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 1)) return;

            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.PlayerFacingChangedEvent
            {
                PlayerId = localPlayerId,
                Facing = facing,
                IsLocalPlayer = true
            });
        }

        // ========================================================================
        // WORLD UPDATE HANDLERS
        // ========================================================================

        private void HandleOtherPlayerUpdate(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_OtherPlayerUpdate - 1)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.PlayerPositionChangedEvent
            {
                PlayerId = playerId,
                Position = new Vector2Int(x, y),
                IsLocalPlayer = false,
                IsCorrection = false
            });

            Core.EventBus.Publish(new Core.PlayerFacingChangedEvent
            {
                PlayerId = playerId,
                Facing = facing,
                IsLocalPlayer = false
            });
        }

        private void HandlePlayerLeft(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);

            Core.EventBus.Publish(new Core.PlayerLeftEvent
            {
                PlayerId = playerId
            });
        }

        private void HandleBatchUpdate(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 1)) return;

            int count = PacketReader.ReadU8(payload, ref offset);

            for (int i = 0; i < count; i++)
            {
                if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_BatchUpdatePerPlayer)) break;

                uint playerId = PacketReader.ReadU32(payload, ref offset);
                int x = PacketReader.ReadU16(payload, ref offset);
                int y = PacketReader.ReadU16(payload, ref offset);
                int facing = PacketReader.ReadU8(payload, ref offset);

                // Skip our own updates (we use client-side prediction)
                if (playerId == localPlayerId) continue;

                Core.EventBus.Publish(new Core.PlayerPositionChangedEvent
                {
                    PlayerId = playerId,
                    Position = new Vector2Int(x, y),
                    IsLocalPlayer = false,
                    IsCorrection = false
                });

                Core.EventBus.Publish(new Core.PlayerFacingChangedEvent
                {
                    PlayerId = playerId,
                    Facing = facing,
                    IsLocalPlayer = false
                });
            }
        }

        private void HandlePlayerJoined(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 9)) return; // 4+2+2+1 minimum

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);

            // Could also read player name here if included in packet

            Core.EventBus.Publish(new Core.PlayerJoinedEvent
            {
                PlayerId = playerId,
                Position = new Vector2Int(x, y),
                Facing = facing
            });
        }

        // ========================================================================
        // COMBAT HANDLERS (stubs for future implementation)
        // ========================================================================

        private void HandlePlayEffect(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 5)) return; // 2+2+2+1 = 7, minus opcode = 6... let's say 5 minimum

            ushort effectId = PacketReader.ReadU16(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            byte param = PacketReader.ReadU8(payload, ref offset);

            // TODO: Publish effect event when combat system is implemented
            Debug.Log($"PacketHandler: Effect {effectId} at ({x}, {y}) param={param}");
        }

        private void HandleDamage(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 10)) return; // 4+2+2+2

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            ushort damage = PacketReader.ReadU16(payload, ref offset);
            ushort currentHp = PacketReader.ReadU16(payload, ref offset);
            ushort maxHp = PacketReader.ReadU16(payload, ref offset);

            // TODO: Publish damage event when combat system is implemented
            Debug.Log($"PacketHandler: Player {playerId} took {damage} damage ({currentHp}/{maxHp})");
        }

        private void HandleHeal(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 10)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            ushort amount = PacketReader.ReadU16(payload, ref offset);
            ushort currentHp = PacketReader.ReadU16(payload, ref offset);
            ushort maxHp = PacketReader.ReadU16(payload, ref offset);

            // TODO: Publish heal event when combat system is implemented
            Debug.Log($"PacketHandler: Player {playerId} healed {amount} ({currentHp}/{maxHp})");
        }

        private void HandleDeath(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);

            // TODO: Publish death event when combat system is implemented
            Debug.Log($"PacketHandler: Player {playerId} died");
        }

        private void HandleRespawn(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 8)) return; // 4+2+2

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);

            // TODO: Publish respawn event when combat system is implemented
            Debug.Log($"PacketHandler: Player {playerId} respawned at ({x}, {y})");
        }

        // ========================================================================
        // SYSTEM HANDLERS
        // ========================================================================

        private void HandlePong(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;

            uint timestamp = PacketReader.ReadU32(payload, ref offset);
            uint now = (uint)(Time.realtimeSinceStartup * 1000);
            uint latency = now - timestamp;

            Debug.Log($"PacketHandler: Ping latency = {latency}ms");

            // TODO: Publish latency event for UI display
        }

        private void HandleKick(byte[] payload, int offset)
        {
            string reason = "Unknown";

            if (PacketReader.HasBytes(payload, offset, 1))
            {
                byte reasonLength = PacketReader.ReadU8(payload, ref offset);
                if (PacketReader.HasBytes(payload, offset, reasonLength))
                {
                    byte[] reasonBytes = new byte[reasonLength];
                    for (int i = 0; i < reasonLength; i++)
                    {
                        reasonBytes[i] = PacketReader.ReadU8(payload, ref offset);
                    }
                    reason = System.Text.Encoding.UTF8.GetString(reasonBytes);
                }
            }

            Debug.LogWarning($"PacketHandler: Kicked from server - {reason}");

            Core.EventBus.Publish(new Core.DisconnectedFromServerEvent
            {
                Reason = reason
            });
        }
    }
}