// MultiplayerGridRenderer.cs
using UnityEngine;
using System.Collections.Generic;

public class MultiplayerGridRenderer : MonoBehaviour
{
    [SerializeField] private GameObject localPlayerPrefab;
    [SerializeField] private GameObject otherPlayerPrefab;
    [SerializeField] private float cellSize = 1f;
    [SerializeField] private Sprite gridCellSprite;
    [SerializeField] private Color gridColor = new Color(1, 1, 1, 0.1f);

    [SerializeField] private Sprite[] directionSprites;
    
    private GameObject localPlayerInstance;
    private Dictionary<uint, GameObject> otherPlayerInstances = new Dictionary<uint, GameObject>();
    private NetworkManager networkManager;
    void Start()
    {
        networkManager = FindFirstObjectByType<NetworkManager>();

        if (networkManager != null)
        {
            networkManager.OnMyPositionUpdated += OnMyPositionUpdated;
            networkManager.OnOtherPlayerUpdated += OnOtherPlayerUpdated;
            networkManager.OnPlayerLeft += OnPlayerLeft;
            networkManager.OnPlayerIDAssigned += OnPlayerIDAssigned;
            networkManager.OnMyFacingUpdated += OnMyFacingUpdated;

        }

        CreateSpriteGrid();
    }

    private void OnPlayerIDAssigned(uint playerId)
    {
        Debug.Log($"Creating local player visual for ID: {playerId}");
        
        if (localPlayerInstance == null && localPlayerPrefab != null)
        {
            localPlayerInstance = Instantiate(localPlayerPrefab);
            localPlayerInstance.name = $"LocalPlayer_{playerId}";
            
            // Get the PlayerController and give it to NetworkManager
            PlayerController controller = localPlayerInstance.GetComponent<PlayerController>();
            networkManager.SetLocalPlayerController(controller);
            
            UpdateLocalPlayerPosition();
            OnMyFacingUpdated(networkManager.MyFacing);
        }
    }

    private void OnMyFacingUpdated(int facing)
    {
        if (localPlayerInstance != null && directionSprites != null && facing < directionSprites.Length)
        {
            SpriteRenderer sr = localPlayerInstance.GetComponent<SpriteRenderer>();
            if (sr != null)
            {
                sr.sprite = directionSprites[facing];
            }
        }
    }

    private void OnMyPositionUpdated(Vector2Int newPos)
    {
        UpdateLocalPlayerPosition();
    }

    private void OnOtherPlayerUpdated(uint playerId, Vector2Int newPos)
    {
        // Create player instance if it doesn't exist
        if (!otherPlayerInstances.ContainsKey(playerId))
        {
            if (otherPlayerPrefab != null)
            {
                GameObject playerObj = Instantiate(otherPlayerPrefab);
                playerObj.name = $"Player_{playerId}";
                otherPlayerInstances[playerId] = playerObj;
                
                Debug.Log($"Created visual for player {playerId}");
            }
        }

        // Update position
        if (otherPlayerInstances.ContainsKey(playerId))
        {
            Vector3 worldPos = GridToWorld(newPos.x, newPos.y);
            otherPlayerInstances[playerId].transform.position = worldPos;
            
            if (networkManager.OtherFacing.TryGetValue(playerId, out int facing))
            {
                if (directionSprites != null && facing < directionSprites.Length)
                {
                    SpriteRenderer sr = otherPlayerInstances[playerId].GetComponent<SpriteRenderer>();
                    if (sr != null)
                    {
                        sr.sprite = directionSprites[facing];
                    }
                }
            }
        }
    }

    private void OnPlayerLeft(uint playerId)
    {
        if (otherPlayerInstances.ContainsKey(playerId))
        {
            Destroy(otherPlayerInstances[playerId]);
            otherPlayerInstances.Remove(playerId);
            Debug.Log($"Removed visual for player {playerId}");
        }
    }

    private void CreateSpriteGrid()
    {
        GameObject gridParent = new GameObject("SpriteGrid");

        for (int x = 0; x < 10; x++)
        {
            for (int y = 0; y < 10; y++)
            {
                GameObject cell = new GameObject($"Cell_{x}_{y}");
                cell.transform.SetParent(gridParent.transform);

                SpriteRenderer sr = cell.AddComponent<SpriteRenderer>();
                if (gridCellSprite != null)
                {
                    sr.sprite = gridCellSprite;
                }
                sr.color = gridColor;
                sr.sortingOrder = -1; // Behind players

                cell.transform.position = GridToWorld(x, y);
                cell.transform.localScale = Vector3.one * cellSize;
            }
        }
    }

    private void UpdateLocalPlayerPosition()
    {
        if (localPlayerInstance != null && networkManager != null)
        {
            Vector3 worldPos = GridToWorld(networkManager.MyPosition.x, networkManager.MyPosition.y);
            localPlayerInstance.transform.position = worldPos;
        }
    }

    private Vector3 GridToWorld(int x, int y)
    {
        return new Vector3(
            x * cellSize + cellSize * 0.5f,
            y * cellSize + cellSize * 0.5f,
            0
        );
    }

    void OnDestroy()
    {
        if (networkManager != null)
        {
            networkManager.OnMyPositionUpdated -= OnMyPositionUpdated;
            networkManager.OnOtherPlayerUpdated -= OnOtherPlayerUpdated;
            networkManager.OnPlayerLeft -= OnPlayerLeft;
            networkManager.OnPlayerIDAssigned -= OnPlayerIDAssigned;
            networkManager.OnMyFacingUpdated -= OnMyFacingUpdated;
        }
    }
}