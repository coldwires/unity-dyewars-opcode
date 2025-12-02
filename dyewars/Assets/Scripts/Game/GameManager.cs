// GameManager.cs
// Bootstrap class that initializes the game systems in the correct order.
// This is the entry point - attach it to a GameObject in your scene.
//
// Initialization order matters! Services must be registered before
// other systems try to use them. GameManager ensures this happens correctly.
//
// Scene Setup:
//   Create a "GameManager" GameObject with these components:
//     - GameManager (this script)
//     - NetworkService
//     - PlayerRegistry
//     - PlayerViewFactory
//     - InputService
//     - GridService

using UnityEngine;
using DyeWars.Core;
using DyeWars.Network;
using DyeWars.Player;
using DyeWars.Input;

namespace DyeWars.Game
{
    public class GameManager : MonoBehaviour
    {
        [Header("Debug")]
        [SerializeField] private bool showDebugInfo = true;

        // Singleton instance
        public static GameManager Instance { get; private set; }

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            // Singleton pattern
            if (Instance != null && Instance != this)
            {
                Debug.LogWarning("GameManager: Duplicate instance destroyed");
                Destroy(gameObject);
                return;
            }
            Instance = this;

            Debug.Log("GameManager: Initializing...");
        }

        private void Start()
        {
            ValidateServices();
            Debug.Log("GameManager: Initialization complete!");
        }

        private void OnDestroy()
        {
            if (Instance == this)
            {
                Instance = null;
                EventBus.Clear();
                ServiceLocator.Clear();
                Debug.Log("GameManager: Shutdown complete");
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
                Debug.LogError("GameManager: Some services are missing! Check that all required components are attached.");
            }
        }

        private bool ValidateService<T>(string serviceName) where T : class
        {
            if (!ServiceLocator.Has<T>())
            {
                Debug.LogError($"GameManager: Missing service - {serviceName}");
                return false;
            }

            if (showDebugInfo)
            {
                Debug.Log($"GameManager: ✓ {serviceName} registered");
            }

            return true;
        }

        // ====================================================================
        // DEBUG UI
        // ====================================================================

        private void OnGUI()
        {
            if (!showDebugInfo) return;

            GUILayout.BeginArea(new Rect(10, 10, 400, 300));
            GUILayout.BeginVertical("box");

            GUILayout.Label("=== DyeWars Debug ===");

            // Network status
            var network = ServiceLocator.Get<INetworkService>();
            if (network != null)
            {
                GUILayout.Label($"Connected: {network.IsConnected}");
                GUILayout.Label($"Player ID: {network.LocalPlayerId}");
                
                //Last Sent Packet
                var networkService = network as NetworkService;
                if (networkService != null && networkService.LastSentPacket != null)
                {
                    byte[] packet = networkService.LastSentPacket;
                    string hex = FormatPacketHex(packet);
                    GUILayout.Label($"Last Sent ({packet.Length} bytes):");
                    GUILayout.Label(hex);
                }
            }

            // Player count
            var registry = ServiceLocator.Get<PlayerRegistry>();
            if (registry != null)
            {
                GUILayout.Label($"Players: {registry.PlayerCount}");

                if (registry.LocalPlayer != null)
                {
                    GUILayout.Label($"Position: {registry.LocalPlayer.Position}");
                    GUILayout.Label($"Facing: {Direction.GetName(registry.LocalPlayer.Facing)}");
                }
            }

            // Input
            var input = ServiceLocator.Get<InputService>();
            if (input != null && input.CurrentDirection != Direction.None)
            {
                GUILayout.Label($"Input: {Direction.GetName(input.CurrentDirection)}");
            }

            GUILayout.EndVertical();
            GUILayout.EndArea();
        }
        
        private string FormatPacketHex(byte[] packet)
        {
            if (packet == null || packet.Length == 0) return "(empty)";
    
            // Format as: "11 68 00 03 01 02 01"
            System.Text.StringBuilder sb = new System.Text.StringBuilder();
            for (int i = 0; i < packet.Length && i < 20; i++)  // Limit to 20 bytes for display
            {
                if (i > 0) sb.Append(" ");
                sb.Append(packet[i].ToString("X2"));
            }
    
            if (packet.Length > 20)
            {
                sb.Append(" ...");
            }
    
            return sb.ToString();
        }
    }
}