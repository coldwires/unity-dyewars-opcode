#include "include/server/GameServer.h"
#include <iostream>
#include "include/server/BandwidthMonitor.h"

GameServer::GameServer(asio::io_context& io_context, short port)
        : acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::address::from_string("192.168.1.3"), port)),
          lua_engine_(std::make_shared<LuaGameEngine>()),
          next_player_id_(1) {

    // Initialize Map 10x10
    game_map_ = std::make_unique<GameMap>(10, 10);

    std::cout << "Server starting on port " << port << "..." << std::endl;
    StartAccept();
    StartConsole();

    game_loop_thread_ = std::thread(&GameServer::RunGameLoop, this);
}

GameServer::~GameServer() {
    server_running_ = false;
    if (game_loop_thread_.joinable()) {
        game_loop_thread_.join();
    }
}

void GameServer::RunGameLoop() {
    const int TICKS_PER_SECOND = 20;
    const std::chrono::milliseconds TICK_RATE(1000 / TICKS_PER_SECOND); // 50ms

    while (server_running_) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Process Logic
        ProcessUpdates();

        // Update bandwidth stats every tick, prints every second
        BandwidthMonitor::Instance().Tick();


        // 2. Sleep until next tick
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (duration < TICK_RATE) {
            std::this_thread::sleep_for(TICK_RATE - duration);
        }
    }
}

void GameServer::ProcessUpdates() {
    std::cout << " Player Moving: 2" << std::endl;
    // Lock logic just long enough to grab data
    std::vector<PlayerData> moving_players;
    std::vector<std::shared_ptr<GameSession>> all_receivers;

    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& pair : sessions_) {
            auto session = pair.second;
            all_receivers.push_back(session); // Everyone receives updates

            if (session->IsDirty()) {
                moving_players.push_back(session->GetPlayerData());
                session->SetDirty(false); // Reset flag
                PlayerData data = session->GetPlayerData();
            }
        }
    }

    std::cout << " Player Moving: 3" << std::endl;
    if (moving_players.empty()) return;

    std::cout << " Player Moving: 4" << std::endl;
    // Now call Lua OUTSIDE the lock
    for (const auto& data : moving_players) {
        std::cout << " Player Moving: " << std::endl;
        lua_engine_->OnPlayerMoved(data.player_id, data.x, data.y, data.facing);
    }

    // --- OPTIMIZATION: PACKET COALESCING ---
    // Instead of sending N small packets, we build ONE big packet.

    Packet batchPacket;
    PacketWriter::WriteByte(batchPacket.payload, 0x20);

    // We can fit about 200 player updates in a standard 1400 byte MTU packet.
    // If you have more, you'd loop this, but for now let's assume < 200 moves/tick.
    uint8_t count = static_cast<uint8_t>(moving_players.size());
    PacketWriter::WriteByte(batchPacket.payload, count);

    for (const auto& p : moving_players) {
        // Append Player ID (4 bytes)
        PacketWriter::WriteUInt(batchPacket.payload, p.player_id);
        // Append X, Y (1 byte each)
        PacketWriter::WriteShort(batchPacket.payload, p.x);
        PacketWriter::WriteShort(batchPacket.payload, p.y);
        // Append Facing
        PacketWriter::WriteByte(batchPacket.payload, p.facing);

    }

    batchPacket.size = static_cast<uint16_t>(batchPacket.payload.size());

    // Serialize ONCE, send to ALL.
    // This is incredibly fast because we don't rebuild the vector for every client.
    auto encoded_bytes = std::make_shared<std::vector<uint8_t>>(batchPacket.ToBytes());

    for (auto& session : all_receivers) {
        // In a perfect world, you check 'session->GetPlayerID()'
        // to avoid sending a player their own movement, but sending it
        // is actually good for sync correction (anti-cheat).
        session->RawSend(encoded_bytes);
    }
}

void GameServer::BroadcastToOthers(uint32_t exclude_id, std::function<void(std::shared_ptr<GameSession>)> action) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& pair : sessions_) {
        if (pair.first != exclude_id) action(pair.second);
    }
}

void GameServer::BroadcastToAll(std::function<void(std::shared_ptr<GameSession>)> action) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    for (auto& pair : sessions_) action(pair.second);
}

std::vector<PlayerData> GameServer::GetAllPlayers() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<PlayerData> players;
    for (auto& pair : sessions_) players.push_back(pair.second->GetPlayerData());
    return players;
}

void GameServer::RemoveSession(uint32_t player_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    sessions_.erase(player_id);
}

void GameServer::StartAccept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
        if (!ec) {
            uint32_t id = next_player_id_++;
            auto session = std::make_shared<GameSession>(std::move(socket), lua_engine_, this, id);
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_[id] = session;
            }
            session->Start();
        }
        StartAccept();
    });
}

void GameServer::StartConsole() {
    std::thread([this]() {
        std::string cmd;
        while (true) {
            std::cout << "Server> ";
            std::getline(std::cin, cmd);
            if (cmd == "r") lua_engine_->ReloadScripts();
            else if (cmd == "bandwidth") std::cout << BandwidthMonitor::Instance().GetStats() << std::endl;
            else if (cmd == "q") exit(0);
        }
    }).detach();
}