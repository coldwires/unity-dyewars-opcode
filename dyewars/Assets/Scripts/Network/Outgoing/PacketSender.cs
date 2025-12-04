// PacketSender.cs
// All outgoing packet construction in one place.
// When you add a new client->server packet, add it here.
//
// Single responsibility: construct and send outgoing packets.
//
// Each method:
//   1. Creates the packet using PacketWriter
//   2. Sends it via NetworkConnection
//
// This keeps all "what do we send to the server?" logic in one file.

using UnityEngine;
using DyeWars.Network.Protocol;
using DyeWars.Network.Connection;


namespace DyeWars.Network.Outbound
{
    public class PacketSender
    {
        private readonly NetworkConnection connection;

        public PacketSender(NetworkConnection connection)
        {
            this.connection = connection;
        }

        /// <summary>
        /// Send the handshake packet. Must be called immediately after connecting.
        /// </summary>
        public void SendHandshake()
        {
            var packet = PacketWriter.CreatePacket( Protocol.Opcode.Connection.C_Handshake_Request, writer =>
            {
                writer.WriteU16(PacketHeader.ProtocolVersion);
                writer.WriteU32(PacketHeader.ClientMagic);
            });

            connection.SendRaw(packet);
            Debug.Log("PacketSender: Handshake sent");
        }
        
        // ========================================================================
        // MOVEMENT PACKETS (0x01 - 0x0F)
        // ========================================================================

        /// <summary>
        /// Send a move request to the server.
        /// Server will validate and respond with position confirmation.
        /// </summary>
        public void SendMove(int direction, int facing)
        {
            var packet = PacketWriter.CreatePacket(Opcode.Movement.C_Move_Request, writer =>
            {
                writer.WriteU8((byte)direction);
                writer.WriteU8((byte)facing);
            });
            connection.SendRaw(packet);
        }

        /// <summary>
        /// Send a turn request to the server.
        /// Changes facing direction without moving.
        /// </summary>
        public void SendTurn(int direction)
        {
            var packet = PacketWriter.CreatePacket(Opcode.Movement.C_Turn_Request, writer =>
            {
                writer.WriteU8((byte)direction);
            });
            connection.SendRaw(packet);
        }

        /// <summary>
        /// Send an interact request (interact with object in front of player).
        /// </summary>
        public void SendInteract()
        {
            var packet = PacketWriter.CreatePacket(Opcode.Movement.C_Interact_Request);
            connection.SendRaw(packet);
        }

        // ========================================================================
        // COMBAT PACKETS (0x40 - 0x4F)
        // ========================================================================

        /// <summary>
        /// Send an attack in the player's facing direction.
        /// </summary>
        public void SendAttack()
        {
            var packet = PacketWriter.CreatePacket(Opcode.Combat.C_Attack_Request);
            connection.SendRaw(packet);
        }

        /// <summary>
        /// Send a skill/ability use.
        /// </summary>
        public void SendUseSkill(ushort skillId, int targetX, int targetY)
        {
            var packet = PacketWriter.CreatePacket(Opcode.Combat.C_Skill_Use_Request, writer =>
            {
                writer.WriteU16(skillId);
                writer.WriteU16((ushort)targetX);
                writer.WriteU16((ushort)targetY);
            });
            connection.SendRaw(packet);
        }

        // ========================================================================
        // CHAT PACKETS (0x50 - 0x5F)
        // ========================================================================

        /// <summary>
        /// Send a chat message.
        /// </summary>
        public void SendChatMessage(byte channelId, string message)
        {
            var packet = PacketWriter.CreatePacket(Opcode.Chat.C_Message_Send, writer =>
            {
                writer.WriteU8(channelId);
                // Convert string to bytes (UTF8)
                byte[] messageBytes = System.Text.Encoding.UTF8.GetBytes(message);
                writer.WriteU16((ushort)messageBytes.Length);
                foreach (byte b in messageBytes)
                {
                    writer.WriteU8(b);
                }
            });
            connection.SendRaw(packet);
        }

        /// <summary>
        /// Send an emote/gesture.
        /// </summary>
        public void SendEmote(byte emoteId)
        {
            var packet = PacketWriter.CreatePacket(Opcode.Chat.C_Emote_Send, writer =>
            {
                writer.WriteU8(emoteId);
            });
            connection.SendRaw(packet);
        }

        // ========================================================================
        // SYSTEM PACKETS (0xF0 - 0xFF)
        // ========================================================================

        /// <summary>
        /// Send a ping request to measure latency.
        /// </summary>
        public void SendPing()
        {
            uint timestamp = (uint)(Time.realtimeSinceStartup * 1000);
            var packet = PacketWriter.CreatePacket(Opcode.Connection.C_Ping_Request, writer =>
            {
                writer.WriteU32(timestamp);
            });
            connection.SendRaw(packet);
        }

        /// <summary>
        /// Send a graceful disconnect notification.
        /// </summary>
        public void SendDisconnect()
        {
            var packet = PacketWriter.CreatePacket(Opcode.Connection.C_Disconnect_Request);
            connection.SendRaw(packet);
        }
    }
}