#pragma once
#include <asio.hpp>
#include <random>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include "lua/LuaEngine.h"
#include "server/ClientConnection.h" // Full definition needed here
#include "game/GameMap.h"

class GameServer {
public:
    GameServer(asio::io_context& io_context);
    ~GameServer();//Destructor
    void Shutdown();

    void ReloadScripts() { lua_engine_->ReloadScripts(); }

    //Registers client connection as a real client
    void RegisterSession(std::shared_ptr<ClientConnection> session);
    void RemoveSession(uint32_t player_id);
    void BroadcastToAll(std::function<void(std::shared_ptr<ClientConnection>)> action);
    void BroadcastToOthers(uint32_t exclude_id, const std::function<void(std::shared_ptr<ClientConnection>)> &action);
    std::vector<PlayerData> GetAllPlayers();
    // Getter so Sessions can see the map
    const GameMap& GetMap() const { return *game_map_; }



private:
    void StartAccept();
    void RunGameLoop();
    void ProcessUpdates();
    uint32_t GenerateUniquePlayerID();

    // References
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;

    // State
    std::atomic<bool> server_running_{true};
    std::atomic<bool> shutdown_requested_{false};

    // Sessions
    std::map<uint32_t, std::shared_ptr<ClientConnection>> sessions_;
    std::mutex sessions_mutex_;

    // Threads
    std::thread game_loop_thread_;


    // Game
    std::unique_ptr<GameMap> game_map_; // The Server owns the Map
    std::shared_ptr<LuaGameEngine> lua_engine_;

    // ID Generation
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist_{1, 0xFFFFFFFF};
};