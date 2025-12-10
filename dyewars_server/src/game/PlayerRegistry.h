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
#include <mutex>
#include <functional>
#include "core/Log.h"
#include "Player.h"
/// ============================================================================
/// PLAYER REGISTRY
///
/// Responsibilities:
/// - Player lifecycle (create, remove)
/// - Client ID <-> Player ID mapping
/// - Dirty tracking (who needs broadcast)
/// - Thread-safe access to player data
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

        uint64_t player_id = GenerateUniqueID();
        assert(player_id != 0 && "Failed to generate unique player ID");
        auto player = std::make_shared<Player>(
                player_id,
                start_x,
                start_y,
                facing);

        player->SetClientID(client_id);

        {
            std::lock_guard lock(mutex_);

            // Check if this client already has a player.
            // This shouldn't happen in normal operation, but could if:
            // - A bug calls CreatePlayer twice for the same client
            // - A race condition in login handling
            // Without this check, we'd overwrite the old player mapping,
            // orphaning the old player in memory (leak) and breaking lookups.
            if (client_to_player_.contains(client_id)) {
                Log::Error("CreatePlayer: client {} already has player {}",
                           client_id, client_to_player_[client_id]);
                return nullptr;
            }

            players_[player_id] = player;
            client_to_player_[client_id] = player_id;
            player_to_client_[player_id] = client_id;  // Reverse mapping for O(1) lookup
        }

        Log::Trace("Player {} created for client {}", player_id, client_id);
        return player;
    }

    void RemovePlayer(uint64_t player_id) {
        {
            std::lock_guard lock(mutex_);

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
            }
        }
        Log::Info("Player {} removed", player_id);
    }

    /// Remove a player by client ID
    void RemoveByClientID(uint64_t client_id) {
        {
            std::lock_guard lock(mutex_);

            auto it = client_to_player_.find(client_id);
            if (it != client_to_player_.end()) {
                uint64_t player_id = it->second;

                // Remove from dirty set if present
                auto player_it = players_.find(player_id);
                if (player_it != players_.end()) {
                    dirty_players_.erase(player_it->second);
                    players_.erase(player_it);
                }

                // Remove both mappings
                player_to_client_.erase(player_id);
                client_to_player_.erase(it);
                Log::Info("Player {} removed (by client {})", player_id, client_id);
            }
        }
    }

    // Lifecycle
    void Login(uint64_t client_id);

    void Logout();


    /// ========================================================================
    /// PLAYER LOOKUP
    /// ========================================================================

    /// Get player by player ID
    std::shared_ptr<Player> GetByID(uint64_t player_id) {
        std::lock_guard lock(mutex_);
        auto it = players_.find(player_id);
        return (it != players_.end()) ? it->second : nullptr;
    }

    /// Get player by client ID
    std::shared_ptr<Player> GetByClientID(uint64_t client_id) {
        std::lock_guard lock(mutex_);

        auto it = client_to_player_.find(client_id);
        if (it == client_to_player_.end()) return nullptr;

        auto pit = players_.find(it->second);
        return (pit != players_.end()) ? pit->second : nullptr;
    }

    /// Get player ID for a client connection
    /// Returns 0 if not found
    uint64_t GetPlayerIDForClient(uint64_t client_id) {
        std::lock_guard lock(mutex_);
        auto it = client_to_player_.find(client_id);
        return (it != client_to_player_.end()) ? it->second : 0;
    }

    /// ========================================================================
    /// DIRTY TRACKING
    /// ========================================================================

    /// Mark a player as dirty (needs broadcast)
    /// Thread-safe - can be called from game thread
    void MarkDirty(const std::shared_ptr<Player> &player) {
        std::lock_guard lock(mutex_);
        dirty_players_.insert(player);
    }

    /// Mark a player as dirty by ID
    void MarkDirty(uint64_t player_id) {
        std::lock_guard lock(mutex_);
        auto it = players_.find(player_id);
        if (it != players_.end()) {
            dirty_players_.insert(it->second);
        }
    }

    /// Consume and return all dirty players (clears the set)
    std::vector<std::shared_ptr<Player>> ConsumeDirtyPlayers() {
        std::lock_guard lock(mutex_);
        std::vector<std::shared_ptr<Player>> result(dirty_players_.begin(), dirty_players_.end());
        dirty_players_.clear();
        return result;
    }

    /// Check if there are dirty players
    bool HasDirtyPlayers() {
        std::lock_guard lock(mutex_);
        return !dirty_players_.empty();
    }

    /// ========================================================================
    /// QUERIES
    /// ========================================================================

    /// Get all players (copy of shared_ptrs)
    std::vector<std::shared_ptr<Player>> GetAllPlayers() {
        std::lock_guard lock(mutex_);
        std::vector<std::shared_ptr<Player>> result;
        result.reserve(players_.size());
        for (const auto &[id, player]: players_) {
            result.push_back(player);
        }
        return result;
    }

    /// Get player count
    size_t Count() {
        std::lock_guard lock(mutex_);
        return players_.size();
    }

    /// ========================================================================
    /// ITERATION (for broadcasting)
    /// ========================================================================

    /// Iterate over all players
    /// Takes a snapshot under lock, then iterates without holding lock
    void ForEachPlayer(const std::function<void(const std::shared_ptr<Player> &)> &func) {
        std::vector<std::shared_ptr<Player>> snapshot;
        {
            std::lock_guard lock(mutex_);
            snapshot.reserve(players_.size());
            for (const auto &[id, player]: players_) {
                snapshot.push_back(player);
            }
        } // Lock released
        for (const auto &player: snapshot) {
            func(player);
        }
    }

private:
    /// ========================================================================
    /// INTERNAL
    /// ========================================================================

    /// Generate a unique player ID
    uint64_t GenerateUniqueID() {
        std::lock_guard lock(mutex_);

        for (int attempts = 0; attempts < 100; attempts++) {
            uint64_t id = id_dist_(rng_);
            if (!players_.contains(id)) {
                return id;
            }
        }

        // Fallback: find next available sequential ID
        for (int attempts = 0; attempts < 100; attempts++) {
            uint64_t id = next_fallback_id_++;
            if (!players_.contains(id)) {
                return id;
            }
        }
        assert(false && "Failed to generate Player ID");
        return 0;
    }

    // ========================================================================
    /// DATA
    /// ========================================================================

    /// Player storage: player_id -> Player
    std::unordered_map<uint64_t, std::shared_ptr<Player>> players_;

    /// Client mapping: client_id -> player_id
    std::unordered_map<uint64_t, uint64_t> client_to_player_;

    /// Reverse mapping: player_id -> client_id (for O(1) removal by player_id)
    std::unordered_map<uint64_t, uint64_t> player_to_client_;

    /// Dirty players: need broadcast this tick
    std::unordered_set<std::shared_ptr<Player>> dirty_players_;

    /// Thread safety
    mutable std::mutex mutex_;

    /// ID generation
    std::mt19937_64 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint64_t> id_dist_{1, UINT64_MAX - 1};
    uint64_t next_fallback_id_{1};
};

