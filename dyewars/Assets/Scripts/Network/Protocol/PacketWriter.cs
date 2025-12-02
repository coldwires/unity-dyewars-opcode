// PacketWriter.cs
// Static helper methods for writing binary data to network packets.
// Handles the packet framing (magic header + size) automatically.
//
// Usage:
//   var packet = PacketWriter.CreatePacket(opcode, (writer) => {
//       writer.WriteU32(playerId);
//       writer.WriteU16(x);
//       writer.WriteU16(y);
//   });

using System;
using System.Collections.Generic;

namespace DyeWars.Network.Protocol
{
    public static class PacketWriter
    {
        // Magic header bytes that start every packet
        private const byte MAGIC_1 = 0x11;
        private const byte MAGIC_2 = 0x00;

        /// <summary>
        /// Create a complete packet with header, size, and payload.
        /// </summary>
        /// <param name="opcode">The packet opcode (first byte of payload).</param>
        /// <param name="writePayload">Action to write additional payload data.</param>
        /// <returns>Complete packet ready to send.</returns>
        public static byte[] CreatePacket(byte opcode, Action<PayloadBuilder> writePayload = null)
        {
            var builder = new PayloadBuilder();
            builder.WriteU8(opcode);
            writePayload?.Invoke(builder);

            var payload = builder.ToArray();
            var packet = new byte[4 + payload.Length];

            // Write header
            packet[0] = MAGIC_1;
            packet[1] = MAGIC_2;
            packet[2] = (byte)((payload.Length >> 8) & 0xFF);
            packet[3] = (byte)(payload.Length & 0xFF);

            // Write payload
            Array.Copy(payload, 0, packet, 4, payload.Length);

            return packet;
        }

        /// <summary>
        /// Simple packet with just an opcode (no additional data).
        /// </summary>
        public static byte[] CreatePacket(byte opcode)
        {
            return CreatePacket(opcode, null);
        }

        /// <summary>
        /// Helper class for building packet payloads.
        /// </summary>
        public class PayloadBuilder
        {
            private readonly List<byte> data = new List<byte>();

            public void WriteU8(byte value)
            {
                data.Add(value);
            }

            public void WriteU16(ushort value)
            {
                data.Add((byte)((value >> 8) & 0xFF));
                data.Add((byte)(value & 0xFF));
            }

            public void WriteU32(uint value)
            {
                data.Add((byte)((value >> 24) & 0xFF));
                data.Add((byte)((value >> 16) & 0xFF));
                data.Add((byte)((value >> 8) & 0xFF));
                data.Add((byte)(value & 0xFF));
            }

            public void WriteS16(short value)
            {
                WriteU16((ushort)value);
            }

            public void WriteS32(int value)
            {
                WriteU32((uint)value);
            }

            public byte[] ToArray()
            {
                return data.ToArray();
            }
        }
    }

    /// <summary>
    /// Packet opcodes - centralized definition of all packet types.
    /// </summary>
    public static class PacketOpcode
    {
        // Client -> Server
        public const byte Move = 0x01;
        public const byte Turn = 0x04;

        // Server -> Client
        public const byte MyPosition = 0x10;
        public const byte CustomResponse = 0x11;
        public const byte OtherPlayerUpdate = 0x12;
        public const byte PlayerIdAssignment = 0x13;
        public const byte PlayerLeft = 0x14;
        public const byte FacingUpdate = 0x15;
        public const byte BatchUpdate = 0x20;
    }
}