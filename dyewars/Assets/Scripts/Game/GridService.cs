// GridService.cs
// Centralized grid utilities. Handles coordinate conversion between grid and world space.
//
// This extracts the GridToWorld logic that was duplicated across classes.
// Any system that needs to convert coordinates uses this service.

using UnityEngine;
using DyeWars.Core;

namespace DyeWars.Game
{
    public class GridService : MonoBehaviour
    {
        [Header("Grid Settings")]
        [SerializeField] private float cellSize = 1f;
        [SerializeField] private int gridWidth = 10;
        [SerializeField] private int gridHeight = 10;

        // Public access to settings
        public float CellSize => cellSize;
        public int GridWidth => gridWidth;
        public int GridHeight => gridHeight;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<GridService>(this);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<GridService>();
        }

        // ====================================================================
        // COORDINATE CONVERSION
        // ====================================================================

        /// <summary>
        /// Convert grid coordinates to world position (centered in cell).
        /// </summary>
        public Vector3 GridToWorld(int x, int y)
        {
            return new Vector3(
                x * cellSize + cellSize * 0.5f,
                y * cellSize + cellSize * 0.5f,
                0
            );
        }

        /// <summary>
        /// Convert grid coordinates to world position.
        /// </summary>
        public Vector3 GridToWorld(Vector2Int gridPos)
        {
            return GridToWorld(gridPos.x, gridPos.y);
        }

        /// <summary>
        /// Convert world position to grid coordinates.
        /// </summary>
        public Vector2Int WorldToGrid(Vector3 worldPos)
        {
            return new Vector2Int(
                Mathf.FloorToInt(worldPos.x / cellSize),
                Mathf.FloorToInt(worldPos.y / cellSize)
            );
        }

        // ====================================================================
        // BOUNDS CHECKING
        // ====================================================================

        /// <summary>
        /// Check if a grid position is within bounds.
        /// </summary>
        public bool IsInBounds(int x, int y)
        {
            return x >= 0 && x < gridWidth && y >= 0 && y < gridHeight;
        }

        /// <summary>
        /// Check if a grid position is within bounds.
        /// </summary>
        public bool IsInBounds(Vector2Int gridPos)
        {
            return IsInBounds(gridPos.x, gridPos.y);
        }

        /// <summary>
        /// Clamp a position to grid bounds.
        /// </summary>
        public Vector2Int ClampToBounds(Vector2Int gridPos)
        {
            return new Vector2Int(
                Mathf.Clamp(gridPos.x, 0, gridWidth - 1),
                Mathf.Clamp(gridPos.y, 0, gridHeight - 1)
            );
        }

        // ====================================================================
        // DISTANCE & MOVEMENT
        // ====================================================================

        /// <summary>
        /// Get Manhattan distance between two grid positions.
        /// Manhattan distance = |x1-x2| + |y1-y2| (no diagonals)
        /// </summary>
        public int GetManhattanDistance(Vector2Int a, Vector2Int b)
        {
            return Mathf.Abs(a.x - b.x) + Mathf.Abs(a.y - b.y);
        }

        /// <summary>
        /// Check if two positions are adjacent (Manhattan distance of 1).
        /// </summary>
        public bool AreAdjacent(Vector2Int a, Vector2Int b)
        {
            return GetManhattanDistance(a, b) == 1;
        }

        /// <summary>
        /// Get the position after moving in a direction.
        /// </summary>
        public Vector2Int GetPositionInDirection(Vector2Int from, int direction)
        {
            return from + Direction.GetDelta(direction);
        }
    }
}