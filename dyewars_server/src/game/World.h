/// =======================================
/// DyeWarsServer - World
///
/// Owns all world data:
/// - TileMap: static tile data (terrain, walls)
/// - SpatialHash: dynamic entity positions (players, NPCs)
/// - VisibilityTracker: who can see whom
///
/// Single point of access for all spatial queries.
///
/// THREAD SAFETY:
/// --------------
/// This class is ONLY accessed from the game thread.
/// Child components (SpatialHash, VisibilityTracker) have their own
/// thread safety assertions that will catch violations.
///
/// TileMap is READ-ONLY after construction, so it's inherently thread-safe.
/// If you ever add tile modification (e.g., destructible terrain), you'll
/// need to add synchronization.
///
/// Created by Anonymous on Dec 07, 2025
/// =======================================
#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <cmath>
#include <functional>
#include "TileMap.h"
#include "SpatialHash.h"
#include "VisibilityTracker.h"
#include "core/ThreadSafety.h"

class Player;  // Forward declare

/// ============================================================================
/// WORLD
///
/// The authoritative source for:
/// - "Can I walk here?" (TileMap)
/// - "Who's near here?" (SpatialHash)
/// - "Can player A see player B?" (Combines both)
///
/// Design:
/// - TileMap is pure data, doesn't know about players
/// - SpatialHash tracks player positions, doesn't know about tiles
/// - World coordinates between them
/// ============================================================================
class World {
public:
    /// ========================================================================
    /// CONFIGURATION
    /// ========================================================================

    /// How far players can see (in tiles) in EACH direction.
    ///
    /// VIEW_RANGE = 5 means:
    /// - Player at (5,5) sees from (0,0) to (10,10)
    /// - That's an 11×11 tile rectangle (VIEW_RANGE*2 + 1)
    ///
    /// VIEW_RANGE = 10 means:
    /// - Player at (50,50) sees from (40,40) to (60,60)
    /// - That's a 21×21 tile rectangle
    ///
    /// Adjust based on your client's visible area.
    static constexpr int16_t VIEW_RANGE = 5;  // TODO: Could be uint16_t

    /// ========================================================================
    /// CONSTRUCTION
    /// ========================================================================

    /// Create a world with a new tilemap
    explicit World(int16_t width, int16_t height)  // TODO: Could be uint16_t
            : tilemap_(std::make_unique<TileMap>(width, height)) {
        // Initialize flat grid for O(1) spatial lookups (no hash map overhead)
        spatial_hash_.InitFlatGrid(width, height);
    }

    /// Create a world with an existing tilemap (for loading saved maps)
    explicit World(std::unique_ptr<TileMap> tilemap)
            : tilemap_(std::move(tilemap)) {
        // Initialize flat grid for O(1) spatial lookups
        spatial_hash_.InitFlatGrid(tilemap_->GetWidth(), tilemap_->GetHeight());
    }

    /// ========================================================================
    /// TILEMAP ACCESS - Static World Data
    /// ========================================================================

    /// Get the tilemap (for direct tile queries)
    TileMap &GetMap() { return *tilemap_; }

    const TileMap &GetMap() const { return *tilemap_; }


    /// Convenience: Check if position is in bounds
    bool InBounds(int16_t x, int16_t y) const {  // TODO: Could be uint16_t
        return tilemap_->InBounds(x, y);
    }

    /// ========================================================================
    /// PLAYER MANAGEMENT - Dynamic Entity Tracking
    /// ========================================================================

    /// Add a player to the world
    /// Call when player spawns or enters this world/zone
    void AddPlayer(uint64_t player_id,
                   int16_t x,  // TODO: Could be uint16_t
                   int16_t y,  // TODO: Could be uint16_t
                   std::shared_ptr<Player> player = nullptr) {
        spatial_hash_.Add(player_id, x, y, player);
    }

    /// Remove a player from the world
    /// Call when player despawns, disconnects, or changes zones
    void RemovePlayer(uint64_t player_id) {
        spatial_hash_.Remove(player_id);
    }

    /// Update a player's position
    /// Call when player moves. Returns true if player changed spatial cells.
    bool UpdatePlayerPosition(uint64_t player_id,
                              int16_t new_x,   // TODO: Could be uint16_t
                              int16_t new_y) { // TODO: Could be uint16_t
        return spatial_hash_.Update(player_id, new_x, new_y);
    }

    /// Get a player by ID
    std::shared_ptr<Player> GetPlayer(uint64_t player_id) const {
        return spatial_hash_.GetEntity(player_id);
    }

    /// Check if player exists in this world
    bool HasPlayer(uint64_t player_id) const {
        return spatial_hash_.Contains(player_id);
    }

    /// Check if a position is occupied by another player
    /// Used for collision detection during movement
    bool IsPositionOccupied(int16_t x, int16_t y, uint64_t exclude_player_id = 0) const {
        return spatial_hash_.IsPlayerAt(x, y, exclude_player_id);
    }

    /// Get total player count in this world
    size_t PlayerCount() const {
        return spatial_hash_.Count();
    }

    /// ========================================================================
    /// SPATIAL QUERIES - Range and Visibility
    /// ========================================================================

    /// Get all players within VIEW_RANGE of a position
    ///
    /// Example with VIEW_RANGE = 5:
    /// GetPlayersInRange(10, 10) returns players in rectangle (5,5) to (15,15)
    std::vector<std::shared_ptr<Player>> GetPlayersInRange(int16_t x, int16_t y) const {
        return GetPlayersInRange(x, y, VIEW_RANGE);
    }

    /// Get all players within a custom range of a position
    /// Range is distance in each direction (total area = range*2+1 squared)
    std::vector<std::shared_ptr<Player>> GetPlayersInRange(int16_t x,      // TODO: Could be uint16_t
                                                           int16_t y,      // TODO: Could be uint16_t
                                                           int16_t range)  // TODO: Could be uint16_t
    const {
        // Coarse filter: get candidates from spatial hash
        auto candidates = spatial_hash_.GetNearbyEntities(x, y, range);

        // Fine filter: exact distance check (rectangular)
        std::vector<std::shared_ptr<Player>> result;
        result.reserve(candidates.size());

        for (const auto &player: candidates) {
            if (IsInRange(x, y, player->GetX(), player->GetY(), range)) {
                result.push_back(player);
            }
        }
        return result;
    }

    /// Zero-copy iteration over players in VIEW_RANGE.
    /// Calls func for each player. No vector allocation or shared_ptr copies.
    /// Use in hot paths like BroadcastDirtyPlayers.
    template<typename Func>
    void ForEachPlayerInRange(int16_t x, int16_t y, Func&& func) const {
        ForEachPlayerInRange(x, y, VIEW_RANGE, std::forward<Func>(func));
    }

    /// Zero-copy iteration with custom range.
    template<typename Func>
    void ForEachPlayerInRange(int16_t x, int16_t y, int16_t range, Func&& func) const {
        spatial_hash_.ForEachNearby(x, y, range, [&](const std::shared_ptr<Player>& player) {
            if (IsInRange(x, y, player->GetX(), player->GetY(), range)) {
                func(player);
            }
        });
    }

    /// Get all player IDs within VIEW_RANGE (when you just need IDs)
    std::vector<uint64_t> GetPlayerIDsInRange(int16_t x, int16_t y) const {
        return GetPlayerIDsInRange(x, y, VIEW_RANGE);
    }

    /// Get all player IDs within a custom range
    std::vector<uint64_t> GetPlayerIDsInRange(int16_t x,      // TODO: Could be uint16_t
                                              int16_t y,      // TODO: Could be uint16_t
                                              int16_t range)  // TODO: Could be uint16_t
    const {
        auto candidates = spatial_hash_.GetNearbyIDs(x, y, range);

        // Fine filter with distance check
        std::vector<uint64_t> result;
        result.reserve(candidates.size());

        for (uint64_t id: candidates) {
            auto player = spatial_hash_.GetEntity(id);
            if (player && IsInRange(x, y, player->GetX(), player->GetY(), range)) {
                result.push_back(id);
            }
        }
        return result;
    }

    /// Get all players that can see a specific position
    /// (Same as GetPlayersInRange - if A can see B, B can see A)
    std::vector<std::shared_ptr<Player>> GetViewersOf(int16_t x, int16_t y) const {
        return GetPlayersInRange(x, y, VIEW_RANGE);
    }

    /// ========================================================================
    /// VISIBILITY CHECKS
    /// ========================================================================

    /// Check if two positions are within view range of each other
    /// Uses rectangular distance (faster than circular, matches tile-based games)
    bool IsInView(int16_t x1, int16_t y1, int16_t x2, int16_t y2) const {
        return IsInRange(x1, y1, x2, y2, VIEW_RANGE);
    }

    /// Check if two positions are within a custom range
    /// Rectangular distance: max of |dx| and |dy| must be <= range
    static bool IsInRange(int16_t x1, int16_t y1,   // TODO: Could be uint16_t
                          int16_t x2, int16_t y2,   // TODO: Could be uint16_t
                          int16_t range) {          // TODO: Could be uint16_t
        int16_t dx = (x1 > x2) ? (x1 - x2) : (x2 - x1);  // abs without cmath
        int16_t dy = (y1 > y2) ? (y1 - y2) : (y2 - y1);
        return (dx <= range && dy <= range);
    }

    /// Check if a player can see a position
    bool CanPlayerSee(uint64_t player_id, int16_t x, int16_t y) const {
        auto player = GetPlayer(player_id);
        if (!player) return false;
        return IsInView(player->GetX(), player->GetY(), x, y);
    }

    /// Check if player A can see player B
    bool CanSee(uint64_t viewer_id, uint64_t target_id) const {
        auto viewer = GetPlayer(viewer_id);
        auto target = GetPlayer(target_id);
        if (!viewer || !target) return false;
        return IsInView(viewer->GetX(), viewer->GetY(), target->GetX(), target->GetY());
    }

    /// ========================================================================
    /// ITERATION
    /// ========================================================================

    /// Iterate over all players in this world
    void ForEachPlayer(const std::function<void(uint64_t, const std::shared_ptr<Player> &)> &func) const {
        spatial_hash_.ForEach(func);
    }

    /// Get all players (copy of shared_ptrs)
    std::vector<std::shared_ptr<Player>> GetAllPlayers() const {
        std::vector<std::shared_ptr<Player>> result;
        result.reserve(PlayerCount());
        ForEachPlayer([&result](uint64_t, const std::shared_ptr<Player> &p) {
            result.push_back(p);
        });
        return result;
    }

    /// ========================================================================
    /// DEBUG / STATS
    /// ========================================================================

    /// Get number of active spatial hash cells
    size_t ActiveCellCount() const {
        return spatial_hash_.CellCount();
    }

    /// ========================================================================
    /// VISIBILITY TRACKING
    /// Access to the VisibilityTracker for enter/leave view events
    /// See VisibilityTracker.h for detailed documentation
    /// ========================================================================

    VisibilityTracker& Visibility() { return visibility_; }
    const VisibilityTracker& Visibility() const { return visibility_; }

private:
    /// ========================================================================
    /// DATA
    /// ========================================================================

    /// Static world data: tiles, terrain, walls
    std::unique_ptr<TileMap> tilemap_;

    /// Dynamic entity tracking: player positions
    SpatialHash spatial_hash_;

    /// Visibility tracking: who each player knows about
    VisibilityTracker visibility_;
};