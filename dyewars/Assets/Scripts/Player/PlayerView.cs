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
// For remote players, it listens to events to update visuals.
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
        [SerializeField] private Sprite[] bodySprites;   // 12 sprites: 4 directions × 3 frames
        [SerializeField] private Sprite[] headSprites;
        [SerializeField] private Sprite[] weaponSprites;

        // Animation state
        private readonly int[] walkFrameSequence = { 0, 1, 0, 2 };
        private int currentFrameIndex = 0;
        private float animationTimer = 0f;
        private float frameTime = 0.0875f;  // Calculated from move duration

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
        private uint? trackedPlayerId = null;
        private bool isLocalPlayer = false;

        // ====================================================================
        // INITIALIZATION
        // ====================================================================

        /// <summary>
        /// Initialize as local player view.
        /// </summary>
        public void InitializeAsLocalPlayer()
        {
            isLocalPlayer = true;
            trackedPlayerId = null;
            gridService = ServiceLocator.Get<GridService>();

            UpdateSprites(currentFacing, 0);
        }

        /// <summary>
        /// Initialize as remote player view.
        /// </summary>
        public void InitializeAsRemotePlayer(uint playerId)
        {
            isLocalPlayer = false;
            trackedPlayerId = playerId;
            gridService = ServiceLocator.Get<GridService>();

            // Subscribe to events for this specific player
            EventBus.Subscribe<PlayerPositionChangedEvent>(OnRemotePositionChanged);
            EventBus.Subscribe<PlayerFacingChangedEvent>(OnRemoteFacingChanged);

            UpdateSprites(currentFacing, 0);
        }

        private void OnDestroy()
        {
            if (!isLocalPlayer)
            {
                EventBus.Unsubscribe<PlayerPositionChangedEvent>(OnRemotePositionChanged);
                EventBus.Unsubscribe<PlayerFacingChangedEvent>(OnRemoteFacingChanged);
            }
        }

        // ====================================================================
        // UNITY UPDATE
        // ====================================================================

        private void Update()
        {
            if (isMoving)
            {
                UpdateMovement();
            }
        }

        private void UpdateMovement()
        {
            // Advance move timer
            moveTimer += Time.deltaTime;
            float progress = moveTimer / moveDuration;

            // Lerp position
            transform.position = Vector3.Lerp(startPosition, targetPosition, progress);

            // Advance animation
            animationTimer += Time.deltaTime;
            if (animationTimer >= frameTime)
            {
                animationTimer = 0f;
                currentFrameIndex = (currentFrameIndex + 1) % walkFrameSequence.Length;
                UpdateSprites(currentFacing, walkFrameSequence[currentFrameIndex]);
            }

            // Check if complete
            if (progress >= 1f)
            {
                isMoving = false;
                transform.position = targetPosition;
                UpdateSprites(currentFacing, 0);  // Idle frame
            }
        }

        // ====================================================================
        // PUBLIC API
        // ====================================================================

        /// <summary>
        /// Start moving to a grid position with animation.
        /// </summary>
        public void MoveTo(Vector2Int gridPos, float duration)
        {
            if (gridService == null)
            {
                gridService = ServiceLocator.Get<GridService>();
            }

            moveDuration = duration;
            frameTime = duration / 4f;  // 4 frames per move cycle

            startPosition = transform.position;
            targetPosition = gridService.GridToWorld(gridPos);

            isMoving = true;
            moveTimer = 0f;
            currentFrameIndex = 0;
            animationTimer = 0f;
        }

        /// <summary>
        /// Instantly move to a grid position (no animation).
        /// </summary>
        public void SnapToPosition(Vector2Int gridPos)
        {
            if (gridService == null)
            {
                gridService = ServiceLocator.Get<GridService>();
            }

            transform.position = gridService.GridToWorld(gridPos);
            isMoving = false;
            UpdateSprites(currentFacing, 0);
        }

        /// <summary>
        /// Set facing direction and update sprites.
        /// </summary>
        public void SetFacing(int facing)
        {
            currentFacing = facing;

            if (!isMoving)
            {
                UpdateSprites(currentFacing, 0);
            }
        }

        // ====================================================================
        // REMOTE PLAYER EVENT HANDLERS
        // ====================================================================

        private void OnRemotePositionChanged(PlayerPositionChangedEvent evt)
        {
            // Only handle events for our tracked player
            if (evt.PlayerId != trackedPlayerId) return;
            if (evt.IsLocalPlayer) return;

            MoveTo(evt.Position, 0.35f);
        }

        private void OnRemoteFacingChanged(PlayerFacingChangedEvent evt)
        {
            // Only handle events for our tracked player
            if (evt.PlayerId != trackedPlayerId) return;
            if (evt.IsLocalPlayer) return;

            SetFacing(evt.Facing);
        }

        // ====================================================================
        // SPRITE MANAGEMENT
        // ====================================================================

        /// <summary>
        /// Calculate sprite index from facing direction and frame offset.
        /// 
        /// Sprite sheet layout: 3 frames per direction, 4 directions
        /// Index = (facing * 3) + frameOffset
        /// 
        /// Example for facing Right (1), frame 1:
        ///   (1 * 3) + 1 = 4
        /// </summary>
        private int GetSpriteIndex(int facing, int frameOffset)
        {
            return (facing * 3) + frameOffset;
        }

        /// <summary>
        /// Update all sprite layers to show the correct frame.
        /// </summary>
        private void UpdateSprites(int facing, int frameOffset)
        {
            int index = GetSpriteIndex(facing, frameOffset);

            // Body
            if (bodySprites != null && index < bodySprites.Length && bodyRenderer != null)
            {
                bodyRenderer.sprite = bodySprites[index];
            }

            // Head (with sort order adjustment for facing up)
            if (headSprites != null && index < headSprites.Length && headRenderer != null)
            {
                headRenderer.sprite = headSprites[index];
                // When facing up, head should render behind body
                headRenderer.sortingOrder = (facing == Direction.Up) ? -1 : 1;
            }

            // Weapon
            if (weaponSprites != null && index < weaponSprites.Length && weaponRenderer != null)
            {
                weaponRenderer.sprite = weaponSprites[index];
            }
        }
    }
}