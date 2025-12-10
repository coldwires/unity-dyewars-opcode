// DebugOverlay.cs
// Displays debug information on screen. Separate from GameBootstrap for clean separation of concerns.
// Attach to the same GameObject as GameBootstrap, or any GameObject in the scene.

using UnityEngine;
using DyeWars.Core;
using DyeWars.Network;
using DyeWars.Player;
using DyeWars.Input;

namespace DyeWars.Game
{
    public class DebugOverlay : MonoBehaviour
    {
        [Header("Settings")]
        [SerializeField] private bool showDebugInfo = true;
        [SerializeField] private KeyCode toggleKey = KeyCode.F1;

        private void Update()
        {
            if (UnityEngine.Input.GetKeyDown(toggleKey))
            {
                showDebugInfo = !showDebugInfo;
            }
        }

        private void OnGUI()
        {
            if (!showDebugInfo) return;

            GUILayout.BeginArea(new Rect(10, 10, 400, 300));
            GUILayout.BeginVertical("box");

            GUILayout.Label("=== DyeWars Debug (F1 to toggle) ===");

            // Network status
            var network = ServiceLocator.Get<INetworkService>();
            var playerRegistry = ServiceLocator.Get<PlayerRegistry>();

            if (network != null)
            {
                GUILayout.Label($"Connected: {network.IsConnected}");
                GUILayout.Label($"Player ID: {playerRegistry?.LocalPlayer?.PlayerId}");

                // Last Sent Packet
                var networkService = network as NetworkService;
                if (networkService != null && networkService.LastSentPacket != null)
                {
                    byte[] packet = networkService.LastSentPacket;
                    string hex = FormatPacketHex(packet);
                    GUILayout.Label($"Last Sent ({packet.Length} bytes):");
                    GUILayout.Label(hex);
                }

                // Last Received Packet
                if (networkService != null && networkService.lastReceivedPacket != null)
                {
                    byte[] packet = networkService.lastReceivedPacket;
                    string hex = FormatPacketHex(packet);
                    GUILayout.Label($"Last Received ({packet.Length} bytes):");
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

            var sb = new System.Text.StringBuilder();
            for (int i = 0; i < packet.Length && i < 20; i++)
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
