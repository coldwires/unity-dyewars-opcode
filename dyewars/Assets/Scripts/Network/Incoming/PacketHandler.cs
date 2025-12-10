// PacketHandler.cs
// All incoming packet processing in one place.
// When you add a new server->client packet, add it here.
//
// Single responsibility: parse incoming packets and publish events.
//
// This class does NOT contain game logic. It:
//   1. Parses the packet bytes
//   2. Publishes events via EventBus
//
// This keeps all "what did the server send us?" logic in one file.
// Subscribers (PlayerRegistry, PlayerViewFactory, etc.) handle the events.

using UnityEngine;
using DyeWars.Network.Protocol;
using DyeWars.Core;
using System;

namespace DyeWars.Network.Inbound
{
    public class PacketHandler
    {
        public PacketHandler()
        {
        }

        /// <summary>
        /// Process an incoming packet payload. Called from main thread.
        /// </summary>
        public void ProcessPacket(byte[] payload)
        {
            if (payload == null || payload.Length < 1) return;

            byte opcode = payload[0];
            int offset = 1;

            try
            {
                switch (opcode)
                {
                    case Opcode.LocalPlayer.S_Position_Correction:
                        HandleLocalPositionCorrection(payload, offset);
                        break;

                    case Opcode.LocalPlayer.S_Welcome:
                        HandleWelcome(payload, offset);
                        break;

                    case Opcode.LocalPlayer.S_Facing_Correction:
                        HandleLocalFacingCorrection(payload, offset);
                        break;

                    case Opcode.Batch.S_Player_Spatial:
                        HandleBatchPlayerSpatial(payload, offset);
                        break;

                    case Opcode.RemotePlayer.S_Left_Game:
                        HandlePlayerLeft(payload, offset);
                        break;

                    case Opcode.Connection.S_Ping_Request:
                        HandleServerPingRequest(payload, offset);
                        break;

                    case Opcode.Connection.S_ServerShutdown:
                        HandleServerShutdown(payload, offset);
                        break;

                    default:
                        Debug.LogWarning($"PacketHandler: Unknown opcode 0x{opcode:X2}");
                        break;
                }
            }
            catch (Exception ex)
            {
                Debug.LogError($"PacketHandler: Error processing opcode 0x{opcode:X2}: {ex.Message}");
            }
        }


        // ========================================================================
        // LOCAL PLAYER HANDLERS
        // ========================================================================
        private void HandleWelcome(byte[] payload, int offset)
        {
            // Welcome packet: playerId(8) + x(2) + y(2) + facing(1)
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Welcome - 1)) return;
            ulong playerId = PacketReader.ReadU64(payload, ref offset);
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);
            var position = new Vector2Int(x, y);
            Debug.Log($"PacketHandler: Welcome received, player ID {playerId} at {position} facing {facing}");
            // Listeners: PlayerRegistry, PlayerViewFactory
            EventBus.Publish(new WelcomeReceivedEvent { PlayerId = playerId, Position = position, Facing = facing });
        }

        private void HandleLocalPositionCorrection(byte[] payload, int offset)
        {
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Position_Correction - 1)) return;
            int x = PacketReader.ReadU16(payload, ref offset);
            int y = PacketReader.ReadU16(payload, ref offset);
            int facing = PacketReader.ReadU8(payload, ref offset);
            // Listeners: PlayerRegistry.HandleLocalPositionCorrection, LocalPlayerController.OnLocalPositionCorrected
            EventBus.Publish(new LocalPlayerPositionCorrectedEvent { Position = new Vector2Int(x, y) });
            // Listeners: PlayerRegistry.HandleLocalFacingCorrection, LocalPlayerController.OnLocalFacingChanged
            EventBus.Publish(new LocalPlayerFacingChangedEvent { Facing = facing });
        }

        private void HandleLocalFacingCorrection(byte[] payload, int offset)
        {
            // Facing correction: facing(1)
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_LocalPlayer_Facing_Correction - 1)) return;
            int facing = PacketReader.ReadU8(payload, ref offset);
            // Listeners: PlayerRegistry.HandleLocalFacingCorrection, LocalPlayerController.OnLocalFacingChanged
            EventBus.Publish(new LocalPlayerFacingChangedEvent { Facing = facing });
        }

        private void HandleBatchPlayerSpatial(byte[] payload, int offset)
        {
            // Batch player spatial sync: [count:1][[playerId:8][x:2][y:2][facing:1]]...
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_Batch_Player_Spatial_Header - 1)) return;
            int count = PacketReader.ReadU8(payload, ref offset);
            Debug.Log($"PacketHandler: Batch player spatial sync, {count} players");

            for (int i = 0; i < count; i++)
            {
                // Per player: playerId(8) + x(2) + y(2) + facing(1)
                if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_Batch_Player_Spatial_PerPlayer)) break;
                ulong playerId = PacketReader.ReadU64(payload, ref offset);
                int x = PacketReader.ReadU16(payload, ref offset);
                int y = PacketReader.ReadU16(payload, ref offset);
                int facing = PacketReader.ReadU8(payload, ref offset);
                var position = new Vector2Int(x, y);

                // Listeners: PlayerRegistry, PlayerViewFactory - will create/update player as needed
                EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = playerId, Position = position, Facing = facing });
            }
        }

        private void HandlePlayerLeft(byte[] payload, int offset)
        {
            // Player left: playerId(8)
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_RemotePlayer_Left_Game - 1)) return;
            ulong playerId = PacketReader.ReadU64(payload, ref offset);
            // Listeners: PlayerRegistry.HandlePlayerLeft, PlayerViewFactory.DestroyPlayerView
            EventBus.Publish(new PlayerLeftEvent { PlayerId = playerId });
        }

        // ========================================================================
        // SYSTEM HANDLERS
        // ========================================================================

        private void HandleServerPingRequest(byte[] payload, int offset)
        {
            // Server ping request: timestamp(4)
            if (!PacketReader.HasBytes(payload, offset, PayloadSize.S_Connection_Ping_Request - 1)) return;
            uint timestamp = PacketReader.ReadU32(payload, ref offset);
            // Echo back immediately
            var networkService = ServiceLocator.Get<INetworkService>();
            networkService?.Sender.SendPongResponse(timestamp);
        }

        private void HandleServerShutdown(byte[] payload, int offset)
        {
            // Server shutdown: reason(1)
            int reasonCode = PacketReader.HasBytes(payload, offset, PayloadSize.S_Connection_ServerShutdown - 1)
                ? PacketReader.ReadU8(payload, ref offset) : 0;
            string reason = reasonCode switch
            {
                0 => "Server shutting down",
                1 => "Server maintenance",
                _ => $"Server shutdown (code {reasonCode})"
            };
            Debug.LogWarning("PacketHandler: Server shutdown - " + reason);
            EventBus.Publish(new DisconnectedFromServerEvent { Reason = reason });
        }
    }
}
