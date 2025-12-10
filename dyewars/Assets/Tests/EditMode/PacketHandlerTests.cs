using NUnit.Framework;
using UnityEngine;
using DyeWars.Network.Inbound;
using DyeWars.Network.Protocol;
using DyeWars.Core;

public class PacketHandlerTests
{
    private PacketHandler handler;

    [SetUp]
    public void SetUp()
    {
        EventBus.ClearAll();
        handler = new PacketHandler();
    }

    [TearDown]
    public void TearDown()
    {
        EventBus.ClearAll();
    }

    // ====================================================================
    // Welcome Packet (0x10)
    // ====================================================================

    [Test]
    public void ProcessPacket_Welcome_PublishesWelcomeEvent()
    {
        WelcomeReceivedEvent? received = null;
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => received = evt);

        // Build welcome packet: opcode(1) + playerId(8) + x(2) + y(2) + facing(1)
        byte[] packet = new byte[]
        {
            Opcode.LocalPlayer.S_Welcome,                   // 0x10
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, // playerId = 42
            0x00, 0x05,                                     // x = 5
            0x00, 0x0A,                                     // y = 10
            0x02                                            // facing = 2 (down)
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(received, "WelcomeReceivedEvent was not published");
        Assert.AreEqual(42ul, received.Value.PlayerId);
        Assert.AreEqual(new Vector2Int(5, 10), received.Value.Position);
        Assert.AreEqual(Direction.Down, received.Value.Facing);
    }

    [Test]
    public void ProcessPacket_Welcome_TooShort_DoesNotPublish()
    {
        WelcomeReceivedEvent? received = null;
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => received = evt);

        // Incomplete packet - missing facing byte
        byte[] packet = new byte[]
        {
            Opcode.LocalPlayer.S_Welcome,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A,
            0x00, 0x05,
            0x00, 0x0A
            // missing facing
        };

        handler.ProcessPacket(packet);

        Assert.IsNull(received, "Event should not be published for malformed packet");
    }

    // ====================================================================
    // Position Correction (0x11)
    // ====================================================================

    [Test]
    public void ProcessPacket_PositionCorrection_PublishesBothEvents()
    {
        LocalPlayerPositionCorrectedEvent? posEvent = null;
        LocalPlayerFacingChangedEvent? facingEvent = null;
        EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(evt => posEvent = evt);
        EventBus.Subscribe<LocalPlayerFacingChangedEvent>(evt => facingEvent = evt);

        // Position correction: opcode(1) + x(2) + y(2) + facing(1)
        byte[] packet = new byte[]
        {
            Opcode.LocalPlayer.S_Position_Correction,
            0x00, 0x03,  // x = 3
            0x00, 0x07,  // y = 7
            0x01         // facing = 1 (right)
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(posEvent);
        Assert.AreEqual(new Vector2Int(3, 7), posEvent.Value.Position);
        Assert.IsNotNull(facingEvent);
        Assert.AreEqual(Direction.Right, facingEvent.Value.Facing);
    }

    // ====================================================================
    // Facing Correction (0x12)
    // ====================================================================

    [Test]
    public void ProcessPacket_FacingCorrection_PublishesEvent()
    {
        LocalPlayerFacingChangedEvent? received = null;
        EventBus.Subscribe<LocalPlayerFacingChangedEvent>(evt => received = evt);

        // Facing correction: opcode(1) + facing(1)
        byte[] packet = new byte[]
        {
            Opcode.LocalPlayer.S_Facing_Correction,
            0x03  // facing = 3 (left)
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(received);
        Assert.AreEqual(Direction.Left, received.Value.Facing);
    }

    // ====================================================================
    // Player Left (0x26)
    // ====================================================================

    [Test]
    public void ProcessPacket_PlayerLeft_PublishesEvent()
    {
        PlayerLeftEvent? received = null;
        EventBus.Subscribe<PlayerLeftEvent>(evt => received = evt);

        byte[] packet = new byte[]
        {
            Opcode.RemotePlayer.S_Left_Game,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63  // playerId = 99
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(received);
        Assert.AreEqual(99ul, received.Value.PlayerId);
    }

    // ====================================================================
    // Batch Player Spatial (0x25)
    // ====================================================================

    [Test]
    public void ProcessPacket_BatchPlayerSpatial_PublishesMultipleEvents()
    {
        var received = new System.Collections.Generic.List<RemotePlayerUpdateEvent>();
        EventBus.Subscribe<RemotePlayerUpdateEvent>(evt => received.Add(evt));

        // Batch: opcode(1) + count(1) + [playerId(8) + x(2) + y(2) + facing(1)] * count
        byte[] packet = new byte[]
        {
            Opcode.Batch.S_Player_Spatial,
            0x02,  // count = 2 players
            // Player 1
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,  // playerId = 1
            0x00, 0x02,  // x = 2
            0x00, 0x03,  // y = 3
            0x00,        // facing = 0 (up)
            // Player 2
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,  // playerId = 2
            0x00, 0x08,  // x = 8
            0x00, 0x09,  // y = 9
            0x03         // facing = 3 (left)
        };

        handler.ProcessPacket(packet);

        Assert.AreEqual(2, received.Count);
        Assert.AreEqual(1ul, received[0].PlayerId);
        Assert.AreEqual(new Vector2Int(2, 3), received[0].Position);
        Assert.AreEqual(Direction.Up, received[0].Facing);
        Assert.AreEqual(2ul, received[1].PlayerId);
        Assert.AreEqual(new Vector2Int(8, 9), received[1].Position);
        Assert.AreEqual(Direction.Left, received[1].Facing);
    }

    [Test]
    public void ProcessPacket_BatchPlayerSpatial_ZeroCount_NoEvents()
    {
        var received = new System.Collections.Generic.List<RemotePlayerUpdateEvent>();
        EventBus.Subscribe<RemotePlayerUpdateEvent>(evt => received.Add(evt));

        byte[] packet = new byte[]
        {
            Opcode.Batch.S_Player_Spatial,
            0x00  // count = 0
        };

        handler.ProcessPacket(packet);

        Assert.AreEqual(0, received.Count);
    }

    // ====================================================================
    // Server Shutdown (0xF2)
    // ====================================================================

    [Test]
    public void ProcessPacket_ServerShutdown_PublishesDisconnectedEvent()
    {
        DisconnectedFromServerEvent? received = null;
        EventBus.Subscribe<DisconnectedFromServerEvent>(evt => received = evt);

        // Server shutdown: opcode(1) + reason(1)
        byte[] packet = new byte[]
        {
            Opcode.Connection.S_ServerShutdown,
            0x00  // reason code 0 = "Server shutting down"
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(received);
        Assert.AreEqual("Server shutting down", received.Value.Reason);
    }

    [Test]
    public void ProcessPacket_ServerShutdown_Maintenance_PublishesCorrectReason()
    {
        DisconnectedFromServerEvent? received = null;
        EventBus.Subscribe<DisconnectedFromServerEvent>(evt => received = evt);

        byte[] packet = new byte[]
        {
            Opcode.Connection.S_ServerShutdown,
            0x01  // reason code 1 = "Server maintenance"
        };

        handler.ProcessPacket(packet);

        Assert.IsNotNull(received);
        Assert.AreEqual("Server maintenance", received.Value.Reason);
    }

    // ====================================================================
    // Unknown / Edge Cases
    // ====================================================================

    [Test]
    public void ProcessPacket_UnknownOpcode_DoesNotThrow()
    {
        byte[] packet = new byte[] { 0xEE, 0x01, 0x02, 0x03 };

        Assert.DoesNotThrow(() => handler.ProcessPacket(packet));
    }

    [Test]
    public void ProcessPacket_EmptyPacket_DoesNotThrow()
    {
        Assert.DoesNotThrow(() => handler.ProcessPacket(new byte[0]));
    }

    [Test]
    public void ProcessPacket_NullPacket_DoesNotThrow()
    {
        Assert.DoesNotThrow(() => handler.ProcessPacket(null));
    }

    [Test]
    public void ProcessPacket_OpcodeOnly_DoesNotThrow()
    {
        // Just an opcode with no payload
        byte[] packet = new byte[] { Opcode.LocalPlayer.S_Welcome };

        Assert.DoesNotThrow(() => handler.ProcessPacket(packet));
    }
}
