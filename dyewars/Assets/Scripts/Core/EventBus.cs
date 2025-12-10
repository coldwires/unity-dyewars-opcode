using System;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using Debug = UnityEngine.Debug;

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

    private static void PublishInternal<T>(T data) where T : struct
    {
        var type = typeof(T);

        if (!publishCounts.ContainsKey(type))
            publishCounts[type] = 0;

        lastPublished[type] = data;
        publishCounts[type]++;

        if (channels.TryGetValue(type, out var list))
        {
            var listeners = (List<Action<T>>)list;
            for (int i = listeners.Count - 1; i >= 0; i--)
            {
                try
                {
                    listeners[i]?.Invoke(data);
                }
                catch (Exception ex)
                {
                    Debug.LogError($"[EventBus] Error in {typeof(T).Name} handler: {ex}");
                }
            }
        }
    }

    // Debug API
    public static List<Type> GetRegisteredTypes() => new(channels.Keys);
    public static List<ListenerInfo> GetListenerInfosForType(Type t) => listenerInfos.GetValueOrDefault(t) ?? new();
    public static List<PublishInfo> GetPublishersForType(Type t) => recentPublishers.GetValueOrDefault(t) ?? new();
    public static int GetPublishCountForType(Type t) => publishCounts.GetValueOrDefault(t);
    public static object GetLastPublishedForType(Type t) => lastPublished.GetValueOrDefault(t);

    public static void ClearAllStats()
    {
        var types = new List<Type>(publishCounts.Keys);
        foreach (var type in types)
            publishCounts[type] = 0;
        lastPublished.Clear();
        recentPublishers.Clear();
    }

    public static void ClearAll()
    {
        channels.Clear();
        listenerInfos.Clear();
        publishCounts.Clear();
        lastPublished.Clear();
        recentPublishers.Clear();
    }
}