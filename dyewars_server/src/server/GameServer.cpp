#include "GameServer.h"
#include <iostream>
#include "network/BandwidthMonitor.h"
#include "core/Log.h"
#include "network/packets/Sender.h"

using std::vector;
using std::make_shared;
using std::make_unique;
using std::cout;
using std::endl;
using std::thread;
using std::lock_guard;
using std::mutex;
using std::shared_ptr;


GameServer::GameServer(asio::io_context& io_context)
        : io_context_(io_context),
         acceptor_(io_context,
                    asio::ip::tcp::endpoint(
                            asio::ip::address::from_string(Protocol::ADDRESS),
                            Protocol::PORT)),
          lua_engine_(make_shared<LuaGameEngine>()),
              world_(10,10)
          {

    // TODO Replace this with tilemap
    // Initialize Map 10x10
    //game_map_ = make_unique<GameMap>(10, 10);

    Log::Info("Server starting on port {}...",Protocol::PORT);
    StartAccept();
    game_loop_thread_ = thread(&GameServer::GameLogicThread, this);
}

GameServer::~GameServer() {
        Shutdown();
}

void GameServer::GameLogicThread() {
    const int TICKS_PER_SECOND = 20;
    const std::chrono::milliseconds TICK_RATE(1000 / TICKS_PER_SECOND); // 50ms

    while (server_running_) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Process Logic
        ProcessTick();

        // Update bandwidth stats every tick, prints every second
        BandwidthMonitor::Instance().Tick();


        // 2. Sleep until next tick
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        if (duration < TICK_RATE) {
            std::this_thread::sleep_for(TICK_RATE - duration);
        }
    }
    Log::Info("Game Loop Ended.");
}

void GameServer::Shutdown() {
    if (shutdown_requested_.exchange(true)) return;

    Log::Info("Shutting down server...");
    server_running_ = false;

    // Stop accepting connections
    std::error_code ec;
    acceptor_.close(ec);

    clients_.CloseAll();

    // Close all client sockets first
    /*
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        for (auto& [id, session] : clients_) {
            session->CloseSocket();
        }
        clients_.clear();  // Clear while lock is held
    }
    */

    if (game_loop_thread_.joinable()) {
        game_loop_thread_.join();
    }

    io_context_.stop();
}

void GameServer::StartAccept() {
    acceptor_.async_accept([this](
        std::error_code ec,
        asio::ip::tcp::socket socket) {
        if (!ec && server_running_) {

            // If we fail getting ip, close socket and listen for next connection
            std::string ip;
            try {
                ip = socket.remote_endpoint().address().to_string();
            }
            catch (...) {
                socket.close();
                if (server_running_) StartAccept();
                return;
            }
            Log::Info("IP: {} trying to connect.", ip);

            if (limiter_.IsBanned(ip)) {
                Log::Trace("Rejected banned IP: {}", ip);
                socket.close();
            }
            else if (!limiter_.CheckRateLimit(ip)) {
                Log::Trace("Rate limited IP: {}", ip);
                socket.close();
            }
            else if (!limiter_.CanConnect(ip)) {
                Log::Trace("Connection limit reached for IP: {}", ip);
                socket.close();
            }
            else {
                limiter_.AddConnection(ip);


                uint64_t client_id = next_client_id_++;

                // Create the session. Note: the session is NOT in the sessions_ map yet.
                // It will register itself after a successful handshake.
                auto client = std::make_shared<ClientConnection>(
                    std::move(socket),
                    lua_engine_,
                    this,
                    client_id);

                // Start the session's async read chain and handshake timer.
                // The session keeps itself alive via shared_from_this() in its async callbacks.
                // If the handshake fails or times out, the session will clean itself up.
                client->Start();
            }
        }
        else if (server_running_) {
            // Only log if it's not a shutdown
            Log::Error("Accept failed: {}", ec.message());
        }
        // Only restart if still running
        if (server_running_) {
            StartAccept();
        }
        });
}

void GameServer::ProcessTick() {
    auto dirty_players = players_.GetDirtyPlayers();
    if (dirty_players.empty()) return;

    // Broadcast updates
    // TODO: Implement view-based broadcasting

    vector<PlayerData> moving_players;
    vector<shared_ptr<ClientConnection>> all_receivers;

    // Lock logic just long enough to grab data
    {
       lock_guard<mutex> lock(sessions_mutex_);
        for (auto& pair : clients_) {
            auto session = pair.second;
            all_receivers.push_back(session); // Everyone receives updates

            if (session->IsDirty()) {
                moving_players.push_back(session->GetPlayerData());
                session->SetDirty(false); // Reset flag
            }
        }
    }

    if (moving_players.empty()) return;

    ///
    /// Authorize Movement
    ///


    // Now call Lua OUTSIDE the lock
    for (const auto& data : moving_players) {
        lua_engine_->OnPlayerMoved(data.player_id, data.x, data.y, data.facing);
    }

    // TODO: this takes every moving players data, builds one packet, and sends it to every connected player
    // Extremely unnecessary, just a good example of packet coalescing.
    // Will change it to send to view/etc later

    // --- OPTIMIZATION: PACKET COALESCING ---
    // Instead of sending N small packets, we build ONE big packet.

    uint8_t count = static_cast<uint8_t>(moving_players.size());
    Protocol::Packet batchPacket;
    Protocol::PacketWriter::WriteByte(batchPacket.payload, 0x20);

    // We can fit about 200 player updates in a standard 1400 byte MTU packet.
    // If you have more, you'd loop this, but for now let's assume < 200 moves/tick.
    Protocol::PacketWriter::WriteByte(batchPacket.payload, count);

    for (const auto& p : moving_players) {
        // Append Player ID (4 bytes)
        Protocol::PacketWriter::WriteUInt(batchPacket.payload, p.player_id);
        // Append X, Y (2 bytes each)
        Protocol::PacketWriter::WriteShort(batchPacket.payload, p.x);
        Protocol::PacketWriter::WriteShort(batchPacket.payload, p.y);
        // Append Facing
        Protocol::PacketWriter::WriteByte(batchPacket.payload, p.facing);
    }

    batchPacket.size = static_cast<uint16_t>(batchPacket.payload.size());

    // Serialize ONCE, send to ALL.
    // This is incredibly fast because we don't rebuild the vector for every client.
    auto encoded_bytes = make_shared<std::vector<uint8_t>>(batchPacket.ToBytes());

    for (auto& session : all_receivers) {
        // In a perfect world, you check 'session->GetPlayerID()'
        // to avoid sending a player their own movement, but sending it
        // is actually good for sync correction (anti-cheat).
        session->RawSend(encoded_bytes);
    }
}




/*

std::vector<PlayerData> GameServer::GetAllPlayers() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    std::vector<PlayerData> players;
    for (auto& pair : clients_) players.push_back(pair.second->GetPlayerData());
    return players;
}

void GameServer::RemoveSession(uint32_t player_id) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    clients_.erase(player_id);
}

uint32_t GameServer::GenerateUniquePlayerID(){
    std::lock_guard<std::mutex> lock(sessions_mutex_);

    uint32_t id;
    int attempts = 0;
    const int max_attempts = 150;   //Safety Limit

    do{
        id = id_dist_(rng_);
        if(!clients_.contains(id))
            return id;
        attempts++;
    } while (attempts < max_attempts);

    // Fallback: this should basically never happen with 4 billion possible IDs
    // and only ~250 players, but safety first
    return 0;
    //throw std::runtime_error("Failed to generate unique player ID");
}

void GameServer::RegisterSession(std::shared_ptr<ClientConnection> session) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    clients_[session->GetPlayerID()] = session;
    std::cout << "Player " << session->GetPlayerID() << " registered (handshake complete)" << std::endl;
}

*/