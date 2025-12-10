// LocalPlayerController.cs
// Handles local player input processing, movement logic, and state.
// This is the "Controller" in MVC - it connects input to model updates.
//
// Responsibilities:
//   - Listen for input events
//   - Decide whether to move, turn, or queue
//   - Handle pivot grace period logic
//   - Manage movement cooldowns
//   - Trigger network sends
//   - Communicate with PlayerView for animations

using System.Collections;
using UnityEngine;
using DyeWars.Core;
using DyeWars.Network;
using DyeWars.Game;
using DyeWars.Input;

namespace DyeWars.Player
{
    public class LocalPlayerController : MonoBehaviour
    {
        [Header("Movement Settings")]
        [SerializeField] private float moveDuration = 0.35f;
        [SerializeField] private float turnCooldown = 0.22f;
        [SerializeField] private float pivotGraceTime = 0.01f;

        // State
        private bool isMoving = false;
        private float cooldownTimer = 0f;
        private int queuedDirection = Direction.None;
        private int currentFacing = Direction.Down;
        private Vector2Int currentPosition = Vector2Int.zero;

        // Service references (cached for performance)
        private INetworkService networkService;
        private PlayerRegistry playerRegistry;
        private GridService gridService;
        private InputService inputService;

        // The visual representation of this player
        private PlayerView playerView;

        // Public access
        public bool IsMoving => isMoving;
        public bool IsBusy => isMoving || cooldownTimer > 0f;
        public int CurrentFacing => currentFacing;
        public Vector2Int CurrentPosition => currentPosition;
        public float MoveDuration => moveDuration;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private bool isInitialized = false;

        private void Start()
        {
            // Cache service references
            networkService = ServiceLocator.Get<INetworkService>();
            playerRegistry = ServiceLocator.Get<PlayerRegistry>();
            gridService = ServiceLocator.Get<GridService>();
            inputService = ServiceLocator.Get<InputService>();

            // Get the view component on this GameObject
            playerView = GetComponent<PlayerView>();
        }

        private void OnEnable()
        {
            EventBus.Subscribe<DirectionInputEvent>(OnDirectionInput);
            EventBus.Subscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPlayerPositionCorrected);
            EventBus.Subscribe<LocalPlayerFacingChangedEvent>(OnLocalPlayerFacingChanged);
        }

        /// <summary>
        /// Initialize the controller with position and facing.
        /// Called by PlayerViewFactory after instantiation.
        /// </summary>
        public void Initialize(Vector2Int position, int facing)
        {
            if (isInitialized)
            {
                Debug.LogWarning("LocalPlayerController: Already initialized!");
                return;
            }

            currentPosition = position;
            currentFacing = facing;
            isInitialized = true;
            Debug.Log($"LocalPlayerController: Initialized at {currentPosition}, facing {Direction.GetName(currentFacing)}");
        }

        private void OnDisable()
        {
            EventBus.Unsubscribe<DirectionInputEvent>(OnDirectionInput);
            EventBus.Unsubscribe<LocalPlayerPositionCorrectedEvent>(OnLocalPlayerPositionCorrected);
            EventBus.Unsubscribe<LocalPlayerFacingChangedEvent>(OnLocalPlayerFacingChanged);
            StopAllCoroutines();
        }

        private void Update()
        {
            // Tick cooldown timer
            if (cooldownTimer > 0f)
            {
                cooldownTimer -= Time.deltaTime;
            }
        }

        // ====================================================================
        // EVENT HANDLERS
        // ====================================================================

        private void OnDirectionInput(DirectionInputEvent evt)
        {
            if (!isInitialized) return;
            HandleDirectionInput(evt.Direction, evt.TimeSinceRelease);
        }

        private void OnLocalPlayerPositionCorrected(LocalPlayerPositionCorrectedEvent evt)
        {
            if (!isInitialized) return;

            // Server correction
            int dx = Mathf.Abs(currentPosition.x - evt.Position.x);
            int dy = Mathf.Abs(currentPosition.y - evt.Position.y);

            if (dx > 1 || dy > 1)
            {
                // Large correction - snap
                SnapToPosition(evt.Position);
            }
            else if (dx > 0 || dy > 0)
            {
                // Small correction - lerp
                currentPosition = evt.Position;
                playerView?.MoveTo(evt.Position, moveDuration);
            }
        }

        private void OnLocalPlayerFacingChanged(LocalPlayerFacingChangedEvent evt)
        {
            if (!isInitialized) return;

            if (currentFacing != evt.Facing)
            {
                currentFacing = evt.Facing;
                playerView?.SetFacing(evt.Facing);
            }
        }

        // ====================================================================
        // INPUT HANDLING
        // ====================================================================

        private void HandleDirectionInput(int direction, float timeSinceRelease)
        {
            // If we're moving, queue the direction for later
            if (isMoving)
            {
                if (direction != currentFacing)
                {
                    queuedDirection = direction;
                }
                return;
            }

            // If we're in cooldown, ignore input
            if (cooldownTimer > 0f) return;

            // Already facing this direction? Move!
            if (currentFacing == direction)
            {
                TryMove(direction);
            }
            else
            {
                // Turn to face this direction
                Turn(direction, timeSinceRelease);
            }
        }

        // ====================================================================
        // MOVEMENT
        // ====================================================================

        private void TryMove(int direction)
        {
            // Calculate predicted position
            Vector2Int predictedPos = currentPosition + Direction.GetDelta(direction);

            // Bounds check (server will also validate)
            if (gridService != null && !gridService.IsInBounds(predictedPos))
            {
                Debug.Log("TryMove: Out of bounds");
                return;
            }

            // If the desired tile contains a player
            if (playerRegistry != null && playerRegistry.IsPositionOccupied(predictedPos))
            {
                return;
            }


            // Send to server
            networkService?.Sender.SendMove(direction, currentFacing);

            // Client-side prediction
            currentPosition = predictedPos;
            playerRegistry?.PredictLocalPosition(predictedPos);

            // Start animation
            isMoving = true;
            playerView?.MoveTo(predictedPos, moveDuration);

            // Wait for move to complete
            StartCoroutine(WaitForMoveComplete());
        }

        private IEnumerator WaitForMoveComplete()
        {
            yield return new WaitForSeconds(moveDuration);
            OnMoveComplete();
        }

        private void OnMoveComplete()
        {
            isMoving = false;

            // Process queued direction
            if (queuedDirection != Direction.None)
            {
                int direction = queuedDirection;
                queuedDirection = Direction.None;

                // Check current input state
                int currentInput = inputService?.CurrentDirection ?? Direction.None;

                int directionToUse;
                if (currentInput == Direction.None)
                {
                    // No key held, use queued
                    directionToUse = direction;
                }
                else if (currentInput == direction)
                {
                    // Still holding queued direction
                    directionToUse = direction;
                }
                else
                {
                    // Holding different direction, use that
                    directionToUse = currentInput;
                }

                // Execute turn (0ms since it was queued during movement)
                if (directionToUse != currentFacing)
                {
                    Turn(directionToUse, 0f);
                }
            }
        }

        // ====================================================================
        // TURNING
        // ====================================================================

        private void Turn(int direction, float timeSinceRelease)
        {
            currentFacing = direction;

            // Send to server
            networkService?.Sender.SendTurn(direction);

            // Update registry
            playerRegistry?.SetLocalFacing(direction);

            // Update visual
            playerView?.SetFacing(direction);

            // Apply cooldown based on pivot grace
            // If the player released and pressed quickly (< pivotGraceTime),
            // they're doing a seamless pivot - no cooldown
            // If they waited longer, apply cooldown so they can turn without moving
            if (timeSinceRelease > pivotGraceTime)
            {
                cooldownTimer = turnCooldown;
            }
        }

        // ====================================================================
        // CORRECTIONS
        // ====================================================================

        private void SnapToPosition(Vector2Int position)
        {
            currentPosition = position;
            isMoving = false;
            queuedDirection = Direction.None;
            playerView?.SnapToPosition(position);
        }
    }
}