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
// Thread Safety: Uses dataLock for property writes.

using UnityEngine;

namespace DyeWars.Player
{
    public class PlayerData
    {
        // Thread Safety: Protects mutable state. Why lock each property individually?
        // Without this, SetPosition() could be interrupted mid-write by a reader,
        // causing inconsistent state (e.g., Position updated but PreviousPosition stale).
        private readonly object dataLock = new object();

        // Identity (immutable after construction - no lock needed)
        public ulong PlayerId { get; }
        public bool IsLocalPlayer { get; }

        // Position and movement (mutable, protected by dataLock)
        // Private backing fields ensure all access goes through locked getters/setters
        private Vector2Int _position;
        private Vector2Int _previousPosition;
        private int _facing;
        private bool _isMoving;
        private bool _isDirty;

        public Vector2Int Position
        {
            get { lock (dataLock) { return _position; } }
        }

        public Vector2Int PreviousPosition
        {
            get { lock (dataLock) { return _previousPosition; } }
        }

        public int Facing
        {
            get { lock (dataLock) { return _facing; } }
        }

        public bool IsMoving
        {
            get { lock (dataLock) { return _isMoving; } }
            set { lock (dataLock) { _isMoving = value; } }
        }

        public bool IsDirty
        {
            get { lock (dataLock) { return _isDirty; } }
            set { lock (dataLock) { _isDirty = value; } }
        }

        public PlayerData(ulong playerId, bool isLocalPlayer = false)
        {
            PlayerId = playerId;
            IsLocalPlayer = isLocalPlayer;
            _position = Vector2Int.zero;
            _previousPosition = Vector2Int.zero;
            _facing = Core.Direction.Down;
            _isMoving = false;
            _isDirty = true;  // New players need initial visual setup
        }

        /// <summary>
        /// Update position and mark as dirty.
        /// </summary>
        public void SetPosition(Vector2Int newPosition)
        {
            lock (dataLock)
            {
                if (_position != newPosition)
                {
                    _previousPosition = _position;
                    _position = newPosition;
                    _isDirty = true;
                }
            }
        }

        /// <summary>
        /// Update facing and mark as dirty.
        /// </summary>
        public void SetFacing(int newFacing)
        {
            lock (dataLock)
            {
                if (_facing != newFacing)
                {
                    _facing = newFacing;
                    _isDirty = true;
                }
            }
        }

        /// <summary>
        /// Get predicted position after moving in a direction.
        /// Returns a NEW Vector2Int (doesn't modify this object).
        /// </summary>
        public Vector2Int GetPredictedPosition(int direction)
        {
            lock (dataLock)
            {
                return _position + Core.Direction.GetDelta(direction);
            }
        }
    }
}
