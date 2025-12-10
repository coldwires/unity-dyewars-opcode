// PacketWriter.cs
// Static helper methods for writing binary data to network packets.
// Handles the packet framing (magic header + size) automatically.
//
// Usage:
//   var packet = PacketWriter.CreatePacket(Opcode.C_Move, (writer) => {
//       writer.WriteU8(direction);
//       writer.WriteU8(facing);
//   });

using System;
using System.Collections.Generic;

namespace DyeWars.Network.Protocol
{
    public static class PacketWriter
    {
        /// <summary>
        /// Create a complete packet with header, size, and payload.
        /// </summary>
        /// <param name="opcode">The packet opcode (first byte of payload).</param>
        /// <param name="writePayload">Action to write additional payload data.</param>
        /// <returns>Complete packet ready to send.</returns>
        public static byte[] CreatePacket(byte opcode, Action<PayloadBuilder> writePayload = null)
        {
            // 1. Build the payload (opcode + any additional data)
            var builder = new PayloadBuilder();
            builder.WriteU8(opcode);
            writePayload?.Invoke(builder);

            // 2. Get the payload bytes
            var payload = builder.ToArray();
            
            // 3. Create packet buffer: 4 bytes header + payload
            var packet = new byte[PacketHeader.HeaderSize + payload.Length];

            // 4. Write header using centralized constants (0x11 0x68 + size)
            PacketHeader.WriteHeader(packet, (ushort)payload.Length);

            // 5. Copy payload after header
            Array.Copy(payload, 0, packet, PacketHeader.HeaderSize, payload.Length);

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
        /// Mutable: we add bytes to it as we build the payload.
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
                data.Add((byte)((value >> 8) & 0xFF));  // High byte first (big-endian)
                data.Add((byte)(value & 0xFF));         // Low byte
            }

            public void WriteU32(uint value)
            {
                data.Add((byte)((value >> 24) & 0xFF));
                data.Add((byte)((value >> 16) & 0xFF));
                data.Add((byte)((value >> 8) & 0xFF));
                data.Add((byte)(value & 0xFF));
            }

            public void WriteU64(ulong value)
            {
                data.Add((byte)((value >> 56) & 0xFF));
                data.Add((byte)((value >> 48) & 0xFF));
                data.Add((byte)((value >> 40) & 0xFF));
                data.Add((byte)((value >> 32) & 0xFF));
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

            public void WriteBytes(byte[] bytes)
            {
                data.AddRange(bytes);
            }

            /// <summary>
            /// Write a length-prefixed UTF8 string (1-byte length prefix).
            /// Maximum string length is 255 bytes after UTF8 encoding.
            /// </summary>
            public void WriteString(string value)
            {
                byte[] bytes = System.Text.Encoding.UTF8.GetBytes(value ?? "");
                if (bytes.Length > 255)
                {
                    UnityEngine.Debug.LogWarning($"PacketWriter: String truncated from {bytes.Length} to 255 bytes");
                    byte[] truncated = new byte[255];
                    Array.Copy(bytes, truncated, 255);
                    bytes = truncated;
                }
                WriteU8((byte)bytes.Length);
                data.AddRange(bytes);
            }

            public byte[] ToArray()
            {
                return data.ToArray();
            }
        }
    }
}
