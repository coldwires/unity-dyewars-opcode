/// =======================================
/// DyeWarsServer - VisibilityTracker
///
/// Tracks which players each player "knows about" (has been told about).
/// Used for efficient enter/leave view events.
///
/// Game thread only. Uses bidirectional maps for O(K) disconnect cleanup.
///
/// Created by Anonymous on Dec 10, 2025
/// =======================================
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstdint>
#include <memory>
#include <functional>
#include "core/ThreadSafety.h"

class Player;

/// Tracks bidirectional visibility relationships between players.
/// Uses two maps: known_players_ (who I see) and known_by_ (who sees me).
/// The reverse map enables O(K) disconnect cleanup instead of O(N).
class VisibilityTracker {
public:
    struct Diff {
        std::vector<std::shared_ptr<Player>> entered;
        std::vector<uint64_t> left;
    };

    /// Compare currently visible players against what player already knows.
    /// Returns who entered/left view and updates internal state.
    /// Uses scratch buffers to avoid per-call allocations.
    Diff Update(uint64_t player_id, const std::vector<std::shared_ptr<Player>>& visible_now) {
        AssertGameThread();
        Diff diff;
        auto& known = known_players_[player_id];

        // Reuse scratch buffer - clear keeps capacity
        scratch_visible_ids_.clear();

        // Build set of currently visible IDs and find newly entered players
        for (const auto& p : visible_now) {
            uint64_t pid = p->GetID();
            if (pid == player_id) continue;

            scratch_visible_ids_.insert(pid);

            if (!known.contains(pid)) {
                diff.entered.push_back(p);
                known.insert(pid);
                known_by_[pid].insert(player_id);
            }
        }

        // Reuse scratch buffer for removals
        scratch_to_remove_.clear();

        // Find players who left view
        for (uint64_t known_id : known) {
            if (!scratch_visible_ids_.contains(known_id)) {
                diff.left.push_back(known_id);
                scratch_to_remove_.push_back(known_id);
            }
        }

        // Remove players who left
        for (uint64_t id : scratch_to_remove_) {
            known.erase(id);
            auto it = known_by_.find(id);
            if (it != known_by_.end()) {
                it->second.erase(player_id);
                if (it->second.empty()) known_by_.erase(it);
            }
        }

        return diff;
    }

    /// Initialize visibility on player login.
    /// Call AFTER sending initial BatchPlayerSpatial.
    void Initialize(uint64_t player_id, const std::vector<uint64_t>& initial_visible) {
        AssertGameThread();
        auto& known = known_players_[player_id];
        known.clear();

        for (uint64_t id : initial_visible) {
            if (id != player_id) {
                known.insert(id);
                known_by_[id].insert(player_id);
            }
        }
    }

    /// Add a single player to someone's known set.
    void AddKnown(uint64_t player_id, uint64_t known_id) {
        AssertGameThread();
        if (player_id != known_id) {
            known_players_[player_id].insert(known_id);
            known_by_[known_id].insert(player_id);
        }
    }

    /// Position getter function type
    using GetPosFunc = std::function<std::pair<int16_t, int16_t>(uint64_t)>;

    /// Check observers who lost sight of mover after movement.
    /// Returns IDs of players who need S_Left_Game packet.
    std::vector<uint64_t> NotifyObserversOfDeparture(
            uint64_t mover_id,
            int16_t mover_x,
            int16_t mover_y,
            int16_t view_range,
            const GetPosFunc& get_player_pos) {
        AssertGameThread();
        std::vector<uint64_t> observers_who_lost_sight;

        auto known_by_it = known_by_.find(mover_id);
        if (known_by_it == known_by_.end()) return observers_who_lost_sight;

        std::vector<uint64_t> to_remove;

        for (uint64_t observer_id : known_by_it->second) {
            auto [obs_x, obs_y] = get_player_pos(observer_id);

            int16_t dx = (mover_x > obs_x) ? (mover_x - obs_x) : (obs_x - mover_x);
            int16_t dy = (mover_y > obs_y) ? (mover_y - obs_y) : (obs_y - mover_y);

            if (dx > view_range || dy > view_range) {
                observers_who_lost_sight.push_back(observer_id);
                to_remove.push_back(observer_id);
            }
        }

        for (uint64_t observer_id : to_remove) {
            auto obs_known_it = known_players_.find(observer_id);
            if (obs_known_it != known_players_.end()) {
                obs_known_it->second.erase(mover_id);
            }
            known_by_it->second.erase(observer_id);
        }

        if (known_by_it->second.empty()) {
            known_by_.erase(known_by_it);
        }

        return observers_who_lost_sight;
    }

    /// Remove player on disconnect. O(K) where K = players who knew about them.
    void RemovePlayer(uint64_t player_id) {
        AssertGameThread();

        // Remove from everyone who knew about this player
        auto known_by_it = known_by_.find(player_id);
        if (known_by_it != known_by_.end()) {
            for (uint64_t other_id : known_by_it->second) {
                auto other_known_it = known_players_.find(other_id);
                if (other_known_it != known_players_.end()) {
                    other_known_it->second.erase(player_id);
                }
            }
            known_by_.erase(known_by_it);
        }

        // Clean up this player's known set
        auto known_it = known_players_.find(player_id);
        if (known_it != known_players_.end()) {
            for (uint64_t known_id : known_it->second) {
                auto it = known_by_.find(known_id);
                if (it != known_by_.end()) {
                    it->second.erase(player_id);
                    if (it->second.empty()) known_by_.erase(it);
                }
            }
            known_players_.erase(known_it);
        }
    }

    const std::unordered_set<uint64_t>* GetKnownPlayers(uint64_t player_id) const {
        AssertGameThread();
        auto it = known_players_.find(player_id);
        return (it != known_players_.end()) ? &it->second : nullptr;
    }

    const std::unordered_set<uint64_t>* GetKnownBy(uint64_t player_id) const {
        AssertGameThread();
        auto it = known_by_.find(player_id);
        return (it != known_by_.end()) ? &it->second : nullptr;
    }

    size_t TrackedPlayerCount() const { AssertGameThread(); return known_players_.size(); }
    void Clear() { AssertGameThread(); known_players_.clear(); known_by_.clear(); }

private:
    void AssertGameThread() const {
        ASSERT_GAME_THREAD(thread_owner_);
        if (!thread_owner_.IsOwnerSet()) thread_owner_.SetOwner();
    }

    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> known_players_;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> known_by_;
    mutable ThreadOwner thread_owner_;

    /// Scratch buffers - reused across Update() calls to avoid allocations.
    /// clear() preserves capacity, so after a few calls these stabilize.
    std::unordered_set<uint64_t> scratch_visible_ids_;
    std::vector<uint64_t> scratch_to_remove_;
};
