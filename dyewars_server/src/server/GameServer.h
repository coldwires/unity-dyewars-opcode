#pragma once
#include <asio.hpp>
#include <atomic>
#include <thread>
#include "ClientManager.h"
#include "lua/LuaEngine.h"
#include "network/ConnectionLimiter.h"
#include "game/GameMap.h"

class GameServer {
public:
    GameServer(asio::io_context& io_context);
    ~GameServer();//Destructor
    void Shutdown();

    //Accessors
    ConnectionLimiter& Limiter() { return limiter_; }
    ClientManager& Clients() { return clients_;}
    PlayerRegistry& Players() { return players_; }
    World& GetWorld() { return world_; }
    asio::io_context& GetIOContext() { return io_context_; }

    bool IsRunning() const { return server_running_.load(); }

    /*
    // TODO Move all this:
    void ReloadScripts() { lua_engine_->ReloadScripts(); }

    //Registers client connection as a real client
    void RegisterSession(std::shared_ptr<ClientConnection> session);
    void RemoveSession(uint32_t player_id);

    std::vector<PlayerData> GetAllPlayers();
    // Getter so Sessions can see the map
    const GameMap &GetMap() const { return *game_map_; }
    */


private:
    void StartAccept();
    void GameLogicThread();
    void ProcessTick();

    
    // References
    asio::io_context& io_context_;
    asio::ip::tcp::acceptor acceptor_;

    // State
    std::atomic<bool> server_running_{true};
    std::atomic<bool> shutdown_requested_{false};

    // Sessions
    // Instead of a map of shared ptrs to clientconnection, we get the system
    //std::map<uint32_t, std::shared_ptr<ClientConnection>> clients_;
    std::mutex sessions_mutex_;
    
    // Regular objects since server owns 
    ClientManager clients_;
    PlayerRegistry players_;
    World world_;
    ConnectionLimiter limiter_;
    
    // Threads
    std::thread game_loop_thread_;


    // Game
    //std::unique_ptr<GameMap> game_map_; // The Server owns the Map
    std::shared_ptr<LuaGameEngine> lua_engine_;

    /// <summary>
    /// For a game server, uint64_t is the pragmatic choice. 
    /// Even at 1 million connections per second, it takes 584,000 years to wrap.
    /// </summary>
    std::atomic<uint64_t> next_client_id_ = 1;

    // ID Generation
    std::mt19937 rng_{std::random_device{}()};
    std::uniform_int_distribution<uint32_t> id_dist_{1, 0xFFFFFFFF};
};