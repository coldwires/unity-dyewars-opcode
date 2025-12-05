/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "PlayerRegistry.h"
#include "core/Common.h"
#include "core/Log.h"
#include <mutex>
#include <cassert>
using namespace std;

shared_ptr<Player> PlayerRegistry::CreatePlayer(uint64_t client_id) {
    uint64_t player_id = GenerateUniqueID();
    assert(player_id != 0 && "PlayerRegistry CreatePlayer failed playerid");

    auto player = make_shared<Player>(player_id, 0, 0);
    player->SetClientID(client_id);

    {
        lock_guard<mutex> lock(mutex_);
        players_[player_id] = player;
        client_to_player_[client_id] = player_id;
    }
    Log::Info("Player {} created for client {}", player_id, client_id);
    return player;
}

void PlayerRegistry::RemovePlayer(uint64_t player_id) {

    {// Find and remove client mapping
        lock_guard<mutex> lock(mutex_);
        for (auto it = client_to_player_.begin(); it != client_to_player_.end(); it++)
        {
            if(it->second == player_id)
            {
                client_to_player_.erase(it);
                break;
            }
        }
    }
    players_.erase(player_id);
    Log::Info("Player {} removed", player_id);
}

void PlayerRegistry::RemoveByClientID(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = client_to_player_.find(client_id);
    if (it != client_to_player_.end()) {
        uint32_t player_id = it->second;
        players_.erase(player_id);
        client_to_player_.erase(it);
        Log::Info("Player {} removed (by client {})", player_id, client_id);
    }
}



void PlayerRegistry::QueueAction(const Actions::Action &action, uint64_t client_id)
{
    lock_guard<mutex> lock(mutex_);
    auto it = client_to_player_.find(client_id);

    action_queue_.push(action);
}

// TODO returns dirty players, but its calculated here. should calculate in subsystems
std::vector<std::shared_ptr<Player>> PlayerRegistry::ProcessCommands(
        TileMap &map,
        ClientManager &clients) {

    std::vector<std::shared_ptr<Player>> updated_players;
    // Grab all queued actions
    // Lock mutex, steal all queued actions, unlock immediately.
    // Now we own actions locally, network thread can keep adding to action_queue_.
    std::queue<Actions::Action> actions;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        //puts action queue into actions, and null into action que (swap)
        //no allocation. just pointer swap
        std::swap(actions, action_queue_);  // Grab all commands, release lock
    }

    GameContext ctx{*this, map, clients};

    while (!actions.empty()) {
        //Loop through each action, get reference to front.
        //returns ref to first element in queue without removing it
        auto &action = actions.front();

        auto result = std::visit([&](auto &&cmd) {
            return cmd.Execute(ctx);
        }, action);

        if(result)
        {
            updated_players.push_back(result);
        }

        // Remove processed action from queue.
        actions.pop();
    }

    // Return all players that need broadcast.
    return updated_players;
}

// TODO check
uint64_t PlayerRegistry::GetPlayerIDForClient(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = client_to_player_.find(client_id);
    return it != client_to_player_.end() ? it->second : 0;
}

std::shared_ptr<Player> PlayerRegistry::GetByID(uint64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(player_id);
    return it != players_.end() ? it->second : nullptr;
}

std::shared_ptr<Player> PlayerRegistry::GetByClientID(uint64_t client_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = client_to_player_.find(client_id);
    if (it == client_to_player_.end()) return nullptr;

    auto pit = players_.find(it->second);
    return pit != players_.end() ? pit->second : nullptr;
}

vector<shared_ptr<Player>> PlayerRegistry::GetAllPlayers() {
    lock_guard<mutex> lock(mutex_);
    vector<shared_ptr<Player>> result;
    result.reserve(players_.size());
    for(const auto &[id,player] : players_)
        result.push_back(player);
    return result;
}

vector<shared_ptr<Player>> PlayerRegistry::GetDirtyPlayers() {
    lock_guard<mutex> lock(mutex_);
    vector<shared_ptr<Player>> result;
    for(const auto &[id, player] : players_){
        if(player->IsDirty()){
            result.push_back(player);
            player->SetDirty(false);
        }
    }
    return result;
}

size_t PlayerRegistry::Count() {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_.size();
}

uint64_t PlayerRegistry::GenerateUniqueID() {
    int attempts = 0;
    while (attempts < 100) {
        uint64_t id = id_dist_(rng_);
        if (!players_.contains(id))
            return id;
        attempts++;
    }
    return 0;
}
