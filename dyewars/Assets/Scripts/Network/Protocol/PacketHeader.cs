// PacketHeader.cs
// Constants for packet framing. Change these once, affects entire codebase.
//
// Packet structure:
//   [Magic1][Magic2][SizeHigh][SizeLow][Payload...]
//   
// The magic bytes identify valid packets and help detect corruption
// or desync in the byte stream.

namespace DyeWars.Network.Protocol
{
    public static class PacketHeader
    {
        /// <summary>
        /// Protocol version. Server and client must match.
        /// Increment this when you make breaking protocol changes.
        /// </summary>
        public const ushort ProtocolVersion = 0x0001;

        /// <summary>
        /// Client identifier magic bytes. "DYEW" in ASCII.
        /// Server uses this to verify it's talking to a real DyeWars client.
        /// </summary>
        public const uint ClientMagic = 0x44594557;  // 'D' 'Y' 'E' 'W'
        
        /// <summary>
        /// First magic byte. Combined with Magic2 to identify valid packets.
        /// </summary>
        public const byte Magic1 = 0x11;

        /// <summary>
        /// Second magic byte. Combined with Magic1 to identify valid packets.
        /// </summary>
        public const byte Magic2 = 0x68;

        /// <summary>
        /// Total header size in bytes (magic + size).
        /// </summary>
        public const int HeaderSize = 4;

        /// <summary>
        /// Maximum allowed payload size (64KB).
        /// </summary>
        public const int MaxPayloadSize = 65535;

        /// <summary>
        /// Validate that a buffer contains valid magic bytes.
        /// </summary>
        public static bool IsValidMagic(byte[] buffer)
        {
            return buffer != null &&
                   buffer.Length >= 2 &&
                   buffer[0] == Magic1 &&
                   buffer[1] == Magic2;
        }

        /// <summary>
        /// Read payload size from header buffer (bytes 2-3, big-endian).
        /// </summary>
        public static ushort ReadPayloadSize(byte[] headerBuffer)
        {
            return (ushort)((headerBuffer[2] << 8) | headerBuffer[3]);
        }

        /// <summary>
        /// Write header bytes to a buffer.
        /// </summary>
        public static void WriteHeader(byte[] buffer, ushort payloadSize)
        {
            buffer[0] = Magic1;
            buffer[1] = Magic2;
            buffer[2] = (byte)((payloadSize >> 8) & 0xFF);
            buffer[3] = (byte)(payloadSize & 0xFF);
        }
    }
}