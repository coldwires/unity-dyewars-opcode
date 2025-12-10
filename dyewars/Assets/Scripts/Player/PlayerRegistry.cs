// PlayerRegistry.cs
// Centralized storage for all player data (local and remote).
// This is the single source of truth for player state.
//
// Other systems query this registry to get player information.
// Subscribes to EventBus events to update player data.

using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Game;

namespace DyeWars.Player
{
    public class PlayerRegistry : MonoBehaviour
    {
        // All players indexed by their ID
        private readonly Dictionary<ulong, PlayerData> players = new Dictionary<ulong, PlayerData>();

        // Quick reference to local player
        private PlayerData localPlayer;

        // Grid service for bounds validation
        private GridService gridService;

        // Public access
        public PlayerData LocalPlayer => localPlayer;
        public IReadOnlyDictionary<ulong, PlayerData> AllPlayers => players;
        public int PlayerCount => players.Count;

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
        // PUBLIC API - Queries
        // ====================================================================

        public PlayerData GetPlayer(ulong playerId)
        {
            return players.TryGetValue(playerId, out var player) ? player : null;
        }

        public bool TryGetLocalPlayerID(out ulong playerId)
        {
            if (localPlayer != null)
            {
                playerId = localPlayer.PlayerId;
                return true;
            }
            playerId = default;
            return false;
        }

        public bool IsPositionOccupied(Vector2Int position)
        {
            foreach (var player in players.Values)
            {
                if (player.Position == position)
                    return true;
            }
            return false;
        }

        public bool HasPlayer(ulong playerId)
        {
            return players.ContainsKey(playerId);
        }

        public IEnumerable<PlayerData> GetRemotePlayers()
        {
            foreach (var player in players.Values)
            {
                if (!player.IsLocalPlayer)
                    yield return player;
            }
        }

        // ====================================================================
        // PUBLIC API - Local Player Actions (called by LocalPlayerController)
        // ====================================================================

        public void PredictLocalPosition(Vector2Int newPosition)
        {
            if (localPlayer == null) return;
            localPlayer.SetPosition(newPosition);
        }

        public void SetLocalFacing(int facing)
        {
            if (localPlayer == null) return;
            localPlayer.SetFacing(facing);
        }

        // ====================================================================
        // VALIDATION
        // ====================================================================

        /// <summary>
        /// Validates and clamps a position to grid bounds.
        /// Returns true if position was valid, false if it was clamped.
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
            if (localPlayer != null)
            {
                Debug.LogWarning("PlayerRegistry: Local player already exists!");
                return;
            }

            var position = evt.Position;
            ValidatePosition(ref position);

            Debug.Log($"PlayerRegistry: Creating local player with ID {evt.PlayerId} at {position} facing {evt.Facing}");
            localPlayer = new PlayerData(evt.PlayerId, isLocalPlayer: true);
            localPlayer.SetPosition(position);
            localPlayer.SetFacing(evt.Facing);
            players[evt.PlayerId] = localPlayer;
            // Publish for other listeners (UI, etc.)
            EventBus.Publish(new LocalPlayerIdAssignedEvent { PlayerId = evt.PlayerId }, this);
        }

        private void OnLocalPositionCorrected(LocalPlayerPositionCorrectedEvent evt)
        {
            if (localPlayer == null) return;

            var position = evt.Position;
            ValidatePosition(ref position);

            int dx = Mathf.Abs(localPlayer.Position.x - position.x);
            int dy = Mathf.Abs(localPlayer.Position.y - position.y);

            if (dx > 1 || dy > 1)
                Debug.Log($"PlayerRegistry: Large correction, snapping to {position}");
            else if (dx > 0 || dy > 0)
                Debug.Log($"PlayerRegistry: Small correction to {position}");

            localPlayer.SetPosition(position);
        }

        private void OnLocalFacingChanged(LocalPlayerFacingChangedEvent evt)
        {
            if (localPlayer == null) return;
            localPlayer.SetFacing(evt.Facing);
        }

        private void OnPlayerJoined(PlayerJoinedEvent evt)
        {
            if (players.ContainsKey(evt.PlayerId))
            {
                Debug.LogWarning($"PlayerRegistry: Player {evt.PlayerId} already exists");
                return;
            }

            var position = evt.Position;
            ValidatePosition(ref position);

            var player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
            player.SetPosition(position);
            player.SetFacing(evt.Facing);
            players[evt.PlayerId] = player;

            Debug.Log($"PlayerRegistry: Remote player {evt.PlayerId} joined at {position}");
        }

        private void OnRemotePlayerUpdate(RemotePlayerUpdateEvent evt)
        {
            // Skip our own updates
            if (localPlayer != null && evt.PlayerId == localPlayer.PlayerId) return;

            var position = evt.Position;
            ValidatePosition(ref position);

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

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            if (players.Remove(evt.PlayerId))
                Debug.Log("PlayerRegistry: Player " + evt.PlayerId + " removed");
        }
    }
}
