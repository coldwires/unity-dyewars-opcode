using NUnit.Framework;
using UnityEngine;
using DyeWars.Player;
using DyeWars.Core;

public class PlayerDataTests
{
    // ====================================================================
    // Construction
    // ====================================================================

    [Test]
    public void Constructor_SetsPlayerId()
    {
        var player = new PlayerData(42);
        Assert.AreEqual(42ul, player.PlayerId);
    }

    [Test]
    public void Constructor_DefaultsToRemotePlayer()
    {
        var player = new PlayerData(1);
        Assert.IsFalse(player.IsLocalPlayer);
    }

    [Test]
    public void Constructor_CanSetLocalPlayer()
    {
        var player = new PlayerData(1, isLocalPlayer: true);
        Assert.IsTrue(player.IsLocalPlayer);
    }

    [Test]
    public void Constructor_DefaultPosition_IsZero()
    {
        var player = new PlayerData(1);
        Assert.AreEqual(Vector2Int.zero, player.Position);
    }

    [Test]
    public void Constructor_DefaultFacing_IsDown()
    {
        var player = new PlayerData(1);
        Assert.AreEqual(Direction.Down, player.Facing);
    }

    [Test]
    public void Constructor_IsDirty_IsTrue()
    {
        var player = new PlayerData(1);
        Assert.IsTrue(player.IsDirty);
    }

    // ====================================================================
    // SetPosition
    // ====================================================================

    [Test]
    public void SetPosition_UpdatesPosition()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 10));
        Assert.AreEqual(new Vector2Int(5, 10), player.Position);
    }

    [Test]
    public void SetPosition_UpdatesPreviousPosition()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 10));
        player.SetPosition(new Vector2Int(6, 10));

        Assert.AreEqual(new Vector2Int(5, 10), player.PreviousPosition);
        Assert.AreEqual(new Vector2Int(6, 10), player.Position);
    }

    [Test]
    public void SetPosition_SamePosition_DoesNotMarkDirty()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 10));
        player.IsDirty = false;

        player.SetPosition(new Vector2Int(5, 10)); // Same position

        Assert.IsFalse(player.IsDirty);
    }

    [Test]
    public void SetPosition_DifferentPosition_MarksDirty()
    {
        var player = new PlayerData(1);
        player.IsDirty = false;

        player.SetPosition(new Vector2Int(5, 10));

        Assert.IsTrue(player.IsDirty);
    }

    // ====================================================================
    // SetFacing
    // ====================================================================

    [Test]
    public void SetFacing_UpdatesFacing()
    {
        var player = new PlayerData(1);
        player.SetFacing(Direction.Right);
        Assert.AreEqual(Direction.Right, player.Facing);
    }

    [Test]
    public void SetFacing_SameFacing_DoesNotMarkDirty()
    {
        var player = new PlayerData(1);
        player.SetFacing(Direction.Up);
        player.IsDirty = false;

        player.SetFacing(Direction.Up); // Same facing

        Assert.IsFalse(player.IsDirty);
    }

    [Test]
    public void SetFacing_DifferentFacing_MarksDirty()
    {
        var player = new PlayerData(1);
        player.IsDirty = false;

        player.SetFacing(Direction.Left);

        Assert.IsTrue(player.IsDirty);
    }

    // ====================================================================
    // GetPredictedPosition
    // ====================================================================

    [Test]
    public void GetPredictedPosition_Up_AddsOneY()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 5));

        var predicted = player.GetPredictedPosition(Direction.Up);

        Assert.AreEqual(new Vector2Int(5, 6), predicted);
    }

    [Test]
    public void GetPredictedPosition_Down_SubtractsOneY()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 5));

        var predicted = player.GetPredictedPosition(Direction.Down);

        Assert.AreEqual(new Vector2Int(5, 4), predicted);
    }

    [Test]
    public void GetPredictedPosition_DoesNotModifyActualPosition()
    {
        var player = new PlayerData(1);
        player.SetPosition(new Vector2Int(5, 5));

        player.GetPredictedPosition(Direction.Up);

        Assert.AreEqual(new Vector2Int(5, 5), player.Position); // Unchanged
    }

    // ====================================================================
    // Immutability of Identity
    // ====================================================================

    [Test]
    public void PlayerId_IsImmutable()
    {
        var player = new PlayerData(42);
        // PlayerId has no setter, so this is a compile-time guarantee
        // This test documents the intent
        Assert.AreEqual(42ul, player.PlayerId);
    }

    [Test]
    public void IsLocalPlayer_IsImmutable()
    {
        var player = new PlayerData(1, isLocalPlayer: true);
        // IsLocalPlayer has no setter
        Assert.IsTrue(player.IsLocalPlayer);
    }
}
