// NetworkService.cs
// The main network orchestrator. This MonoBehaviour ties together:
//   - NetworkConnection (raw TCP, runs receive on background thread)
//   - PacketSender (constructs and sends outgoing packets)
//   - PacketHandler (parses incoming packets, publishes events)
//
// Key responsibility: Bridge between background thread and main thread.
// NetworkConnection receives packets on a background thread, but Unity
// and our game systems need to run on the main thread. NetworkService
// queues received packets and processes them in Update().
//
// This is a thin orchestration layer. The actual logic lives in:
//   - PacketSender for outgoing packets
//   - PacketHandler for incoming packets

using System;
using System.Collections.Generic;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Network.Connection;
using DyeWars.Network.Outbound;
using DyeWars.Network.Inbound;
using DyeWars.Network.Protocol;

namespace DyeWars.Network
{
    public class NetworkService : MonoBehaviour, INetworkService
    {
        [Header("Connection Settings")]
        [SerializeField] private string serverHost = "127.0.0.1";
        [SerializeField] private int serverPort = 8080;
        [SerializeField] private bool connectOnStart = true;

        // Sub-components (composition over inheritance)
        private NetworkConnection connection;
        private PacketSender sender;
        private PacketHandler handler;

        // Main thread dispatch queue
        // Packets arrive on background thread, we queue them here,
        // then process them in Update() on the main thread.
        private readonly Queue<byte[]> packetQueue = new Queue<byte[]>();
        private readonly object queueLock = new object();

        // INetworkService implementation
        public bool IsConnected => connection?.IsConnected ?? false;
        public uint LocalPlayerId { get; private set; } = 0;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            // Create sub-components
            connection = new NetworkConnection();
            sender = new PacketSender(connection);
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
            if (connectOnStart)
            {
                Connect(serverHost, serverPort);
            }
        }

        private void Update()
        {
            // Process all queued packets on the main thread
            // This is where background thread data becomes safe to use
            ProcessPacketQueue();
        }

        private void OnDestroy()
        {
            // Unsubscribe from connection events
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
        private void ProcessPacketQueue()
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
            }

            // Process outside the lock (no blocking the background thread)
            if (packetsToProcess != null)
            {
                foreach (var packet in packetsToProcess)
                {
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
        /// We queue the packet for main thread processing.
        /// </summary>
        private void OnPacketReceivedFromBackground(byte[] payload)
        {
            // Check for player ID assignment (we need this for handler)
            // This is safe because we're just reading bytes, not touching Unity
            if (payload.Length >= 5 && payload[0] == Opcode.S_PlayerIdAssignment)
            {
                int offset = 1;
                LocalPlayerId = PacketReader.ReadU32(payload, ref offset);
                handler.SetLocalPlayerId(LocalPlayerId);
            }

            // Queue for main thread processing
            lock (queueLock)
            {
                packetQueue.Enqueue(payload);
            }
        }

        /// <summary>
        /// Called when connected. RUNS ON BACKGROUND THREAD!
        /// </summary>
        private void OnConnectedFromBackground()
        {
            // Queue an action to publish the event on main thread
            lock (queueLock)
            {
                // We could use a separate action queue, but for simplicity
                // we'll publish the event when we process packets.
                // The connected event will naturally fire before any packets.
            }

            Debug.Log("NetworkService: Connected (from background thread)");
        }

        /// <summary>
        /// Called when disconnected. RUNS ON BACKGROUND THREAD!
        /// </summary>
        private void OnDisconnectedFromBackground(string reason)
        {
            Debug.Log($"NetworkService: Disconnected - {reason} (from background thread)");
        }

        // ====================================================================
        // INetworkService IMPLEMENTATION
        // ====================================================================

        public void Connect(string host, int port)
        {
            connection.Connect(host, port);

            // Publish connected event (we're on main thread here if called from Start)
            if (connection.IsConnected)
            {
                EventBus.Publish(new ConnectedToServerEvent());
            }
        }

        public void Disconnect()
        {
            connection?.Disconnect();
            EventBus.Publish(new DisconnectedFromServerEvent { Reason = "Disconnected" });
        }

        /// <summary>
        /// Send a move command. Delegates to PacketSender.
        /// </summary>
        public void SendMove(int direction, int facing)
        {
            sender.SendMove(direction, facing);
        }

        /// <summary>
        /// Send a turn command. Delegates to PacketSender.
        /// </summary>
        public void SendTurn(int direction)
        {
            sender.SendTurn(direction);
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

        /// <summary>
        /// Send a ping to measure latency.
        /// </summary>
        public void SendPing() => sender.SendPing();

        /// <summary>
        /// Send an attack command.
        /// </summary>
        public void SendAttack() => sender.SendAttack();

        /// <summary>
        /// Send a chat message.
        /// </summary>
        public void SendChatMessage(byte channelId, string message)
        {
            sender.SendChatMessage(channelId, message);
        }
    }
}