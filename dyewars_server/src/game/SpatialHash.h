/// =======================================
/// DyeWarsServer - SpatialHash
/// O(1) spatial lookups for dynamic entities
///
/// Divides the world into grid cells. Each cell tracks which entities are in it.
/// Used by World for efficient "who's nearby" queries.
///
/// THREAD SAFETY:
/// --------------
/// This class is ONLY accessed from the game thread.
/// In debug builds, we verify this with ThreadOwner assertions.
///
/// WHY GAME-THREAD ONLY:
/// The spatial hash uses nested data structures (maps of sets).
/// Concurrent access would require complex locking and could cause:
/// - Iterator invalidation during iteration
/// - Torn reads of multi-step operations
/// - Deadlocks with nested lock acquisition
///
/// Since all spatial updates (player movement, spawn, despawn) happen
/// during the game loop, single-thread access is natural and efficient.
///
/// Created by Anonymous on Dec 07, 2025
/// =======================================
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cstdint>
#include <functional>
#include <algorithm>

#include "Player.h"
#include "core/ThreadSafety.h"

/// ============================================================================
/// SPATIAL HASH
///
/// Grid-based spatial partitioning for O(K) range queries.
///
/// Example: VIEW_RANGE = 5, player at (5,5) sees rectangle (0,0) to (10,10)
///
/// Without spatial hash: "Who's near X?" = check all N entities = O(N)
/// With spatial hash:    "Who's near X?" = check K entities in nearby cells = O(K)
///
/// K is typically 10-50, while N could be 500+. Big difference!
/// ============================================================================
class SpatialHash {
public:
    /// ========================================================================
    /// CONFIGURATION
    /// ========================================================================

    /// Cell size in tiles.
    ///
    /// With VIEW_RANGE = 5 (11x11 view area):
    /// - CELL_SIZE = 11 means each cell exactly covers the view range
    /// - cells_radius = (5 / 11) + 1 = 0 + 1 = 1, so 3x3 = 9 cells worst case
    /// - But most queries hit only 1-4 cells since cell boundaries are rare
    ///
    /// Larger cells = fewer cells to check, more entities per cell
    /// Smaller cells = more cells to check, fewer entities per cell
    static constexpr int16_t CELL_SIZE = 11;  // Match VIEW_RANGE * 2 + 1

    /// ========================================================================
    /// ENTITY MANAGEMENT
    /// ========================================================================

    /// Add an entity to the spatial hash.
    /// Call when entity spawns or enters the world.
    void Add(uint64_t entity_id,
             int16_t x,    // TODO: Could be uint16_t
             int16_t y,    // TODO: Could be uint16_t
             std::shared_ptr<Player> entity = nullptr) {
        AssertGameThread();
        int64_t key = CellKey(x, y);
        cells_[key].insert(entity_id);
        entity_cells_[entity_id] = key;

        if (entity) {
            entity_ptrs_[entity_id] = entity;
            cell_entities_[key].push_back(entity);

            // Also add to flat grid if enabled
            if (use_flat_grid_) {
                int32_t cx = x / CELL_SIZE;
                int32_t cy = y / CELL_SIZE;
                if (cx >= 0 && cx < grid_width_ && cy >= 0 && cy < grid_height_) {
                    flat_grid_[cy * grid_width_ + cx].push_back(entity);
                }
            }
        }
    }

    /// Remove an entity from the spatial hash.
    /// Call when entity despawns or leaves the world.
    void Remove(uint64_t entity_id) {
        AssertGameThread();
        auto it = entity_cells_.find(entity_id);
        if (it == entity_cells_.end()) return;

        int64_t key = it->second;
        cells_[key].erase(entity_id);

        // Remove from cell_entities_ vector
        auto cell_it = cell_entities_.find(key);
        if (cell_it != cell_entities_.end()) {
            auto& vec = cell_it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(),
                [entity_id](const std::shared_ptr<Player>& p) {
                    return p && p->GetID() == entity_id;
                }), vec.end());
            if (vec.empty()) {
                cell_entities_.erase(cell_it);
            }
        }

        // Remove from flat grid if enabled
        if (use_flat_grid_) {
            // Derive cell from key (NOT from player position - it may have changed!)
            int32_t cx = static_cast<int32_t>(key >> 32);
            int32_t cy = static_cast<int32_t>(key & 0xFFFFFFFF);
            if (cx >= 0 && cx < grid_width_ && cy >= 0 && cy < grid_height_) {
                auto& vec = flat_grid_[cy * grid_width_ + cx];
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [entity_id](const std::shared_ptr<Player>& p) {
                        return p && p->GetID() == entity_id;
                    }), vec.end());
            }
        }

        // Clean up empty cells to prevent memory bloat
        if (cells_[key].empty()) {
            cells_.erase(key);
        }

        entity_cells_.erase(it);
        entity_ptrs_.erase(entity_id);
    }

    /// Update an entity's position.
    /// Call when entity moves. Only modifies data if entity changed cells.
    /// Returns true if entity changed cells (useful for enter/leave events).
    bool Update(uint64_t entity_id,
                int16_t new_x,   // TODO: Could be uint16_t
                int16_t new_y) { // TODO: Could be uint16_t
        AssertGameThread();
        auto it = entity_cells_.find(entity_id);
        if (it == entity_cells_.end()) {
            return false;  // Entity not in spatial hash, can't update
        }

        int64_t new_key = CellKey(new_x, new_y);

        // Same cell? No update needed (common case!)
        if (it->second == new_key) {
            return false;
        }

        int64_t old_key = it->second;

        // Remove from old cell (ID set)
        cells_[old_key].erase(entity_id);
        if (cells_[old_key].empty()) {
            cells_.erase(old_key);
        }

        // Move entity pointer from old cell to new cell
        auto ptr_it = entity_ptrs_.find(entity_id);
        if (ptr_it != entity_ptrs_.end() && ptr_it->second) {
            // Remove from old cell_entities_
            auto old_cell_it = cell_entities_.find(old_key);
            if (old_cell_it != cell_entities_.end()) {
                auto& old_vec = old_cell_it->second;
                old_vec.erase(std::remove_if(old_vec.begin(), old_vec.end(),
                    [entity_id](const std::shared_ptr<Player>& p) {
                        return p && p->GetID() == entity_id;
                    }), old_vec.end());
                if (old_vec.empty()) {
                    cell_entities_.erase(old_cell_it);
                }
            }
            cell_entities_[new_key].push_back(ptr_it->second);

            // Update flat grid if enabled
            if (use_flat_grid_) {
                // Derive old cell from old_key (NOT from player position - it's already updated!)
                int32_t old_cx = static_cast<int32_t>(old_key >> 32);
                int32_t old_cy = static_cast<int32_t>(old_key & 0xFFFFFFFF);
                int32_t new_cx = new_x / CELL_SIZE;
                int32_t new_cy = new_y / CELL_SIZE;

                // Remove from old flat cell
                if (old_cx >= 0 && old_cx < grid_width_ && old_cy >= 0 && old_cy < grid_height_) {
                    auto& old_vec = flat_grid_[old_cy * grid_width_ + old_cx];
                    old_vec.erase(std::remove_if(old_vec.begin(), old_vec.end(),
                        [entity_id](const std::shared_ptr<Player>& p) {
                            return p && p->GetID() == entity_id;
                        }), old_vec.end());
                }
                // Add to new flat cell
                if (new_cx >= 0 && new_cx < grid_width_ && new_cy >= 0 && new_cy < grid_height_) {
                    flat_grid_[new_cy * grid_width_ + new_cx].push_back(ptr_it->second);
                }
            }
        }

        // Add to new cell (ID set)
        cells_[new_key].insert(entity_id);
        entity_cells_[entity_id] = new_key;

        return true;  // Changed cells
    }

    /// ========================================================================
    /// SPATIAL QUERIES
    /// ========================================================================

    /// Get all entity IDs within range of a position.
    /// Returns entities in cells that overlap with the range.
    /// Note: This is a COARSE filter. Caller should do exact distance check.
    ///
    /// For a 2D tile RPG with VIEW_RANGE = 5:
    /// Player at (5,5) sees tiles (0,0) to (10,10) — an 11×11 rectangle
    std::vector<uint64_t> GetNearbyIDs(int16_t x,      // TODO: Could be uint16_t
                                       int16_t y,      // TODO: Could be uint16_t
                                       int16_t range)  // TODO: Could be uint16_t
    const {
        AssertGameThread();
        std::vector<uint64_t> result;

        // Convert position to cell index
        int32_t center_cx = x / CELL_SIZE;
        int32_t center_cy = y / CELL_SIZE;

        // How many cells do we need to check in each direction?
        // +1 to handle entities at cell boundaries
        int32_t cells_radius = (range / CELL_SIZE) + 1;

        // Check all cells in the square region around the center cell
        for (int32_t dcx = -cells_radius; dcx <= cells_radius; dcx++) {
            for (int32_t dcy = -cells_radius; dcy <= cells_radius; dcy++) {
                int32_t cx = center_cx + dcx;
                int32_t cy = center_cy + dcy;

                // Skip negative cells (map starts at 0,0)
                if (cx < 0 || cy < 0) continue;

                int64_t key = MakeCellKey(cx, cy);

                auto it = cells_.find(key);
                if (it != cells_.end()) {
                    for (uint64_t id: it->second) {
                        result.push_back(id);
                    }
                }
            }
        }
        return result;
    }

    /// Get all entity pointers within range.
    /// More convenient when you need actual entity data.
    /// Optimized: uses flat_grid_ for O(1) cell access when available.
    std::vector<std::shared_ptr<Player>> GetNearbyEntities(int16_t x,     // TODO: Could be uint16_t
                                                           int16_t y,     // TODO: Could be uint16_t
                                                           int16_t range) // TODO: Could be uint16_t
    const {
        AssertGameThread();
        std::vector<std::shared_ptr<Player>> result;

        // Convert position to cell index
        int32_t center_cx = x / CELL_SIZE;
        int32_t center_cy = y / CELL_SIZE;
        int32_t cells_radius = (range / CELL_SIZE) + 1;

        // Check all cells in the square region around the center cell
        for (int32_t dcx = -cells_radius; dcx <= cells_radius; dcx++) {
            for (int32_t dcy = -cells_radius; dcy <= cells_radius; dcy++) {
                int32_t cx = center_cx + dcx;
                int32_t cy = center_cy + dcy;

                if (cx < 0 || cy < 0) continue;

                // Use flat grid if available for O(1) cell access
                if (use_flat_grid_ && cx < grid_width_ && cy < grid_height_) {
                    size_t idx = cy * grid_width_ + cx;
                    for (const auto& entity : flat_grid_[idx]) {
                        if (entity) {
                            result.push_back(entity);
                        }
                    }
                } else {
                    int64_t key = MakeCellKey(cx, cy);
                    auto cell_it = cell_entities_.find(key);
                    if (cell_it != cell_entities_.end()) {
                        for (const auto& entity : cell_it->second) {
                            if (entity) {
                                result.push_back(entity);
                            }
                        }
                    }
                }
            }
        }
        return result;
    }

    /// Zero-copy iteration over nearby entities.
    /// Calls func for each entity in range. No vector allocation or shared_ptr copies.
    /// Use this in hot paths instead of GetNearbyEntities().
    template<typename Func>
    void ForEachNearby(int16_t x, int16_t y, int16_t range, Func&& func) const {
        AssertGameThread();

        int32_t center_cx = x / CELL_SIZE;
        int32_t center_cy = y / CELL_SIZE;
        int32_t cells_radius = (range / CELL_SIZE) + 1;

        for (int32_t dcx = -cells_radius; dcx <= cells_radius; dcx++) {
            for (int32_t dcy = -cells_radius; dcy <= cells_radius; dcy++) {
                int32_t cx = center_cx + dcx;
                int32_t cy = center_cy + dcy;

                if (cx < 0 || cy < 0) continue;

                // Use flat grid if available, otherwise fall back to hash map
                if (use_flat_grid_ && cx < grid_width_ && cy < grid_height_) {
                    size_t idx = cy * grid_width_ + cx;
                    for (const auto& entity : flat_grid_[idx]) {
                        if (entity) {
                            func(entity);
                        }
                    }
                } else {
                    int64_t key = MakeCellKey(cx, cy);
                    auto cell_it = cell_entities_.find(key);
                    if (cell_it != cell_entities_.end()) {
                        for (const auto& entity : cell_it->second) {
                            if (entity) {
                                func(entity);
                            }
                        }
                    }
                }
            }
        }
    }

    /// Initialize flat grid for known world size (call once at startup)
    /// This eliminates hash lookups for much faster spatial queries
    void InitFlatGrid(int16_t world_width, int16_t world_height) {
        grid_width_ = (world_width / CELL_SIZE) + 1;
        grid_height_ = (world_height / CELL_SIZE) + 1;
        flat_grid_.resize(grid_width_ * grid_height_);
        use_flat_grid_ = true;
    }

    /// ========================================================================
    /// ENTITY LOOKUP
    /// ========================================================================

    /// Get entity pointer by ID.
    std::shared_ptr<Player> GetEntity(uint64_t entity_id) const {
        AssertGameThread();
        auto it = entity_ptrs_.find(entity_id);
        return (it != entity_ptrs_.end()) ? it->second : nullptr;
    }

    /// Check if entity exists in spatial hash.
    bool Contains(uint64_t entity_id) const {
        AssertGameThread();
        return entity_cells_.find(entity_id) != entity_cells_.end();
    }

    /// Check if a player is at exact position (excluding a specific player).
    /// Returns true if any player other than exclude_id is at (x, y).
    bool IsPlayerAt(int16_t x, int16_t y, uint64_t exclude_id = 0) const {
        AssertGameThread();
        int64_t key = CellKey(x, y);
        auto cell_it = cells_.find(key);
        if (cell_it == cells_.end()) return false;

        for (uint64_t id : cell_it->second) {
            if (id == exclude_id) continue;
            auto ptr_it = entity_ptrs_.find(id);
            if (ptr_it != entity_ptrs_.end() && ptr_it->second) {
                if (ptr_it->second->GetX() == x && ptr_it->second->GetY() == y) {
                    return true;
                }
            }
        }
        return false;
    }

    /// Get total entity count.
    size_t Count() const {
        AssertGameThread();
        return entity_cells_.size();
    }

    /// ========================================================================
    /// ITERATION
    /// ========================================================================

    /// Iterate over all entities.
    void ForEach(const std::function<void(uint64_t, const std::shared_ptr<Player> &)> &func) const {
        AssertGameThread();
        for (const auto &[id, entity]: entity_ptrs_) {
            if (entity) {
                func(id, entity);
            }
        }
    }

    /// ========================================================================
    /// MAINTENANCE
    /// ========================================================================

    /// Clear all data.
    void Clear() {
        AssertGameThread();
        cells_.clear();
        cell_entities_.clear();
        entity_cells_.clear();
        entity_ptrs_.clear();
    }

    /// Get number of active cells (for debugging).
    size_t CellCount() const {
        AssertGameThread();
        return cells_.size();
    }

private:
    /// ========================================================================
    /// THREAD SAFETY HELPER
    /// ========================================================================
    void AssertGameThread() const {
        ASSERT_GAME_THREAD(thread_owner_);
        if (!thread_owner_.IsOwnerSet()) {
            thread_owner_.SetOwner();
        }
    }
    /// ========================================================================
    /// INTERNAL - Cell Key Calculation
    /// ========================================================================

    /// Create cell key from cell indices
    static int64_t MakeCellKey(int32_t cx, int32_t cy) {
        return (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cy);
    }

    /// Convert world coordinates to a unique cell key
    /// Map coordinates are always positive (0,0 to width,height)
    static int64_t CellKey(int16_t x, int16_t y) {  // TODO: Could be uint16_t
        int32_t cx = x / CELL_SIZE;
        int32_t cy = y / CELL_SIZE;
        return MakeCellKey(cx, cy);
    }

    /// ========================================================================
    /// DATA
    /// ========================================================================

    /// cell_key -> set of entity IDs in that cell
    std::unordered_map<int64_t, std::unordered_set<uint64_t>> cells_;

    /// cell_key -> vector of entity pointers (for fast GetNearbyEntities)
    /// Avoids ID→ptr lookup indirection in hot path
    std::unordered_map<int64_t, std::vector<std::shared_ptr<Player>>> cell_entities_;

    /// entity_id -> current cell key (for O(1) removal and move detection)
    std::unordered_map<uint64_t, int64_t> entity_cells_;

    /// entity_id -> entity pointer (for GetEntity by ID)
    std::unordered_map<uint64_t, std::shared_ptr<Player>> entity_ptrs_;

    /// Flat grid for O(1) cell access (no hash lookups)
    std::vector<std::vector<std::shared_ptr<Player>>> flat_grid_;
    int32_t grid_width_ = 0;
    int32_t grid_height_ = 0;
    bool use_flat_grid_ = false;

    /// Thread owner for debug assertions (mutable for const methods)
    mutable ThreadOwner thread_owner_;
};