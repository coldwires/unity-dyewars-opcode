// PlayerView.cs
// Handles all visual representation and animation for a player.
// This is the "View" in MVC - only visuals, no game logic.
//
// Responsibilities:
//   - Sprite rendering (body, head, weapon layers)
//   - Movement animation (lerping between positions)
//   - Walk cycle animation (frame switching)
//   - Facing direction visuals
//
// This is attached to both local and remote player GameObjects.
// For remote players, PlayerViewFactory calls UpdateFromServer directly.
// For local players, LocalPlayerController calls methods directly.

using UnityEngine;
using DyeWars.Core;
using DyeWars.Game;

namespace DyeWars.Player
{
    public class PlayerView : MonoBehaviour
    {
        [Header("Sprite Layers")]
        [SerializeField] private SpriteRenderer bodyRenderer;
        [SerializeField] private SpriteRenderer headRenderer;
        [SerializeField] private SpriteRenderer weaponRenderer;

        [Header("Sprite Sheets (sliced)")]
        [SerializeField] private Sprite[] bodySprites;
        [SerializeField] private Sprite[] headSprites;
        [SerializeField] private Sprite[] weaponSprites;

        // Animation state
        private readonly int[] walkFrameSequence = { 0, 1, 0, 2 };
        private int currentFrameIndex = 0;
        private float animationTimer = 0f;
        private float frameTime = 0.0875f;

        // Movement state
        private bool isMoving = false;
        private Vector3 startPosition;
        private Vector3 targetPosition;
        private float moveTimer = 0f;
        private float moveDuration = 0.35f;

        // Current visual state
        private int currentFacing = Direction.Down;

        // Grid service for coordinate conversion
        private GridService gridService;

        // Remote player tracking
        private ulong? trackedPlayerId = null;
        private bool isLocalPlayer = false;

        // ====================================================================
        // INITIALIZATION
        // ====================================================================

        public void InitializeAsLocalPlayer()
        {
            isLocalPlayer = true;
            trackedPlayerId = null;
            gridService = ServiceLocator.Get<GridService>();
            UpdateSprites(currentFacing, 0);
        }

        public void InitializeAsRemotePlayer(ulong playerId)
        {
            isLocalPlayer = false;
            trackedPlayerId = playerId;
            gridService = ServiceLocator.Get<GridService>();
            UpdateSprites(currentFacing, 0);
        }

        // ====================================================================
        // UNITY UPDATE
        // ====================================================================

        private void Update()
        {
            if (isMoving)
                UpdateMovement();
        }

        private void UpdateMovement()
        {
            moveTimer += Time.deltaTime;
            float progress = moveTimer / moveDuration;

            transform.position = Vector3.Lerp(startPosition, targetPosition, progress);

            animationTimer += Time.deltaTime;
            if (animationTimer >= frameTime)
            {
                animationTimer = 0f;
                currentFrameIndex = (currentFrameIndex + 1) % walkFrameSequence.Length;
                UpdateSprites(currentFacing, walkFrameSequence[currentFrameIndex]);
            }

            if (progress >= 1f)
            {
                isMoving = false;
                transform.position = targetPosition;
                UpdateSprites(currentFacing, 0);
            }
        }

        // ====================================================================
        // PUBLIC API - Called by LocalPlayerController or PlayerViewFactory
        // ====================================================================

        public void MoveTo(Vector2Int gridPos, float duration)
        {
            if (gridService == null)
                gridService = ServiceLocator.Get<GridService>();

            moveDuration = duration;
            frameTime = duration / 4f;

            startPosition = transform.position;
            targetPosition = gridService.GridToWorld(gridPos);

            isMoving = true;
            moveTimer = 0f;
            currentFrameIndex = 0;
            animationTimer = 0f;
        }

        public void SnapToPosition(Vector2Int gridPos)
        {
            if (gridService == null)
                gridService = ServiceLocator.Get<GridService>();

            transform.position = gridService.GridToWorld(gridPos);
            isMoving = false;
            UpdateSprites(currentFacing, 0);
        }

        public void SetFacing(int facing)
        {
            currentFacing = facing;
            if (!isMoving)
                UpdateSprites(currentFacing, 0);
        }

        // Called by PlayerViewFactory for remote player updates
        public void UpdateFromServer(Vector2Int position, int facing)
        {
            if (gridService == null)
                gridService = ServiceLocator.Get<GridService>();

            Vector2Int currentGridPos = gridService.WorldToGrid(transform.position);
            
            if (position != currentGridPos)
                MoveTo(position, 0.35f);

            if (facing != currentFacing)
                SetFacing(facing);
        }

        // ====================================================================
        // SPRITE MANAGEMENT
        // ====================================================================

        private int GetSpriteIndex(int facing, int frameOffset)
        {
            return (facing * 3) + frameOffset;
        }

        private void UpdateSprites(int facing, int frameOffset)
        {
            int index = GetSpriteIndex(facing, frameOffset);

            if (bodySprites != null && index < bodySprites.Length && bodyRenderer != null)
                bodyRenderer.sprite = bodySprites[index];

            if (headSprites != null && index < headSprites.Length && headRenderer != null)
            {
                headRenderer.sprite = headSprites[index];
                headRenderer.sortingOrder = (facing == Direction.Up) ? -1 : 1;
            }

            if (weaponSprites != null && index < weaponSprites.Length && weaponRenderer != null)
                weaponRenderer.sprite = weaponSprites[index];
        }
    }
}
