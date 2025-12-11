/// =======================================
/// DyeWarsServer - Player
///
/// Player entity with movement validation.
/// Owns its own state, asks TileMap for collision.
///
/// THREAD SAFETY:
/// --------------
/// Player objects are owned by PlayerRegistry and should ONLY be
/// accessed from the game thread. In debug builds, we verify this
/// using ThreadOwner assertions.
///
/// WHY SINGLE-THREAD:
/// - Player state (position, facing, cooldowns) is complex
/// - Multiple fields must be consistent (can't be mid-move)
/// - Locking every access would be slow and error-prone
/// - Game logic naturally runs on one thread anyway
///
/// HOW TO ACCESS PLAYER FROM IO THREAD:
/// Don't. Use message passing instead:
/// 1. IO thread receives packet
/// 2. IO thread calls QueueAction() with a lambda
/// 3. Game thread executes lambda, accesses player safely
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include "game/TileMap.h"
#include "core/ThreadSafety.h"

/// Result of a movement attempt - explains why movement failed
enum class MoveResult : uint8_t {
    Success,           // Move succeeded
    OnCooldown,        // Too soon since last move
    WrongFacing,       // Player not facing movement direction
    InvalidDirection,  // Direction value out of range (0-3)
    Blocked,           // Tile is not walkable (wall, out of bounds)
    OccupiedByPlayer   // Another player is on the target tile
};

/// ============================================================================
/// PLAYER
///
/// Represents a player entity in the game world.
///
/// Responsibilities:
/// - Own position, facing, identity
/// - Validate movement (cooldown, facing, direction)
/// - Ask TileMap for collision (doesn't own map data)
///
/// Does NOT:
/// - Know about other players (that's World/SpatialHash)
/// - Know about networking (that's ClientConnection)
/// - Manage its own lifecycle (that's PlayerRegistry)
///
/// THREAD SAFETY:
/// All methods that read or modify state assert game thread ownership.
/// The first access sets the owner; subsequent accesses verify it.
/// ============================================================================
class Player {
public:
    explicit Player(
            uint64_t player_id,
            int start_x,
            int start_y,
            uint8_t facing = 0)
            : id_(player_id),
              x_(start_x),
              y_(start_y),
              facing_(facing),
              client_id_(0),
              last_move_time_(std::chrono::steady_clock::now() - std::chrono::seconds(1)) {
        // Owner will be set on first access from game thread
    }

    /// ========================================================================
    /// IDENTITY
    /// ========================================================================

    /// Get player's unique ID. IMMUTABLE - no thread safety needed.
    uint64_t GetID() const { return id_; }

    /// Set which client connection owns this player.
    /// Called once during login setup.
    void SetClientID(uint64_t client_id) {
        AssertGameThread();
        client_id_ = client_id;
    }

    /// Get which client connection owns this player.
    uint64_t GetClientID() const {
        AssertGameThread();
        return client_id_;
    }

    /// Set player's display name.
    void SetName(const std::string &name) {
        AssertGameThread();
        name_ = name;
    }

    /// Get player's display name.
    const std::string &GetName() const {
        AssertGameThread();
        return name_;
    }

    /// ========================================================================
    /// POSITION
    /// ========================================================================

    /// Get current X coordinate.
    int16_t GetX() const {
        AssertGameThread();
        return x_;
    }

    /// Get current Y coordinate.
    int16_t GetY() const {
        AssertGameThread();
        return y_;
    }

    /// Direct position set (for teleport, spawn, admin commands).
    /// Does NOT validate - caller is responsible for checking walkability.
    ///
    /// WHY NO VALIDATION:
    /// This is intentionally "raw" for cases where normal rules don't apply:
    /// - Teleport commands (admin can move anywhere)
    /// - Spawn placement (initial login position)
    /// - Knockback effects (forced movement ignoring blocking)
    ///
    /// For normal movement, use AttemptMove() which validates everything.
    void SetPosition(int16_t x, int16_t y) {
        AssertGameThread();
        x_ = x;
        y_ = y;
    }

    /// ========================================================================
    /// FACING
    /// ========================================================================

    /// Get current facing direction (0=North, 1=East, 2=South, 3=West).
    uint8_t GetFacing() const {
        AssertGameThread();
        return facing_;
    }

    /// Set facing direction directly (0=North, 1=East, 2=South, 3=West).
    /// Ignores invalid values silently.
    void SetFacing(uint8_t facing) {
        AssertGameThread();
        if (facing <= 3) {
            facing_ = facing;
        }
    }

    /// Occupancy check callback type: takes (x, y) and returns true if occupied
    using OccupancyCheck = std::function<bool(int16_t, int16_t)>;

    /// Attempt to move in a direction with full validation.
    ///
    /// VALIDATION ORDER (short-circuits on first failure):
    /// 1. Cooldown - prevents speed hacking
    /// 2. Facing - must face movement direction
    /// 3. Direction - must be valid (0-3)
    /// 4. Tile blocking - walls, out of bounds
    /// 5. Player blocking - another player on target tile
    ///
    /// @param direction Which way to move (0=N, 1=E, 2=S, 3=W)
    /// @param sent_facing The facing direction the client thinks we have
    /// @param map The tilemap for collision checking
    /// @param client_ping_ms Client's ping for cooldown adjustment
    /// @param is_occupied Callback to check if target tile has a player
    /// @return MoveResult explaining success or why it failed
    MoveResult AttemptMove(uint8_t direction, uint8_t sent_facing, const TileMap &map,
                           uint32_t client_ping_ms = 0, OccupancyCheck is_occupied = nullptr) {
        AssertGameThread();
        const auto now = std::chrono::steady_clock::now();
        // ====================================================================
        // COOLDOWN CHECK
        // Prevent speed hacking by enforcing minimum time between moves
        // Adjust for client ping - higher ping = more grace
        // ====================================================================
        auto adjusted_cooldown = GetAdjustedCooldown(client_ping_ms);
        if (now - last_move_time_ < adjusted_cooldown) {
            return MoveResult::OnCooldown;
        }
        // ====================================================================
        // FACING CHECK
        // Player must be facing the direction they want to move
        // This prevents "moonwalking" and ensures animation sync
        // ====================================================================
        if (direction != facing_ || sent_facing != facing_) {
            return MoveResult::WrongFacing;
        }

        // ====================================================================
        // DIRECTION VALIDATION
        // 0=North, 1=East, 2=South, 3=West
        // ====================================================================
        if (direction > 3) {
            return MoveResult::InvalidDirection;
        }

        // ====================================================================
        // CALCULATE NEW POSITION
        // ====================================================================
        int16_t new_x = x_;
        int16_t new_y = y_;

        switch (direction) {
            case 0:
                new_y++;
                break;  // North (Y+ is up in our coord system)
            case 1:
                new_x++;
                break;  // East
            case 2:
                new_y--;
                break;  // South
            case 3:
                new_x--;
                break;  // West
            default:
                return MoveResult::InvalidDirection;
        }

        // ====================================================================
        // TILE COLLISION CHECK
        // Ask the map if destination is walkable (walls, out of bounds)
        // ====================================================================
        if (map.IsTileBlocked(new_x, new_y)) {
            return MoveResult::Blocked;
        }

        // ====================================================================
        // PLAYER COLLISION CHECK
        // Ask if another player occupies the target tile
        // ====================================================================
        if (is_occupied && is_occupied(new_x, new_y)) {
            return MoveResult::OccupiedByPlayer;
        }

        // ====================================================================
        // SUCCESS - Update state
        // ====================================================================
        last_move_time_ = now;
        x_ = new_x;
        y_ = new_y;

        return MoveResult::Success;
    }

    /// Check if player can move (not on cooldown) - uses base cooldown.
    /// Useful for UI hints ("you can move now" indicator).
    bool CheckMoveCooldown() const {
        AssertGameThread();
        auto now = std::chrono::steady_clock::now();
        return (now - last_move_time_) >= std::chrono::milliseconds(BASE_MOVE_COOLDOWN_MS);
    }

    /// Get time until next move is allowed (for client prediction).
    /// Returns 0 if player can move immediately.
    std::chrono::milliseconds TimeUntilCanMove() const {
        AssertGameThread();
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_move_time_);
        auto base_cooldown = std::chrono::milliseconds(BASE_MOVE_COOLDOWN_MS);
        if (elapsed >= base_cooldown) {
            return std::chrono::milliseconds(0);
        }
        return base_cooldown - elapsed;
    }

    /// Attempt to turn to face a new direction.
    /// Turning has its own cooldown (faster than movement).
    /// @param new_facing Direction to face (0=N, 1=E, 2=S, 3=W)
    /// @return true if turn succeeded, false if invalid direction or on cooldown
    bool AttemptTurn(uint8_t new_facing) {
        AssertGameThread();

        if (new_facing > 3 || new_facing == facing_) {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now - last_turn_time_ < TURN_COOLDOWN) {
            return false;
        }

        last_turn_time_ = now;
        facing_ = new_facing;
        return true;
    }


private:
    /// ========================================================================
    /// THREAD SAFETY HELPER
    ///
    /// Called at the start of every method that accesses mutable state.
    /// In debug builds:
    ///   - First call sets the owner thread
    ///   - Subsequent calls verify we're still on that thread
    /// In release builds:
    ///   - Compiles to nothing (zero overhead)
    /// ========================================================================
    void AssertGameThread() const {
        ASSERT_GAME_THREAD(thread_owner_);
        if (!thread_owner_.IsOwnerSet()) {
            thread_owner_.SetOwner();
        }
    }
    /// ========================================================================
    /// CONFIGURATION
    /// ========================================================================

    /// Movement cooldowns
    /// Client sends moves every 350ms
    /// Base cooldown is lower to account for network variance
    static constexpr int BASE_MOVE_COOLDOWN_MS = 280;
    static constexpr int MIN_MOVE_COOLDOWN_MS = 200;   // Floor to prevent speed hacks
    static constexpr int MAX_PING_ADJUSTMENT_MS = 100; // Cap ping adjustment
    static constexpr auto TURN_COOLDOWN = std::chrono::milliseconds(150);  // Client: 220ms

    /// Calculate cooldown adjusted for client ping
    /// Higher ping = lower cooldown (more forgiveness)
    static std::chrono::milliseconds GetAdjustedCooldown(uint32_t ping_ms) {
        // Half of RTT is one-way latency - that's how late packets arrive
        int one_way_latency = static_cast<int>(ping_ms) / 2;

        // Cap the adjustment to prevent abuse
        if (one_way_latency > MAX_PING_ADJUSTMENT_MS) {
            one_way_latency = MAX_PING_ADJUSTMENT_MS;
        }

        // Reduce cooldown by one-way latency
        int adjusted = BASE_MOVE_COOLDOWN_MS - one_way_latency;

        // Never go below minimum (anti-cheat)
        if (adjusted < MIN_MOVE_COOLDOWN_MS) {
            adjusted = MIN_MOVE_COOLDOWN_MS;
        }

        return std::chrono::milliseconds(adjusted);
    }

    // =========================================================================
    // DATA MEMBERS
    // =========================================================================

    /// Thread owner for debug assertions.
    /// MUST be first so it's initialized before other members are accessed.
    /// mutable because AssertGameThread() is called from const methods.
    ///
    /// WHY MUTABLE:
    /// We want to call AssertGameThread() from const methods like GetX().
    /// But AssertGameThread() needs to modify thread_owner_ on first call
    /// to set the owner. Without mutable, we couldn't do this from const.
    ///
    /// This is safe because:
    /// 1. ThreadOwner is thread-safe internally (uses atomics)
    /// 2. Setting the owner is idempotent (doing it twice is fine)
    /// 3. It doesn't affect the logical const-ness of the Player
    mutable ThreadOwner thread_owner_;

    // --- Identity (id_ is immutable, others are mutable) ---
    const uint64_t id_;          // Immutable after construction
    uint64_t client_id_ = 0;     // Set once during login
    std::string name_;           // Can be changed by player

    // --- Position (game thread only) ---
    int16_t x_;
    int16_t y_;
    uint8_t facing_ = 2; // Default: facing down (0=up, 1=right, 2=down, 3=left)

    // --- Cooldown Timers (game thread only) ---
    std::chrono::steady_clock::time_point last_move_time_{};
    std::chrono::steady_clock::time_point last_turn_time_{};
};