using NUnit.Framework;
using DyeWars.Network.Protocol;

public class PacketReaderTests
{
    // ====================================================================
    // HasBytes - bounds checking
    // ====================================================================

    [Test]
    public void HasBytes_ExactLength_ReturnsTrue()
    {
        byte[] data = new byte[10];
        Assert.IsTrue(PacketReader.HasBytes(data, 0, 10));
    }

    [Test]
    public void HasBytes_ZeroBytes_ReturnsTrue()
    {
        byte[] data = new byte[5];
        Assert.IsTrue(PacketReader.HasBytes(data, 0, 0));
    }

    [Test]
    public void HasBytes_TooShort_ReturnsFalse()
    {
        byte[] data = new byte[5];
        Assert.IsFalse(PacketReader.HasBytes(data, 0, 10));
    }

    [Test]
    public void HasBytes_WithOffset_ReturnsFalse()
    {
        byte[] data = new byte[10];
        Assert.IsFalse(PacketReader.HasBytes(data, 5, 10)); // Only 5 bytes left
    }

    [Test]
    public void HasBytes_EmptyArray_ReturnsFalse()
    {
        byte[] data = new byte[0];
        Assert.IsFalse(PacketReader.HasBytes(data, 0, 1));
    }

    [Test]
    public void HasBytes_EmptyArray_ZeroBytes_ReturnsTrue()
    {
        byte[] data = new byte[0];
        Assert.IsTrue(PacketReader.HasBytes(data, 0, 0));
    }

    // ====================================================================
    // ReadU8 - single byte
    // ====================================================================

    [Test]
    public void ReadU8_ReadsCorrectValue()
    {
        byte[] data = new byte[] { 0x42 };
        int offset = 0;

        byte result = PacketReader.ReadU8(data, ref offset);

        Assert.AreEqual(0x42, result);
        Assert.AreEqual(1, offset);
    }

    [Test]
    public void ReadU8_AdvancesOffset()
    {
        byte[] data = new byte[] { 0x01, 0x02, 0x03 };
        int offset = 0;

        PacketReader.ReadU8(data, ref offset);
        PacketReader.ReadU8(data, ref offset);
        byte third = PacketReader.ReadU8(data, ref offset);

        Assert.AreEqual(0x03, third);
        Assert.AreEqual(3, offset);
    }

    // ====================================================================
    // ReadU16 - big-endian 16-bit
    // ====================================================================

    [Test]
    public void ReadU16_BigEndian_ParsesCorrectly()
    {
        // 0x1234 in big-endian = [0x12, 0x34]
        byte[] data = new byte[] { 0x12, 0x34 };
        int offset = 0;

        ushort result = PacketReader.ReadU16(data, ref offset);

        Assert.AreEqual(0x1234, result);
        Assert.AreEqual(2, offset);
    }

    [Test]
    public void ReadU16_MaxValue_ParsesCorrectly()
    {
        byte[] data = new byte[] { 0xFF, 0xFF };
        int offset = 0;

        ushort result = PacketReader.ReadU16(data, ref offset);

        Assert.AreEqual(ushort.MaxValue, result);
    }

    [Test]
    public void ReadU16_Zero_ParsesCorrectly()
    {
        byte[] data = new byte[] { 0x00, 0x00 };
        int offset = 0;

        ushort result = PacketReader.ReadU16(data, ref offset);

        Assert.AreEqual(0, result);
    }

    // ====================================================================
    // ReadU32 - big-endian 32-bit
    // ====================================================================

    [Test]
    public void ReadU32_BigEndian_ParsesCorrectly()
    {
        // 0x12345678 in big-endian = [0x12, 0x34, 0x56, 0x78]
        byte[] data = new byte[] { 0x12, 0x34, 0x56, 0x78 };
        int offset = 0;

        uint result = PacketReader.ReadU32(data, ref offset);

        Assert.AreEqual(0x12345678u, result);
        Assert.AreEqual(4, offset);
    }

    [Test]
    public void ReadU32_MaxValue_ParsesCorrectly()
    {
        byte[] data = new byte[] { 0xFF, 0xFF, 0xFF, 0xFF };
        int offset = 0;

        uint result = PacketReader.ReadU32(data, ref offset);

        Assert.AreEqual(uint.MaxValue, result);
    }

    // ====================================================================
    // ReadU64 - big-endian 64-bit (player IDs)
    // ====================================================================

    [Test]
    public void ReadU64_BigEndian_ParsesCorrectly()
    {
        // Player ID example
        byte[] data = new byte[] { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A }; // 42
        int offset = 0;

        ulong result = PacketReader.ReadU64(data, ref offset);

        Assert.AreEqual(42ul, result);
        Assert.AreEqual(8, offset);
    }

    [Test]
    public void ReadU64_LargeValue_ParsesCorrectly()
    {
        // 0x0102030405060708
        byte[] data = new byte[] { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
        int offset = 0;

        ulong result = PacketReader.ReadU64(data, ref offset);

        Assert.AreEqual(0x0102030405060708ul, result);
    }

    [Test]
    public void ReadU64_MaxValue_ParsesCorrectly()
    {
        byte[] data = new byte[] { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
        int offset = 0;

        ulong result = PacketReader.ReadU64(data, ref offset);

        Assert.AreEqual(ulong.MaxValue, result);
    }

    // ====================================================================
    // ReadString - length-prefixed UTF-8
    // ====================================================================

    [Test]
    public void ReadString_SimpleString_ParsesCorrectly()
    {
        // Length-prefixed: [length:1][utf8 bytes...]
        byte[] data = new byte[] { 0x05, 0x48, 0x65, 0x6C, 0x6C, 0x6F }; // "Hello"
        int offset = 0;

        string result = PacketReader.ReadString(data, ref offset);

        Assert.AreEqual("Hello", result);
        Assert.AreEqual(6, offset); // 1 length byte + 5 chars
    }

    [Test]
    public void ReadString_EmptyString_ReturnsEmpty()
    {
        byte[] data = new byte[] { 0x00 }; // Length 0
        int offset = 0;

        string result = PacketReader.ReadString(data, ref offset);

        Assert.AreEqual("", result);
        Assert.AreEqual(1, offset);
    }

    [Test]
    public void ReadString_TruncatedData_ReturnsNull()
    {
        // Says length is 10, but only has 3 bytes
        byte[] data = new byte[] { 0x0A, 0x48, 0x65, 0x6C };
        int offset = 0;

        string result = PacketReader.ReadString(data, ref offset);

        Assert.IsNull(result);
    }

    // ====================================================================
    // Sequential reads - simulating packet parsing
    // ====================================================================

    [Test]
    public void SequentialReads_WelcomePacket_ParsesCorrectly()
    {
        // Simulate welcome packet: playerId(8) + x(2) + y(2) + facing(1)
        byte[] packet = new byte[]
        {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, // playerId = 1
            0x00, 0x05,                                     // x = 5
            0x00, 0x0A,                                     // y = 10
            0x02                                            // facing = 2 (down)
        };
        int offset = 0;

        ulong playerId = PacketReader.ReadU64(packet, ref offset);
        ushort x = PacketReader.ReadU16(packet, ref offset);
        ushort y = PacketReader.ReadU16(packet, ref offset);
        byte facing = PacketReader.ReadU8(packet, ref offset);

        Assert.AreEqual(1ul, playerId);
        Assert.AreEqual(5, x);
        Assert.AreEqual(10, y);
        Assert.AreEqual(2, facing);
        Assert.AreEqual(13, offset); // Total bytes read
    }

    [Test]
    public void SequentialReads_WithOffset_StartsAtCorrectPosition()
    {
        byte[] data = new byte[] { 0xFF, 0xFF, 0x00, 0x2A }; // Skip first 2 bytes
        int offset = 2;

        ushort result = PacketReader.ReadU16(data, ref offset);

        Assert.AreEqual(42, result); // 0x002A = 42
        Assert.AreEqual(4, offset);
    }
}
