// PlayerData.cs
// Pure data class representing a player's state. No Unity dependencies.
// This is the "Model" in MVC - just data, no behavior or visuals.
//
// WHY A CLASS (not a struct)?
// PlayerData is mutable (position, facing change often) and stored in a Dictionary.
// With a struct, retrieving from Dictionary gives a COPY, so modifications
// wouldn't affect the original. With a class, we get a REFERENCE, so
// modifications update the actual stored data.
//
// See the conversation for a detailed explanation of mutable vs immutable,
// and when to use class vs struct.

using UnityEngine;

namespace DyeWars.Player
{
    public class PlayerData
    {
        // Identity (immutable after construction)
        public ulong PlayerId { get; }
        public bool IsLocalPlayer { get; }

        // Position and movement (mutable)
        public Vector2Int Position { get; private set; }
        public Vector2Int PreviousPosition { get; private set; }
        public int Facing { get; private set; }

        // State flags (mutable)
        public bool IsMoving { get; set; }
        public bool IsDirty { get; set; }  // True if state changed, needs visual update

        public PlayerData(ulong playerId, bool isLocalPlayer = false)
        {
            PlayerId = playerId;
            IsLocalPlayer = isLocalPlayer;
            Position = Vector2Int.zero;
            PreviousPosition = Vector2Int.zero;
            Facing = Core.Direction.Down;
            IsMoving = false;
            IsDirty = true;  // New players need initial visual setup
        }

        /// <summary>
        /// Update position and mark as dirty.
        /// </summary>
        public void SetPosition(Vector2Int newPosition)
        {
            if (Position != newPosition)
            {
                PreviousPosition = Position;
                Position = newPosition;
                IsDirty = true;
            }
        }

        /// <summary>
        /// Update facing and mark as dirty.
        /// </summary>
        public void SetFacing(int newFacing)
        {
            if (Facing != newFacing)
            {
                Facing = newFacing;
                IsDirty = true;
            }
        }

        /// <summary>
        /// Get predicted position after moving in a direction.
        /// Returns a NEW Vector2Int (doesn't modify this object).
        /// </summary>
        public Vector2Int GetPredictedPosition(int direction)
        {
            return Position + Core.Direction.GetDelta(direction);
        }
    }
}