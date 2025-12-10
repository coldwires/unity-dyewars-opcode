// PacketReader.cs
// Static helper methods for reading binary data from network packets.
// All methods take a byte array and a ref offset, reading data and advancing the offset.
// This keeps packet parsing code clean and consistent.
//
// Usage:
//   int offset = 1;  // Skip opcode
//   ulong playerId = PacketReader.ReadU64(payload, ref offset);
//   int x = PacketReader.ReadU16(payload, ref offset);
namespace DyeWars.Network.Protocol
{
    public static class PacketReader
    {
        /// <summary>
        /// Check if there are enough bytes remaining to read.
        /// </summary>
        public static bool HasBytes(byte[] data, int offset, int count)
        {
            return data != null && offset >= 0 && offset + count <= data.Length;
        }

        private static void CheckBounds(byte[] data, int offset, int count, string typeName)
        {
            if (data == null)
                throw new System.ArgumentNullException(nameof(data), $"PacketReader.Read{typeName}: data is null");
            if (offset < 0)
                throw new System.ArgumentOutOfRangeException(nameof(offset), $"PacketReader.Read{typeName}: offset ({offset}) is negative");
            if (offset + count > data.Length)
                throw new System.ArgumentOutOfRangeException(nameof(offset),
                    $"PacketReader.Read{typeName}: not enough bytes (need {count}, have {data.Length - offset} at offset {offset})");
        }

        /// <summary>
        /// Read a single byte (uint8) and advance offset by 1.
        /// </summary>
        public static byte ReadU8(byte[] data, ref int offset)
        {
            CheckBounds(data, offset, 1, "U8");
            return data[offset++];
        }

        /// <summary>
        /// Read two bytes as big-endian uint16 and advance offset by 2.
        /// </summary>
        public static ushort ReadU16(byte[] data, ref int offset)
        {
            CheckBounds(data, offset, 2, "U16");
            ushort value = (ushort)((data[offset] << 8) | data[offset + 1]);
            offset += 2;
            return value;
        }

        /// <summary>
        /// Read four bytes as big-endian uint32 and advance offset by 4.
        /// </summary>
        public static uint ReadU32(byte[] data, ref int offset)
        {
            CheckBounds(data, offset, 4, "U32");
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
        /// Read eight bytes as big-endian uint64 and advance offset by 8.
        /// </summary>
        public static ulong ReadU64(byte[] data, ref int offset)
        {
            CheckBounds(data, offset, 8, "U64");
            ulong value =
                ((ulong)data[offset] << 56) |
                ((ulong)data[offset + 1] << 48) |
                ((ulong)data[offset + 2] << 40) |
                ((ulong)data[offset + 3] << 32) |
                ((ulong)data[offset + 4] << 24) |
                ((ulong)data[offset + 5] << 16) |
                ((ulong)data[offset + 6] << 8) |
                ((ulong)data[offset + 7]);
            offset += 8;
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
        /// Read a length-prefixed UTF8 string (1-byte length prefix).
        /// Returns null if not enough bytes available.
        /// </summary>
        public static string ReadString(byte[] data, ref int offset)
        {
            if (!HasBytes(data, offset, 1)) return null;

            int length = ReadU8(data, ref offset);
            if (!HasBytes(data, offset, length)) return null;

            string result = System.Text.Encoding.UTF8.GetString(data, offset, length);
            offset += length;
            return result;
        }
    }
}