using System;
using System.Collections.Generic;
using UnityEngine;

[System.Serializable]
public class ListenerInfo
{
    public string gameObject;
    public string component;
    public string method;
}

public abstract class GameEvent<T> : ScriptableObject where T : struct
{

    [Header("Debug")]
    [SerializeField] private T lastPublished;
    [SerializeField] private int publishCount;
    [SerializeField] private int subscriberCount;
    [SerializeField] private List<ListenerInfo> listenerInfo = new();

    private readonly List<Action<T>> listeners = new();

    private void UpdateListenerNames()
    {
        listenerInfo.Clear();
        foreach (var listener in listeners)
        {
            var info = new ListenerInfo { method = listener.Method.Name };

            if (listener.Target is MonoBehaviour mb)
            {
                info.gameObject = mb.gameObject.name;
                info.component = mb.GetType().Name;
            }

            listenerInfo.Add(info);
        }
    }

    public void Subscribe(Action<T> listener)
    {
        if (!listeners.Contains(listener))
        {
            listeners.Add(listener);
            subscriberCount = listeners.Count;
            UpdateListenerNames();
        }
    }

    public void Unsubscribe(Action<T> listener)
    {
        if (listeners.Contains(listener))
        {
            listeners.Remove(listener);
            subscriberCount = listeners.Count;
            UpdateListenerNames();
        }
    }

    public void Publish(T data)
    {
        // Iterate in reverse to allow safe removal during iteration
        lastPublished = data;
        publishCount++;
        for (int i = listeners.Count - 1; i >= 0; i--)
        {
            try
            {
                listeners[i]?.Invoke(data);
            }
            catch (Exception ex)
            {
                Debug.LogError($"EventBusSO: Error invoking listener: {ex}");
            }
        }
    }

    public void Clear()
    {
        publishCount = 0;
        subscriberCount = 0;
        lastPublished = default;
        listeners.Clear();
    }

}