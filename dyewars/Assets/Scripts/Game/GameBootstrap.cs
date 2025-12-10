// GameBootstrap.cs
// Bootstrap class that initializes the game systems in the correct order.
// This is the entry point - attach it to a GameObject in your scene.
//
// Services register themselves in Awake(), then resolve dependencies in Start().
// Unity guarantees all Awake() calls complete before any Start() calls.
//
// Scene Setup:
//   Create a "GameBootstrap" GameObject with these components:
//     - GameBootstrap (this script)
//     - NetworkService
//     - PlayerRegistry
//     - PlayerViewFactory
//     - InputService
//     - GridService
//     - DebugOverlay (optional, for debug UI)

using UnityEngine;
using DyeWars.Core;
using DyeWars.Network;
using DyeWars.Player;
using DyeWars.Input;

namespace DyeWars.Game
{
    public class GameBootstrap : MonoBehaviour
    {
        [Header("Debug")]
        [SerializeField] private bool logServiceRegistration = true;

        // Singleton instance
        public static GameBootstrap Instance { get; private set; }

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            // Singleton pattern
            if (Instance != null && Instance != this)
            {
                Debug.LogWarning("GameBootstrap: Duplicate instance destroyed");
                Destroy(gameObject);
                return;
            }
            Instance = this;

            Debug.Log("GameBootstrap: Initializing...");
        }

        private void Start()
        {
            ValidateServices();
            Debug.Log("GameBootstrap: Initialization complete!");
        }

        private void OnDestroy()
        {
            if (Instance == this)
            {
                Instance = null;
                EventBus.ClearAllStats();
                ServiceLocator.Clear();
                Debug.Log("GameBootstrap: Shutdown complete");
            }
        }

        // ====================================================================
        // VALIDATION
        // ====================================================================

        private void ValidateServices()
        {
            bool allValid = true;

            allValid &= ValidateService<INetworkService>("NetworkService");
            allValid &= ValidateService<PlayerRegistry>("PlayerRegistry");
            allValid &= ValidateService<PlayerViewFactory>("PlayerViewFactory");
            allValid &= ValidateService<InputService>("InputService");
            allValid &= ValidateService<GridService>("GridService");

            if (!allValid)
            {
                Debug.LogError("GameBootstrap: Some services are missing! Check that all required components are attached.");
            }
        }

        private bool ValidateService<T>(string serviceName) where T : class
        {
            if (!ServiceLocator.Has<T>())
            {
                Debug.LogError($"GameBootstrap: Missing service - {serviceName}");
                return false;
            }

            if (logServiceRegistration)
            {
                Debug.Log($"GameBootstrap: ✓ {serviceName} registered");
            }

            return true;
        }
    }
}