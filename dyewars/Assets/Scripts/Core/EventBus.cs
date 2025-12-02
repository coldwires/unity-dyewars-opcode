// EventBus.cs
// A simple publish/subscribe event system that decouples publishers from subscribers.
// Any class can publish events without knowing who's listening.
// Any class can subscribe to events without knowing who's publishing.
//
// Usage:
//   EventBus.Subscribe<PlayerMovedEvent>(OnPlayerMoved);
//   EventBus.Publish(new PlayerMovedEvent { PlayerId = id, Position = pos });
//   EventBus.Unsubscribe<PlayerMovedEvent>(OnPlayerMoved);

using System;
using System.Collections.Generic;
using UnityEngine;

namespace DyeWars.Core
{
    public static class EventBus
    {
        private static readonly Dictionary<Type, List<Delegate>> subscribers = new Dictionary<Type, List<Delegate>>();

        /// <summary>
        /// Subscribe to an event type. Your handler will be called when that event is published.
        /// </summary>
        public static void Subscribe<T>(Action<T> handler) where T : struct
        {
            var type = typeof(T);

            if (!subscribers.ContainsKey(type))
            {
                subscribers[type] = new List<Delegate>();
            }

            subscribers[type].Add(handler);
        }

        /// <summary>
        /// Unsubscribe from an event type. Always unsubscribe in OnDestroy to prevent memory leaks.
        /// </summary>
        public static void Unsubscribe<T>(Action<T> handler) where T : struct
        {
            var type = typeof(T);

            if (subscribers.TryGetValue(type, out var handlers))
            {
                handlers.Remove(handler);
            }
        }

        /// <summary>
        /// Publish an event. All subscribers to this event type will be notified.
        /// </summary>
        public static void Publish<T>(T eventData) where T : struct
        {
            var type = typeof(T);

            if (subscribers.TryGetValue(type, out var handlers))
            {
                // Iterate in reverse so handlers can unsubscribe during iteration
                for (int i = handlers.Count - 1; i >= 0; i--)
                {
                    try
                    {
                        (handlers[i] as Action<T>)?.Invoke(eventData);
                    }
                    catch (Exception e)
                    {
                        Debug.LogError($"EventBus: Error invoking handler for {type.Name}: {e.Message}");
                    }
                }
            }
        }

        /// <summary>
        /// Clear all subscribers. Call this when reloading the game.
        /// </summary>
        public static void Clear()
        {
            subscribers.Clear();
            Debug.Log("EventBus: Cleared all subscribers");
        }

        /// <summary>
        /// Get subscriber count for debugging.
        /// </summary>
        public static int GetSubscriberCount<T>() where T : struct
        {
            var type = typeof(T);
            return subscribers.TryGetValue(type, out var handlers) ? handlers.Count : 0;
        }
    }
}