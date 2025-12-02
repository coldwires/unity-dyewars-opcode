# DyeWars Client Architecture - Data Flow

This document explains how data flows through the refactored Unity client, from input to network to visuals.

---

## High-Level Overview

The client follows an event-driven architecture where systems communicate through a central EventBus rather than direct references. This decouples systems so they can be developed, tested, and modified independently.

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   Input     │────▶│   Logic     │────▶│   Visual    │
│  Service    │     │ Controllers │     │    Views    │
└─────────────┘     └─────────────┘     └─────────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │   Network   │
                    │   Service   │
                    └─────────────┘
                           │
                           ▼
                      [ Server ]
```

---

## Initialization Flow

When Unity starts, the following sequence occurs. Understanding this helps debug "service not found" errors.

### Phase 1: Awake (Frame 1)

Every MonoBehaviour's `Awake()` runs before any `Start()`. Our services use Awake to register themselves with the ServiceLocator. The order depends on Unity's internal ordering, but since registration is just adding to a dictionary, order doesn't matter here.

```
GameManager.Awake()         → Sets singleton instance
GridService.Awake()         → ServiceLocator.Register<GridService>(this)
NetworkService.Awake()      → ServiceLocator.Register<INetworkService>(this)
                            → Creates NetworkConnection, PacketSender, PacketHandler
                            → Subscribes to connection events
PlayerRegistry.Awake()      → ServiceLocator.Register<PlayerRegistry>(this)
InputService.Awake()        → ServiceLocator.Register<InputService>(this)
PlayerViewFactory.Awake()   → ServiceLocator.Register<PlayerViewFactory>(this)
```

### Phase 2: OnEnable (Frame 1)

After Awake, OnEnable runs. Our systems use this to subscribe to EventBus events.

```
PlayerRegistry.OnEnable()       → Subscribes to LocalPlayerIdAssignedEvent,
                                  PlayerPositionChangedEvent, PlayerFacingChangedEvent,
                                  PlayerLeftEvent
                                  
PlayerViewFactory.OnEnable()    → Subscribes to LocalPlayerIdAssignedEvent,
                                  PlayerJoinedEvent, PlayerLeftEvent
```

### Phase 3: Start (Frame 1)

Start runs after all Awake and OnEnable calls complete. This is where systems can safely access other services.

```
GameManager.Start()         → Validates all services are registered
                            → Logs success or errors

NetworkService.Start()      → If connectOnStart is true, calls Connect()
                            → NetworkConnection.Connect() opens TCP socket
                            → Starts background receive thread
                            → Publishes ConnectedToServerEvent

PlayerViewFactory.Start()   → Caches references to PlayerRegistry, GridService
```

### Phase 4: Connection Established

Once connected, the server sends us our player ID. This triggers player creation.

```
Server sends 0x13 (PlayerIdAssignment) packet
        │
        ▼
NetworkConnection.ReceiveLoop() [BACKGROUND THREAD]
        │ Reads packet bytes, validates header
        ▼
NetworkService.OnPacketReceivedFromBackground() [BACKGROUND THREAD]
        │ Queues packet bytes for main thread
        ▼
NetworkService.Update() [MAIN THREAD]
        │ Dequeues packet, calls PacketHandler.ProcessPacket()
        ▼
PacketHandler.HandlePlayerIdAssignment()
        │ Parses player ID from bytes
        │ Publishes LocalPlayerIdAssignedEvent
        ▼
PlayerRegistry.OnLocalPlayerIdAssigned()
        │ Creates PlayerData for local player
        │ Publishes PlayerJoinedEvent
        ▼
PlayerViewFactory.OnLocalPlayerIdAssigned()
        │ Instantiates LocalPlayer prefab
        │ Calls PlayerView.InitializeAsLocalPlayer()
        │ LocalPlayerController.Start() runs, calls InitializeFromRegistry()
```

---

## Input Flow

This is the flow when you press an arrow key.

### Step 1: Input Detection

InputService runs in Update() every frame, checking keyboard state.

```
InputService.Update()
        │
        ▼
InputService.ProcessInput()
        │ Reads keyboard via UnityEngine.InputSystem
        │ Detects: currentDirection = Direction.Right (for example)
        │ Detects: directionChangedThisFrame = true (if different from last frame)
        │ Captures: timeSinceKeyRelease (for pivot grace period)
        │
        ▼
EventBus.Publish(DirectionInputEvent)
        │ Direction = 1 (Right)
        │ TimeSinceRelease = 0.072 (72ms since last key released)
        │ IsNewDirection = true
```

### Step 2: Controller Processing

LocalPlayerController subscribes to DirectionInputEvent and decides what to do.

```
LocalPlayerController.OnDirectionInput(evt)
        │
        ▼
LocalPlayerController.HandleDirectionInput(direction, timeSinceRelease)
        │
        ├─── If isMoving: Queue the direction for later, return
        │
        ├─── If cooldownTimer > 0: Ignore input, return
        │
        ├─── If already facing this direction: TryMove(direction)
        │
        └─── Else: Turn(direction, timeSinceRelease)
```

### Step 3a: Movement Flow

If we're already facing the direction and try to move:

```
LocalPlayerController.TryMove(direction)
        │
        │ Calculate predictedPos = currentPosition + Direction.GetDelta(direction)
        │
        │ Bounds check via GridService.IsInBounds(predictedPos)
        │
        ▼
NetworkService.SendMove(direction, facing)
        │ PacketSender creates packet bytes
        │ NetworkConnection.SendRaw() writes to TCP stream
        │
        ▼ (simultaneously)
PlayerRegistry.PredictLocalPosition(predictedPos)
        │ Updates PlayerData.Position
        │ Publishes PlayerPositionChangedEvent (IsCorrection = false)
        │
        ▼
PlayerView.MoveTo(predictedPos, moveDuration)
        │ Sets up lerp animation
        │ isMoving = true
        │
        ▼
LocalPlayerController starts coroutine WaitForMoveComplete()
        │ Waits for moveDuration seconds
        │
        ▼
LocalPlayerController.OnMoveComplete()
        │ isMoving = false
        │ Processes any queued direction
```

### Step 3b: Turn Flow

If we need to turn first:

```
LocalPlayerController.Turn(direction, timeSinceRelease)
        │
        │ Update currentFacing
        │
        ▼
NetworkService.SendTurn(direction)
        │ PacketSender creates packet bytes
        │ NetworkConnection.SendRaw() writes to TCP stream
        │
        ▼
PlayerRegistry.SetLocalFacing(direction)
        │ Updates PlayerData.Facing
        │ Publishes PlayerFacingChangedEvent
        │
        ▼
PlayerView.SetFacing(direction)
        │ Updates sprites to show new direction
        │
        ▼
If timeSinceRelease > pivotGraceTime:
        │ cooldownTimer = turnCooldown
        │ (This prevents immediate movement, allowing "turn only" actions)
```

---

## Network Receive Flow

When the server sends us data, here's how it reaches the game systems.

### Background Thread Reception

```
NetworkConnection.ReceiveLoop() [RUNS ON BACKGROUND THREAD]
        │
        │ stream.Read(headerBuffer, 0, 4)  ← Blocks until data arrives
        │
        │ Validate magic bytes via PacketHeader.IsValidMagic()
        │
        │ Read payload size via PacketHeader.ReadPayloadSize()
        │
        │ stream.Read(payload, 0, payloadSize)
        │
        ▼
NetworkConnection.OnPacketReceived?.Invoke(payload)
        │ This fires the event, but we're still on background thread!
        │
        ▼
NetworkService.OnPacketReceivedFromBackground(payload) [BACKGROUND THREAD]
        │
        │ lock(queueLock) { packetQueue.Enqueue(payload); }
        │
        │ The packet is now safely queued for main thread
```

### Main Thread Processing

```
NetworkService.Update() [MAIN THREAD, runs every frame]
        │
        ▼
NetworkService.ProcessPacketQueue()
        │
        │ lock(queueLock) { copy packets to local list, clear queue }
        │
        │ For each packet:
        ▼
PacketHandler.ProcessPacket(payload)
        │
        │ Read opcode = payload[0]
        │
        │ Switch on opcode:
        │   case 0x10: HandleMyPosition()
        │   case 0x12: HandleOtherPlayerUpdate()
        │   case 0x13: HandlePlayerIdAssignment()
        │   etc.
        │
        ▼
Handler extracts data using PacketReader
        │ PacketReader.ReadU32(), ReadU16(), ReadU8()
        │
        ▼
EventBus.Publish(appropriate event)
        │
        ▼
Subscribers handle the event
        │ PlayerRegistry updates data
        │ PlayerView updates visuals
        │ LocalPlayerController handles corrections
```

---

## Server Correction Flow

When the server disagrees with our predicted position:

```
Server sends 0x10 (MyPosition) with position different from ours
        │
        ▼
PacketHandler.HandleMyPosition()
        │ Publishes PlayerPositionChangedEvent with IsCorrection = true
        │
        ▼
PlayerRegistry.OnPlayerPositionChanged()
        │ Logs correction, updates PlayerData.Position
        │
        ▼
LocalPlayerController.OnPositionChanged()
        │
        │ Calculate difference: dx, dy
        │
        ├─── If dx > 1 or dy > 1: SnapToPosition() (teleport)
        │
        └─── Else: MoveTo() (smooth correction)
```

---

## Remote Player Flow

When another player moves:

```
Server sends 0x12 (OtherPlayerUpdate) or 0x20 (BatchUpdate)
        │
        ▼
PacketHandler publishes PlayerPositionChangedEvent
        │ IsLocalPlayer = false
        │
        ▼
PlayerRegistry.OnPlayerPositionChanged()
        │ If player doesn't exist: Create PlayerData, publish PlayerJoinedEvent
        │ Update PlayerData.Position
        │
        ▼
PlayerViewFactory.OnPlayerJoined() (if new player)
        │ Instantiates RemotePlayer prefab
        │ Calls PlayerView.InitializeAsRemotePlayer(playerId)
        │
        ▼
PlayerView.OnRemotePositionChanged()
        │ Filters by trackedPlayerId (ignores events for other players)
        │ Calls MoveTo() to animate
```

---

## Event Summary

Here's a quick reference of all events and who publishes/subscribes to them.

| Event | Published By | Subscribed By |
|-------|--------------|---------------|
| ConnectedToServerEvent | NetworkService | (available for UI) |
| DisconnectedFromServerEvent | NetworkService, PacketHandler | (available for UI) |
| LocalPlayerIdAssignedEvent | PacketHandler | PlayerRegistry, PlayerViewFactory |
| PlayerPositionChangedEvent | PacketHandler, PlayerRegistry | PlayerRegistry, LocalPlayerController, PlayerView (remote) |
| PlayerFacingChangedEvent | PacketHandler, PlayerRegistry | PlayerRegistry, LocalPlayerController, PlayerView (remote) |
| PlayerJoinedEvent | PlayerRegistry | PlayerViewFactory |
| PlayerLeftEvent | PacketHandler | PlayerRegistry, PlayerViewFactory |
| DirectionInputEvent | InputService | LocalPlayerController |

---

## Thread Safety Summary

Only two places deal with threading in this architecture. First, NetworkConnection.ReceiveLoop() runs on a background thread, but it only reads from the socket and enqueues byte arrays. Second, NetworkService.packetQueue is the bridge between threads, protected by a lock where the background thread enqueues and the main thread dequeues.

Everything else runs on the main thread and is safe to access Unity objects.

---
---

# DyeWars Refactoring Summary

This document details what was refactored, why each change was made, and what patterns were applied.

---

## The Problem: Monolithic Architecture

Before the refactor, the codebase had two main files doing everything.

**NetworkManager.cs (~400 lines)** was responsible for TCP connection management, packet sending and receiving, packet parsing, input detection, input timing tracking, player state storage (MyPosition, MyFacing, OtherPlayers dictionary), client-side prediction logic, event declarations and invocations, and main thread dispatching.

**GridRenderer.cs (~150 lines)** handled grid visual creation, local player instantiation, remote player instantiation, position update handling, facing update handling, sprite management, and coordinate conversion.

This created several problems. First, there was tight coupling: changing input handling required editing NetworkManager, risking breaking network code. Second, there was scattered responsibility: player data lived in NetworkManager, player visuals in GridRenderer, and it was unclear where to add new player features. Third, testing was difficult: you couldn't test input logic without a network connection. Fourth, scalability was limited: adding 50 new packet types would make NetworkManager unreadable.

---

## The Solution: Separation of Concerns

We split the code into focused, single-responsibility classes organized by domain.

### New Structure

```
Scripts/
├── Core/                    # Foundation infrastructure
│   ├── ServiceLocator.cs    # Dependency injection
│   ├── EventBus.cs          # Pub/sub messaging
│   └── GameEvents.cs        # Event definitions
│
├── Network/                 # All networking
│   ├── Connection/          # TCP management
│   ├── Protocol/            # Packet format definitions
│   ├── Inbound/             # Incoming packet handlers
│   └── Outbound/            # Outgoing packet builders
│
├── Player/                  # Player systems
│   ├── PlayerData.cs        # Data model
│   ├── PlayerRegistry.cs    # Data storage
│   ├── LocalPlayerController.cs  # Input/movement logic
│   ├── PlayerView.cs        # Visuals/animation
│   └── PlayerViewFactory.cs # Instantiation
│
├── Input/                   # Input handling
│   └── InputService.cs      # Centralized input
│
└── Game/                    # Game management
    ├── GameManager.cs       # Bootstrap
    └── GridService.cs       # Grid utilities
```

---

## Patterns Applied

### 1. Service Locator Pattern

**What it is:** A central registry where services register themselves and other code can retrieve them.

**Before (tight coupling):**
```csharp
// Scattered throughout the codebase
networkManager = FindFirstObjectByType<NetworkManager>();
gridRenderer = FindFirstObjectByType<GridRenderer>();
```

**After (loose coupling):**
```csharp
// Services register themselves in Awake()
ServiceLocator.Register<INetworkService>(this);

// Anyone can retrieve without knowing the concrete type
var network = ServiceLocator.Get<INetworkService>();
```

**Why it helps:** No more expensive FindObjectOfType calls. Easy to swap implementations (real network vs mock for testing). Clear dependency declaration.

---

### 2. Event Bus (Publish/Subscribe) Pattern

**What it is:** A central message broker where publishers send events without knowing who's listening, and subscribers receive events without knowing who sent them.

**Before (direct coupling):**
```csharp
// NetworkManager had to know about GridRenderer
public System.Action<Vector2Int> OnMyPositionUpdated;

// GridRenderer had to subscribe directly to NetworkManager
networkManager.OnMyPositionUpdated += OnMyPositionUpdated;
```

**After (decoupled):**
```csharp
// Publisher doesn't know who's listening
EventBus.Publish(new PlayerPositionChangedEvent { 
    PlayerId = id, 
    Position = pos 
});

// Subscriber doesn't know who's publishing
EventBus.Subscribe<PlayerPositionChangedEvent>(OnPositionChanged);
```

**Why it helps:** Adding a new subscriber (like a minimap) doesn't require changing the publisher. Systems can be added/removed without breaking others. Clear event contracts via struct definitions.

---

### 3. Model-View-Controller (MVC) Pattern

**What it is:** Separating data (Model), visuals (View), and logic (Controller) into distinct classes.

**Before (mixed concerns):**
```csharp
// GridRenderer did data storage AND visuals
private Dictionary<uint, GameObject> otherPlayerInstances;
// Updated position data AND sprite in same method
otherPlayerInstances[playerId].transform.position = worldPos;
sr.sprite = directionSprites[facing];
```

**After (separated):**
```csharp
// Model: Pure data, no Unity dependencies
public class PlayerData {
    public Vector2Int Position { get; set; }
    public int Facing { get; set; }
}

// View: Only visuals, no game logic
public class PlayerView : MonoBehaviour {
    public void MoveTo(Vector2Int pos, float duration) { /* animate */ }
    public void SetFacing(int facing) { /* update sprites */ }
}

// Controller: Logic connecting input to model
public class LocalPlayerController : MonoBehaviour {
    private void HandleDirectionInput(int dir) { /* decide move/turn */ }
}
```

**Why it helps:** PlayerData can be tested without Unity. PlayerView can be redesigned without touching movement logic. Clear ownership of each concern.

---

### 4. Factory Pattern

**What it is:** A dedicated class responsible for creating objects, encapsulating instantiation logic.

**Before (scattered instantiation):**
```csharp
// GridRenderer created players inline
if (localPlayerInstance == null && localPlayerPrefab != null)
{
    localPlayerInstance = Instantiate(localPlayerPrefab);
    // ... setup code mixed with event handling
}
```

**After (dedicated factory):**
```csharp
public class PlayerViewFactory : MonoBehaviour {
    private void CreateLocalPlayer(uint playerId) {
        localPlayerView = Instantiate(localPlayerPrefab, worldPos, Quaternion.identity);
        var view = localPlayerView.GetComponent<PlayerView>();
        view.InitializeAsLocalPlayer();
    }
}
```

**Why it helps:** Single place to change instantiation logic. Easy to add object pooling later. Clear responsibility.

---

### 5. Interface Segregation

**What it is:** Defining interfaces so consumers only depend on what they need.

**Implementation:**
```csharp
public interface INetworkService {
    bool IsConnected { get; }
    uint LocalPlayerId { get; }
    void Connect(string host, int port);
    void Disconnect();
    void SendMove(int direction, int facing);
    void SendTurn(int direction);
}
```

**Why it helps:** LocalPlayerController only knows about INetworkService, not the concrete NetworkService. Could swap in a MockNetworkService for testing. Clear contract of what's available.

---

## Specific Refactoring Decisions

### Extracting InputService

**What was moved:** All input detection and timing logic from NetworkManager.

**Old location in NetworkManager:**
```csharp
private float timeSinceKeyRelease = 999f;
private float capturedTimeSinceRelease = 999f;
private int lastDirection = -1;
private bool directionChangedThisFrame = false;

void Update() {
    // 50+ lines of input detection and timing
}
```

**New dedicated class:** InputService handles all of this and publishes DirectionInputEvent.

**Why:** Input has nothing to do with networking. Separating it means we can change input handling (add gamepad support, rebinding) without touching network code.

---

### Extracting PacketSender and PacketHandler

**What was moved:** Packet construction (sending) and packet parsing (receiving) from NetworkService.

**Old approach:** NetworkService had SendMoveCommand(), SendTurnCommand(), and a giant ProcessPacket() switch statement all in one file.

**New approach:** PacketSender contains all Send methods. PacketHandler contains all Handle methods. NetworkService just orchestrates.

**Why:** When you add a new packet type, you now know exactly where to go. Adding a trade system? Add SendTradeRequest() to PacketSender, HandleTradeResponse() to PacketHandler. No hunting through a 500-line file.

---

### Extracting PacketHeader Constants

**What was moved:** Magic bytes (0x11, 0x68) that were hardcoded in multiple places.

**Old approach:**
```csharp
// In NetworkService send method
packet[0] = 0x11;
packet[1] = 0x68;

// In NetworkService receive method
if (headerBuffer[0] != 0x11 || headerBuffer[1] != 0x68)
```

**New approach:**
```csharp
public static class PacketHeader {
    public const byte Magic1 = 0x11;
    public const byte Magic2 = 0x68;
    
    public static bool IsValidMagic(byte[] buffer) { ... }
}
```

**Why:** If you change your protocol header, you change it in one place. DRY principle (Don't Repeat Yourself).

---

### Separating PlayerData from PlayerView

**What was moved:** Player state (position, facing) into a pure data class.

**Old approach:** GridRenderer stored player GameObjects directly in a dictionary. To check a player's position, you'd access transform.position.

**New approach:** PlayerRegistry stores PlayerData objects. PlayerView reads from events, doesn't store authoritative state.

**Why:** The server is authoritative. PlayerData represents the "true" state. PlayerView is just a visual representation that might be mid-animation. This separation makes server corrections cleaner: update PlayerData, let PlayerView animate to catch up.

---

### Adding Namespaces

**What was added:** Namespace organization matching folder structure.

```csharp
namespace DyeWars.Core { }        // ServiceLocator, EventBus
namespace DyeWars.Network { }     // NetworkService, INetworkService
namespace DyeWars.Network.Protocol { }  // PacketReader, PacketWriter, Opcodes
namespace DyeWars.Network.Inbound { }   // PacketHandler
namespace DyeWars.Network.Outbound { }  // PacketSender
namespace DyeWars.Player { }      // PlayerData, PlayerView, etc.
namespace DyeWars.Input { }       // InputService
namespace DyeWars.Game { }        // GameManager, GridService
```

**Why:** Prevents naming collisions as the project grows. Makes it clear where code belongs. Standard C# practice for larger projects.

---

## What Stayed the Same

Some things didn't need refactoring.

**Packet binary format:** Still using the same header (0x11 0x68 + 2-byte size) and payload structure. The server doesn't need changes.

**Client-side prediction:** Still predict locally, send to server, correct if server disagrees. Just cleaner code now.

**Sprite indexing:** Still using (facing * 3) + frameOffset formula. Just moved to PlayerView.

**Threading model:** Still receive on background thread, process on main thread. Just cleaner separation of concerns.

---

## Migration Checklist

If you're migrating from the old codebase, here's what maps to what.

| Old Code | New Code |
|----------|----------|
| NetworkManager.MyPosition | PlayerRegistry.LocalPlayer.Position |
| NetworkManager.MyFacing | PlayerRegistry.LocalPlayer.Facing |
| NetworkManager.MyPlayerID | NetworkService.LocalPlayerId |
| NetworkManager.OtherPlayers | PlayerRegistry.AllPlayers |
| NetworkManager.SendMoveCommand() | NetworkService.SendMove() |
| NetworkManager.SendTurnCommand() | NetworkService.SendTurn() |
| NetworkManager.OnMyPositionUpdated | EventBus PlayerPositionChangedEvent |
| GridRenderer (player creation) | PlayerViewFactory |
| GridRenderer (coordinate conversion) | GridService |
| PlayerController (old) | Split into LocalPlayerController + PlayerView |

---

## Future Extensibility

This architecture makes future features easier to add.

**Adding combat:** Create `Combat/` folder with CombatService, add combat events to GameEvents.cs, add combat packet handlers to PacketHandler, add combat sends to PacketSender.

**Adding inventory:** Create `Inventory/` folder with InventoryService, InventoryData, InventoryView. Subscribe to inventory events. No changes to existing systems.

**Adding minimap:** Create MinimapView that subscribes to PlayerPositionChangedEvent. Zero changes to movement code.

**Adding gamepad support:** Extend InputService to check gamepad. Nothing else changes.

**Adding unit tests:** Mock INetworkService, test LocalPlayerController logic without actual network. Test PlayerData without Unity.
