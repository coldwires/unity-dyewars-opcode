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
using DyeWars.Network.Debugging;

namespace DyeWars.Network.Connection
{
    public class NetworkConnection
    {
        // Connection state
        private TcpClient client;
        private NetworkStream stream;
        private volatile bool isConnected = false;
        private volatile byte[] lastSentPacket = null;

        // Callback for when data is received (called on background thread!)
        public event Action<byte[]> OnPacketReceived;
        public event Action OnConnected;
        public event Action<string> OnDisconnected;

        // Public state
        public bool IsConnected => isConnected;
        public byte[] LastSentPacket => lastSentPacket;

        //Connection timeout in milliseconds
        private const int ConnectionTimeout = 5000;

        //Connection thread for async connect
        private Thread connectionThread;
        private volatile bool isConnecting = false;

        public bool IsConnecting => isConnecting;

        // ========================================================================
        // CONNECTION MANAGEMENT
        // ========================================================================

        /// <summary>
        /// Connect to a server asynchronously. Returns immediately; connection
        /// result will be reported via OnConnected or OnDisconnected events. 
        /// </summary>
        public void Connect(string host, int port)
        {
            if (isConnected)
            {
                Debug.LogWarning("NetworkConnection: Already connected");
                return;
            }

            if (isConnecting)
            {
                Debug.LogWarning("NetworkConnection: Already connecting");
                return;
            }

            isConnecting = true;

            // Run connection in a separate thread to avoid blocking main thread
            connectionThread = new Thread(() => ConnectThreaded(host, port))
            {
                IsBackground = true,
                Name = "NetworkConnectThread"
            };
            connectionThread.Start();
        }

        private void ConnectThreaded(string host, int port)
        {
            try
            {
                Debug.Log($"NetworkConnection: Connecting to {host}:{port}...");

                client = new TcpClient();

                // Use async connect with timeout
                var connectResult = client.BeginConnect(host, port, null, null);
                bool success = connectResult.AsyncWaitHandle.WaitOne(ConnectionTimeout);

                if (!success)
                // Timeout - clean up and report failure
                {
                    client.Close();
                    client = null;
                    isConnecting = false;
                    Debug.LogError("NetworkConnection: Connection timed out");
                    OnDisconnected?.Invoke("Connection timed out");
                    return;
                }

                // Complete the connection
                client.EndConnect(connectResult);
                stream = client.GetStream();
                isConnected = true;
                isConnecting = false;

                Debug.Log("NetworkConnection: Connected!");
                OnConnected?.Invoke();

                // Continue on this thread as the recieve loop
                ReceiveLoop();
            }
            catch (Exception e)
            {
                isConnecting = false;
                client?.Close();
                client = null;
                Debug.LogError($"NetworkConnection: Connection failed - {e.Message}");
                OnDisconnected?.Invoke(e.Message);
            }
        }

        /// <summary>
        /// Disconnect from the server.
        /// </summary>
        public void Disconnect()
        {
            if (!isConnected && !isConnecting) return;

            isConnected = false;
            isConnecting = false;

            // Close socket - this will cause blocking Connect/Read to throw,
            // which makes the thread exit
            stream?.Close();
            stream = null;

            client?.Close();
            client = null;

            // Wait for receive thread to finish
            // (connectThread becomes the receive thread after successful connection)
            if (connectionThread != null && connectionThread.IsAlive)
            {
                connectionThread.Join(1000);
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

                // Track sent packet for debugging (skip header, track payload)
                if (data.Length > PacketHeader.HeaderSize)
                {
                    byte[] payload = new byte[data.Length - PacketHeader.HeaderSize];
                    Array.Copy(data, PacketHeader.HeaderSize, payload, 0, payload.Length);
                    PacketDebugger.TrackSent(payload);
                }

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
                    // Read packet header (must read all bytes - TCP may fragment)
                    if (!ReadExact(headerBuffer, 0, PacketHeader.HeaderSize))
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

                    if (payloadSize > 0 && payloadSize <= PacketHeader.MaxIncomingPayload)
                    {
                        // Read payload (must read all bytes - TCP may fragment)
                        byte[] payload = new byte[payloadSize];
                        if (!ReadExact(payload, 0, payloadSize))
                        {
                            Debug.LogError("NetworkConnection: Failed to read payload");
                            break;
                        }

                        // Notify listeners (still on background thread!)
                        OnPacketReceived?.Invoke(payload);
                    }
                    else if (payloadSize > PacketHeader.MaxIncomingPayload)
                    {
                        Debug.LogError($"NetworkConnection: Payload too large ({payloadSize} bytes, max {PacketHeader.MaxIncomingPayload})");
                        break;
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

        /// <summary>
        /// Read exactly 'count' bytes from the stream. TCP may fragment data,
        /// so a single Read() call might not return all requested bytes.
        /// </summary>
        private bool ReadExact(byte[] buffer, int offset, int count)
        {
            int totalRead = 0;
            while (totalRead < count)
            {
                int bytesRead = stream.Read(buffer, offset + totalRead, count - totalRead);
                if (bytesRead == 0)
                {
                    // Connection closed
                    return false;
                }
                totalRead += bytesRead;
            }
            return true;
        }
    }
}