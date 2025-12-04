// GameEvents.cs
// All game events defined in one place. This makes it easy to see what events exist
// and what data they carry. Events are structs (immutable-ish, stack-allocated).
//
// Naming convention: [Subject][Verb]Event
// Examples: PlayerMovedEvent, PlayerConnectedEvent, PacketReceivedEvent

using UnityEngine;

namespace DyeWars.Core
{
    // ========================================================================
    // NETWORK EVENTS - Fired when network state changes
    // ========================================================================

    /// <summary>
    /// Fired when successfully connected to the server.
    /// </summary>
    public struct ConnectedToServerEvent { }

    /// <summary>
    /// Fired when disconnected from the server.
    /// </summary>
    public struct DisconnectedFromServerEvent
    {
        public string Reason;
    }

    /// <summary>
    /// Fired when we receive our player ID from the server.
    /// </summary>
    public struct LocalPlayerIdAssignedEvent
    {
        public uint PlayerId;
    }

    // ========================================================================
    // PLAYER EVENTS - Fired when player state changes
    // ========================================================================

    /// <summary>
    /// Fired when a player's position changes (local or remote).
    /// </summary>
    public struct OtherPlayerPositionChangedEvent
    {
        public uint PlayerId;
        public Vector2Int Position;
        public bool IsCorrection;  // True if this is a server correction
    }

    public struct LocalPlayerPositionCorrectedEvent
    {
        public Vector2Int Position;
    }

    public struct LocalPlayerPositionChangedEvent
    {
        public Vector2Int Position;
        public bool IsCorrection;  // True if this is a server correction
    }

    /// <summary>
    /// Fired when a player's facing direction changes.
    /// </summary>
    public struct OtherPlayerFacingChangedEvent
    {
        public uint PlayerId;
        public int Facing;
    }

    public struct LocalPlayerFacingChangedEvent
    {
        public int Facing;
    }

    /// <summary>
    /// Fired when a new player joins (not the local player).
    /// </summary>
    public struct PlayerJoinedEvent
    {
        public uint PlayerId;
        public Vector2Int Position;
        public int Facing;
    }

    /// <summary>
    /// Fired when a player leaves.
    /// </summary>
    public struct PlayerLeftEvent
    {
        public uint PlayerId;
    }

    /// <summary>
    /// Fired when local player starts moving (for animation).
    /// </summary>
    public struct LocalPlayerMoveStartedEvent
    {
        public Vector2Int FromPosition;
        public Vector2Int ToPosition;
        public int Facing;
    }

    /// <summary>
    /// Fired when local player finishes moving (animation complete).
    /// </summary>
    public struct LocalPlayerMoveCompletedEvent
    {
        public Vector2Int Position;
    }

    /// <summary>
    /// Fired when local player turns.
    /// </summary>
    public struct LocalPlayerTurnedEvent
    {
        public int Facing;
        public float TimeSinceKeyRelease;  // For pivot grace logic
    }

    /// <summary>
    /// Fired when a queued direction should be processed.
    /// </summary>
    public struct QueuedDirectionReadyEvent
    {
        public int Direction;
    }

    // ========================================================================
    // INPUT EVENTS - Fired when input changes
    // ========================================================================

    /// <summary>
    /// Fired when player presses a direction key.
    /// </summary>
    public struct DirectionInputEvent
    {
        public int Direction;           // 0=up, 1=right, 2=down, 3=left
        public float TimeSinceRelease;  // Time since any key was released
        public bool IsNewDirection;     // True if direction changed this frame
    }

    // ========================================================================
    // HELPER: Direction constants
    // ========================================================================

    /// <summary>
    /// Direction constants and helper methods.
    /// Immutable: these values never change, they're just definitions.
    /// </summary>
    public static class Direction
    {
        public const int Up = 0;
        public const int Right = 1;
        public const int Down = 2;
        public const int Left = 3;
        public const int None = -1;

        /// <summary>
        /// Get the movement delta for a direction.
        /// Returns a new Vector2Int (doesn't modify anything).
        /// </summary>
        public static Vector2Int GetDelta(int direction)
        {
            return direction switch
            {
                Up => new Vector2Int(0, 1),
                Right => new Vector2Int(1, 0),
                Down => new Vector2Int(0, -1),
                Left => new Vector2Int(-1, 0),
                _ => Vector2Int.zero
            };
        }

        /// <summary>
        /// Get a readable name for a direction.
        /// </summary>
        public static string GetName(int direction)
        {
            return direction switch
            {
                Up => "Up",
                Right => "Right",
                Down => "Down",
                Left => "Left",
                _ => "None"
            };
        }
    }
}