// ServiceLocator.cs
// A simple service locator pattern that lets any class get references to services
// without using FindObjectOfType (which is slow) or tight coupling.
//
// Usage:
//   ServiceLocator.Register<INetworkService>(networkService);
//   var network = ServiceLocator.Get<INetworkService>();
//
// Thread Safety: All operations are synchronized with a lock.

using System;
using System.Collections.Generic;
using UnityEngine;

public static class ServiceLocator
{
    // Thread Safety: Protects dictionary from concurrent Register/Get/Unregister calls.
    // Static dictionaries are shared across all threads - without a lock, two threads
    // calling Register simultaneously could corrupt the dictionary's internal state.
    private static readonly object serviceLock = new object();

    // Dictionary storing all registered services, keyed by their type
    private static readonly Dictionary<Type, object> services = new Dictionary<Type, object>();

    /// <summary>
    /// Register a service instance. Usually done during game initialization.
    /// </summary>
    public static void Register<T>(T service) where T : class
    {
        var type = typeof(T);

        lock (serviceLock)
        {
            if (services.ContainsKey(type))
            {
                Debug.LogWarning($"ServiceLocator: Overwriting existing service of type {type.Name}");
            }

            services[type] = service;
        }

        Debug.Log($"ServiceLocator: Registered {type.Name}");
    }

    /// <summary>
    /// Get a registered service. Returns null if not found.
    /// </summary>
    public static T Get<T>() where T : class
    {
        var type = typeof(T);

        lock (serviceLock)
        {
            if (services.TryGetValue(type, out var service))
            {
                return service as T;
            }
        }

        Debug.LogError($"ServiceLocator: Service of type {type.Name} not found!");
        return null;
    }

    /// <summary>
    /// Check if a service is registered.
    /// </summary>
    public static bool Has<T>() where T : class
    {
        lock (serviceLock)
        {
            return services.ContainsKey(typeof(T));
        }
    }

    /// <summary>
    /// Remove a service. Usually called during cleanup.
    /// </summary>
    public static void Unregister<T>() where T : class
    {
        var type = typeof(T);
        bool removed;

        lock (serviceLock)
        {
            removed = services.Remove(type);
        }

        if (removed)
        {
            Debug.Log($"ServiceLocator: Unregistered {type.Name}");
        }
    }

    /// <summary>
    /// Clear all services. Call this when reloading the game or during cleanup.
    /// </summary>
    public static void Clear()
    {
        lock (serviceLock)
        {
            services.Clear();
        }

        Debug.Log("ServiceLocator: Cleared all services");
    }
}
