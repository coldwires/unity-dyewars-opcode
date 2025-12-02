// PacketReader.cs
// Static helper methods for reading binary data from network packets.
// All methods take a byte array and a ref offset, reading data and advancing the offset.
// This keeps packet parsing code clean and consistent.
//
// Usage:
//   int offset = 1;  // Skip opcode
//   uint playerId = PacketReader.ReadU32(payload, ref offset);
//   int x = PacketReader.ReadU16(payload, ref offset);
namespace DyeWars.Network.Protocol
{
    public static class PacketReader
    {
        /// <summary>
        /// Read a single byte (uint8) and advance offset by 1.
        /// </summary>
        public static byte ReadU8(byte[] data, ref int offset)
        {
            return data[offset++];
        }

        /// <summary>
        /// Read two bytes as big-endian uint16 and advance offset by 2.
        /// </summary>
        public static ushort ReadU16(byte[] data, ref int offset)
        {
            ushort value = (ushort)((data[offset] << 8) | data[offset + 1]);
            offset += 2;
            return value;
        }

        /// <summary>
        /// Read four bytes as big-endian uint32 and advance offset by 4.
        /// </summary>
        public static uint ReadU32(byte[] data, ref int offset)
        {
            uint value = (uint)(
                (data[offset] << 24) |
                (data[offset + 1] << 16) |
                (data[offset + 2] << 8) |
                data[offset + 3]
            );
            offset += 4;
            return value;
        }

        /// <summary>
        /// Read a signed 16-bit integer (big-endian) and advance offset by 2.
        /// </summary>
        public static short ReadS16(byte[] data, ref int offset)
        {
            return (short)ReadU16(data, ref offset);
        }

        /// <summary>
        /// Read a signed 32-bit integer (big-endian) and advance offset by 4.
        /// </summary>
        public static int ReadS32(byte[] data, ref int offset)
        {
            return (int)ReadU32(data, ref offset);
        }

        /// <summary>
        /// Check if there are enough bytes remaining to read.
        /// </summary>
        public static bool HasBytes(byte[] data, int offset, int count)
        {
            return offset + count <= data.Length;
        }
    }
}