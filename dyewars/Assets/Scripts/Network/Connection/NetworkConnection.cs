// NetworkConnection.cs
// Handles raw TCP connection: connect, disconnect, send bytes, receive bytes.
// This class doesn't know what the bytes mean—that's PacketHandler's job.
//
// Single responsibility: manage the socket connection.
//
// Thread Safety: Uses connectionLock for state transitions and stream access.
// Shutdown is coordinated via shouldShutdown flag to prevent duplicate events.

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
        // Thread Safety: Single enum state instead of 3 booleans.
        // Prevents invalid combinations like (isConnected=true && isConnecting=true).
        private enum ConnectionState { Disconnected, Connecting, Connected, Disconnecting }

        // Thread Safety: Protects all connection state (client, stream, state).
        // Main thread calls Connect/Disconnect/SendRaw, background thread runs ReceiveLoop.
        // Without this lock, Disconnect() could close the stream while ReceiveLoop reads from it.
        private readonly object connectionLock = new object();

        // Connection state (protected by connectionLock)
        private ConnectionState state = ConnectionState.Disconnected;
        private TcpClient client;
        private NetworkStream stream;
        private byte[] lastSentPacket = null;

        // Callback for when data is received (called on background thread!)
        public event Action<byte[]> OnPacketReceived;
        public event Action OnConnected;
        public event Action<string> OnDisconnected;

        // Public state (thread-safe reads via enum comparison)
        public bool IsConnected
        {
            get { lock (connectionLock) { return state == ConnectionState.Connected; } }
        }

        public bool IsConnecting
        {
            get { lock (connectionLock) { return state == ConnectionState.Connecting; } }
        }

        public byte[] LastSentPacket
        {
            get { lock (connectionLock) { return lastSentPacket; } }
        }

        // Connection timeout in milliseconds
        private const int ConnectionTimeout = 5000;

        // Connection thread for async connect
        private Thread connectionThread;

        // ========================================================================
        // CONNECTION MANAGEMENT
        // ========================================================================

        /// <summary>
        /// Connect to a server asynchronously. Returns immediately; connection
        /// result will be reported via OnConnected or OnDisconnected events.
        /// </summary>
        public void Connect(string host, int port)
        {
            lock (connectionLock)
            {
                if (state == ConnectionState.Connected)
                {
                    Debug.LogWarning("NetworkConnection: Already connected");
                    return;
                }

                if (state == ConnectionState.Connecting)
                {
                    Debug.LogWarning("NetworkConnection: Already connecting");
                    return;
                }

                state = ConnectionState.Connecting;
            }

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

                var newClient = new TcpClient();

                // Use async connect with timeout
                var connectResult = newClient.BeginConnect(host, port, null, null);
                bool success = connectResult.AsyncWaitHandle.WaitOne(ConnectionTimeout);

                if (!success)
                {
                    // Timeout - clean up and report failure
                    newClient.Close();
                    lock (connectionLock)
                    {
                        state = ConnectionState.Disconnected;
                    }
                    Debug.LogError("NetworkConnection: Connection timed out");
                    OnDisconnected?.Invoke("Connection timed out");
                    return;
                }

                // Check if disconnect was requested during connection attempt
                lock (connectionLock)
                {
                    if (state == ConnectionState.Disconnecting)
                    {
                        newClient.Close();
                        state = ConnectionState.Disconnected;
                        return;
                    }
                }

                // Complete the connection
                newClient.EndConnect(connectResult);
                var newStream = newClient.GetStream();

                lock (connectionLock)
                {
                    client = newClient;
                    stream = newStream;
                    state = ConnectionState.Connected;
                }

                Debug.Log("NetworkConnection: Connected!");
                OnConnected?.Invoke();

                // Continue on this thread as the receive loop
                ReceiveLoop();
            }
            catch (Exception e)
            {
                lock (connectionLock)
                {
                    state = ConnectionState.Disconnected;
                    client?.Close();
                    client = null;
                    stream = null;
                }
                Debug.LogError($"NetworkConnection: Connection failed - {e.Message}");
                OnDisconnected?.Invoke(e.Message);
            }
        }

        /// <summary>
        /// Disconnect from the server.
        /// </summary>
        public void Disconnect()
        {
            bool wasConnected = false;

            lock (connectionLock)
            {
                // Only disconnect if we're connected or connecting
                if (state == ConnectionState.Disconnected || state == ConnectionState.Disconnecting)
                    return;

                wasConnected = (state == ConnectionState.Connected);
                state = ConnectionState.Disconnecting;

                // Close socket - this will cause blocking Connect/Read to throw,
                // which makes the thread exit
                stream?.Close();
                stream = null;

                client?.Close();
                client = null;

                state = ConnectionState.Disconnected;
            }

            // Wait for receive thread to finish (outside lock to avoid deadlock)
            // (connectionThread becomes the receive thread after successful connection)
            if (connectionThread != null && connectionThread.IsAlive)
            {
                connectionThread.Join(1000);
            }

            Debug.Log("NetworkConnection: Disconnected");

            // Only invoke if we were actually connected (ReceiveLoop checks Disconnecting state)
            if (wasConnected)
            {
                OnDisconnected?.Invoke("Disconnected");
            }
        }

        // ========================================================================
        // SENDING DATA
        // ========================================================================

        /// <summary>
        /// Send raw bytes to the server. The packet should already include header.
        /// </summary>
        public void SendRaw(byte[] data)
        {
            // PATTERN: Capture reference inside lock, use outside lock.
            // This minimizes lock duration while ensuring we have a valid stream reference.
            NetworkStream currentStream;

            lock (connectionLock)
            {
                if (state != ConnectionState.Connected || stream == null)
                {
                    Debug.LogWarning("NetworkConnection: Cannot send - not connected");
                    return;
                }

                lastSentPacket = data;
                currentStream = stream; // Capture reference
            }

            // Write outside lock - stream.Write is thread-safe for single writer
            try
            {
                // Track sent packet for debugging (skip header, track payload)
                if (data.Length > PacketHeader.HeaderSize)
                {
                    byte[] payload = new byte[data.Length - PacketHeader.HeaderSize];
                    Array.Copy(data, PacketHeader.HeaderSize, payload, 0, payload.Length);
                    PacketDebugger.TrackSent(payload);
                }

                currentStream.Write(data, 0, data.Length);
                currentStream.Flush();
            }
            catch (Exception e)
            {
                Debug.LogError($"NetworkConnection: Send failed - {e.Message}");
                lock (connectionLock)
                {
                    state = ConnectionState.Disconnected;
                }
            }
        }

        // ========================================================================
        // RECEIVING DATA (runs on background thread)
        // ========================================================================

        private void ReceiveLoop()
        {
            byte[] headerBuffer = new byte[PacketHeader.HeaderSize];
            NetworkStream currentStream;

            lock (connectionLock)
            {
                currentStream = stream;
            }

            while (true)
            {
                // Check if we should exit (not connected or shutting down)
                lock (connectionLock)
                {
                    if (state != ConnectionState.Connected)
                        break;
                }

                try
                {
                    // Read packet header (must read all bytes - TCP may fragment)
                    if (!ReadExact(currentStream, headerBuffer, 0, PacketHeader.HeaderSize))
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
                        if (!ReadExact(currentStream, payload, 0, payloadSize))
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
                    ConnectionState currentState;
                    lock (connectionLock)
                    {
                        currentState = state;
                    }

                    // Only log error if we weren't intentionally disconnecting
                    if (currentState == ConnectionState.Connected)
                    {
                        Debug.LogError($"NetworkConnection: Receive error - {e.Message}");
                    }
                    break;
                }
            }

            // Clean exit - only invoke OnDisconnected if not a requested shutdown
            bool invokeDisconnected = false;
            lock (connectionLock)
            {
                // If we're still "Connected", connection was lost unexpectedly
                if (state == ConnectionState.Connected)
                {
                    state = ConnectionState.Disconnected;
                    invokeDisconnected = true;
                }
            }

            if (invokeDisconnected)
            {
                OnDisconnected?.Invoke("Connection lost");
            }
        }

        /// <summary>
        /// Read exactly 'count' bytes from the stream. TCP may fragment data,
        /// so a single Read() call might not return all requested bytes.
        /// </summary>
        private bool ReadExact(NetworkStream readStream, byte[] buffer, int offset, int count)
        {
            int totalRead = 0;
            while (totalRead < count)
            {
                int bytesRead = readStream.Read(buffer, offset + totalRead, count - totalRead);
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
