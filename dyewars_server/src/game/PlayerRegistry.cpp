/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "PlayerRegistry.h"
#include "core/Log.h"
//using namespace std;

shared_ptr<Player> PlayerRegistry::CreatePlayer(uint64_t client_id) {
    uint64_t player_id = GenerateUniqueID();
    assert(player_id != 0 && "PlayerRegistry CreatePlayer failed playerid");

    auto player = make_shared<Player>(player_id, 0, 0);
    {
        lock_guard<mutex> lock(mutex_);
        players_[player_id] = player;
        client_to_player_[client_id] = player_id;
    }
    Log::Info("Player {} created for client {}", player_id, client_id);
    return player;
}

void PlayerRegistry::RemovePlayer(uint64_t player_id) {

    {
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
//using Action = std::variant<>;

void PlayerRegistry::QueueAction(Action action, uint64_t client_id)
{
    lock_guard<mutex> lock(mutex_);
    auto it = client_to_player_.find(client_id);

    action_queue_.push(action);
}

std::vector<std::shared_ptr<Player>> PlayerRegistry::ProcessCommands(TileMap &map) {
    std::vector<std::shared_ptr<Player>> moved_players;

    std::queue<MoveCommand> commands;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::swap(commands, move_queue_);  // Grab all commands, release lock
    }

    while (!commands.empty()) {
        auto cmd = commands.front();
        commands.pop();

        auto player = GetByID(cmd.player_id);
        if (!player) continue;

        if (cmd.direction == cmd.facing && cmd.direction == player->GetFacing()) {
            if (player->AttemptMove(cmd.direction, map)) {
                player->SetDirty(true);
                moved_players.push_back(player);
            }
        } else {
            player->SetFacing(cmd.direction);
            player->SetDirty(true);
            moved_players.push_back(player);
        }
    }

    return moved_players;
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

uint32_t PlayerRegistry::GenerateUniqueID() {
    int attempts = 0;
    while (attempts < 100) {
        uint32_t id = id_dist_(rng_);
        if (!players_.contains(id))
            return id;
        attempts++;
    }
    return 0;
}

bool ClientConnection::HasPlayer() const {
    return !player_.expired();
}

std::shared_ptr<Player> ClientConnection::GetPlayer() const {
    return player_.lock();
}

void ClientConnection::SetPlayer(std::weak_ptr<Player> player) {
    player_ = player;
}

void ClientConnection::ClearPlayer() {
    player_.reset();
}

// TODO Move to registry
// --- NEW: Dirty Flag Management ---
// Atomic ensures we don't crash if the Tick thread and Network thread touch this at the same time
bool IsDirty() const { return is_dirty_; }
void SetDirty(bool val) { is_dirty_ = val; }