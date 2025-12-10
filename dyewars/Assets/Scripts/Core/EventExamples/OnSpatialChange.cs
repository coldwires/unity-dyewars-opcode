using System;
using System.Collections.Generic;
using UnityEngine;

[CreateAssetMenu(menuName = "Events/OnSpatialChange")]
public class OnSpatialChange : GameEvent<OnSpatialChange.Data>
{
    [System.Serializable]
    public struct Data
    {
        public Vector2Int OldPosition;
        public Vector2Int NewPosition;
    }
}