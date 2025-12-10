using System.Collections;
using NUnit.Framework;
using UnityEngine;
using UnityEngine.TestTools;
using DyeWars.Game;
using DyeWars.Core;

public class GridServicePlayTests
{
    private GameObject gridObject;
    private GridService gridService;

    // GridService defaults (documented here for test clarity)
    private const float DefaultCellSize = 1f;
    private const int DefaultGridWidth = 10;
    private const int DefaultGridHeight = 10;

    [SetUp]
    public void SetUp()
    {
        ServiceLocator.Clear();

        gridObject = new GameObject("GridService");
        gridService = gridObject.AddComponent<GridService>();
    }

    [TearDown]
    public void TearDown()
    {
        if (gridObject != null)
            Object.Destroy(gridObject);

        ServiceLocator.Clear();
    }

    // ====================================================================
    // Coordinate Conversion
    // ====================================================================

    [UnityTest]
    public IEnumerator GridToWorld_OriginCell_ReturnsCenteredPosition()
    {
        yield return null;

        // Cell (0,0) center is at (0.5, 0.5) with cell size 1
        Vector3 worldPos = gridService.GridToWorld(0, 0);

        Assert.AreEqual(DefaultCellSize * 0.5f, worldPos.x, 0.001f);
        Assert.AreEqual(DefaultCellSize * 0.5f, worldPos.y, 0.001f);
        Assert.AreEqual(0f, worldPos.z, 0.001f);
    }

    [UnityTest]
    public IEnumerator GridToWorld_Vector2Int_SameAsInts()
    {
        yield return null;

        Vector3 fromInts = gridService.GridToWorld(3, 5);
        Vector3 fromVector = gridService.GridToWorld(new Vector2Int(3, 5));

        Assert.AreEqual(fromInts, fromVector);
    }

    [UnityTest]
    public IEnumerator WorldToGrid_CenteredPosition_ReturnsCorrectCell()
    {
        yield return null;

        // (0.5, 0.5) is center of cell (0,0)
        Vector2Int gridPos = gridService.WorldToGrid(new Vector3(0.5f, 0.5f, 0f));

        Assert.AreEqual(new Vector2Int(0, 0), gridPos);
    }

    [UnityTest]
    public IEnumerator WorldToGrid_EdgeOfCell_RoundsDown()
    {
        yield return null;

        // (0.99, 0.99) should still be cell (0,0)
        Vector2Int gridPos = gridService.WorldToGrid(new Vector3(0.99f, 0.99f, 0f));

        Assert.AreEqual(new Vector2Int(0, 0), gridPos);
    }

    [UnityTest]
    public IEnumerator WorldToGrid_NegativeCoordinates_RoundsCorrectly()
    {
        yield return null;

        // Negative world positions should floor to negative grid cells
        Vector2Int gridPos = gridService.WorldToGrid(new Vector3(-0.5f, -0.5f, 0f));

        Assert.AreEqual(new Vector2Int(-1, -1), gridPos);
    }

    [UnityTest]
    public IEnumerator WorldToGrid_LargeCoordinates_HandlesCorrectly()
    {
        yield return null;

        // Large world position
        Vector2Int gridPos = gridService.WorldToGrid(new Vector3(1000.5f, 2000.5f, 0f));

        Assert.AreEqual(new Vector2Int(1000, 2000), gridPos);
    }

    [UnityTest]
    public IEnumerator RoundTrip_GridToWorldToGrid_PreservesPosition()
    {
        yield return null;

        Vector2Int original = new Vector2Int(5, 7);
        Vector3 world = gridService.GridToWorld(original);
        Vector2Int result = gridService.WorldToGrid(world);

        Assert.AreEqual(original, result);
    }

    [UnityTest]
    public IEnumerator RoundTrip_NegativeGrid_PreservesPosition()
    {
        yield return null;

        // Negative grid positions (outside default bounds but still valid coordinates)
        Vector2Int original = new Vector2Int(-5, -3);
        Vector3 world = gridService.GridToWorld(original);
        Vector2Int result = gridService.WorldToGrid(world);

        Assert.AreEqual(original, result);
    }

    // ====================================================================
    // Bounds Checking
    // ====================================================================

    [UnityTest]
    public IEnumerator IsInBounds_InsideGrid_ReturnsTrue()
    {
        yield return null;

        // Default grid is 10x10 (0-9)
        Assert.IsTrue(gridService.IsInBounds(0, 0));
        Assert.IsTrue(gridService.IsInBounds(5, 5));
        Assert.IsTrue(gridService.IsInBounds(DefaultGridWidth - 1, DefaultGridHeight - 1));
    }

    [UnityTest]
    public IEnumerator IsInBounds_OutsideGrid_ReturnsFalse()
    {
        yield return null;

        Assert.IsFalse(gridService.IsInBounds(-1, 0));
        Assert.IsFalse(gridService.IsInBounds(0, -1));
        Assert.IsFalse(gridService.IsInBounds(DefaultGridWidth, 0)); // Grid is 0 to Width-1
        Assert.IsFalse(gridService.IsInBounds(0, DefaultGridHeight));
    }

    [UnityTest]
    public IEnumerator IsInBounds_Vector2Int_SameAsInts()
    {
        yield return null;

        Assert.AreEqual(
            gridService.IsInBounds(5, 5),
            gridService.IsInBounds(new Vector2Int(5, 5))
        );
    }

    [UnityTest]
    public IEnumerator ClampToBounds_InsideGrid_Unchanged()
    {
        yield return null;

        var pos = new Vector2Int(5, 5);
        var clamped = gridService.ClampToBounds(pos);

        Assert.AreEqual(pos, clamped);
    }

    [UnityTest]
    public IEnumerator ClampToBounds_OutsideGrid_ClampedToEdge()
    {
        yield return null;

        Assert.AreEqual(new Vector2Int(0, 0), gridService.ClampToBounds(new Vector2Int(-5, -5)));
        Assert.AreEqual(
            new Vector2Int(DefaultGridWidth - 1, DefaultGridHeight - 1),
            gridService.ClampToBounds(new Vector2Int(100, 100))
        );
    }

    [UnityTest]
    public IEnumerator ClampToBounds_PartiallyOutside_ClampsCorrectAxis()
    {
        yield return null;

        // X is out, Y is in
        Assert.AreEqual(new Vector2Int(0, 5), gridService.ClampToBounds(new Vector2Int(-10, 5)));
        // X is in, Y is out
        Assert.AreEqual(new Vector2Int(5, 0), gridService.ClampToBounds(new Vector2Int(5, -10)));
    }

    // ====================================================================
    // Distance and Movement
    // ====================================================================

    [UnityTest]
    public IEnumerator GetManhattanDistance_SamePosition_ReturnsZero()
    {
        yield return null;

        int dist = gridService.GetManhattanDistance(new Vector2Int(5, 5), new Vector2Int(5, 5));

        Assert.AreEqual(0, dist);
    }

    [UnityTest]
    public IEnumerator GetManhattanDistance_Adjacent_ReturnsOne()
    {
        yield return null;

        var a = new Vector2Int(5, 5);

        Assert.AreEqual(1, gridService.GetManhattanDistance(a, new Vector2Int(5, 6)));
        Assert.AreEqual(1, gridService.GetManhattanDistance(a, new Vector2Int(6, 5)));
        Assert.AreEqual(1, gridService.GetManhattanDistance(a, new Vector2Int(5, 4)));
        Assert.AreEqual(1, gridService.GetManhattanDistance(a, new Vector2Int(4, 5)));
    }

    [UnityTest]
    public IEnumerator GetManhattanDistance_Diagonal_ReturnsTwo()
    {
        yield return null;

        // Diagonal neighbors have Manhattan distance of 2
        int dist = gridService.GetManhattanDistance(new Vector2Int(5, 5), new Vector2Int(6, 6));

        Assert.AreEqual(2, dist);
    }

    [UnityTest]
    public IEnumerator GetManhattanDistance_FarAway_ReturnsCorrectValue()
    {
        yield return null;

        // |10-0| + |20-0| = 30
        int dist = gridService.GetManhattanDistance(new Vector2Int(0, 0), new Vector2Int(10, 20));

        Assert.AreEqual(30, dist);
    }

    [UnityTest]
    public IEnumerator GetManhattanDistance_IsSymmetric()
    {
        yield return null;

        var a = new Vector2Int(2, 3);
        var b = new Vector2Int(7, 9);

        Assert.AreEqual(
            gridService.GetManhattanDistance(a, b),
            gridService.GetManhattanDistance(b, a)
        );
    }

    [UnityTest]
    public IEnumerator AreAdjacent_AdjacentCells_ReturnsTrue()
    {
        yield return null;

        var a = new Vector2Int(5, 5);

        Assert.IsTrue(gridService.AreAdjacent(a, new Vector2Int(5, 6)));
        Assert.IsTrue(gridService.AreAdjacent(a, new Vector2Int(6, 5)));
        Assert.IsTrue(gridService.AreAdjacent(a, new Vector2Int(5, 4)));
        Assert.IsTrue(gridService.AreAdjacent(a, new Vector2Int(4, 5)));
    }

    [UnityTest]
    public IEnumerator AreAdjacent_DiagonalCells_ReturnsFalse()
    {
        yield return null;

        // Diagonal is not adjacent in 4-directional movement
        Assert.IsFalse(gridService.AreAdjacent(new Vector2Int(5, 5), new Vector2Int(6, 6)));
    }

    [UnityTest]
    public IEnumerator AreAdjacent_SameCell_ReturnsFalse()
    {
        yield return null;

        Assert.IsFalse(gridService.AreAdjacent(new Vector2Int(5, 5), new Vector2Int(5, 5)));
    }

    [UnityTest]
    public IEnumerator AreAdjacent_IsSymmetric()
    {
        yield return null;

        var a = new Vector2Int(5, 5);
        var b = new Vector2Int(5, 6);

        Assert.AreEqual(
            gridService.AreAdjacent(a, b),
            gridService.AreAdjacent(b, a)
        );
    }

    [UnityTest]
    public IEnumerator GetPositionInDirection_ReturnsCorrectPosition()
    {
        yield return null;

        var start = new Vector2Int(5, 5);

        Assert.AreEqual(new Vector2Int(5, 6), gridService.GetPositionInDirection(start, Direction.Up));
        Assert.AreEqual(new Vector2Int(6, 5), gridService.GetPositionInDirection(start, Direction.Right));
        Assert.AreEqual(new Vector2Int(5, 4), gridService.GetPositionInDirection(start, Direction.Down));
        Assert.AreEqual(new Vector2Int(4, 5), gridService.GetPositionInDirection(start, Direction.Left));
    }

    [UnityTest]
    public IEnumerator GetPositionInDirection_FromOrigin_HandlesNegative()
    {
        yield return null;

        var origin = new Vector2Int(0, 0);

        // Moving down or left from origin should give negative coordinates
        Assert.AreEqual(new Vector2Int(0, -1), gridService.GetPositionInDirection(origin, Direction.Down));
        Assert.AreEqual(new Vector2Int(-1, 0), gridService.GetPositionInDirection(origin, Direction.Left));
    }
}
