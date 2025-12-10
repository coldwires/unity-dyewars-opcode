# Unity Pub/Sub Event System

A lightweight, debuggable event system for Unity that solves the core problem: **traceability without sacrificing convenience**.

## The Problem

Unity's architecture makes dependency management awkward. You can't pass references through constructors (MonoBehaviour owns them), so you end up with:

- `GetComponent` / `Find` calls (fragile)
- Drag-and-drop serialized references (hidden in scene files)
- Singletons (global state)
- Events (decoupled but hard to trace)

A static EventBus is convenient but becomes a debugging nightmare - you can't see who's listening or publishing without reading code.

## The Solution

Two complementary approaches:

1. **Static EventBus with Debug Window** - convenient, globally accessible, with editor tooling for traceability
2. **ScriptableObject Events** - visible assets you can click and inspect, with debug info baked in

---

## Approach 1: Static EventBus

### Core Implementation

```csharp
public static class EventBus
{
    private static Dictionary<Type, object> channels = new();
    private static Dictionary<Type, List<ListenerInfo>> listenerInfos = new();
    private static Dictionary<Type, int> publishCounts = new();
    private static Dictionary<Type, object> lastPublished = new();
    private static Dictionary<Type, List<PublishInfo>> recentPublishers = new();

    [Serializable]
    public class ListenerInfo
    {
        public string gameObjectName;
        public string componentType;
        public string methodName;
    }

    [Serializable]
    public class PublishInfo
    {
        public string gameObjectName;
        public string componentType;
        public string methodName;
        public string timestamp;
        public int count;
        public string Key => $"{gameObjectName}_{componentType}_{methodName}";
    }

    public static void Subscribe<T>(Action<T> listener) where T : struct
    {
        var type = typeof(T);

        if (!channels.ContainsKey(type))
        {
            channels[type] = new List<Action<T>>();
            listenerInfos[type] = new List<ListenerInfo>();
            publishCounts[type] = 0;
            recentPublishers[type] = new List<PublishInfo>();
        }

        ((List<Action<T>>)channels[type]).Add(listener);

        // Track listener info for debugging
        var info = new ListenerInfo { methodName = listener.Method.Name };
        if (listener.Target is MonoBehaviour mb)
        {
            info.gameObjectName = mb.gameObject.name;
            info.componentType = mb.GetType().Name;
        }
        listenerInfos[type].Add(info);
    }

    public static void Unsubscribe<T>(Action<T> listener) where T : struct
    {
        var type = typeof(T);
        if (channels.TryGetValue(type, out var list))
        {
            var index = ((List<Action<T>>)list).IndexOf(listener);
            if (index >= 0)
            {
                ((List<Action<T>>)list).RemoveAt(index);
                listenerInfos[type].RemoveAt(index);
            }
        }
    }

    public static void Publish<T>(T data, MonoBehaviour publisher) where T : struct
    {
        var stack = new StackTrace(1, false);
        var method = stack.GetFrame(0)?.GetMethod()?.Name ?? "unknown";
        TrackPublisher<T>(publisher.gameObject.name, publisher.GetType().Name, method);
        PublishInternal(data);
    }

    public static void Publish<T>(T data) where T : struct
    {
        // Falls back to stack trace if no publisher provided
        CapturePublisherFromStack<T>();
        PublishInternal(data);
    }
}
```

### Defining Events

Events are simple structs:

```csharp
public struct DirectionInputEvent
{
    public int Direction;
    public float TimeSinceRelease;
}

public struct PlayerDamagedEvent
{
    public int Damage;
    public int CurrentHealth;
    public GameObject Source;
}

public struct SpatialChangeEvent
{
    public Vector2Int OldPosition;
    public Vector2Int NewPosition;
}
```

### Usage

**Subscribing (listeners):**

```csharp
public class HealthUI : MonoBehaviour
{
    void OnEnable()
    {
        EventBus.Subscribe<PlayerDamagedEvent>(OnDamage);
    }

    void OnDisable()
    {
        EventBus.Unsubscribe<PlayerDamagedEvent>(OnDamage);
    }

    void OnDamage(PlayerDamagedEvent evt)
    {
        healthBar.SetValue(evt.CurrentHealth);
    }
}
```

**Publishing:**

```csharp
public class Player : MonoBehaviour
{
    void TakeDamage(int amount)
    {
        health -= amount;
        
        // Pass 'this' for better debug tracking
        EventBus.Publish(new PlayerDamagedEvent 
        { 
            Damage = amount, 
            CurrentHealth = health 
        }, this);
    }
}
```

### Debug Window

Open via **Window → EventBus Debug**

Shows for each event type:
- Flow summary: `Publisher.Component.Method → Listener.Component.Method (x count)`
- Listener count and publish count
- Last published data
- Expandable listener/publisher details

### Scene Cleanup

The EventBus auto-clears on scene load:

```csharp
[RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.BeforeSceneLoad)]
private static void SetupSceneCallback()
{
    SceneManager.sceneLoaded -= OnSceneLoaded;
    SceneManager.sceneLoaded += OnSceneLoaded;
}

private static void OnSceneLoaded(Scene scene, LoadSceneMode mode)
{
    ClearAll();
}
```

---

## Approach 2: ScriptableObject Events

More explicit - events are visible assets in your Project folder.

### Base Class with Debug Info

```csharp
public abstract class GameEvent<T> : ScriptableObject where T : struct
{
    [Header("Debug")]
    [SerializeField] private T lastPublished;
    [SerializeField] private int publishCount;
    [SerializeField] private List<string> listenerNames = new();

    private readonly List<Action<T>> listeners = new();

    public void Subscribe(Action<T> listener)
    {
        listeners.Add(listener);
        UpdateListenerNames();
    }

    public void Unsubscribe(Action<T> listener)
    {
        listeners.Remove(listener);
        UpdateListenerNames();
    }

    public void Publish(T data)
    {
        lastPublished = data;
        publishCount++;

        for (int i = listeners.Count - 1; i >= 0; i--)
            listeners[i]?.Invoke(data);
    }

    private void UpdateListenerNames()
    {
        listenerNames.Clear();
        foreach (var listener in listeners)
        {
            if (listener.Target is MonoBehaviour mb)
                listenerNames.Add($"{mb.gameObject.name} → {mb.GetType().Name}.{listener.Method.Name}");
        }
    }

    [ContextMenu("Clear Stats")]
    private void ClearStats()
    {
        publishCount = 0;
        lastPublished = default;
    }
}
```

### Concrete Event

```csharp
[CreateAssetMenu(menuName = "Events/OnSpatialChange")]
public class OnSpatialChange : GameEvent<OnSpatialChange.Data>
{
    [System.Serializable]  // Required for Inspector visibility
    public struct Data
    {
        public Vector2Int OldPosition;
        public Vector2Int NewPosition;
    }
}
```

### Usage

```csharp
public class PlayerMovement : MonoBehaviour
{
    [SerializeField] private OnSpatialChange spatialChangeEvent;

    void Move(Vector2Int newPos)
    {
        var oldPos = currentPos;
        currentPos = newPos;
        
        spatialChangeEvent.Publish(new OnSpatialChange.Data
        {
            OldPosition = oldPos,
            NewPosition = newPos
        });
    }
}

public class FootstepAudio : MonoBehaviour
{
    [SerializeField] private OnSpatialChange spatialChangeEvent;

    void OnEnable() => spatialChangeEvent.Subscribe(OnMove);
    void OnDisable() => spatialChangeEvent.Unsubscribe(OnMove);

    void OnMove(OnSpatialChange.Data data)
    {
        PlayFootstep();
    }
}
```

### Setup

1. Right-click Project → Create → Events → OnSpatialChange
2. Name it (e.g., "PlayerSpatialChange")
3. Drag into both publisher and subscriber Inspector slots

### Tracing

- Click the SO asset in Project window
- Inspector shows: last published data, publish count, listener names
- Right-click → Find References In Scene → see all connected components

---

## Comparison

| Aspect | Static EventBus | ScriptableObject Events |
|--------|-----------------|------------------------|
| Setup | Zero - just use it | Create asset, wire in Inspector |
| Tracing | Debug window | Click asset, Inspector shows info |
| Boilerplate | Minimal | One class + one asset per event |
| Compile-time safety | Generic type checking | Same |
| Visibility | Requires window open | Always visible in Inspector |
| Multiple events per component | Easy | Requires multiple fields |

## Recommendations

**Use Static EventBus when:**
- Rapid prototyping
- Many event types
- Events used across many components
- You want minimal ceremony

**Use ScriptableObject Events when:**
- Critical game systems (must not fail silently)
- Team projects (visibility matters)
- You want Inspector-first workflow
- Fewer, more important events

**Hybrid approach:**
- ScriptableObject events for core game flow (player death, level complete)
- Static EventBus for frequent/minor events (input, UI updates, analytics)

---

## Critical Rules

1. **Always unsubscribe in OnDisable** - prevents memory leaks and zombie listeners
2. **Pass `this` to Publish** - enables debug tracking
3. **Use structs for event data** - value types, no allocation
4. **Mark nested structs `[System.Serializable]`** - required for Inspector visibility in SOs

---

## Files

- `EventBus.cs` - Static event bus with debug tracking
- `EventBusDebugWindow.cs` - Editor window (put in Editor folder)
- `GameEvent.cs` - Abstract SO base class
- `OnSpatialChange.cs` (etc.) - Concrete event types
