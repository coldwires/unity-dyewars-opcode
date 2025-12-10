using NUnit.Framework;
using DyeWars.Network.Protocol;

public class PacketWriterTests
{
    // Header is 4 bytes: [magic 0x11][magic 0x68][size high][size low]
    // Payload starts at index 4: [opcode][data...]
    private const int HeaderSize = PacketHeader.HeaderSize;

    // ====================================================================
    // WriteU8 - single byte
    // ====================================================================

    [Test]
    public void WriteU8_CorrectValue()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU8(0x42);
        });

        Assert.AreEqual(0x42, packet[HeaderSize + 1]);
    }

    [Test]
    public void WriteU8_MultipleWrites_CorrectOrder()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU8(0xAA);
            writer.WriteU8(0xBB);
            writer.WriteU8(0xCC);
        });

        Assert.AreEqual(0xAA, packet[HeaderSize + 1]);
        Assert.AreEqual(0xBB, packet[HeaderSize + 2]);
        Assert.AreEqual(0xCC, packet[HeaderSize + 3]);
    }

    // ====================================================================
    // WriteU16 - big-endian 16-bit
    // ====================================================================

    [Test]
    public void WriteU16_BigEndian_CorrectBytes()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU16(0x1234);
        });

        // After header (4) + opcode (1), data starts at index 5
        Assert.AreEqual(0x12, packet[HeaderSize + 1]);
        Assert.AreEqual(0x34, packet[HeaderSize + 2]);
    }

    [Test]
    public void WriteU16_MaxValue_CorrectBytes()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU16(ushort.MaxValue);
        });

        Assert.AreEqual(0xFF, packet[HeaderSize + 1]);
        Assert.AreEqual(0xFF, packet[HeaderSize + 2]);
    }

    // ====================================================================
    // WriteU32 - big-endian 32-bit
    // ====================================================================

    [Test]
    public void WriteU32_BigEndian_CorrectBytes()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU32(0x12345678);
        });

        Assert.AreEqual(0x12, packet[HeaderSize + 1]);
        Assert.AreEqual(0x34, packet[HeaderSize + 2]);
        Assert.AreEqual(0x56, packet[HeaderSize + 3]);
        Assert.AreEqual(0x78, packet[HeaderSize + 4]);
    }

    // ====================================================================
    // WriteU64 - big-endian 64-bit
    // ====================================================================

    [Test]
    public void WriteU64_BigEndian_CorrectBytes()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU64(0x0102030405060708);
        });

        Assert.AreEqual(0x01, packet[HeaderSize + 1]);
        Assert.AreEqual(0x02, packet[HeaderSize + 2]);
        Assert.AreEqual(0x03, packet[HeaderSize + 3]);
        Assert.AreEqual(0x04, packet[HeaderSize + 4]);
        Assert.AreEqual(0x05, packet[HeaderSize + 5]);
        Assert.AreEqual(0x06, packet[HeaderSize + 6]);
        Assert.AreEqual(0x07, packet[HeaderSize + 7]);
        Assert.AreEqual(0x08, packet[HeaderSize + 8]);
    }

    [Test]
    public void WriteU64_SmallValue_PadsWithZeros()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU64(42); // Small value should have leading zeros
        });

        // First 7 bytes should be 0x00, last byte is 0x2A (42)
        for (int i = 1; i <= 7; i++)
        {
            Assert.AreEqual(0x00, packet[HeaderSize + i], $"Byte {i} should be 0x00");
        }
        Assert.AreEqual(0x2A, packet[HeaderSize + 8]);
    }

    // ====================================================================
    // WriteString - length-prefixed UTF-8
    // ====================================================================

    [Test]
    public void WriteString_SimpleString_CorrectFormat()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteString("Hi");
        });

        // Format: [length:1][utf8 bytes...]
        Assert.AreEqual(2, packet[HeaderSize + 1]);  // Length = 2
        Assert.AreEqual(0x48, packet[HeaderSize + 2]); // 'H'
        Assert.AreEqual(0x69, packet[HeaderSize + 3]); // 'i'
    }

    [Test]
    public void WriteString_EmptyString_WritesZeroLength()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteString("");
        });

        Assert.AreEqual(0, packet[HeaderSize + 1]); // Length = 0
        Assert.AreEqual(HeaderSize + 1 + 1, packet.Length); // Header + opcode + length byte only
    }

    [Test]
    public void WriteString_NullString_WritesZeroLength()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteString(null);
        });

        Assert.AreEqual(0, packet[HeaderSize + 1]); // Length = 0
    }

    // ====================================================================
    // Round-trip: Write then Read should match
    // ====================================================================

    [Test]
    public void RoundTrip_U8_Matches()
    {
        byte original = 0xAB;

        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU8(original);
        });

        int offset = HeaderSize + 1;
        byte result = PacketReader.ReadU8(packet, ref offset);

        Assert.AreEqual(original, result);
    }

    [Test]
    public void RoundTrip_U16_Matches()
    {
        ushort original = 12345;

        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU16(original);
        });

        int offset = HeaderSize + 1;
        ushort result = PacketReader.ReadU16(packet, ref offset);

        Assert.AreEqual(original, result);
    }

    [Test]
    public void RoundTrip_U32_Matches()
    {
        uint original = 0xDEADBEEF;

        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU32(original);
        });

        int offset = HeaderSize + 1;
        uint result = PacketReader.ReadU32(packet, ref offset);

        Assert.AreEqual(original, result);
    }

    [Test]
    public void RoundTrip_U64_Matches()
    {
        ulong original = 0x123456789ABCDEF0;

        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU64(original);
        });

        int offset = HeaderSize + 1;
        ulong result = PacketReader.ReadU64(packet, ref offset);

        Assert.AreEqual(original, result);
    }

    [Test]
    public void RoundTrip_String_Matches()
    {
        string original = "Hello, World!";

        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteString(original);
        });

        int offset = HeaderSize + 1;
        string result = PacketReader.ReadString(packet, ref offset);

        Assert.AreEqual(original, result);
    }

    // ====================================================================
    // Simulate outgoing packets
    // ====================================================================

    [Test]
    public void MovePacket_CorrectFormat()
    {
        byte direction = 2;  // down
        byte facing = 2;

        var packet = PacketWriter.CreatePacket(Opcode.Movement.C_Move_Request, writer =>
        {
            writer.WriteU8(direction);
            writer.WriteU8(facing);
        });

        Assert.AreEqual(7, packet.Length);  // header[4] + opcode[1] + dir[1] + facing[1]
        Assert.AreEqual(Opcode.Movement.C_Move_Request, packet[HeaderSize]);  // opcode at index 4
        Assert.AreEqual(direction, packet[HeaderSize + 1]);
        Assert.AreEqual(facing, packet[HeaderSize + 2]);
    }

    [Test]
    public void Header_ContainsMagicBytes()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer => { });

        Assert.AreEqual(0x11, packet[0]); // Magic byte 1
        Assert.AreEqual(0x68, packet[1]); // Magic byte 2
    }

    [Test]
    public void Header_ContainsCorrectSize()
    {
        var packet = PacketWriter.CreatePacket(0x01, writer =>
        {
            writer.WriteU32(0x12345678); // 4 bytes of payload data
        });

        // Size is big-endian, includes opcode + data = 1 + 4 = 5
        ushort size = (ushort)((packet[2] << 8) | packet[3]);
        Assert.AreEqual(5, size);
    }
}
