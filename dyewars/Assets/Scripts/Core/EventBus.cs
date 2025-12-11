using System;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using Debug = UnityEngine.Debug;

public static class EventBus
{
    // Thread Safety: Single lock protects all dictionary access.
    // Without this, Subscribe/Unsubscribe could modify dictionaries while Publish iterates them.
    private static readonly object subscriberLock = new object();

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
        public override string ToString() => $"{gameObjectName} → {componentType}.{methodName}";
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

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
    private static void Init()
    {
        // Clear on domain reload (entering play mode)
        ClearAll();
    }

    public static void Subscribe<T>(Action<T> listener) where T : struct
    {
        var type = typeof(T);

        lock (subscriberLock)
        {
            if (!channels.ContainsKey(type))
            {
                channels[type] = new List<Action<T>>();
                listenerInfos[type] = new List<ListenerInfo>();
                publishCounts[type] = 0;
                recentPublishers[type] = new List<PublishInfo>();
            }

            ((List<Action<T>>)channels[type]).Add(listener);

            var info = new ListenerInfo { methodName = listener.Method.Name };

            if (listener.Target is MonoBehaviour mb)
            {
                info.gameObjectName = mb.gameObject.name;
                info.componentType = mb.GetType().Name;
            }
            else if (listener.Target != null)
            {
                info.gameObjectName = "(no GameObject)";
                info.componentType = listener.Target.GetType().Name;
            }
            else
            {
                info.gameObjectName = "(static)";
                info.componentType = "";
            }

            listenerInfos[type].Add(info);
        }
    }

    public static void Unsubscribe<T>(Action<T> listener) where T : struct
    {
        var type = typeof(T);
        lock (subscriberLock)
        {
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
    }

    public static void Publish<T>(T data, MonoBehaviour publisher) where T : struct
    {
        var stack = new StackTrace(1, false);
        var frame = stack.GetFrame(0);
        var method = frame?.GetMethod()?.Name ?? "unknown";

        TrackPublisher<T>(publisher.gameObject.name, publisher.GetType().Name, method);
        PublishInternal(data);
    }

    public static void Publish<T>(T data) where T : struct
    {
        CapturePublisherFromStack<T>();
        PublishInternal(data);
    }

    private static void CapturePublisherFromStack<T>() where T : struct
    {
        var stack = new StackTrace(2, false);
        var frame = stack.GetFrame(0);
        var method = frame?.GetMethod();

        var goName = "(via stack trace)";
        var compType = method?.DeclaringType?.Name ?? "unknown";
        var methodName = method?.Name ?? "unknown";

        TrackPublisher<T>(goName, compType, methodName);
    }

    private static void TrackPublisher<T>(string goName, string compType, string method) where T : struct
    {
        var type = typeof(T);

        lock (subscriberLock)
        {
            if (!recentPublishers.ContainsKey(type))
                recentPublishers[type] = new List<PublishInfo>();

            var list = recentPublishers[type];
            var key = $"{goName}_{compType}_{method}";

            var existing = list.Find(p => p.Key == key);

            if (existing != null)
            {
                existing.count++;
                existing.timestamp = DateTime.Now.ToString("HH:mm:ss.fff");
            }
            else
            {
                list.Add(new PublishInfo
                {
                    timestamp = DateTime.Now.ToString("HH:mm:ss.fff"),
                    gameObjectName = goName,
                    componentType = compType,
                    methodName = method,
                    count = 1
                });

                if (list.Count > 10)
                    list.RemoveAt(0);
            }
        }
    }

    private static void PublishInternal<T>(T data) where T : struct
    {
        var type = typeof(T);
        Action<T>[] listenersCopy = null;

        // CRITICAL PATTERN: Copy listeners inside lock, invoke outside lock.
        // Why? If we held the lock while invoking, and a listener tried to Subscribe/Unsubscribe,
        // it would deadlock (listener waiting for lock we hold, us waiting for listener to return).
        lock (subscriberLock)
        {
            if (!publishCounts.ContainsKey(type))
                publishCounts[type] = 0;

            lastPublished[type] = data;
            publishCounts[type]++;

            if (channels.TryGetValue(type, out var list))
            {
                var listeners = (List<Action<T>>)list;
                // ToArray() creates a snapshot - safe to iterate even if original list changes
                listenersCopy = listeners.ToArray();
            }
        } // Lock released BEFORE invoking listeners

        // Safe to invoke without lock - we're iterating our private copy
        if (listenersCopy != null)
        {
            for (int i = listenersCopy.Length - 1; i >= 0; i--)
            {
                try
                {
                    listenersCopy[i]?.Invoke(data);
                }
                catch (Exception ex)
                {
                    Debug.LogError($"[EventBus] Error in {typeof(T).Name} handler: {ex}");
                }
            }
        }
    }

    // Debug API
    public static List<Type> GetRegisteredTypes()
    {
        lock (subscriberLock)
        {
            return new List<Type>(channels.Keys);
        }
    }

    public static List<ListenerInfo> GetListenerInfosForType(Type t)
    {
        lock (subscriberLock)
        {
            return listenerInfos.TryGetValue(t, out var list) ? new List<ListenerInfo>(list) : new List<ListenerInfo>();
        }
    }

    public static List<PublishInfo> GetPublishersForType(Type t)
    {
        lock (subscriberLock)
        {
            return recentPublishers.TryGetValue(t, out var list) ? new List<PublishInfo>(list) : new List<PublishInfo>();
        }
    }

    public static int GetPublishCountForType(Type t)
    {
        lock (subscriberLock)
        {
            return publishCounts.GetValueOrDefault(t);
        }
    }

    public static object GetLastPublishedForType(Type t)
    {
        lock (subscriberLock)
        {
            return lastPublished.GetValueOrDefault(t);
        }
    }

    public static void ClearAllStats()
    {
        lock (subscriberLock)
        {
            var types = new List<Type>(publishCounts.Keys);
            foreach (var type in types)
                publishCounts[type] = 0;
            lastPublished.Clear();
            recentPublishers.Clear();
        }
    }

    public static void ClearAll()
    {
        lock (subscriberLock)
        {
            channels.Clear();
            listenerInfos.Clear();
            publishCounts.Clear();
            lastPublished.Clear();
            recentPublishers.Clear();
        }
    }
}
