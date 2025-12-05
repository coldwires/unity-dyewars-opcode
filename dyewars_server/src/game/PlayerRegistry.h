/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#pragma once
#include <memory>
#include <queue>
#include <variant>
#include <unordered_map>
#include <random>
#include "Player.h"
#include "Actions.h"
#include "server/ClientManager.h"

enum class SpawnPoints : uint32_t {
    mainArea = 0x00050005,
};
struct MoveCommand {
    uint64_t player_id;
    uint8_t direction;
    uint8_t facing;
};
using Action = std::variant<
        MoveCommand
        >;

class TileMap;

class PlayerRegistry {
public:
    // Lifecycle
    void Login(uint64_t client_id);
    void Logout();
    std::shared_ptr<Player> CreatePlayer(uint64_t client_id);
    void RemovePlayer(uint64_t player_id);
    void RemoveByClientID(uint64_t client_id);

    // Lookups
    std::shared_ptr<Player> GetByID(uint64_t player_id);
    std::shared_ptr<Player> GetByClientID(uint64_t client_id);
    uint64_t GetPlayerIDForClient(uint64_t client_id);

    // Command queue (called from packet handlers on io thread)
    void QueueAction(const Actions::Action &action, uint64_t client_id);

    // Process all queued commands (called from game loop)
    std::vector<std::shared_ptr<Player>> ProcessCommands(
            TileMap& map,
            ClientManager &manager);

    // Queries
    std::vector<std::shared_ptr<Player>> GetAllPlayers();
    std::vector<std::shared_ptr<Player>> GetDirtyPlayers();
    size_t Count();

private:
    uint64_t GenerateUniqueID();

    std::queue<Actions::Action> action_queue_;
    std::unordered_map<uint64_t, std::shared_ptr<Player>> players_;
    std::unordered_map<uint32_t, uint32_t> client_to_player_;
    std::mutex mutex_;

    std::atomic<bool> is_dirty_{false};

    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist_{1, UINT32_MAX};
};
