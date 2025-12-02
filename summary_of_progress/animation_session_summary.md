# DyeWars Session: Animation & Movement System

## Overview

Implemented a complete 2D animation and movement system with multi-layer sprites, lerped movement, input queuing, and a pivot mechanic for tactical gameplay.

---

## Features Implemented

### 1. PlayerController Component

A new Unity component that handles all player visuals and animation:

**Multi-Layer Sprite System:**
- Separate SpriteRenderers for body, head, and weapon
- All layers animate together based on facing direction and movement state
- Head sorting order changes based on facing (behind body when facing up)

**Movement System:**
- Smooth lerping between grid positions
- Configurable `moveDuration` for movement speed
- `SnapToPosition()` for server corrections

**Animation System:**
- Walk animation sequence: frames 0 → 1 → 0 → 2 → loop
- Frame time auto-calculated as `moveDuration / 4`
- Sprite index calculation: `(facing * 3) + frameOffset`

**State Properties:**
- `IsMoving` - Currently animating movement
- `IsBusy` - Moving OR in cooldown (blocks input)

---

### 2. Pivot Mechanic

A tactical feature allowing players to turn without stepping:

**The Problem:**
- Players need to aim spells by facing a direction
- Accidentally stepping when trying to turn feels bad
- But seamless direction changes while running should feel smooth

**The Solution:**
- Track time between key releases and new key presses
- If gap < `pivotGraceTime` (~50ms): Seamless pivot, no cooldown
- If gap > `pivotGraceTime`: Apply turn cooldown before allowing movement

**Input Tracking in NetworkManager:**
```csharp
private int lastDirection = -1;
private float capturedTimeSinceRelease = 999f;
private float timeSinceKeyRelease = 999f;

// Capture time only when direction changes
if (currentDirection != -1 && currentDirection != lastDirection)
{
    capturedTimeSinceRelease = timeSinceKeyRelease;
}
```

---

### 3. Input Queuing

Allows direction input during movement to be remembered and executed when the move completes:

**Flow:**
1. Player is moving right
2. Player presses down (during move)
3. Direction is queued in PlayerController
4. Move completes → `OnQueuedDirectionReady` event fires
5. NetworkManager checks current key state:
   - No key held → Execute queued turn
   - Queued key held → Execute queued turn
   - Different key held → Use new direction instead

---

## Configuration

### PlayerController Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `moveDuration` | 0.35f | Time to move one tile |
| `turnCooldown` | 0.2f | Cooldown after turning (when not pivoting) |
| `cellSize` | 1f | Grid cell size in Unity units |
| `pivotGraceTime` | 0.05f | Max gap for seamless pivot (50ms) |

### Sprite Setup

Sprite sheets should be organized as:
```
Index 0-2:   Walk Up (idle, step1, step2)
Index 3-5:   Walk Right
Index 6-8:   Walk Down
Index 9-11:  Walk Left
```

---

## Architecture

### Event Flow

```
Input (NetworkManager)
    │
    ├─ If not moving & not busy:
    │   ├─ Same direction → SendMoveCommand() → PlayerController.MoveTo()
    │   └─ Different direction → SendTurnCommand() → PlayerController.SetFacing()
    │
    └─ If moving:
        └─ Queue direction → PlayerController.QueueDirection()
                                    │
                                    ▼ (when move ends)
                            OnQueuedDirectionReady event
                                    │
                                    ▼
                            NetworkManager decides action
```

### Component Responsibilities

| Component | Responsibility |
|-----------|----------------|
| NetworkManager | Input handling, packet sending, key timing |
| PlayerController | Animation, lerping, cooldowns, queueing |
| GridRenderer | Creates player instances, connects components |

---

## Files Modified

### New Files
- `Assets/code/PlayerController.cs` — Animation and movement component

### Modified Files
- `Assets/code/NetworkManager.cs`:
  - Added key timing tracking (`timeSinceKeyRelease`, `capturedTimeSinceRelease`)
  - Added `lastDirection` for detecting direction changes
  - Added `SetLocalPlayerController()` for component connection
  - Added `OnQueuedDirectionReady` handler
  - Updated `HandleDirectionInput()` to support queuing during movement

- `Assets/code/GridRenderer.cs`:
  - Connects PlayerController to NetworkManager on player creation

---

## Testing Scenarios

| Scenario | Expected Behavior |
|----------|-------------------|
| Idle → tap arrow | Turn to face that direction, wait cooldown, then can move |
| Idle → hold arrow | Turn, wait cooldown, start moving |
| Hold right → hold down | Seamless turn and continue moving (no cooldown) |
| Hold right → release → tap down | Turn only, no movement |
| Moving → press different direction | Queue it, execute when move ends |
| Moving → press then release different direction | Queue ignored if key released |
