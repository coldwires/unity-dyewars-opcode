# DyeWars Client Architecture

## Overview

The DyeWars Unity client is built on an **event-driven architecture** with clear separation of concerns. The codebase follows the **Model-View-Controller (MVC)** pattern for player logic, uses a **Service Locator** for dependency injection, and employs an **Event Bus** for decoupled communication between systems.

This document explains how every piece fits together, the flow of data through the system, and provides step-by-step guides for adding new features.

---

## Core Architecture Patterns

### Service Locator Pattern

The `ServiceLocator` is a global registry that allows any class to find services without direct references or `FindObjectOfType` calls.

**How it works:**

1. Services register themselves in their `Awake()` method
2. Other classes retrieve services via `ServiceLocator.Get<T>()`
3. Services unregister in their `OnDestroy()` method

**Registered Services:**

| Service | Responsibility |
|---------|----------------|
| `INetworkService` | Network communication (connect, send packets) |
| `PlayerRegistry` | Stores all player data (local and remote) |
| `PlayerViewFactory` | Creates/destroys player GameObjects |
| `InputService` | Tracks keyboard input and timing |
| `GridService` | Coordinate conversion and bounds checking |

**Example usage:**

```csharp
// Get a service anywhere in your code
var network = ServiceLocator.Get<INetworkService>();
network.SendMove(direction, facing);
```

### Event Bus Pattern

The `EventBus` is a publish/subscribe system that decouples components. Publishers don't know who's listening; subscribers don't know who's publishing.

**How it works:**

1. Define an event struct in `GameEvents.cs`
2. Publishers call `EventBus.Publish(new YourEvent { ... })`
3. Subscribers call `EventBus.Subscribe<YourEvent>(handler)` in `OnEnable()`
4. Subscribers call `EventBus.Unsubscribe<YourEvent>(handler)` in `OnDisable()`

**Important threading note:** `EventBus.Publish()` calls all subscribers **synchronously** on the **same thread**. This means you must only publish from the main thread if any subscriber might touch Unity objects.

**Current Events:**

| Event | When Published | Typical Subscribers |
|-------|----------------|---------------------|
| `ConnectedToServerEvent` | TCP connection + handshake complete | UI, game initialization |
| `DisconnectedFromServerEvent` | Connection lost or kicked | UI, cleanup systems |
| `LocalPlayerIdAssignedEvent` | Server sends player ID | PlayerRegistry, PlayerViewFactory |
| `PlayerPositionChangedEvent` | Position update (local or remote) | PlayerRegistry, PlayerView |
| `PlayerFacingChangedEvent` | Facing direction changed | PlayerView |
| `PlayerJoinedEvent` | New player enters game | PlayerViewFactory |
| `PlayerLeftEvent` | Player disconnects | PlayerViewFactory, PlayerRegistry |
| `DirectionInputEvent` | Direction key held | LocalPlayerController |

### Model-View-Controller for Players

Player logic is split into three parts:

**Model (`PlayerData`):** Pure data class containing player state (position, facing, ID). No Unity dependencies. Stored in `PlayerRegistry`.

**View (`PlayerView`):** Handles all visual representation—sprite rendering, movement animation, walk cycles. Attached to player GameObjects.

**Controller (`LocalPlayerController`):** Processes input, decides actions (move vs turn vs queue), manages cooldowns, triggers network sends. Only exists for the local player.

---

## System Initialization Order

When the game starts, systems initialize in this order:

```
1. GameManager.Awake()
   └── Sets up singleton instance

2. All services call Awake() (order varies, but each registers itself):
   ├── NetworkService.Awake()
   │   └── Creates NetworkConnection, PacketSender, PacketHandler
   │   └── Registers as INetworkService
   ├── PlayerRegistry.Awake()
   │   └── Registers as PlayerRegistry
   ├── PlayerViewFactory.Awake()
   │   └── Registers as PlayerViewFactory
   ├── InputService.Awake()
   │   └── Registers as InputService
   └── GridService.Awake()
       └── Registers as GridService

3. All services call Start():
   ├── NetworkService.Start()
   │   └── If connectOnStart=true, calls Connect()
   └── GameManager.Start()
       └── ValidateServices() - confirms all services registered

4. Connection established (async):
   └── OnConnectedFromBackground() [background thread]
       └── SendHandshake()
       └── Queue ConnectedToServerEvent for main thread

5. Server sends player ID:
   └── PacketHandler.HandlePlayerIdAssignment()
       └── Publishes LocalPlayerIdAssignedEvent

6. PlayerRegistry receives LocalPlayerIdAssignedEvent:
   └── Creates PlayerData for local player
   └── Publishes PlayerJoinedEvent

7. PlayerViewFactory receives LocalPlayerIdAssignedEvent:
   └── Instantiates local player prefab
   └── Initializes PlayerView and LocalPlayerController

8. LocalPlayerController.Start():
   └── Caches service references
   └── Initializes from PlayerRegistry (gets position/facing)
   └── Ready to process input
```

---

## Network Layer Deep Dive

### Threading Model

The network layer uses **two threads**:

**Main Thread (Unity):**
- Runs all MonoBehaviour methods (`Update`, `Start`, etc.)
- Processes game logic
- Updates UI
- Can safely access Unity objects

**Background Thread (Network):**
- Runs `NetworkConnection.ReceiveLoop()`
- Blocks waiting for incoming packets
- Fires callbacks when data arrives
- **Cannot** access Unity objects

### Thread Safety Pattern

Data flows from background thread to main thread via queues:

```
BACKGROUND THREAD                      MAIN THREAD
─────────────────                      ───────────
ReceiveLoop()                          
  └── Packet arrives                   
  └── OnPacketReceivedFromBackground() 
       └── lock(queueLock)             
       └── packetQueue.Enqueue(packet) 
       └── unlock                      
                                       Update()
                                         └── ProcessQueues()
                                              └── lock(queueLock)
                                              └── Copy queue to local list
                                              └── Clear queue
                                              └── unlock
                                              └── Process packets (SAFE!)
                                              └── Call handler.ProcessPacket()
                                              └── EventBus.Publish() (SAFE!)
```

**Key insight:** The lock is held briefly to copy data, then released. Processing happens outside the lock so the background thread isn't blocked.

### Packet Flow: Sending

When you want to send data to the server:

```
1. Game code calls NetworkService.SendMove(direction, facing)
   
2. NetworkService delegates to PacketSender.SendMove()

3. PacketSender.SendMove() builds the packet:
   └── Creates PayloadBuilder
   └── Writes opcode (0x01)
   └── Writes direction byte
   └── Writes facing byte
   └── PacketWriter.CreatePacket() wraps with header (magic + size)
   
4. PacketSender calls connection.SendRaw(packet)

5. NetworkConnection.SendRaw() writes to TCP stream:
   └── Stores packet in lastSentPacket (for debugging)
   └── stream.Write(data, 0, data.Length)
   └── stream.Flush()
```

### Packet Flow: Receiving

When data arrives from the server:

```
1. Background thread: ReceiveLoop reads 4-byte header
   └── Validates magic bytes (0x11 0x68)
   └── Reads payload size
   
2. Background thread: Reads payload bytes

3. Background thread: OnPacketReceivedFromBackground(payload)
   └── Checks for player ID assignment (special case)
   └── Queues packet for main thread

4. Main thread: Update() → ProcessQueues()
   └── Copies queued packets
   └── For each packet, calls handler.ProcessPacket()

5. PacketHandler.ProcessPacket() switches on opcode:
   └── Parses packet data
   └── Publishes appropriate event(s)
   
6. Subscribers react to events:
   └── PlayerRegistry updates data
   └── PlayerView updates visuals
   └── etc.
```

### Protocol Constants

All protocol values are centralized in `PacketHeader.cs`:

```csharp
public const byte Magic1 = 0x11;
public const byte Magic2 = 0x68;
public const ushort ProtocolVersion = 0x0001;
public const uint ClientMagic = 0x44594557;  // "DYEW"
public const int HeaderSize = 4;
```

### Opcodes

Opcodes are defined in `PacketOpcodes.cs`. The ranges are:

| Range | Direction | Purpose |
|-------|-----------|---------|
| `0x00` | C→S | Handshake |
| `0x01-0x0F` | C→S | Player actions (move, turn, interact) |
| `0x10-0x1F` | S→C | Local player updates |
| `0x20-0x2F` | S→C | World updates (other players, batch) |
| `0x30-0x3F` | S→C | Combat & effects |
| `0x40-0x4F` | C→S | Combat actions |
| `0x50-0x5F` | Both | Chat & social |
| `0xF0-0xFF` | Both | System (ping, disconnect) |

---

## Input & Movement System

### Input Processing

`InputService` runs every frame and:

1. Reads current arrow key state
2. Detects if direction changed this frame
3. Captures time since last key release (for pivot grace)
4. Publishes `DirectionInputEvent` if any direction is held

### Movement Decision Flow

When `LocalPlayerController` receives a `DirectionInputEvent`:

```
DirectionInputEvent received
    │
    ▼
Currently moving? ──Yes──► Queue the direction for later
    │
    No
    ▼
In cooldown? ──Yes──► Ignore input
    │
    No
    ▼
Already facing this direction? ──Yes──► TryMove()
    │                                      │
    No                                     ▼
    │                               Send C_Move packet
    ▼                               Update predicted position
Turn()                              Start movement animation
    │                               Wait for moveDuration
    ▼                                      │
Send C_Turn packet                         ▼
Update facing                       OnMoveComplete()
Apply cooldown (maybe)                     │
    │                                      ▼
    ▼                               Process queued direction
Check pivot grace:
  - timeSinceRelease < pivotGraceTime? → No cooldown (seamless pivot)
  - timeSinceRelease > pivotGraceTime? → Apply turnCooldown
```

### Pivot Grace Period

The pivot system allows fluid directional changes:

**Fast key switch (< 10ms between release and press):** The player turns instantly with no cooldown. This feels like a seamless directional change—useful for tactical repositioning.

**Slow key switch (> 10ms):** The player turns but has a brief cooldown before they can move. This prevents accidental movement when the player wanted to just turn.

### Client-Side Prediction

When the local player moves:

1. Client immediately updates position (prediction)
2. Client sends move request to server
3. Server validates and responds with actual position
4. If positions match, nothing happens
5. If positions differ slightly, client lerps to correct position
6. If positions differ greatly, client snaps to correct position

---

## Player System

### PlayerData (Model)

A class (not struct) holding player state:

```csharp
public class PlayerData
{
    public uint PlayerId { get; }          // Immutable
    public bool IsLocalPlayer { get; }     // Immutable
    public Vector2Int Position { get; }    // Mutable via SetPosition()
    public int Facing { get; }             // Mutable via SetFacing()
    public bool IsDirty { get; set; }      // Changed flag
}
```

**Why a class?** PlayerData is stored in a Dictionary. If it were a struct, retrieving from the Dictionary would give a copy, so modifications wouldn't affect the stored data.

### PlayerRegistry

The single source of truth for all player data:

- Stores local player reference
- Stores Dictionary of all players by ID
- Listens to network events and updates player data
- Provides API for local player prediction

### PlayerView (View)

Handles all visual representation:

- Multi-layer sprite rendering (body, head, weapon)
- Movement lerping between grid positions
- Walk cycle animation (4 frames per move)
- Facing direction sprite selection

**Sprite indexing:** `index = (facing * 3) + frameOffset`

### PlayerViewFactory

Factory that creates/destroys player GameObjects:

- Listens for `LocalPlayerIdAssignedEvent` → creates local player
- Listens for `PlayerJoinedEvent` → creates remote player
- Listens for `PlayerLeftEvent` → destroys remote player

---

## Adding a New Feature: Step-by-Step Guides

### Example 1: Adding a Jump Ability

**Step 1: Define the event (if needed)**

In `GameEvents.cs`:

```csharp
public struct PlayerJumpedEvent
{
    public uint PlayerId;
    public Vector2Int FromPosition;
    public Vector2Int ToPosition;
}
```

**Step 2: Add the opcode**

In `PacketOpcodes.cs`:

```csharp
// CLIENT -> SERVER
public const byte C_Jump = 0x06;

// SERVER -> CLIENT  
public const byte S_PlayerJumped = 0x16;
```

**Step 3: Add the send method**

In `PacketSender.cs`:

```csharp
public void SendJump(int direction)
{
    var packet = PacketWriter.CreatePacket(Opcode.C_Jump, writer =>
    {
        writer.WriteU8((byte)direction);
    });
    connection.SendRaw(packet);
}
```

**Step 4: Add the receive handler**

In `PacketHandler.cs`, add to the switch statement:

```csharp
case Opcode.S_PlayerJumped:
    HandlePlayerJumped(payload, offset);
    break;
```

Then add the handler method:

```csharp
private void HandlePlayerJumped(byte[] payload, int offset)
{
    if (!PacketReader.HasBytes(payload, offset, 8)) return;
    
    uint playerId = PacketReader.ReadU32(payload, ref offset);
    int toX = PacketReader.ReadU16(payload, ref offset);
    int toY = PacketReader.ReadU16(payload, ref offset);
    
    Core.EventBus.Publish(new Core.PlayerJumpedEvent
    {
        PlayerId = playerId,
        ToPosition = new Vector2Int(toX, toY)
    });
}
```

**Step 5: Handle in LocalPlayerController**

Add input detection (maybe spacebar):

```csharp
private void Update()
{
    // ... existing code ...
    
    if (Keyboard.current.spaceKey.wasPressedThisFrame && !isMoving)
    {
        TryJump();
    }
}

private void TryJump()
{
    Vector2Int jumpTarget = currentPosition + Direction.GetDelta(currentFacing) * 2;
    
    if (gridService.IsInBounds(jumpTarget))
    {
        networkService.GetSender().SendJump(currentFacing);
        // Prediction: assume jump succeeds
        currentPosition = jumpTarget;
        playerView?.JumpTo(jumpTarget, jumpDuration);
    }
}
```

**Step 6: Add visual method to PlayerView**

```csharp
public void JumpTo(Vector2Int gridPos, float duration)
{
    // Similar to MoveTo but with arc motion
    StartCoroutine(JumpAnimation(gridPos, duration));
}

private IEnumerator JumpAnimation(Vector2Int gridPos, float duration)
{
    Vector3 start = transform.position;
    Vector3 end = gridService.GridToWorld(gridPos);
    float elapsed = 0f;
    
    while (elapsed < duration)
    {
        elapsed += Time.deltaTime;
        float t = elapsed / duration;
        
        // Lerp X/Y position
        Vector3 pos = Vector3.Lerp(start, end, t);
        
        // Add arc on Y axis
        float arc = Mathf.Sin(t * Mathf.PI) * 0.5f;
        pos.y += arc;
        
        transform.position = pos;
        yield return null;
    }
    
    transform.position = end;
}
```

### Example 2: Adding a Health System

**Step 1: Extend PlayerData**

```csharp
public class PlayerData
{
    // ... existing fields ...
    
    public int CurrentHealth { get; private set; }
    public int MaxHealth { get; private set; }
    
    public void SetHealth(int current, int max)
    {
        CurrentHealth = current;
        MaxHealth = max;
        IsDirty = true;
    }
}
```

**Step 2: Add events**

```csharp
public struct PlayerHealthChangedEvent
{
    public uint PlayerId;
    public int CurrentHealth;
    public int MaxHealth;
    public int Delta;  // Positive = heal, negative = damage
}

public struct PlayerDiedEvent
{
    public uint PlayerId;
    public Vector2Int DeathPosition;
}
```

**Step 3: Handle incoming packets**

The handlers for `S_Damage`, `S_Heal`, `S_Death` already exist as stubs. Implement them:

```csharp
private void HandleDamage(byte[] payload, int offset)
{
    if (!PacketReader.HasBytes(payload, offset, 10)) return;

    uint playerId = PacketReader.ReadU32(payload, ref offset);
    ushort damage = PacketReader.ReadU16(payload, ref offset);
    ushort currentHp = PacketReader.ReadU16(payload, ref offset);
    ushort maxHp = PacketReader.ReadU16(payload, ref offset);

    Core.EventBus.Publish(new Core.PlayerHealthChangedEvent
    {
        PlayerId = playerId,
        CurrentHealth = currentHp,
        MaxHealth = maxHp,
        Delta = -(int)damage
    });
}
```

**Step 4: Create health bar UI**

Create a `HealthBarView` component that subscribes to `PlayerHealthChangedEvent`:

```csharp
public class HealthBarView : MonoBehaviour
{
    [SerializeField] private Image fillImage;
    private uint trackedPlayerId;
    
    public void Initialize(uint playerId)
    {
        trackedPlayerId = playerId;
        EventBus.Subscribe<PlayerHealthChangedEvent>(OnHealthChanged);
    }
    
    private void OnHealthChanged(PlayerHealthChangedEvent evt)
    {
        if (evt.PlayerId != trackedPlayerId) return;
        
        float percent = (float)evt.CurrentHealth / evt.MaxHealth;
        fillImage.fillAmount = percent;
    }
    
    private void OnDestroy()
    {
        EventBus.Unsubscribe<PlayerHealthChangedEvent>(OnHealthChanged);
    }
}
```

### Example 3: Adding Chat

**Step 1: Create ChatService**

```csharp
public class ChatService : MonoBehaviour
{
    private void Awake()
    {
        ServiceLocator.Register<ChatService>(this);
    }
    
    private void OnEnable()
    {
        EventBus.Subscribe<ChatMessageReceivedEvent>(OnChatReceived);
    }
    
    public void SendMessage(string message, byte channel = 0)
    {
        var network = ServiceLocator.Get<INetworkService>() as NetworkService;
        network?.SendChatMessage(channel, message);
    }
    
    private void OnChatReceived(ChatMessageReceivedEvent evt)
    {
        // Update UI, play sound, etc.
    }
}
```

**Step 2: Add event and handler**

```csharp
// In GameEvents.cs
public struct ChatMessageReceivedEvent
{
    public uint SenderId;
    public byte Channel;
    public string Message;
}

// In PacketHandler.cs - add case for Opcode.ChatMessage (0x50)
private void HandleChatMessage(byte[] payload, int offset)
{
    uint senderId = PacketReader.ReadU32(payload, ref offset);
    byte channel = PacketReader.ReadU8(payload, ref offset);
    ushort length = PacketReader.ReadU16(payload, ref offset);
    
    byte[] msgBytes = new byte[length];
    for (int i = 0; i < length; i++)
        msgBytes[i] = PacketReader.ReadU8(payload, ref offset);
    
    string message = System.Text.Encoding.UTF8.GetString(msgBytes);
    
    Core.EventBus.Publish(new Core.ChatMessageReceivedEvent
    {
        SenderId = senderId,
        Channel = channel,
        Message = message
    });
}
```

---

## Debugging Tips

### Debug UI

`GameManager.OnGUI()` displays:
- Connection status
- Player ID
- Last sent packet (hex)
- Player count
- Position and facing
- Current input

### Packet Inspection

The debug UI shows the last sent packet in hex format:

```
Last Sent (7 bytes):
11 68 00 03 01 02 01
│  │  │  │  │  │  └── facing byte
│  │  │  │  │  └───── direction byte
│  │  │  │  └──────── opcode (C_Move)
│  │  └──┴─────────── payload size (big-endian)
└──┴───────────────── magic bytes
```

### Common Issues

**"Service not found" errors:** Check that all services have their `Awake()` method registering them, and that `GameManager` validates services in `Start()`.

**Player not moving:** Check the debug UI for last sent packet. Verify magic bytes are correct (0x11 0x68). Verify opcode matches what server expects.

**Events not firing:** Ensure you subscribed in `OnEnable()` and the event is being published. Check `EventBus.GetSubscriberCount<T>()`.

**Threading crashes:** If you get "can only be called from the main thread" errors, you're publishing an event or accessing Unity from the background thread. Use the main thread action queue.

---

## File Organization

```
Scripts/
├── Core/
│   ├── EventBus.cs          # Pub/sub event system
│   ├── GameEvents.cs        # All event struct definitions
│   └── ServiceLocator.cs    # Service registry
├── Game/
│   ├── GameManager.cs       # Bootstrap and debug UI
│   └── GridService.cs       # Coordinate conversion
├── Input/
│   └── InputService.cs      # Keyboard input tracking
├── Network/
│   ├── Connection/
│   │   └── NetworkConnection.cs  # Raw TCP socket handling
│   ├── Incoming/
│   │   └── PacketHandler.cs      # Incoming packet processing
│   ├── Outgoing/
│   │   └── PacketSender.cs       # Outgoing packet construction
│   ├── Protocol/
│   │   ├── PacketHeader.cs       # Magic bytes, header constants
│   │   ├── PacketOpcodes.cs      # All opcode definitions
│   │   ├── PacketReader.cs       # Binary reading helpers
│   │   └── PacketWriter.cs       # Binary writing helpers
│   ├── INetworkService.cs        # Network interface
│   └── NetworkService.cs         # Main network orchestrator
└── Player/
    ├── LocalPlayerController.cs  # Input processing, movement logic
    ├── PlayerData.cs             # Pure data model
    ├── PlayerRegistry.cs         # Player data storage
    ├── PlayerView.cs             # Visual representation
    └── PlayerViewFactory.cs      # GameObject creation/destruction
```

---

## Quick Reference: Adding Features Checklist

1. **Define event(s)** in `GameEvents.cs` if cross-system communication needed
2. **Add opcode(s)** in `PacketOpcodes.cs` for new packet types
3. **Add send method** in `PacketSender.cs` for client→server packets
4. **Add handler** in `PacketHandler.cs` for server→client packets
5. **Subscribe to events** in relevant systems
6. **Update PlayerData** if new player state needed
7. **Update PlayerView** if new visuals needed
8. **Test** with debug UI packet inspection
