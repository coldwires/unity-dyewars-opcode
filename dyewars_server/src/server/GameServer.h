#pragma once

#include <asio.hpp>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <memory>
#include <queue>

#include "ClientManager.h"
#include "game/PlayerRegistry.h"
#include "game/World.h"
#include "game/actions/BotStressTest.h"
#include "network/ConnectionLimiter.h"
#include "debug/ServerStats.h"

// Forward Declares
class LuaGameEngine;
class ClientConnection;
class DebugHttpServer;

/// ============================================================================
/// GAME SERVER
///
/// Responsibilities:
/// - Accept client connections
/// - Run game loop on separate thread
/// - Process action queue (network -> game thread)
/// - Broadcast updates to clients (view-based)
/// - Coordinate between subsystems
/// ============================================================================
class GameServer {
public:
    explicit GameServer(asio::io_context &io_context);

    ~GameServer();//Destructor

    /// ========================================================================
    /// SERVER CONTROL
    /// ========================================================================

    void Shutdown();

    void ReloadScripts() const;

    /// ========================================================================
    /// ACTION QUEUE (thread-safe)
    /// Called from network thread, executed on game thread
    /// ========================================================================

    // Queue an action from any thread (IO thread calls this)
    void QueueAction(std::function<void()> action);

    /// ========================================================================
    /// CLIENT EVENTS (called from ClientConnection)
    /// ========================================================================


    // Logic
    void OnClientLogin(const std::shared_ptr<ClientConnection> &client);

    void OnClientDisconnect(uint64_t client_id, const std::string &ip);


    /// ========================================================================
    /// ACCESSORS
    /// ========================================================================

    // These just return references to our Game Server internal dependencies
    ClientManager &Clients() { return clients_; }

    PlayerRegistry &Players() { return players_; }

    ConnectionLimiter &Limiter() { return limiter_; }

    World &GetWorld() { return world_; }


private:/// ========================================================================
    /// NETWORKING
    /// ========================================================================

    void StartAccept();

    /// ========================================================================
    /// GAME LOOP
    /// ========================================================================

    void GameLogicThread();

    void ProcessTick();

    void ProcessActionQueue();

    /// View-based broadcasting
    void BroadcastDirtyPlayers(const std::vector<std::shared_ptr<Player>> &dirty_players);

    /// ========================================================================
    /// DATA
    /// ========================================================================

    // Network
    asio::io_context &io_context_;
    asio::ip::tcp::acceptor acceptor_;

    // State
    World world_;
    PlayerRegistry players_;
    ClientManager clients_;
    ConnectionLimiter limiter_;

    // Lua
    std::shared_ptr<LuaGameEngine> lua_engine_;

    //Main Action Queue (network thread -> game thread)
    std::queue<std::function<void()>> action_queue_;
    std::mutex action_mutex_;

    // Game loop thread
    std::thread game_loop_thread_;
    std::atomic<bool> server_running_{true};
    std::atomic<bool> shutdown_requested_{false};


    // ID Generation
    /// <summary>
    /// For a game server, uint64_t is the pragmatic choice.
    /// Even at 1 million connections per second, it takes 584,000 years to wrap.
    /// </summary>
    std::atomic<uint64_t> next_client_id_{1};

    // Ping tracking
    static constexpr int PING_INTERVAL_TICKS = 200;  // Every 10 seconds at 20 TPS
    int ping_tick_counter_{0};

    void SendPingToAllClients();

    // =========================================================================
    // STRESS TEST BOTS
    // =========================================================================
public:
    /// Spawn stress test bots
    /// clustered=true: spawn near player (worst case stress test)
    /// clustered=false: spawn across map (realistic simulation)
    void SpawnBots(size_t count, bool clustered = true);

    /// Remove all stress test bots
    void RemoveBots();

    /// Get current bot count
    size_t BotCount() const { return bot_manager_.bot_ids.size(); }

    /// Get server stats (for debug dashboard)
    ServerStats& Stats() { return stats_; }

private:
    /// Bot manager state
    Actions::BotStressTest::BotManager bot_manager_;

    // =========================================================================
    // DEBUG
    // =========================================================================
    ServerStats stats_;
    std::unique_ptr<DebugHttpServer> debug_server_;
};