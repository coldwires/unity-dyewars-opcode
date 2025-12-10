/// =======================================
/// DyeWarsServer - Player
///
/// Player entity with movement validation.
/// Owns its own state, asks TileMap for collision.
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <cstdint>
#include <atomic>
#include <chrono>
#include <functional>
#include "game/TileMap.h"

/// Forward declare MoveResult (defined in PlayerRegistry.h)
/// Or define it here if you prefer Player to be standalone
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

    }

    /// ========================================================================
    /// IDENTITY
    /// ========================================================================

    uint64_t GetID() const { return id_; }

    void SetClientID(uint64_t client_id) { client_id_ = client_id; }

    // Client link (which network connection owns this player)
    uint64_t GetClientID() const { return client_id_; }

    void SetName(const std::string &name) { name_ = name; }

    const std::string &GetName() const { return name_; }

    /// ========================================================================
    /// POSITION
    /// ========================================================================

    int16_t GetX() const { return x_; }

    int16_t GetY() const { return y_; }

    /// Direct position set (for teleport, spawn, admin commands) \n
    /// Does NOT validate - caller is responsible for checking walkability
    void SetPosition(int16_t x, int16_t y) {
        x_ = x;
        y_ = y;
    }

    /// ========================================================================
    /// FACING
    /// ========================================================================

    uint8_t GetFacing() const { return facing_; }

    /// Set facing direction (0=North, 1=East, 2=South, 3=West)
    void SetFacing(uint8_t facing) {
        if (facing <= 3) {
            facing_ = facing;
        }
    }

    /// Occupancy check callback type: takes (x, y) and returns true if occupied
    using OccupancyCheck = std::function<bool(int16_t, int16_t)>;

    MoveResult AttemptMove(uint8_t direction, uint8_t sent_facing, const TileMap &map,
                           uint32_t client_ping_ms = 0, OccupancyCheck is_occupied = nullptr) {
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

    /// Check if player can move (not on cooldown) - uses base cooldown
    bool CheckMoveCooldown() const {
        auto now = std::chrono::steady_clock::now();
        return (now - last_move_time_) >= std::chrono::milliseconds(BASE_MOVE_COOLDOWN_MS);
    }

    /// Get time until next move is allowed (for client prediction) - uses base cooldown
    std::chrono::milliseconds TimeUntilCanMove() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_move_time_);
        auto base_cooldown = std::chrono::milliseconds(BASE_MOVE_COOLDOWN_MS);
        if (elapsed >= base_cooldown) {
            return std::chrono::milliseconds(0);
        }
        return base_cooldown - elapsed;
    }

    bool AttemptTurn(uint8_t new_facing) {
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

    // Identity
    uint64_t id_;
    uint64_t client_id_ = 0;
    std::string name_;

    // Position
    int16_t x_;
    int16_t y_;
    uint8_t facing_ = 2; // Default: facing down (0=up, 1=right, 2=down, 3=left)

    // Move Time Limits
    std::chrono::steady_clock::time_point last_move_time_{};
    std::chrono::steady_clock::time_point last_turn_time_{};
};