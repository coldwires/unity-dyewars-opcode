/// =======================================
/// DyeWarsServer - VisibilityTracker
///
/// Tracks which players each player "knows about" (has been told about).
/// Used for efficient enter/leave view events.
///
/// Created by Anonymous on Dec 10, 2025
/// =======================================
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <memory>

class Player;  // Forward declare

/// ============================================================================
/// VISIBILITY TRACKER
///
/// Tracks bidirectional visibility relationships between players.
///
/// DATA STRUCTURES:
///
///   known_players_: player_id → set of player IDs they know about
///   known_by_:      player_id → set of player IDs that know about them
///
/// WHY TWO MAPS? (Bidirectional tracking)
///
///   Without known_by_:
///     RemovePlayer(A) must iterate ALL players to remove A from their sets
///     → O(N) where N = total players (500 players = 500 iterations)
///
///   With known_by_:
///     RemovePlayer(A) only iterates players who actually knew about A
///     → O(K) where K = players who knew about A (typically 10-20)
///
///   Trade-off: ~8 extra bytes per visibility relationship (the reverse pointer)
///   Worth it: O(K) vs O(N) on every disconnect
///
/// MEMORY LAYOUT:
///   - known_players_: ~48 bytes per player + 8 bytes per known player
///   - known_by_:      ~48 bytes per player + 8 bytes per knower
///   - 100 players, each seeing 10 others = ~16KB total (still negligible)
///
/// THREAD SAFETY:
///   All methods must be called from the game thread only.
///   No internal synchronization - caller must ensure single-threaded access.
/// ============================================================================
class VisibilityTracker {
public:
    /// ========================================================================
    /// VISIBILITY DIFF
    /// Result of an update - who entered/left view
    /// ========================================================================
    struct Diff {
        std::vector<std::shared_ptr<Player>> entered;  // Players newly visible
        std::vector<uint64_t> left;                    // Player IDs no longer visible
    };

    /// ========================================================================
    /// UPDATE VISIBILITY
    ///
    /// Compare currently visible players against what player already knows.
    /// Returns who entered/left view and updates internal state.
    ///
    /// OPTIMIZATION: Single-pass algorithm
    ///   Old approach: 3 iterations (build set, find entered, find left)
    ///   New approach: 2 iterations (combined entered+marking, find left)
    ///
    /// We avoid creating a temporary unordered_set by using a "seen" flag
    /// approach with the existing known set.
    ///
    /// @param player_id The player whose visibility to update
    /// @param visible_now Players currently in view range
    /// @return Diff containing entered/left players
    /// ========================================================================
    Diff Update(uint64_t player_id, const std::vector<std::shared_ptr<Player>>& visible_now) {
        Diff diff;

        // Get or create this player's known set
        auto& known = known_players_[player_id];

        // OPTIMIZATION: Reserve capacity to avoid repeated reallocations
        // Estimate: most updates have 0-2 changes, but reserve small amount
        diff.entered.reserve(4);
        diff.left.reserve(4);

        // =====================================================================
        // PASS 1: Find ENTERED players and build visible_ids set
        //
        // We need visible_ids for Pass 2 to check "is this known player still visible?"
        // We build it while checking for entered players (single iteration)
        // =====================================================================

        // OPTIMIZATION: Reuse member set instead of allocating new one each call
        // Clear doesn't deallocate - keeps capacity for next call
        visible_ids_scratch_.clear();

        for (const auto& p : visible_now) {
            uint64_t pid = p->GetID();
            if (pid == player_id) continue;  // Skip self

            visible_ids_scratch_.insert(pid);

            // Is this player NEW? (visible but not known)
            if (known.find(pid) == known.end()) {
                diff.entered.push_back(p);

                // Add to known set
                known.insert(pid);

                // Update reverse mapping: pid is now known BY player_id
                known_by_[pid].insert(player_id);
            }
        }

        // =====================================================================
        // PASS 2: Find LEFT players (in known but not in visible_ids_scratch_)
        //
        // We can't modify known while iterating, so collect IDs to remove
        // =====================================================================

        // OPTIMIZATION: Reuse member vector instead of allocating new one
        to_remove_scratch_.clear();

        for (uint64_t known_id : known) {
            if (visible_ids_scratch_.find(known_id) == visible_ids_scratch_.end()) {
                // Player was known but is no longer visible
                diff.left.push_back(known_id);
                to_remove_scratch_.push_back(known_id);
            }
        }

        // Remove players who left
        for (uint64_t id : to_remove_scratch_) {
            known.erase(id);

            // Update reverse mapping: id is no longer known BY player_id
            auto it = known_by_.find(id);
            if (it != known_by_.end()) {
                it->second.erase(player_id);
                // Clean up empty sets to prevent memory bloat
                if (it->second.empty()) {
                    known_by_.erase(it);
                }
            }
        }

        return diff;
    }

    /// ========================================================================
    /// INITIALIZE (on player login)
    ///
    /// Call AFTER sending initial BatchPlayerSpatial so known set matches
    /// what the client has been told about.
    /// ========================================================================
    void Initialize(uint64_t player_id, const std::vector<uint64_t>& initial_visible) {
        auto& known = known_players_[player_id];
        known.clear();

        for (uint64_t id : initial_visible) {
            if (id != player_id) {
                known.insert(id);
                // Update reverse: id is now known BY player_id
                known_by_[id].insert(player_id);
            }
        }
    }

    /// ========================================================================
    /// ADD KNOWN PLAYER
    ///
    /// Add a single player to someone's known set without clearing existing.
    /// Used when a new player joins and we tell existing players about them.
    /// ========================================================================
    void AddKnown(uint64_t player_id, uint64_t known_id) {
        if (player_id != known_id) {
            known_players_[player_id].insert(known_id);
            // Update reverse: known_id is now known BY player_id
            known_by_[known_id].insert(player_id);
        }
    }

    /// ========================================================================
    /// NOTIFY OBSERVERS WHO LOST SIGHT (when mover walks away from them)
    ///
    /// Problem: When B moves away from A, B's visibility updates but A's doesn't.
    /// A still thinks B is visible, but B is now out of range.
    ///
    /// Solution: Check everyone who knew about the mover. If the mover is now
    /// out of their view range, update their known set and return their ID
    /// so caller can send S_Left_Game.
    ///
    /// @param mover_id The player who just moved
    /// @param mover_x New X position of mover
    /// @param mover_y New Y position of mover
    /// @param view_range How far players can see
    /// @param get_player_pos Function to get a player's position: (id) -> {x, y}
    /// @return List of player IDs who lost sight of the mover (need S_Left_Game)
    /// ========================================================================
    template<typename GetPosFunc>
    std::vector<uint64_t> NotifyObserversOfDeparture(
            uint64_t mover_id,
            int16_t mover_x,
            int16_t mover_y,
            int16_t view_range,
            GetPosFunc get_player_pos) {

        std::vector<uint64_t> observers_who_lost_sight;

        // Find who knew about the mover
        auto known_by_it = known_by_.find(mover_id);
        if (known_by_it == known_by_.end()) {
            return observers_who_lost_sight;  // No one knew about this player
        }

        // Check each observer - is mover still in their view?
        // We'll collect IDs to remove after iteration (can't modify during)
        std::vector<uint64_t> to_remove;

        for (uint64_t observer_id : known_by_it->second) {
            auto [obs_x, obs_y] = get_player_pos(observer_id);

            // Check if mover is still in observer's view range
            int16_t dx = (mover_x > obs_x) ? (mover_x - obs_x) : (obs_x - mover_x);
            int16_t dy = (mover_y > obs_y) ? (mover_y - obs_y) : (obs_y - mover_y);

            if (dx > view_range || dy > view_range) {
                // Mover is OUT of observer's view range
                observers_who_lost_sight.push_back(observer_id);
                to_remove.push_back(observer_id);
            }
        }

        // Update visibility state for observers who lost sight
        for (uint64_t observer_id : to_remove) {
            // Remove mover from observer's known set
            auto obs_known_it = known_players_.find(observer_id);
            if (obs_known_it != known_players_.end()) {
                obs_known_it->second.erase(mover_id);
            }

            // Remove observer from mover's known_by set
            known_by_it->second.erase(observer_id);
        }

        // Clean up empty known_by entry
        if (known_by_it->second.empty()) {
            known_by_.erase(known_by_it);
        }

        return observers_who_lost_sight;
    }

    /// ========================================================================
    /// REMOVE PLAYER (on disconnect)
    ///
    /// O(K) where K = players who knew about this player
    /// Instead of O(N) iterating all players!
    ///
    /// Uses known_by_ reverse mapping to only touch affected players.
    /// ========================================================================
    void RemovePlayer(uint64_t player_id) {
        // Step 1: Remove from everyone who knew about this player
        // Using known_by_ makes this O(K) instead of O(N)
        auto known_by_it = known_by_.find(player_id);
        if (known_by_it != known_by_.end()) {
            for (uint64_t other_id : known_by_it->second) {
                // Remove player_id from other's known set
                auto other_known_it = known_players_.find(other_id);
                if (other_known_it != known_players_.end()) {
                    other_known_it->second.erase(player_id);
                }
            }
            known_by_.erase(known_by_it);
        }

        // Step 2: Clean up this player's own known set
        // Also update known_by_ for everyone they knew about
        auto known_it = known_players_.find(player_id);
        if (known_it != known_players_.end()) {
            for (uint64_t known_id : known_it->second) {
                // This player no longer knows about known_id
                auto it = known_by_.find(known_id);
                if (it != known_by_.end()) {
                    it->second.erase(player_id);
                    if (it->second.empty()) {
                        known_by_.erase(it);
                    }
                }
            }
            known_players_.erase(known_it);
        }
    }

    /// ========================================================================
    /// ACCESSORS
    /// ========================================================================

    const std::unordered_set<uint64_t>* GetKnownPlayers(uint64_t player_id) const {
        auto it = known_players_.find(player_id);
        return (it != known_players_.end()) ? &it->second : nullptr;
    }

    /// Get who knows about a specific player (reverse lookup)
    const std::unordered_set<uint64_t>* GetKnownBy(uint64_t player_id) const {
        auto it = known_by_.find(player_id);
        return (it != known_by_.end()) ? &it->second : nullptr;
    }

    size_t TrackedPlayerCount() const { return known_players_.size(); }

    void Clear() {
        known_players_.clear();
        known_by_.clear();
    }

private:
    /// ========================================================================
    /// PRIMARY DATA: Who does each player know about?
    /// player_id → set of player IDs they know about
    /// ========================================================================
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> known_players_;

    /// ========================================================================
    /// REVERSE INDEX: Who knows about each player?
    /// player_id → set of player IDs that know about them
    ///
    /// This is the KEY OPTIMIZATION for RemovePlayer()
    /// Without it: O(N) to remove a player (iterate all players)
    /// With it:    O(K) to remove a player (iterate only affected players)
    /// ========================================================================
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> known_by_;

    /// ========================================================================
    /// SCRATCH BUFFERS: Reused across Update() calls to avoid allocations
    ///
    /// WHY MEMBER VARIABLES?
    ///   - Update() is called ~300 times/second (100 players × 3 moves/sec)
    ///   - Each call was allocating 2 containers (set + vector)
    ///   - Now we reuse the same memory, just clear() between calls
    ///   - clear() keeps capacity, so no reallocation after warmup
    ///
    /// THREAD SAFETY:
    ///   Safe because all calls happen on game thread sequentially
    ///   Would need mutex if called from multiple threads
    /// ========================================================================
    std::unordered_set<uint64_t> visible_ids_scratch_;
    std::vector<uint64_t> to_remove_scratch_;
};
