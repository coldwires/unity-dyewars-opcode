using System.Collections;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;
using DyeWars.Core;
using DyeWars.Player;

public class PlayerRegistryPlayTests
{
    private GameObject registryObject;
    private PlayerRegistry registry;

    [SetUp]
    public void SetUp()
    {
        EventBus.ClearAll();
        ServiceLocator.Clear();

        registryObject = new GameObject("PlayerRegistry");
        registry = registryObject.AddComponent<PlayerRegistry>();
    }

    [TearDown]
    public void TearDown()
    {
        if (registryObject != null)
            Object.Destroy(registryObject);

        EventBus.ClearAll();
        ServiceLocator.Clear();
    }

    // ====================================================================
    // Welcome Event -> Local Player Creation
    // ====================================================================

    [UnityTest]
    public IEnumerator WelcomeReceived_CreatesLocalPlayer()
    {
        yield return null; // Wait one frame for Awake/OnEnable

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 42,
            Position = new Vector2Int(5, 10),
            Facing = Direction.Right
        });

        Assert.IsNotNull(registry.LocalPlayer);
        Assert.AreEqual(42ul, registry.LocalPlayer.PlayerId);
        Assert.IsTrue(registry.LocalPlayer.IsLocalPlayer);
    }

    [UnityTest]
    public IEnumerator WelcomeReceived_SetsPositionAndFacing()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(7, 8),
            Facing = Direction.Left
        });

        Assert.AreEqual(new Vector2Int(7, 8), registry.LocalPlayer.Position);
        Assert.AreEqual(Direction.Left, registry.LocalPlayer.Facing);
    }

    [UnityTest]
    public IEnumerator WelcomeReceived_AddsToAllPlayers()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 99,
            Position = Vector2Int.zero,
            Facing = Direction.Down
        });

        Assert.AreEqual(1, registry.PlayerCount);
        Assert.IsTrue(registry.HasPlayer(99));
    }

    [UnityTest]
    public IEnumerator WelcomeReceived_PublishesLocalPlayerIdAssigned()
    {
        yield return null;

        LocalPlayerIdAssignedEvent? received = null;
        EventBus.Subscribe<LocalPlayerIdAssignedEvent>(evt => received = evt);

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 123,
            Position = Vector2Int.zero,
            Facing = Direction.Down
        });

        Assert.IsNotNull(received);
        Assert.AreEqual(123ul, received.Value.PlayerId);
    }

    [UnityTest]
    public IEnumerator WelcomeReceived_Twice_IgnoresSecond()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 2, Position = Vector2Int.zero, Facing = Direction.Down });

        Assert.AreEqual(1ul, registry.LocalPlayer.PlayerId); // Still first player
        Assert.AreEqual(1, registry.PlayerCount); // Only one player
    }

    // ====================================================================
    // Remote Player Events
    // ====================================================================

    [UnityTest]
    public IEnumerator RemotePlayerUpdate_CreatesNewPlayer()
    {
        yield return null;

        // First create local player
        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });

        // Then remote player update
        EventBus.Publish(new RemotePlayerUpdateEvent
        {
            PlayerId = 50,
            Position = new Vector2Int(3, 4),
            Facing = Direction.Up
        });

        Assert.AreEqual(2, registry.PlayerCount);
        Assert.IsTrue(registry.HasPlayer(50));

        var remote = registry.GetPlayer(50);
        Assert.IsFalse(remote.IsLocalPlayer);
        Assert.AreEqual(new Vector2Int(3, 4), remote.Position);
    }

    [UnityTest]
    public IEnumerator RemotePlayerUpdate_UpdatesExistingPlayer()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = 50, Position = new Vector2Int(0, 0), Facing = Direction.Down });

        // Update same player
        EventBus.Publish(new RemotePlayerUpdateEvent
        {
            PlayerId = 50,
            Position = new Vector2Int(10, 10),
            Facing = Direction.Down
        });

        Assert.AreEqual(2, registry.PlayerCount); // Still 2, not 3
        Assert.AreEqual(new Vector2Int(10, 10), registry.GetPlayer(50).Position);
    }

    [UnityTest]
    public IEnumerator RemotePlayerUpdate_IgnoresLocalPlayerId()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(5, 5),
            Facing = Direction.Up
        });

        // Try to update with local player's ID
        EventBus.Publish(new RemotePlayerUpdateEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(99, 99),
            Facing = Direction.Down
        });

        // Position should NOT be updated (skipped because it's local player)
        Assert.AreEqual(new Vector2Int(5, 5), registry.LocalPlayer.Position);
    }

    [UnityTest]
    public IEnumerator PlayerLeft_RemovesPlayer()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = 50, Position = Vector2Int.zero, Facing = Direction.Down });

        Assert.AreEqual(2, registry.PlayerCount);

        EventBus.Publish(new PlayerLeftEvent { PlayerId = 50 });

        Assert.AreEqual(1, registry.PlayerCount);
        Assert.IsFalse(registry.HasPlayer(50));
    }

    [UnityTest]
    public IEnumerator PlayerLeft_NonExistentPlayer_DoesNotThrow()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });

        // Try to remove a player that doesn't exist
        Assert.DoesNotThrow(() => EventBus.Publish(new PlayerLeftEvent { PlayerId = 999 }));
        Assert.AreEqual(1, registry.PlayerCount);
    }

    // ====================================================================
    // Position Correction Events
    // ====================================================================

    [UnityTest]
    public IEnumerator LocalPositionCorrected_UpdatesPosition()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(0, 0),
            Facing = Direction.Down
        });

        EventBus.Publish(new LocalPlayerPositionCorrectedEvent
        {
            Position = new Vector2Int(10, 20)
        });

        Assert.AreEqual(new Vector2Int(10, 20), registry.LocalPlayer.Position);
    }

    [UnityTest]
    public IEnumerator LocalFacingChanged_UpdatesFacing()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = Vector2Int.zero,
            Facing = Direction.Down
        });

        EventBus.Publish(new LocalPlayerFacingChangedEvent
        {
            Facing = Direction.Right
        });

        Assert.AreEqual(Direction.Right, registry.LocalPlayer.Facing);
    }

    // ====================================================================
    // Query Methods
    // ====================================================================

    [UnityTest]
    public IEnumerator IsPositionOccupied_ReturnsTrue_WhenPlayerAtPosition()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(5, 5),
            Facing = Direction.Down
        });

        Assert.IsTrue(registry.IsPositionOccupied(new Vector2Int(5, 5)));
    }

    [UnityTest]
    public IEnumerator IsPositionOccupied_ReturnsFalse_WhenEmpty()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent
        {
            PlayerId = 1,
            Position = new Vector2Int(5, 5),
            Facing = Direction.Down
        });

        Assert.IsFalse(registry.IsPositionOccupied(new Vector2Int(0, 0)));
    }

    [UnityTest]
    public IEnumerator TryGetLocalPlayerID_ReturnsTrue_WhenExists()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 42, Position = Vector2Int.zero, Facing = Direction.Down });

        bool found = registry.TryGetLocalPlayerID(out ulong id);

        Assert.IsTrue(found);
        Assert.AreEqual(42ul, id);
    }

    [UnityTest]
    public IEnumerator TryGetLocalPlayerID_ReturnsFalse_WhenNoLocalPlayer()
    {
        yield return null;

        bool found = registry.TryGetLocalPlayerID(out ulong id);

        Assert.IsFalse(found);
    }

    [UnityTest]
    public IEnumerator GetPlayer_InvalidId_ReturnsNull()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });

        var player = registry.GetPlayer(999);

        Assert.IsNull(player);
    }

    [UnityTest]
    public IEnumerator GetRemotePlayers_ExcludesLocalPlayer()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = 10, Position = Vector2Int.zero, Facing = Direction.Down });
        EventBus.Publish(new RemotePlayerUpdateEvent { PlayerId = 20, Position = Vector2Int.zero, Facing = Direction.Down });

        int remoteCount = 0;
        foreach (var player in registry.GetRemotePlayers())
        {
            Assert.IsFalse(player.IsLocalPlayer);
            remoteCount++;
        }

        Assert.AreEqual(2, remoteCount);
    }

    // ====================================================================
    // Public API Methods
    // ====================================================================

    [UnityTest]
    public IEnumerator PredictLocalPosition_UpdatesPosition()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = new Vector2Int(0, 0), Facing = Direction.Down });

        registry.PredictLocalPosition(new Vector2Int(1, 0));

        Assert.AreEqual(new Vector2Int(1, 0), registry.LocalPlayer.Position);
    }

    [UnityTest]
    public IEnumerator SetLocalFacing_UpdatesFacing()
    {
        yield return null;

        EventBus.Publish(new WelcomeReceivedEvent { PlayerId = 1, Position = Vector2Int.zero, Facing = Direction.Down });

        registry.SetLocalFacing(Direction.Up);

        Assert.AreEqual(Direction.Up, registry.LocalPlayer.Facing);
    }
}
