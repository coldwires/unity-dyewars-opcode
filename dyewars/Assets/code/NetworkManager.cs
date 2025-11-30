// NetworkManager.cs
using System;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.InputSystem;

public class NetworkManager : MonoBehaviour
{
    private TcpClient client;
    private NetworkStream stream;
    private Thread receiveThread;
    private bool isConnected = false;
    
    // Local player data
    public uint MyPlayerID { get; private set; } = 0;
    public Vector2Int MyPosition { get; private set; } = new Vector2Int(0, 0);

    // Other players data
    public Dictionary<uint, Vector2Int> OtherPlayers { get; private set; } = new Dictionary<uint, Vector2Int>();

    // Events
    public System.Action<Vector2Int> OnMyPositionUpdated;
    public System.Action<uint, Vector2Int> OnOtherPlayerUpdated;
    public System.Action<uint> OnPlayerLeft;
    public System.Action<uint> OnPlayerIDAssigned;

    // Thread-safe queues
    private Queue<Action> mainThreadActions = new Queue<Action>();
    private object queueLock = new object();

    void Start()
    {
        Debug.Log("Connecting to server...");
        ConnectToServer();
    }

    void ConnectToServer()
    {
        try
        {
            client = new TcpClient();
            client.Connect("192.168.1.3", 8080);
            stream = client.GetStream();
            isConnected = true;

            Debug.Log("Connected to server!");

            // Start receiving thread
            receiveThread = new Thread(new ThreadStart(ReceiveData));
            receiveThread.IsBackground = true;
            receiveThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError($"Connection error: {e.Message}");
        }
    }

    void Update()
    {
        // Process all queued actions on main thread
        lock (queueLock)
        {
            while (mainThreadActions.Count > 0)
            {
                mainThreadActions.Dequeue()?.Invoke();
            }
        }

        // Handle input
        if (isConnected && Keyboard.current != null)
        {
            if (Keyboard.current.upArrowKey.wasPressedThisFrame)
                SendMoveCommand(0);
            else if (Keyboard.current.rightArrowKey.wasPressedThisFrame)
                SendMoveCommand(1);
            else if (Keyboard.current.downArrowKey.wasPressedThisFrame)
                SendMoveCommand(2);
            else if (Keyboard.current.leftArrowKey.wasPressedThisFrame)
                SendMoveCommand(3);
        }
    }

    public void SendMoveCommand(int direction)
    {
        byte[] message = new byte[] { 0x01, (byte)direction };
        SendMessage(message);
        // 2. CLIENT-SIDE PREDICTION (Move immediately!)
        Vector2Int predictedPos = MyPosition;

        // Logic must match Lua script exactly
        // (In a real game, share this logic or hardcode strictly)
        if (direction == 0) predictedPos.y += 1;      // UP
        else if (direction == 1) predictedPos.x += 1; // RIGHT
        else if (direction == 2) predictedPos.y -= 1; // DOWN
        else if (direction == 3) predictedPos.x -= 1; // LEFT

        // Simple bounds check (optional, but prevents visual glitches)
        if (predictedPos.x >= 0 && predictedPos.x < 10 && 
            predictedPos.y >= 0 && predictedPos.y < 10)
        {
            MyPosition = predictedPos;
            OnMyPositionUpdated?.Invoke(MyPosition);
        }
    }

    private void SendMessage(byte[] payload)
    {
        if (!isConnected || stream == null) return;

        try
        {
            byte[] packet = new byte[4 + payload.Length];

            packet[0] = 0x11;
            packet[1] = 0x68;

            ushort payloadSize = (ushort)payload.Length;
            packet[2] = (byte)((payloadSize >> 8) & 0xFF);
            packet[3] = (byte)(payloadSize & 0xFF);

            Array.Copy(payload, 0, packet, 4, payload.Length);

            stream.Write(packet, 0, packet.Length);
            stream.Flush();
        }
        catch (Exception e)
        {
            Debug.LogError($"Send error: {e.Message}");
            isConnected = false;
        }
    }

    private void ReceiveData()
    {
        byte[] headerBuffer = new byte[4];

        while (isConnected)
        {
            try
            {
                int bytesRead = stream.Read(headerBuffer, 0, 4);
                if (bytesRead != 4)
                {
                    Debug.LogError("Invalid header received");
                    break;
                }

                if (headerBuffer[0] != 0x11 || headerBuffer[1] != 0x68)
                {
                    Debug.LogError("Invalid header format");
                    break;
                }

                ushort payloadSize = (ushort)((headerBuffer[2] << 8) | headerBuffer[3]);

                if (payloadSize > 0)
                {
                    byte[] payload = new byte[payloadSize];
                    bytesRead = stream.Read(payload, 0, payloadSize);

                    if (bytesRead == payloadSize)
                    {
                        ProcessPacket(payload);
                    }
                }
            }
            catch (Exception e)
            {
                Debug.LogError($"Receive error: {e.Message}");
                break;
            }
        }

        isConnected = false;
        Debug.Log("Disconnected from server");
    }

    private void ProcessPacket(byte[] payload)
    {
        if (payload.Length < 1) return;

        byte messageType = payload[0];

        switch (messageType)
        {
            case 0x10: // My position update
                if (payload.Length >= 3)
                {
                    int x = payload[1];
                    int y = payload[2];
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            // Only update if the server disagrees with our prediction
                            if (MyPosition.x != x || MyPosition.y != y)
                            {
                                Debug.Log($"Correction: Snapped to ({x}, {y})");
                                MyPosition = new Vector2Int(x, y);
                                OnMyPositionUpdated?.Invoke(MyPosition);
                            }
                        });
                    }
                }
                break;

            case 0x12: // Other player update
                if (payload.Length >= 7)
                {
                    uint playerId = (uint)((payload[1] << 24) | (payload[2] << 16) | 
                                           (payload[3] << 8) | payload[4]);
                    int x = payload[5];
                    int y = payload[6];
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            OtherPlayers[playerId] = new Vector2Int(x, y);
                            Debug.Log($"Player {playerId} at ({x}, {y})");
                            OnOtherPlayerUpdated?.Invoke(playerId, new Vector2Int(x, y));
                        });
                    }
                }
                break;

            case 0x13: // Player ID assignment
                if (payload.Length >= 5)
                {
                    uint playerId = (uint)((payload[1] << 24) | (payload[2] << 16) | 
                                           (payload[3] << 8) | payload[4]);
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            MyPlayerID = playerId;
                            Debug.Log($"Assigned Player ID: {playerId}");
                            OnPlayerIDAssigned?.Invoke(playerId);
                        });
                    }
                }
                break;

            case 0x14: // Player left
                if (payload.Length >= 5)
                {
                    uint playerId = (uint)((payload[1] << 24) | (payload[2] << 16) | 
                                           (payload[3] << 8) | payload[4]);
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            if (OtherPlayers.ContainsKey(playerId))
                            {
                                OtherPlayers.Remove(playerId);
                                Debug.Log($"Player {playerId} left");
                                OnPlayerLeft?.Invoke(playerId);
                            }
                        });
                    }
                }
                break;

            case 0x11: // Custom response
                Debug.Log("Received custom message response");
                break;
            
            case 0x20: // Batch Update (Multiple players at once)
                if (payload.Length >= 2)
                {
                    int count = payload[1];
                    int offset = 2; // Start after Opcode and Count

                    for (int i = 0; i < count; i++)
                    {
                        // Check if we have enough bytes left for one player (4 ID + 1 X + 1 Y = 6 bytes)
                        if (offset + 6 > payload.Length) break;

                        // Extract Player ID
                        uint batchId = (uint)((payload[offset] << 24) | (payload[offset + 1] << 16) |
                                              (payload[offset + 2] << 8) | payload[offset + 3]);
                        offset += 4;

                        // Extract Position
                        int batchX = payload[offset];
                        int batchY = payload[offset + 1];
                        offset += 2;

                        // Queue the update for the main thread
                        lock (queueLock)
                        {
                            mainThreadActions.Enqueue(() => {
                                // Don't update ourselves from the batch (we predict or use 0x10)
                                if (batchId != MyPlayerID) 
                                {
                                    OtherPlayers[batchId] = new Vector2Int(batchX, batchY);
                                    OnOtherPlayerUpdated?.Invoke(batchId, new Vector2Int(batchX, batchY));
                                }
                            });
                        }
                    }
                }
                break;
            default:
                Debug.LogWarning($"Unknown message type: 0x{messageType:X2}");
                break;
        }
    }

    void OnDestroy()
    {
        isConnected = false;

        if (stream != null)
        {
            stream.Close();
            stream = null;
        }

        if (client != null)
        {
            client.Close();
            client = null;
        }

        if (receiveThread != null && receiveThread.IsAlive)
        {
            receiveThread.Join(1000);
        }
    }
}