// InputService.cs
// Centralized input handling. Tracks key states, direction changes, and timing.
// Publishes input events that other systems can subscribe to.
//
// This extracts all the input tracking logic into one place:
//   - Current direction held
//   - Time since key release (for pivot grace period)
//   - Direction change detection

using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.SceneManagement;
using DyeWars.Core;

namespace DyeWars.Input
{
    public class InputService : MonoBehaviour
    {
        // Input state tracking
        private int currentDirection = Direction.None;
        private int lastDirection = Direction.None;
        private float timeSinceKeyRelease = 999f;
        private float capturedTimeSinceRelease = 999f;
        private bool directionChangedThisFrame = false;

        // Public read-only access
        public int CurrentDirection => currentDirection;
        public int LastDirection => lastDirection;
        public float TimeSinceKeyRelease => timeSinceKeyRelease;
        public bool IsAnyDirectionHeld => currentDirection != Direction.None;
        public bool DirectionChangedThisFrame => directionChangedThisFrame;

        // ====================================================================
        // UNITY LIFECYCLE
        // ====================================================================

        private void Awake()
        {
            ServiceLocator.Register<InputService>(this);
        }

        private void OnDestroy()
        {
            ServiceLocator.Unregister<InputService>();
        }

        private void Update()
        {
            ProcessInput();
        }

        // ====================================================================
        // INPUT PROCESSING
        // ====================================================================

        private void ProcessInput()
        {
            if (Keyboard.current.leftCtrlKey.isPressed && Keyboard.current.rKey.wasPressedThisFrame)
            {
                SceneManager.LoadScene(SceneManager.GetActiveScene().name);
            }

            // Read current direction from keyboard
            currentDirection = ReadDirectionFromKeyboard();

            bool isKeyHeld = currentDirection != Direction.None;

            // Detect direction change (new direction pressed)
            directionChangedThisFrame = (currentDirection != Direction.None && currentDirection != lastDirection);

            // Capture time only when direction changes
            // This gives us accurate timing for pivot grace period
            if (directionChangedThisFrame)
            {
                capturedTimeSinceRelease = timeSinceKeyRelease;
            }

            // Track time since any key was released
            if (isKeyHeld)
            {
                timeSinceKeyRelease = 0f;
            }
            else
            {
                timeSinceKeyRelease += Time.deltaTime;
            }

            // Publish input event if a direction is held
            if (currentDirection != Direction.None)
            {
                EventBus.Publish(new DirectionInputEvent
                {
                    Direction = currentDirection,
                    TimeSinceRelease = capturedTimeSinceRelease,
                    IsNewDirection = directionChangedThisFrame
                });
            }

            lastDirection = currentDirection;
        }

        private int ReadDirectionFromKeyboard()
        {
            if (Keyboard.current == null) return Direction.None;

            // Priority order: up > right > down > left
            // This determines which direction wins if multiple keys are held
            if (Keyboard.current.upArrowKey.isPressed) return Direction.Up;
            if (Keyboard.current.rightArrowKey.isPressed) return Direction.Right;
            if (Keyboard.current.downArrowKey.isPressed) return Direction.Down;
            if (Keyboard.current.leftArrowKey.isPressed) return Direction.Left;

            return Direction.None;
        }

        // ====================================================================
        // PUBLIC HELPERS
        // ====================================================================

        /// <summary>
        /// Check if a specific direction key is currently held.
        /// </summary>
        public bool IsDirectionHeld(int direction)
        {
            return currentDirection == direction;
        }

        /// <summary>
        /// Get the captured time since release (used for pivot grace period).
        /// This value is only updated when the direction changes.
        /// </summary>
        public float GetCapturedTimeSinceRelease()
        {
            return capturedTimeSinceRelease;
        }
    }
}