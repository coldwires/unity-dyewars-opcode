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
#include "include/lua/LuaEngine.h"
#include "ClientConnection.h" // Full definition needed here
#include "include/server/GameMap.h"

class GameServer {
public:
    GameServer(asio::io_context& io_context, short port);
    ~GameServer();//Destructor

    void BroadcastToOthers(uint32_t exclude_id, std::function<void(std::shared_ptr<ClientConnection>)> action);
    void BroadcastToAll(std::function<void(std::shared_ptr<ClientConnection>)> action);
    std::vector<PlayerData> GetAllPlayers();

    //Registers client connection as a real client
    void RegisterSession(std::shared_ptr<ClientConnection> session);
    void RemoveSession(uint32_t player_id);

    // Getter so Sessions can see the map
    const GameMap& GetMap() const { return *game_map_; }



private:
    void StartAccept();
    void StartConsole();

    void RunGameLoop();
    void ProcessUpdates();

    uint32_t GenerateUniquePlayerID();

    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<LuaGameEngine> lua_engine_;

    //std::atomic<uint32_t> next_player_id_;
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist_{1, 0xFFFFFFFF};

    std::map<uint32_t, std::shared_ptr<ClientConnection>> sessions_;
    std::mutex sessions_mutex_;

    std::thread game_loop_thread_;
    std::atomic<bool> server_running_{true};

    std::unique_ptr<GameMap> game_map_; // The Server owns the Map
};