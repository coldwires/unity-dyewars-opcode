using UnityEngine;

public class TeasPublish : MonoBehaviour
{
    [SerializeField] private OnSpatialChange onSpatialChangeEvent;

    // Start is called once before the first execution of Update after the MonoBehaviour is created
    void Start()
    {
        if (onSpatialChangeEvent != null)
        {
            onSpatialChangeEvent.Publish(new OnSpatialChange.Data
            {
                OldPosition = new Vector2Int(0, 0),
                NewPosition = new Vector2Int(5, 5)
            });

        }
    }

    // Update is called once per frame
    void Update()
    {

    }
}
