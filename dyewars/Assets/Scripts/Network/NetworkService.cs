// NetworkService.cs
// The main network orchestrator. This MonoBehaviour ties together:
//   - NetworkConnection (raw TCP, runs receive on background thread)
//   - PacketSender (constructs and sends outgoing packets)
//   - PacketHandler (parses incoming packets, publishes events via EventBus)
//
// Key responsibility: Bridge between background thread and main thread.
// NetworkConnection receives packets on a background thread, but Unity
// and our game systems need to run on the main thread. NetworkService
// queues received packets and processes them in Update().

using System;
using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Network.Connection;
using DyeWars.Network.Outbound;
using DyeWars.Network.Inbound;
using DyeWars.Network.Protocol;
using DyeWars.Network.Debugging;

namespace DyeWars.Network
{
    public class NetworkService : MonoBehaviour, INetworkService
    {
        [Header("Connection Settings")]
        [SerializeField] private string serverHost = "127.0.0.1";
        [SerializeField] private int serverPort = 8080;
        [SerializeField] private bool connectOnStart = false;

        // Thread Safety: Background thread sets pendingConnectionEvent, main thread reads and clears it.
        // This is safer than volatile alone because we need atomic "check AND clear" operation.
        // Without lock: Thread A checks (true), Thread B checks (true), both publish event = duplicate!
        private readonly object connectionEventLock = new object();
        private bool pendingConnectionEvent = false;

        // Sub-components (composition over inheritance)
        private NetworkConnection connection;
        private PacketSender sender;
        public PacketSender Sender => sender;
        private PacketHandler handler;

        // Main thread dispatch queue
        // Packets arrive on background thread, we queue them here,
        // then process them in Update() on the main thread.
        private readonly Queue<byte[]> packetQueue = new Queue<byte[]>();
        private readonly object queueLock = new object();

        // INetworkService implementation
        public bool IsConnected => connection?.IsConnected ?? false;
        public byte[] LastSentPacket => connection?.LastSentPacket;
        public volatile byte[] lastReceivedPacket;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            // Create connection and sender immediately
            connection = new NetworkConnection();
            sender = new PacketSender(connection);

            // Create PacketHandler in Awake to ensure it's ready before any packets arrive
            handler = new PacketHandler();

            // Subscribe to connection events (these fire on background thread!)
            connection.OnPacketReceived += OnPacketReceivedFromBackground;
            connection.OnConnected += OnConnectedFromBackground;
            connection.OnDisconnected += OnDisconnectedFromBackground;

            // Register this service so other systems can find it
            ServiceLocator.Register<INetworkService>(this);
        }

        private void Start()
        {
            // Only auto-connect in Editor; builds always show connection dialog
            if (connectOnStart && Application.isEditor)
            {
                Connect(serverHost, serverPort);
            }
        }

        private void Update()
        {
            // Check for pending connection event (thread-safe)
            bool shouldPublishConnected = false;
            lock (connectionEventLock)
            {
                if (pendingConnectionEvent)
                {
                    shouldPublishConnected = true;
                    pendingConnectionEvent = false;
                }
            }

            if (shouldPublishConnected)
            {
                EventBus.Publish(new ConnectedToServerEvent(), this);
            }

            ProcessQueues();
        }

        private void OnDestroy()
        {
            if (connection != null)
            {
                connection.OnPacketReceived -= OnPacketReceivedFromBackground;
                connection.OnConnected -= OnConnectedFromBackground;
                connection.OnDisconnected -= OnDisconnectedFromBackground;
            }

            Disconnect();
            ServiceLocator.Unregister<INetworkService>();
        }

        // ====================================================================
        // MAIN THREAD PACKET PROCESSING
        // ====================================================================

        /// <summary>
        /// Process all packets that were queued from the background thread.
        /// This runs on the main thread in Update(), making it safe to:
        ///   - Access Unity objects
        ///   - Publish events
        ///   - Modify game state
        /// </summary>
        private void ProcessQueues()
        {
            // Lock briefly to grab all pending packets
            // We copy to a local list to minimize lock time
            List<byte[]> packetsToProcess = null;

            lock (queueLock)
            {
                if (packetQueue.Count > 0)
                {
                    packetsToProcess = new List<byte[]>(packetQueue);
                    packetQueue.Clear();
                }
            } // Lock released

            if (packetsToProcess != null)
            {
                foreach (var packet in packetsToProcess)
                {
                    lastReceivedPacket = packet;
                    PacketDebugger.TrackReceived(packet);
                    handler.ProcessPacket(packet);
                }
            }
        }

        // ====================================================================
        // BACKGROUND THREAD CALLBACKS
        // ====================================================================

        // IMPORTANT: These methods are called from the background thread!
        // Do NOT access Unity objects or game state here.
        // Only queue data for main thread processing.

        /// <summary>
        /// Called when a packet is received. RUNS ON BACKGROUND THREAD!
        /// We clone the packet and queue it for main thread processing.
        /// Cloning prevents race conditions if the network layer reuses buffers.
        /// </summary>
        private void OnPacketReceivedFromBackground(byte[] payload)
        {
            // Clone the packet to prevent race conditions
            byte[] cloned = new byte[payload.Length];
            System.Array.Copy(payload, cloned, payload.Length);

            lock (queueLock)
            {
                packetQueue.Enqueue(cloned);
            }
        }

        /// <summary>
        /// Called when connected to server. RUNS ON BACKGROUND THREAD!
        /// </summary>
        private void OnConnectedFromBackground()
        {
            // Send handshake packet immediately
            // This is safe because SendHandshake only queues data to send, no Unity access
            sender.SendHandshake();

            // Signal main thread to publish connection event
            lock (connectionEventLock)
            {
                pendingConnectionEvent = true;
            }

            Debug.Log("NetworkService: Connected (from background thread)");
        }

        /// <summary>
        /// Called when disconnected from server. RUNS ON BACKGROUND THREAD!
        /// </summary>
        private void OnDisconnectedFromBackground(string reason)
        {
            Debug.Log("NetworkService: Disconnected - " + reason + " (from background thread)");
        }

        // ====================================================================
        // INetworkService IMPLEMENTATION
        // ====================================================================

        public void Connect(string host, int port)
        {
            connection.Connect(host, port);
        }

        public void Disconnect()
        {
            connection?.Disconnect();
            EventBus.Publish(new DisconnectedFromServerEvent { Reason = "Disconnected" }, this);
        }

        // ====================================================================
        // EXTENDED API (beyond INetworkService)
        // ====================================================================
        // These methods expose PacketSender functionality for systems
        // that need to send other packet types.

        /// <summary>
        /// Get the packet sender for sending custom packets.
        /// </summary>
        public PacketSender GetSender() => sender;
    }
}
