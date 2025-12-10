// PlayerViewFactory.cs
// Factory for creating player GameObjects (local and remote).
// Subscribes to EventBus events published by PacketHandler.
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
        private readonly Dictionary<ulong, GameObject> remotePlayerViews = new Dictionary<ulong, GameObject>();

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
            EventBus.Subscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Subscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Subscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Subscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<WelcomeReceivedEvent>(OnWelcomeReceived);
            EventBus.Unsubscribe<PlayerJoinedEvent>(OnPlayerJoined);
            EventBus.Unsubscribe<PlayerLeftEvent>(OnPlayerLeft);
            EventBus.Unsubscribe<RemotePlayerUpdateEvent>(OnRemotePlayerUpdate);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<PlayerViewFactory>();
            DestroyAllViews();
        }

        // ====================================================================
        // EVENT HANDLERS - Subscribed to EventBus events from PacketHandler
        // ====================================================================

        private void OnWelcomeReceived(WelcomeReceivedEvent evt)
        {
            CreateLocalPlayerView(evt.PlayerId, evt.Position, evt.Facing);
        }

        private void OnPlayerJoined(PlayerJoinedEvent evt)
        {
            CreateRemotePlayerView(evt.PlayerId, evt.Position, evt.Facing);
        }

        private void OnPlayerLeft(PlayerLeftEvent evt)
        {
            DestroyPlayerView(evt.PlayerId);
        }

        private void OnRemotePlayerUpdate(RemotePlayerUpdateEvent evt)
        {
            // Skip local player
            if (playerRegistry?.LocalPlayer != null && evt.PlayerId == playerRegistry.LocalPlayer.PlayerId)
                return;

            // Create view if it does not exist
            if (!remotePlayerViews.ContainsKey(evt.PlayerId))
            {
                CreateRemotePlayerView(evt.PlayerId, evt.Position, evt.Facing);
                return;
            }

            // Update existing view
            if (remotePlayerViews.TryGetValue(evt.PlayerId, out var playerObj))
            {
                var view = playerObj.GetComponent<PlayerView>();
                if (view != null)
                {
                    view.UpdateFromServer(evt.Position, evt.Facing);
                }
            }
        }

        // ====================================================================
        // INTERNAL - View creation/destruction logic
        // ====================================================================

        private void CreateLocalPlayerView(ulong playerId, Vector2Int position, int facing)
        {
            if (localPlayerView != null)
            {
                Debug.LogWarning("PlayerViewFactory: Local player already exists!");
                return;
            }

            if (localPlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Local player prefab not assigned!");
                return;
            }

            Vector3 worldPos = gridService != null ? gridService.GridToWorld(position) : Vector3.zero;

            localPlayerView = Instantiate(localPlayerPrefab, worldPos, Quaternion.identity);
            localPlayerView.name = "LocalPlayer_" + playerId;

            var view = localPlayerView.GetComponent<PlayerView>();
            if (view != null)
            {
                view.InitializeAsLocalPlayer();
                view.SetFacing(facing);
            }

            var controller = localPlayerView.GetComponent<LocalPlayerController>();
            if (controller != null)
            {
                controller.Initialize(position, facing);
            }

            Debug.Log($"PlayerViewFactory: Created local player view for {playerId} at {position}");
        }

        private void CreateRemotePlayerView(ulong playerId, Vector2Int position, int facing)
        {
            if (remotePlayerPrefab == null)
            {
                Debug.LogError("PlayerViewFactory: Remote player prefab not assigned!");
                return;
            }

            if (remotePlayerViews.ContainsKey(playerId))
            {
                Debug.LogWarning("PlayerViewFactory: Remote player " + playerId + " already exists!");
                return;
            }

            Vector3 worldPos = gridService != null ? gridService.GridToWorld(position) : Vector3.zero;

            var playerObj = Instantiate(remotePlayerPrefab, worldPos, Quaternion.identity);
            playerObj.name = "RemotePlayer_" + playerId;

            var view = playerObj.GetComponent<PlayerView>();
            if (view != null)
            {
                view.InitializeAsRemotePlayer(playerId);
                view.SetFacing(facing);
            }

            remotePlayerViews[playerId] = playerObj;
            Debug.Log("PlayerViewFactory: Created remote player view for " + playerId + " at " + position);
        }

        private void DestroyPlayerView(ulong playerId)
        {
            if (remotePlayerViews.TryGetValue(playerId, out var playerObj))
            {
                Destroy(playerObj);
                remotePlayerViews.Remove(playerId);
                Debug.Log("PlayerViewFactory: Destroyed remote player view for " + playerId);
            }
        }

        // ====================================================================
        // INTERNAL
        // ====================================================================

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
                    Destroy(kvp.Value);
            }
            remotePlayerViews.Clear();
        }

        public GameObject GetLocalPlayerObject() => localPlayerView;
        public GameObject GetRemotePlayerObject(ulong playerId) => 
            remotePlayerViews.TryGetValue(playerId, out var obj) ? obj : null;
    }
}
