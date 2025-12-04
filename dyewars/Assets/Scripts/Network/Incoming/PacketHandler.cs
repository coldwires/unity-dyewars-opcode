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

        //cache the service for localplayer lookup (Batch Updates)
        private readonly INetworkService networkService;

        public PacketHandler()
        {
            networkService = ServiceLocator.Get<INetworkService>();
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
                // LOCAL PLAYER UPDATES
                // ============================================================

                case Opcode.LocalPlayer.S_Position_Correction:
                    HandleMyPositionCorrection(payload, offset);
                    break;

                case Opcode.LocalPlayer.S_Welcome:
                    HandlePlayerIdAssignment(payload, offset);
                    break;

                case Opcode.LocalPlayer.S_Facing_Correction:
                    HandleMyFacing(payload, offset);
                    break;

                // ============================================================
                // WORLD UPDATES
                // ============================================================

                // TODO Merge this to batchupdate
                //case Opcode.Batch.S_RemotePlayer_Update:
                //    HandleOtherPlayerUpdate(payload, offset);
                //    break;

                case Opcode.RemotePlayer.S_Left_Game:
                    HandlePlayerLeft(payload, offset);
                    break;

                case Opcode.Batch.S_RemotePlayer_Update:
                    HandleBatchUpdate(payload, offset);
                    break;

                case Opcode.RemotePlayer.S_Joined_Game:
                    HandlePlayerJoined(payload, offset);
                    break;

                // ============================================================
                // COMBAT & EFFECTS (0x30 - 0x3F)
                // ============================================================



                // ============================================================
                // SYSTEM (0xF0 - 0xFF)
                // ============================================================

                case Opcode.Connection.S_Pong_Response:
                    HandlePong(payload, offset);
                    break;

                case Opcode.System.S_Kick_Notification:
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

        private void HandleMyPositionCorrection(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Position_Correction - 1)) return;

            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.LocalPlayerPositionCorrectedEvent()
            {
                Position = new Vector2Int(x, y),
            });

            Core.EventBus.Publish(new Core.LocalPlayerFacingChangedEvent()
            {
                Facing = facing
            });
        }

        private void HandlePlayerIdAssignment(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 4)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);

            Debug.Log($"PacketHandler: Assigning player ID {playerId}");

            Core.EventBus.Publish(new Core.LocalPlayerIdAssignedEvent
            {
                PlayerId = playerId
            });
        }

        private void HandleMyFacing(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, 1)) return;

            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.LocalPlayerFacingChangedEvent()
            {
                Facing = facing,
            });
        }

        // ========================================================================
        // WORLD UPDATE HANDLERS
        // ========================================================================

        private void HandleOtherPlayerUpdate(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Position_Correction - 1)) return;

            uint playerId = PacketReader.ReadU32(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);

            Core.EventBus.Publish(new Core.OtherPlayerPositionChangedEvent
            {
                PlayerId = playerId,
                Position = new Vector2Int(x, y),
                IsCorrection = false
            });

            Core.EventBus.Publish(new Core.OtherPlayerFacingChangedEvent
            {
                PlayerId = playerId,
                Facing = facing,
            });
        }

        private void HandlePlayerLeft(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_RemotePlayer_Left_Game)) return;

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
                if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_Batch_RemotePlayer_Update_PerPlayer)) break;

                uint playerId = PacketReader.ReadU32(payload, ref offset);
                int x = PacketReader.ReadU16(payload, ref offset);
                int y = PacketReader.ReadU16(payload, ref offset);
                int facing = PacketReader.ReadU8(payload, ref offset);

                // Skip our own updates (we use client-side prediction)
                //if (playerId == networkService.LocalPlayerId) continue;


                Core.EventBus.Publish(new Core.OtherPlayerPositionChangedEvent
                {
                    PlayerId = playerId,
                    Position = new Vector2Int(x, y),
                    IsCorrection = false
                });

                Core.EventBus.Publish(new Core.OtherPlayerFacingChangedEvent
                {
                    PlayerId = playerId,
                    Facing = facing,
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