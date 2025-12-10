using NUnit.Framework;
using UnityEngine;
using DyeWars.Core;

/// <summary>
/// Tests EventBus event data structures and pub/sub flow.
/// For full PlayerRegistry behavior tests, see PlayerRegistryPlayTests (Play Mode).
/// </summary>
public class PlayerRegistryTests
{
    [SetUp]
    public void SetUp()
    {
        EventBus.ClearAll();
    }

    [TearDown]
    public void TearDown()
    {
        EventBus.ClearAll();
    }

    // ====================================================================
    // Event Data Integrity Tests
    // ====================================================================

    [Test]
    public void WelcomeReceivedEvent_CarriesAllData()
    {
        var evt = new WelcomeReceivedEvent
        {
            PlayerId = 42,
            Position = new Vector2Int(10, 20),
            Facing = Direction.Left
        };

        Assert.AreEqual(42ul, evt.PlayerId);
        Assert.AreEqual(new Vector2Int(10, 20), evt.Position);
        Assert.AreEqual(Direction.Left, evt.Facing);
    }

    [Test]
    public void RemotePlayerUpdateEvent_CarriesAllData()
    {
        var evt = new RemotePlayerUpdateEvent
        {
            PlayerId = 99,
            Position = new Vector2Int(7, 8),
            Facing = Direction.Up
        };

        Assert.AreEqual(99ul, evt.PlayerId);
        Assert.AreEqual(new Vector2Int(7, 8), evt.Position);
        Assert.AreEqual(Direction.Up, evt.Facing);
    }

    [Test]
    public void PlayerLeftEvent_CarriesPlayerId()
    {
        var evt = new PlayerLeftEvent { PlayerId = 55 };
        Assert.AreEqual(55ul, evt.PlayerId);
    }

    [Test]
    public void LocalPlayerPositionCorrectedEvent_CarriesPosition()
    {
        var evt = new LocalPlayerPositionCorrectedEvent
        {
            Position = new Vector2Int(3, 4)
        };
        Assert.AreEqual(new Vector2Int(3, 4), evt.Position);
    }

    [Test]
    public void LocalPlayerFacingChangedEvent_CarriesFacing()
    {
        var evt = new LocalPlayerFacingChangedEvent { Facing = Direction.Right };
        Assert.AreEqual(Direction.Right, evt.Facing);
    }

    [Test]
    public void LocalPlayerIdAssignedEvent_CarriesPlayerId()
    {
        var evt = new LocalPlayerIdAssignedEvent { PlayerId = 123 };
        Assert.AreEqual(123ul, evt.PlayerId);
    }

    // ====================================================================
    // EventBus Subscription Flow Tests
    // ====================================================================

    [Test]
    public void EventBus_MultipleSubscribers_AllReceive()
    {
        int count = 0;
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => count++);
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => count++);
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => count++);

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1 });

        Assert.AreEqual(3, count);
    }

    [Test]
    public void EventBus_Unsubscribe_StopsReceiving()
    {
        int count = 0;
        System.Action<WelcomeReceivedEvent> handler = evt => count++;

        EventBus.Subscribe(handler);
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1 });
        Assert.AreEqual(1, count);

        EventBus.Unsubscribe(handler);
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 2 });
        Assert.AreEqual(1, count); // Still 1, not 2
    }

    [Test]
    public void EventBus_ClearAll_RemovesAllSubscriptions()
    {
        int count = 0;
        EventBus.Subscribe<WelcomeReceivedEvent>(evt => count++);
        EventBus.Subscribe<PlayerLeftEvent>(evt => count++);

        EventBus.ClearAll();

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1 });
        EventBus.Publish(new PlayerLeftEvent { PlayerId = 1 });

        Assert.AreEqual(0, count);
    }

    [Test]
    public void EventBus_DifferentEventTypes_IndependentSubscriptions()
    {
        int welcomeCount = 0;
        int leftCount = 0;

        EventBus.Subscribe<WelcomeReceivedEvent>(evt => welcomeCount++);
        EventBus.Subscribe<PlayerLeftEvent>(evt => leftCount++);

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1 });
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 2 });
        EventBus.Publish(new PlayerLeftEvent { PlayerId = 3 });

        Assert.AreEqual(2, welcomeCount);
        Assert.AreEqual(1, leftCount);
    }

    // ====================================================================
    // Integration scenario tests (document expected event flow)
    // ====================================================================

    [Test]
    public void Scenario_PlayerJoinsAndLeaves_EventSequence()
    {
        var events = new System.Collections.Generic.List<string>();

        EventBus.Subscribe<WelcomeReceivedEvent>(evt => events.Add("Welcome:" + evt.PlayerId));
        EventBus.Subscribe<RemotePlayerUpdateEvent>(evt => events.Add("RemoteUpdate:" + evt.PlayerId));
        EventBus.Subscribe<PlayerLeftEvent>(evt => events.Add("Left:" + evt.PlayerId));

        // Simulate: local player joins, remote player appears, remote player leaves
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = 2, Position = new Vector2Int(5, 5), Facing = Direction.Right });
        EventBus.Publish(new PlayerLeftEvent { PlayerId = 2 });

        Assert.AreEqual(3, events.Count);
        Assert.AreEqual("Welcome:1", events[0]);
        Assert.AreEqual("RemoteUpdate:2", events[1]);
        Assert.AreEqual("Left:2", events[2]);
    }

    [Test]
    public void Scenario_PositionCorrection_EventSequence()
    {
        var events = new System.Collections.Generic.List<string>();

        EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(evt => events.Add("PosCorrected:" + evt.Position));
        EventBus.Subscribe<LocalPlayerFacingChangedEvent>(evt => events.Add("FacingChanged:" + evt.Facing));

        // Server sends position correction
        EventBus.Publish(new LocalPlayerPositionCorrectedEvent { Position = new Vector2Int(10, 20) });
        EventBus.Publish(new LocalPlayerFacingChangedEvent { Facing = Direction.Up });

        Assert.AreEqual(2, events.Count);
        Assert.IsTrue(events[0].Contains("(10, 20)"));
        Assert.IsTrue(events[1].Contains("Up") || events[1].Contains("0")); // Direction.Up = 0
    }
}
