// PlayerRegistry.cs
// Centralized storage for all player data (local and remote).
// This is the single source of truth for player state.
//
// Other systems query this registry to get player information.
// Subscribes to EventBus events to update player data.
//
// Thread Safety: Uses playerLock for dictionary access.

using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Game;

namespace DyeWars.Player
{
    public class PlayerRegistry : MonoBehaviour
    {
        // Thread Safety: Protects players dictionary and localPlayer reference.
        // Event handlers (from EventBus) can fire while other code iterates players.
        private readonly object playerLock = new object();

        // All players indexed by their ID
        private readonly Dictionary<ulong, PlayerData> players = new Dictionary<ulong, PlayerData>();

        // Quick reference to local player
        private PlayerData localPlayer;

        // Grid service for bounds validation
        private GridService gridService;

        // Public access (thread-safe)
        public PlayerData LocalPlayer
        {
            get { lock (playerLock) { return localPlayer; } }
        }

        public int PlayerCount
        {
            get { lock (playerLock) { return players.Count; } }
        }

        // PATTERN: Return a COPY, not the original dictionary.
        // Why? Caller can safely iterate the copy without holding our lock,
        // and won't see ConcurrentModificationException if players join/leave mid-iteration.
        public Dictionary<ulong, PlayerData> GetAllPlayersSnapshot()
        {
            lock (playerLock)
            {
                return new Dictionary<ulong, PlayerData>(players);
            }
        }

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<PlayerRegistry>(this);
        }

        private void Start()
        {
            gridService = ServiceLocator.Get<GridService>();
        }

        private void OnEnable()
        {
            EventBus.Subscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPositionCorrected);
            EventBus.Subscribe<LocalPlayerFacingChangedEvent>(OnLocalFacingChanged);
            EventBus.Subscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Subscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Subscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Unsubscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPositionCorrected);
            EventBus.Unsubscribe<LocalPlayerFacingChangedEvent>(OnLocalFacingChanged);
            EventBus.Unsubscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Unsubscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Unsubscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<PlayerRegistry>();
        }

        // ====================================================================
        // PUBLIC API - Queries (thread-safe)
        // ====================================================================

        public PlayerData GetPlayer(ulong playerId)
        {
            lock (playerLock)
            {
                return players.TryGetValue(playerId, out var player) ? player : null;
            }
        }

        public bool TryGetLocalPlayerID(out ulong playerId)
        {
            lock (playerLock)
            {
                if (localPlayer != null)
                {
                    playerId = localPlayer.PlayerId;
                    return true;
                }
                playerId = default;
                return false;
            }
        }

        public bool IsPositionOccupied(Vector2Int position)
        {
            lock (playerLock)
            {
                foreach (var player in players.Values)
                {
                    if (player.Position == position)
                        return true;
                }
                return false;
            }
        }

        public bool HasPlayer(ulong playerId)
        {
            lock (playerLock)
            {
                return players.ContainsKey(playerId);
            }
        }

        /// <summary>
        /// Returns a snapshot list of remote players (thread-safe).
        /// </summary>
        public List<PlayerData> GetRemotePlayersSnapshot()
        {
            lock (playerLock)
            {
                return players.Values.Where(p => !p.IsLocalPlayer).ToList();
            }
        }

        // ====================================================================
        // PUBLIC API - Local Player Actions (called by LocalPlayerController)
        // ====================================================================

        public void PredictLocalPosition(Vector2Int newPosition)
        {
            lock (playerLock)
            {
                if (localPlayer == null) return;
                localPlayer.SetPosition(newPosition);
            }
        }

        public void SetLocalFacing(int facing)
        {
            lock (playerLock)
            {
                if (localPlayer == null) return;
                localPlayer.SetFacing(facing);
            }
        }

        // ====================================================================
        // VALIDATION
        // ====================================================================

        /// <summary>
        /// Validates and clamps a position to grid bounds.
        /// Returns true if position was valid, false if it was clamped.
        /// Note: Does not require lock as it only reads gridService.
        /// </summary>
        private bool ValidatePosition(ref Vector2Int position)
        {
            if (gridService == null) return true; // Can't validate without grid

            if (gridService.IsInBounds(position)) return true;

            // Clamp to valid bounds
            var clamped = gridService.ClampToBounds(position);
            Debug.LogWarning($"PlayerRegistry: Position {position} out of bounds, clamped to {clamped}");
            position = clamped;
            return false;
        }

        // ====================================================================
        // EVENT HANDLERS - Subscribed to EventBus events from PacketHandler
        // ====================================================================

        private void OnWelcomeReceived(WelcomeReceivedEvent evt)
        {
            var position = evt.Position;
            ValidatePosition(ref position);

            lock (playerLock)
            {
                if (localPlayer != null)
                {
                    Debug.LogWarning("PlayerRegistry: Local player already exists!");
                    return;
                }

                Debug.Log($"PlayerRegistry: Creating local player with ID {evt.PlayerId} at {position} facing {evt.Facing}");
                localPlayer = new PlayerData(evt.PlayerId, isLocalPlayer: true);
                localPlayer.SetPosition(position);
                localPlayer.SetFacing(evt.Facing);
                players[evt.PlayerId] = localPlayer;
            }

            // Publish for other listeners (UI, etc.) - outside lock to prevent deadlock
            EventBus.Publish(new LocalPlayerIdAssignedEvent { PlayerId = evt.PlayerId }, this);
        }

        private void OnLocalPositionCorrected(LocalPlayerPositionCorrectedEvent evt)
        {
            var position = evt.Position;
            ValidatePosition(ref position);

            lock (playerLock)
            {
                if (localPlayer == null) return;

                int dx = Mathf.Abs(localPlayer.Position.x - position.x);
                int dy = Mathf.Abs(localPlayer.Position.y - position.y);

                if (dx > 1 || dy > 1)
                    Debug.Log($"PlayerRegistry: Large correction, snapping to {position}");
                else if (dx > 0 || dy > 0)
                    Debug.Log($"PlayerRegistry: Small correction to {position}");

                localPlayer.SetPosition(position);
            }
        }

        private void OnLocalFacingChanged(LocalPlayerFacingChangedEvent evt)
        {
            lock (playerLock)
            {
                if (localPlayer == null) return;
                localPlayer.SetFacing(evt.Facing);
            }
        }

        private void OnPlayerJoined(PlayerJoinedEvent evt)
        {
            var position = evt.Position;
            ValidatePosition(ref position);

            lock (playerLock)
            {
                if (players.ContainsKey(evt.PlayerId))
                {
                    Debug.LogWarning($"PlayerRegistry: Player {evt.PlayerId} already exists");
                    return;
                }

                var player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
                player.SetPosition(position);
                player.SetFacing(evt.Facing);
                players[evt.PlayerId] = player;

                Debug.Log($"PlayerRegistry: Remote player {evt.PlayerId} joined at {position}");
            }
        }

        private void OnRemotePlayerUpdate(RemotePlayerUpdateEvent evt)
        {
            var position = evt.Position;
            ValidatePosition(ref position);

            lock (playerLock)
            {
                // Skip our own updates
                if (localPlayer != null && evt.PlayerId == localPlayer.PlayerId) return;

                if (!players.TryGetValue(evt.PlayerId, out var player))
                {
                    // New remote player discovered via batch update
                    player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
                    players[evt.PlayerId] = player;
                    Debug.Log($"PlayerRegistry: New remote player {evt.PlayerId} discovered");
                }

                player.SetPosition(position);
                player.SetFacing(evt.Facing);
            }
        }

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            lock (playerLock)
            {
                if (players.Remove(evt.PlayerId))
                    Debug.Log("PlayerRegistry: Player " + evt.PlayerId + " removed");
            }
        }
    }
}
