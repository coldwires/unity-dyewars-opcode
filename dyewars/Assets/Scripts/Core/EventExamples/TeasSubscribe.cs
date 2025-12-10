using System.Collections.Generic;
using UnityEngine;


// Example subscriber MonoBehaviour for OnSpatialChange event
// Something that listens to spatial changes and reacts accordingly
public class TeasSubscriber : MonoBehaviour
{
    [SerializeField] private OnSpatialChange spatialChangeEvent;




    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Awake()
    {

    }

    void OnEnable()
    {
        spatialChangeEvent?.Subscribe(HandleSpatialChange);
        EventBus.Subscribe<OnSpatialChange.Data>(HandleSpatialChange);
        EventBus.Subscribe<OnSpatialChange.Data>(HandleSpatialChange2);
    }


    void HandleSpatialChange2(OnSpatialChange.Data data)
    {
        Debug.Log($"Spatial 2 changed from {data.OldPosition} to {data.NewPosition}");
    }

    void HandleSpatialChange(OnSpatialChange.Data data)
    {
        Debug.Log($"Spatial changed from {data.OldPosition} to {data.NewPosition}");
    }

    void Start()
    {

    }

    // Update is called once per frame
    void Update()
    {

    }

    void OnDisable()
    {
        spatialChangeEvent?.Unsubscribe(HandleSpatialChange);
        EventBus.Unsubscribe<OnSpatialChange.Data>(HandleSpatialChange);
        EventBus.Unsubscribe<OnSpatialChange.Data>(HandleSpatialChange2);
    }
}
