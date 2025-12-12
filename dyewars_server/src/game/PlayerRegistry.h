/// =======================================
/// DyeWarsServer - PlayerRegistry
///
/// Manages player lifecycle, lookups, and dirty tracking.
/// Does NOT own spatial data (World does).
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <cassert>
#include <functional>
#include <atomic>
#include "core/Log.h"
#include "core/ThreadSafety.h"
#include "Player.h"
/// ============================================================================
/// PLAYER REGISTRY
///
/// Responsibilities:
/// - Player lifecycle (create, remove)
/// - Client ID <-> Player ID mapping
/// - Dirty tracking (who needs to be broadcast)
///
/// THREAD SAFETY:
/// All methods must be called from the game thread only.
/// Debug builds enforce this via ThreadOwner assertions.
///
/// Does NOT own:
/// - Spatial data (World does)
/// - Networking (ClientManager does)
/// - Action processing (GameServer does)
/// ============================================================================

// TODO Move this to? tilemap?
enum class SpawnPoints : uint32_t {
    mainArea = 0x00050005,
};

class PlayerRegistry {
public:
    /// ========================================================================
    /// PLAYER LIFECYCLE
    /// ========================================================================

    /// Create a new player for a client connection
    /// Returns the created player, or nullptr if client already has a player
    std::shared_ptr<Player> CreatePlayer(
        uint64_t client_id,
        uint16_t start_x,
        uint16_t start_y,
        uint8_t facing = 2) {
        AssertGameThread();

        uint64_t player_id = GenerateUniqueID();
        assert(player_id != 0 && "Failed to generate unique player ID");
        auto player = std::make_shared<Player>(
            player_id,
            start_x,
            start_y,
            facing);

        player->SetClientID(client_id);

        // Check if this client already has a player.
        // This shouldn't happen in normal operation, but could if:
        // - A bug calls CreatePlayer twice for the same client
        // Without this check, we'd overwrite the old player mapping,
        // orphaning the old player in memory (leak) and breaking lookups.
        if (client_to_player_.contains(client_id)) {
            Log::Error("CreatePlayer: client {} already has player {}",
                       client_id, client_to_player_[client_id]);
            return nullptr;
        }

        players_[player_id] = player;
        client_to_player_[client_id] = player_id;
        player_to_client_[player_id] = client_id; // Reverse mapping for O(1) lookup
        player_count_.fetch_add(1, std::memory_order_relaxed);

        Log::Trace("Player {} created for client {}", player_id, client_id);
        return player;
    }

    void RemovePlayer(const uint64_t player_id) {
        AssertGameThread();

        // O(1) reverse lookup to find client_id
        auto client_it = player_to_client_.find(player_id);
        if (client_it != player_to_client_.end()) {
            client_to_player_.erase(client_it->second);
            player_to_client_.erase(client_it);
        }

        // Remove from dirty set if present
        auto player_it = players_.find(player_id);
        if (player_it != players_.end()) {
            dirty_players_.erase(player_it->second);
            players_.erase(player_it);
            player_count_.fetch_sub(1, std::memory_order_relaxed);
        }

        Log::Info("Player {} removed", player_id);
    }

    /// Remove a player by client ID
    void RemoveByClientID(const uint64_t client_id) {
        AssertGameThread();

        auto it = client_to_player_.find(client_id);
        if (it != client_to_player_.end()) {
            uint64_t player_id = it->second;

            // Remove from dirty set if present
            auto player_it = players_.find(player_id);
            if (player_it != players_.end()) {
                dirty_players_.erase(player_it->second);
                players_.erase(player_it);
                player_count_.fetch_sub(1, std::memory_order_relaxed);
            }

            // Remove both mappings
            player_to_client_.erase(player_id);
            client_to_player_.erase(it);
            Log::Info("Player {} removed (by client {})", player_id, client_id);
        }
    }

    // Lifecycle
    void Login(uint64_t client_id);

    void Logout();


    /// ========================================================================
    /// PLAYER LOOKUP
    /// ========================================================================

    /// Get player by player ID
    std::shared_ptr<Player> GetByID(const uint64_t player_id) {
        AssertGameThread();
        auto it = players_.find(player_id);
        return (it != players_.end()) ? it->second : nullptr;
    }

    /// Get player by client ID
    std::shared_ptr<Player> GetByClientID(const uint64_t client_id) {
        AssertGameThread();
        auto it = client_to_player_.find(client_id);
        if (it == client_to_player_.end()) return nullptr;

        auto pit = players_.find(it->second);
        return (pit != players_.end()) ? pit->second : nullptr;
    }

    /// Get player ID for a client connection
    /// Returns 0 if not found
    uint64_t GetPlayerIDForClient(const uint64_t client_id) {
        AssertGameThread();
        auto it = client_to_player_.find(client_id);
        return (it != client_to_player_.end()) ? it->second : 0;
    }

    /// ========================================================================
    /// DIRTY TRACKING
    /// ========================================================================

    /// Mark a player as dirty (needs broadcast)
    void MarkDirty(const std::shared_ptr<Player> &player) {
        AssertGameThread();
        dirty_players_.insert(player);
    }

    /// Mark a player as dirty by ID
    void MarkDirty(uint64_t player_id) {
        AssertGameThread();
        auto it = players_.find(player_id);
        if (it != players_.end()) {
            dirty_players_.insert(it->second);
        }
    }

    /// Consume and return all dirty players (clears the set)
    std::vector<std::shared_ptr<Player> > ConsumeDirtyPlayers() {
        AssertGameThread();
        std::vector<std::shared_ptr<Player> > result(dirty_players_.begin(), dirty_players_.end());
        dirty_players_.clear();
        return result;
    }

    /// Check if there are dirty players
    bool HasDirtyPlayers() const {
        AssertGameThread();
        return !dirty_players_.empty();
    }

    /// Get count of dirty players (for stats)
    size_t DirtyCount() const {
        AssertGameThread();
        return dirty_players_.size();
    }

    /// ========================================================================
    /// QUERIES
    /// ========================================================================

    /// Get all players (copy of shared_ptrs)
    std::vector<std::shared_ptr<Player> > GetAllPlayers() {
        AssertGameThread();
        std::vector<std::shared_ptr<Player> > result;
        result.reserve(players_.size());
        for (const auto &[id, player]: players_) {
            result.push_back(player);
        }
        return result;
    }

    /// Get player count (thread-safe, can be called from any thread)
    size_t Count() const {
        return player_count_.load(std::memory_order_relaxed);
    }

    /// ===========================3=============================================
    /// ITERATION (for broadcasting)
    /// ========================================================================

    /// Iterate over all players
    void ForEachPlayer(const std::function<void(const std::shared_ptr<Player> &)> &func) {
        AssertGameThread();
        for (const auto &[id, player]: players_) {
            func(player);
        }
    }

private:
    /// ========================================================================
    /// INTERNAL
    /// ========================================================================

    /// Generate a unique player ID.
    /// With 64-bit random IDs, collision probability is ~1 in 10^19.
    /// The retry loop is purely defensive - it will never execute in practice.
    uint64_t GenerateUniqueID() {
        // No assertion here - called from CreatePlayer which already asserts
        uint64_t id = id_dist_(rng_);
        while (players_.contains(id)) {
            id = id_dist_(rng_); // Astronomically unlikely to ever run
        }
        return id;
    }

    /// Assert we're on the game thread
    void AssertGameThread() const {
        ASSERT_GAME_THREAD(thread_owner_);
        if (!thread_owner_.IsOwnerSet()) {
            thread_owner_.SetOwner();
        }
    }

    // ========================================================================
    /// DATA
    /// ========================================================================

    std::unordered_map<uint64_t, std::shared_ptr<Player> > players_;
    std::unordered_map<uint64_t, uint64_t> client_to_player_;
    std::unordered_map<uint64_t, uint64_t> player_to_client_;
    std::unordered_set<std::shared_ptr<Player> > dirty_players_;

    /// Atomic player count for thread-safe reads from any thread (e.g., stats command)
    std::atomic<size_t> player_count_{0};

    mutable ThreadOwner thread_owner_;

    std::mt19937_64 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> id_dist_{1, UINT64_MAX - 1};
};

