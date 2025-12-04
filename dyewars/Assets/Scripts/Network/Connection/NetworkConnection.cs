// NetworkConnection.cs
// Handles raw TCP connection: connect, disconnect, send bytes, receive bytes.
// This class doesn't know what the bytes mean—that's PacketHandler's job.
//
// Single responsibility: manage the socket connection.

using System;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using DyeWars.Network.Protocol;

namespace DyeWars.Network.Connection
{
    public class NetworkConnection
    {
        // Connection state
        private TcpClient client;
        private NetworkStream stream;
        private Thread receiveThread;
        private volatile bool isConnected = false;
        private volatile byte[] lastSentPacket = null;

        // Callback for when data is received (called on background thread!)
        public event Action<byte[]> OnPacketReceived;
        public event Action OnConnected;
        public event Action<string> OnDisconnected;

        // Public state
        public bool IsConnected => isConnected;
        public byte[] LastSentPacket => lastSentPacket;

        // ========================================================================
        // CONNECTION MANAGEMENT
        // ========================================================================

        /// <summary>
        /// Connect to a server. Blocks until connection succeeds or fails.
        /// </summary>
        public void Connect(string host, int port)
        {
            if (isConnected)
            {
                Debug.LogWarning("NetworkConnection: Already connected");
                return;
            }

            try
            {
                Debug.Log($"NetworkConnection: Connecting to {host}:{port}...");

                client = new TcpClient();
                client.Connect(host, port);
                stream = client.GetStream();
                isConnected = true;

                // Start receive thread
                receiveThread = new Thread(ReceiveLoop)
                {
                    IsBackground = true,
                    Name = "NetworkReceiveThread"
                };
                receiveThread.Start();

                Debug.Log("NetworkConnection: Connected!");
                OnConnected?.Invoke();
            }
            catch (Exception e)
            {
                Debug.LogError($"NetworkConnection: Connection failed - {e.Message}");
                OnDisconnected?.Invoke(e.Message);
            }
        }

        /// <summary>
        /// Disconnect from the server.
        /// </summary>
        public void Disconnect()
        {
            if (!isConnected) return;

            isConnected = false;

            stream?.Close();
            stream = null;

            client?.Close();
            client = null;

            // Wait for receive thread to finish
            if (receiveThread != null && receiveThread.IsAlive)
            {
                receiveThread.Join(1000);
            }

            Debug.Log("NetworkConnection: Disconnected");
            OnDisconnected?.Invoke("Disconnected");
        }

        // ========================================================================
        // SENDING DATA
        // ========================================================================

        /// <summary>
        /// Send raw bytes to the server. The packet should already include header.
        /// </summary>
        public void SendRaw(byte[] data)
        {
            if (!isConnected || stream == null)
            {
                Debug.LogWarning("NetworkConnection: Cannot send - not connected");
                return;
            }

            try
            {
                lastSentPacket = data;
                stream.Write(data, 0, data.Length);
                stream.Flush();
            }
            catch (Exception e)
            {
                Debug.LogError($"NetworkConnection: Send failed - {e.Message}");
                isConnected = false;
            }
        }

        // ========================================================================
        // RECEIVING DATA (runs on background thread)
        // ========================================================================

        private void ReceiveLoop()
        {
            byte[] headerBuffer = new byte[PacketHeader.HeaderSize];

            while (isConnected)
            {
                try
                {
                    // Read packet header
                    int bytesRead = stream.Read(headerBuffer, 0, PacketHeader.HeaderSize);
                    if (bytesRead != PacketHeader.HeaderSize)
                    {
                        Debug.LogError("NetworkConnection: Failed to read header");
                        break;
                    }

                    // Validate magic bytes
                    if (!PacketHeader.IsValidMagic(headerBuffer))
                    {
                        Debug.LogError("NetworkConnection: Invalid magic header");
                        break;
                    }

                    // Read payload size
                    ushort payloadSize = PacketHeader.ReadPayloadSize(headerBuffer);

                    if (payloadSize > 0 && payloadSize < 4096)
                    {
                        // Read payload
                        byte[] payload = new byte[payloadSize];
                        bytesRead = stream.Read(payload, 0, payloadSize);

                        if (bytesRead == payloadSize)
                        {
                            // Notify listeners (still on background thread!)
                            OnPacketReceived?.Invoke(payload);
                        }
                    }
                }
                catch (Exception e)
                {
                    if (isConnected)
                    {
                        Debug.LogError($"NetworkConnection: Receive error - {e.Message}");
                    }
                    break;
                }
            }

            isConnected = false;
            OnDisconnected?.Invoke("Connection lost");
        }
    }
}