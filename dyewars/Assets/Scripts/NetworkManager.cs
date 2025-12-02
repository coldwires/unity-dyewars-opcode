// NetworkManager.cs
using System;
using System.Collections.Generic;
using System.Net.Sockets;
using System.Threading;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.XR;

public class NetworkManager : MonoBehaviour
{
    private TcpClient client;
    private NetworkStream stream;
    private Thread receiveThread;
    private bool isConnected = false;
    
    private float timeSinceKeyRelease = 999f;
    private float capturedTimeSinceRelease = 999f;
    private int lastDirection = -1;
    private bool directionChangedThisFrame = false;

    
    // Local player data
    public uint MyPlayerID { get; private set; } = 0;
    public Vector2Int MyPosition { get; private set; } = new Vector2Int(0, 0);
    public int MyFacing { get; private set; }

    private PlayerController localPlayerController;
    private bool canMove = false;
    
    
    
    // Other players data
    public Dictionary<uint, Vector2Int> OtherPlayers { get; private set; } = new Dictionary<uint, Vector2Int>();
    public Dictionary<uint, int> OtherFacing { get; private set; } = new Dictionary<uint, int>();

    // Events
    public System.Action<Vector2Int> OnMyPositionUpdated;
    public System.Action<uint, Vector2Int> OnOtherPlayerUpdated;
    public System.Action<uint> OnPlayerLeft;
    public System.Action<uint> OnPlayerIDAssigned;
    public System.Action<int> OnMyFacingUpdated;

    // Thread-safe queues
    private Queue<Action> mainThreadActions = new Queue<Action>();
    private object queueLock = new object();

    private ushort ReadU16(byte[] payload, ref int offset)
    {
        // Combine two bytes into one ushort
        // Don't forget to increment offset by 2
        ushort retVal = (ushort)((payload[offset] << 8) | payload[offset+1]);
        offset += 2;
        return retVal;
    }
    private byte ReadU8(byte[] payload, ref int offset)
    {
        return payload[offset++];
    }

    private uint ReadU32(byte[] payload, ref int offset)
    {
        uint value = (uint)((payload[offset] << 24) | (payload[offset + 1] << 16) |
                            (payload[offset + 2] << 8) | payload[offset + 3]);
        offset += 4;
        return value;
    }

    public void SetLocalPlayerController(PlayerController controller)
    {
        localPlayerController = controller;
        localPlayerController.OnMoveComplete += OnLocalMoveComplete;
        localPlayerController.OnQueuedDirectionReady += OnQueuedDirectionReady;
    }

    private void OnLocalMoveComplete()
    {
        canMove = true;
    }
    
    private void OnQueuedDirectionReady(int queuedDir)
    {
        // Get current key held
        int currentDirection = -1;
        if (Keyboard.current != null)
        {
            if (Keyboard.current.upArrowKey.isPressed) currentDirection = 0;
            else if (Keyboard.current.rightArrowKey.isPressed) currentDirection = 1;
            else if (Keyboard.current.downArrowKey.isPressed) currentDirection = 2;
            else if (Keyboard.current.leftArrowKey.isPressed) currentDirection = 3;
        }
    
        // Decide which direction to use
        int directionToUse;
        if (currentDirection == -1)
        {
            // No key held, use queued direction
            directionToUse = queuedDir;
        }
        else if (currentDirection == queuedDir)
        {
            // Holding the queued direction, use it
            directionToUse = queuedDir;
        }
        else
        {
            // Holding different key, use that instead
            directionToUse = currentDirection;
        }
    
        // Execute the turn (with 0ms since it was queued during movement)
        if (directionToUse != MyFacing)
        {
            SendTurnCommand(directionToUse);
            localPlayerController.SetFacing(directionToUse, 0f);  // 0ms = seamless
        }
    }
    
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
        
        // Check if any arrow key is held
        
        // Get current direction (-1 if no key held)
        int currentDirection = -1;
        if (Keyboard.current != null)
        {
            if (Keyboard.current.upArrowKey.isPressed) currentDirection = 0;
            else if (Keyboard.current.rightArrowKey.isPressed) currentDirection = 1;
            else if (Keyboard.current.downArrowKey.isPressed) currentDirection = 2;
            else if (Keyboard.current.leftArrowKey.isPressed) currentDirection = 3;
        }

        bool isKeyHeld = currentDirection != -1;

        
        
        // Detect direction change
        directionChangedThisFrame = (currentDirection != -1 && currentDirection != lastDirection);
    
        // Capture time ONLY when direction changes
        if (directionChangedThisFrame)
        {
            capturedTimeSinceRelease = timeSinceKeyRelease;
            Debug.Log($"Direction → {currentDirection}, time since release: {capturedTimeSinceRelease * 1000f}ms");
        }

        
       

        if (isKeyHeld)
        {
            timeSinceKeyRelease = 0f;
        }
        else
        {
            timeSinceKeyRelease += Time.deltaTime;
        }
        lastDirection = currentDirection;
        
        // Handle input
        if (isConnected && currentDirection != -1)
        {
            HandleDirectionInput(currentDirection, capturedTimeSinceRelease);
        }
    }

    private void HandleDirectionInput(int direction, float timeSinceRelease)
    {
        // Finish moves first
        if (localPlayerController == null)
            return;
        
        // If moving, queue the direction instead
        if (localPlayerController.IsMoving)
        {
            if (direction != MyFacing)
            {
                localPlayerController.QueueDirection(direction);
            }
            return;
        }
    
        if (localPlayerController.IsBusy)
            return;
        
        if (MyFacing == direction)
        {
            //if we're facing this way, send a move
            SendMoveCommand(direction);
        }
        else
        {
            SendTurnCommand(direction);
            localPlayerController.SetFacing(direction, timeSinceRelease);
        }
    }

    public void SendTurnCommand(int direction)
    {
        byte[] message = new byte[] { 0x04, (byte)direction };
        //Send the request
        SendMessage(message);
        
        //Turn inside client for interpolation
        MyFacing = direction;
        OnMyFacingUpdated?.Invoke(MyFacing);
    }
    public void SendMoveCommand(int direction)
    {
        byte[] message = new byte[] { 0x01, (byte)direction, (byte)MyFacing };
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
        // This will be refactored into my map bounds/data
        if (predictedPos.x >= 0 && predictedPos.x < 10 && 
            predictedPos.y >= 0 && predictedPos.y < 10)
        {
            if (localPlayerController != null)
            {
                localPlayerController.MoveTo(predictedPos);
            }
            
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
                if (payload.Length >= 6) // 1 opcode + 2 x + 2 y + 1 facing
                {
                    int offset = 1; //Skip opcode
                    int x = ReadU16(payload, ref offset);//payload[1];
                    int y = ReadU16(payload, ref offset);
                    int facing = ReadU8(payload, ref offset);
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() =>
                        {
                            Vector2Int serverPos = new Vector2Int(x, y);
                            
                            // How far off are we
                            int dx = Mathf.Abs(MyPosition.x - x);
                            int dy = Mathf.Abs(MyPosition.y - y);

                            if (dx > 1 || dy > 1) //more than 1 tile
                            {
                                // Big mismatch - snap immediately
                                Debug.Log($"Big Correction: Snapping to ({x},{y})");
                                localPlayerController?.SnapToPosition(serverPos);
                            } else if (dx > 0 || dy > 0)
                            {
                                // Small mismatch - lerp to correct position
                                Debug.Log($"Small correction: Lerping to ({x}, {y})");
                                localPlayerController?.MoveTo(serverPos);
                            }
                            
                            MyPosition = serverPos;
                            OnMyPositionUpdated?.Invoke(MyPosition);

                            if (MyFacing != facing)
                            {
                                MyFacing = facing;
                                Debug.Log($"Facing Correction: {facing}");
                                localPlayerController.SetFacing(facing);
                                OnMyFacingUpdated?.Invoke(MyFacing);
                            }
                        });
                    }
                }
                break;

            case 0x12: // Other player update
                if (payload.Length >= 10) // 1 opcode, 4 id, 2 x, 2 y, 1 facing
                {
                    int offset = 1;    //skip opcode
                    uint playerId = ReadU32(payload, ref offset);   //(uint)((payload[1] << 24) | (payload[2] << 16) | 
                                                                    // (payload[3] << 8) | payload[4]);
                    int x = ReadU16(payload, ref offset);
                    int y = ReadU16(payload, ref offset);
                    int facing = ReadU8(payload, ref offset);
                    
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            OtherPlayers[playerId] = new Vector2Int(x, y);
                            OtherFacing[playerId] = facing;
                            Debug.Log($"Player {playerId} at ({x}, {y}) facing {facing}");
                            OnOtherPlayerUpdated?.Invoke(playerId, new Vector2Int(x, y));
                        });
                    }
                }
                break;

            case 0x13: // Player ID assignment
                if (payload.Length >= 5)
                {
                    int offset = 1;    //skip opcode
                    uint playerId = ReadU32(payload, ref offset);
                    //uint playerId = (uint)((payload[1] << 24) | (payload[2] << 16) | 
                                           //(payload[3] << 8) | payload[4]);
                    
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
                    int offset = 1;    //skip opcode
                    uint playerId = ReadU32(payload, ref offset);
                    //uint playerId = (uint)((payload[1] << 24) | (payload[2] << 16) | 
                    //                       (payload[3] << 8) | payload[4]);
                    
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
            case 0x15: // Facing update
                if (payload.Length >= 2) // 1 opcode + 1 facing
                {
                    int offset = 1;
                    int facing = ReadU8(payload, ref offset);
        
                    lock (queueLock)
                    {
                        mainThreadActions.Enqueue(() => {
                            if (MyFacing != facing)
                            {
                                Debug.Log($"Facing update: {facing}");
                                MyFacing = facing;
                                OnMyFacingUpdated?.Invoke(MyFacing);
                            }
                        });
                    }
                }
                break;
            case 0x20: // Batch Update (Multiple players at once)
                if (payload.Length >= 9)
                {
                    int offset = 1;
                    int count = ReadU8(payload, ref offset);

                    for (int i = 0; i < count; i++)
                    {
                        // Check if we have enough bytes left for one player (4 ID + 2 X + 2 Y + 1 facing = 9 bytes)
                        if (offset + 9 > payload.Length) break;

                        // Extract Player ID
                        uint batchId = ReadU32(payload, ref offset);
                                              
                        // Extract Position
                        int batchX = ReadU16(payload, ref offset);
                        int batchY = ReadU16(payload, ref offset);
                        
                        // Facing
                        int batchFacing = ReadU8(payload, ref offset);
                        
                        // Capture copies for the lambda
                        uint id = batchId;
                        int x = batchX;
                        int y = batchY;
                        int facing = batchFacing;

                        // Queue the update for the main thread
                        lock (queueLock)
                        {
                            mainThreadActions.Enqueue(() => {
                                // Don't update ourselves from the batch (we predict or use 0x10)
                                if (batchId != MyPlayerID) 
                                {
                                    OtherPlayers[id] = new Vector2Int(x, y);
                                    OtherFacing[id] = facing;
                                    OnOtherPlayerUpdated?.Invoke(id, new Vector2Int(x, y));
                                    /* The lambda captures batchId, batchX, batchY, batchFacing by reference.
                                     By the time the lambda runs on the main thread, the loop may have overwritten these values.
                                    OtherPlayers[batchId] = new Vector2Int(batchX, batchY);
                                    OtherFacing[batchId] = batchFacing;
                                    OnOtherPlayerUpdated?.Invoke(batchId, new Vector2Int(batchX, batchY));
                                     */
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

