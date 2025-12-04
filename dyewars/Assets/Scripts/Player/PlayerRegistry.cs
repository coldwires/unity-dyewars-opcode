// PlayerRegistry.cs
// Centralized storage for all player data (local and remote).
// This is the single source of truth for player state.
//
// Other systems query this registry to get player information.
// The registry listens to network events and updates player data accordingly.

using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;

namespace DyeWars.Player
{
    public class PlayerRegistry : MonoBehaviour
    {
        // All players indexed by their ID
        private readonly Dictionary<uint, PlayerData> players = new Dictionary<uint, PlayerData>();

        // Quick reference to local player
        private PlayerData localPlayer;

        // Public access
        public PlayerData LocalPlayer => localPlayer;
        public IReadOnlyDictionary<uint, PlayerData> AllPlayers => players;
        public int PlayerCount => players.Count;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<PlayerRegistry>(this);
        }

        private void OnEnable()
        {
            EventBus.Subscribe<LocalPlayerIdAssignedEvent>(OnLocalPlayerIdAssigned);
            EventBus.Subscribe<OtherPlayerPositionChangedEvent>(OnOtherPlayerPositionChanged);
            EventBus.Subscribe<OtherPlayerFacingChangedEvent>(OnOtherPlayerFacingChanged);
            EventBus.Subscribe<PlayerLeftEvent>(OnPlayerLeft);

            EventBus.Subscribe<LocalPlayerFacingChangedEvent>(OnLocalPlayerFacingChanged);
            EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPlayerPositionCorrected);

        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<LocalPlayerIdAssignedEvent>(OnLocalPlayerIdAssigned);
            EventBus.Unsubscribe<OtherPlayerPositionChangedEvent>(OnOtherPlayerPositionChanged);
            EventBus.Unsubscribe<OtherPlayerFacingChangedEvent>(OnOtherPlayerFacingChanged);
            EventBus.Unsubscribe<PlayerLeftEvent>(OnPlayerLeft);

            EventBus.Unsubscribe<LocalPlayerFacingChangedEvent>(OnLocalPlayerFacingChanged);
            EventBus.Unsubscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPlayerPositionCorrected);

        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<PlayerRegistry>();
        }

        // ====================================================================
        // PUBLIC API
        // ====================================================================

        public PlayerData GetPlayer(uint playerId)
        {
            return players.TryGetValue(playerId, out var player) ? player : null;
        }

        public bool TryGetLocalPlayerID(out uint playerId)
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
                {
                    return true;
                }
            }

            return false;
        }

        public bool HasPlayer(uint playerId)
        {
            return players.ContainsKey(playerId);
        }

        public IEnumerable<PlayerData> GetRemotePlayers()
        {
            foreach (var player in players.Values)
            {
                if (!player.IsLocalPlayer)
                {
                    yield return player;
                }
            }
        }

        // ====================================================================
        // LOCAL PLAYER ACTIONS
        // ====================================================================

        public void PredictLocalPosition(Vector2Int newPosition)
        {
            if (localPlayer == null) return;

            localPlayer.SetPosition(newPosition);

            EventBus.Publish(new LocalPlayerPositionChangedEvent()
            {
                Position = newPosition,
                IsCorrection = false
            });
        }

        public void SetLocalFacing(int facing)
        {
            if (localPlayer == null) return;

            localPlayer.SetFacing(facing);

            EventBus.Publish(new LocalPlayerFacingChangedEvent()
            {
                Facing = facing,
            });
        }

        // ====================================================================
        // EVENT HANDLERS
        // ====================================================================

        private void OnLocalPlayerIdAssigned(LocalPlayerIdAssignedEvent evt)
        {
            Debug.Log($"PlayerRegistry: Creating local player with ID {evt.PlayerId}");
            localPlayer = new PlayerData(evt.PlayerId, isLocalPlayer: true);
            players[evt.PlayerId] = localPlayer;

            EventBus.Publish(new PlayerJoinedEvent
            {
                PlayerId = evt.PlayerId,
                Position = localPlayer.Position,
                Facing = localPlayer.Facing
            });
        }

        private void OnLocalPlayerPositionCorrected(LocalPlayerPositionCorrectedEvent evt)
        {
            if (localPlayer == null) return;

            int dx = Mathf.Abs(localPlayer.Position.x - evt.Position.x);
            int dy = Mathf.Abs(localPlayer.Position.y - evt.Position.y);

            if (dx > 1 || dy > 1)
            {
                Debug.Log($"PlayerRegistry: Large correction, snapping to {evt.Position}");
            }
            else if (dx > 0 || dy > 0)
            {
                Debug.Log($"PlayerRegistry: Small correction to {evt.Position}");
            }

            localPlayer.SetPosition(evt.Position);

            EventBus.Publish(new LocalPlayerPositionChangedEvent
            {
                Position = evt.Position,
                IsCorrection = true
            });
        }

        private void OnLocalPlayerFacingChanged(LocalPlayerFacingChangedEvent evt)
        {
            localPlayer?.SetFacing(evt.Facing);
        }

        private void OnOtherPlayerPositionChanged(OtherPlayerPositionChangedEvent evt)
        {
            if (!players.TryGetValue(evt.PlayerId, out var player))
            {
                // New remote player discovered
                player = new PlayerData(evt.PlayerId, isLocalPlayer: false);
                players[evt.PlayerId] = player;

                Debug.Log($"PlayerRegistry: New remote player {evt.PlayerId}");

                EventBus.Publish(new PlayerJoinedEvent
                {
                    PlayerId = evt.PlayerId,
                    Position = evt.Position,
                    Facing = Direction.Down
                });
            }

            player.SetPosition(evt.Position);
        }

        private void OnOtherPlayerFacingChanged(OtherPlayerFacingChangedEvent evt)
        {
            if (players.TryGetValue(evt.PlayerId, out var player))
            {
                player.SetFacing(evt.Facing);
            }
        }

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            if (players.Remove(evt.PlayerId))
            {
                Debug.Log($"PlayerRegistry: Player {evt.PlayerId} removed");
            }
        }
    }
}