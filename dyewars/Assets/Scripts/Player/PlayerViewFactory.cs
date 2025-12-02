// PlayerViewFactory.cs
// Factory for creating player GameObjects (local and remote).
// Listens to player join/leave events and creates/destroys views accordingly.
//
// Single responsibility: instantiate and destroy player visuals.
// The actual visual logic lives in PlayerView.

using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Game;

namespace DyeWars.Player
{
    public class PlayerViewFactory : MonoBehaviour
    {
        [Header("Prefabs")]
        [SerializeField] private GameObject localPlayerPrefab;
        [SerializeField] private GameObject remotePlayerPrefab;

        // Track created views for cleanup
        private GameObject localPlayerView;
        private readonly Dictionary<uint, GameObject> remotePlayerViews = new Dictionary<uint, GameObject>();

        // Service references
        private PlayerRegistry playerRegistry;
        private GridService gridService;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<PlayerViewFactory>(this);
        }

        private void Start()
        {
            playerRegistry = ServiceLocator.Get<PlayerRegistry>();
            gridService = ServiceLocator.Get<GridService>();
        }

        private void OnEnable()
        {
            EventBus.Subscribe<LocalPlayerIdAssignedEvent>(OnLocalPlayerIdAssigned);
            EventBus.Subscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Subscribe<PlayerLeftEvent>(OnPlayerLeft);
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<LocalPlayerIdAssignedEvent>(OnLocalPlayerIdAssigned);
            EventBus.Unsubscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Unsubscribe<PlayerLeftEvent>(OnPlayerLeft);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<PlayerViewFactory>();
            DestroyAllViews();
        }

        // ====================================================================
        // EVENT HANDLERS
        // ====================================================================

        private void OnLocalPlayerIdAssigned(LocalPlayerIdAssignedEvent evt)
        {
            if (localPlayerView != null)
            {
                Debug.LogWarning("PlayerViewFactory: Local player already exists!");
                return;
            }

            CreateLocalPlayer(evt.PlayerId);
        }

        private void OnPlayerJoined(PlayerJoinedEvent evt)
        {
            // Check if this is the local player (already handled by OnLocalPlayerIdAssigned)
            if (playerRegistry != null &&
                playerRegistry.LocalPlayer != null &&
                evt.PlayerId == playerRegistry.LocalPlayer.PlayerId)
            {
                return;
            }

            // Remote player
            CreateRemotePlayer(evt.PlayerId, evt.Position, evt.Facing);
        }

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            DestroyRemotePlayer(evt.PlayerId);
        }

        // ====================================================================
        // PLAYER CREATION
        // ====================================================================

        private void CreateLocalPlayer(uint playerId)
        {
            if (localPlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Local player prefab not assigned!");
                return;
            }

            // Get initial position
            Vector3 worldPos = Vector3.zero;
            if (playerRegistry?.LocalPlayer != null && gridService != null)
            {
                worldPos = gridService.GridToWorld(playerRegistry.LocalPlayer.Position);
            }

            // Instantiate prefab
            localPlayerView = Instantiate(localPlayerPrefab, worldPos, Quaternion.identity);
            localPlayerView.name = $"LocalPlayer_{playerId}";

            // Initialize the view
            var view = localPlayerView.GetComponent<PlayerView>();
            if (view != null)
            {
                view.InitializeAsLocalPlayer();
            }

            Debug.Log($"PlayerViewFactory: Created local player view for {playerId}");
        }

        private void CreateRemotePlayer(uint playerId, Vector2Int position, int facing)
        {
            if (remotePlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Remote player prefab not assigned!");
                return;
            }

            if (remotePlayerViews.ContainsKey(playerId))
            {
                Debug.LogWarning($"PlayerViewFactory: Remote player {playerId} already exists!");
                return;
            }

            // Calculate world position
            Vector3 worldPos = gridService != null 
                ? gridService.GridToWorld(position) 
                : Vector3.zero;

            // Instantiate prefab
            var playerObj = Instantiate(remotePlayerPrefab, worldPos, Quaternion.identity);
            playerObj.name = $"RemotePlayer_{playerId}";

            // Initialize the view
            var view = playerObj.GetComponent<PlayerView>();
            if (view != null)
            {
                view.InitializeAsRemotePlayer(playerId);
                view.SetFacing(facing);
            }

            remotePlayerViews[playerId] = playerObj;

            Debug.Log($"PlayerViewFactory: Created remote player view for {playerId} at {position}");
        }

        private void DestroyRemotePlayer(uint playerId)
        {
            if (remotePlayerViews.TryGetValue(playerId, out var playerObj))
            {
                Destroy(playerObj);
                remotePlayerViews.Remove(playerId);
                Debug.Log($"PlayerViewFactory: Destroyed remote player view for {playerId}");
            }
        }

        private void DestroyAllViews()
        {
            if (localPlayerView != null)
            {
                Destroy(localPlayerView);
                localPlayerView = null;
            }

            foreach (var kvp in remotePlayerViews)
            {
                if (kvp.Value != null)
                {
                    Destroy(kvp.Value);
                }
            }
            remotePlayerViews.Clear();
        }

        // ====================================================================
        // PUBLIC API
        // ====================================================================

        public GameObject GetLocalPlayerObject()
        {
            return localPlayerView;
        }

        public GameObject GetRemotePlayerObject(uint playerId)
        {
            return remotePlayerViews.TryGetValue(playerId, out var obj) ? obj : null;
        }
    }
}