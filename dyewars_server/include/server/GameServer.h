#pragma once
#include <asio.hpp>
#include <map>
#include <mutex>
#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include "include/lua/LuaEngine.h"
#include "GameSession.h" // Full definition needed here
#include "include/server/GameMap.h"

class GameServer {
public:
    GameServer(asio::io_context& io_context, short port);
    ~GameServer();//Destructor

    void BroadcastToOthers(uint32_t exclude_id, std::function<void(std::shared_ptr<GameSession>)> action);
    void BroadcastToAll(std::function<void(std::shared_ptr<GameSession>)> action);
    std::vector<PlayerData> GetAllPlayers();
    void RemoveSession(uint32_t player_id);

    // Getter so Sessions can see the map
    const GameMap& GetMap() const { return *game_map_; }

private:
    void StartAccept();
    void StartConsole();

    void RunGameLoop();
    void ProcessUpdates();

    asio::ip::tcp::acceptor acceptor_;
    std::shared_ptr<LuaGameEngine> lua_engine_;
    std::atomic<uint32_t> next_player_id_;
    std::map<uint32_t, std::shared_ptr<GameSession>> sessions_;
    std::mutex sessions_mutex_;

    std::thread game_loop_thread_;
    std::atomic<bool> server_running_{true};

    std::unique_ptr<GameMap> game_map_; // The Server owns the Map
};