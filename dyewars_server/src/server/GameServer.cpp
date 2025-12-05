/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#include "GameServer.h"
#include "ClientConnection.h"
#include "network/BandwidthMonitor.h"
#include "lua/LuaEngine.h"
#include "network/packets/outgoing/PacketSender.h"
#include "core/Log.h"

GameServer::GameServer(asio::io_context& io_context)
        : io_context_(io_context),
         acceptor_(io_context,
                    asio::ip::tcp::endpoint(
                            asio::ip::address::from_string(Protocol::ADDRESS),
                            Protocol::PORT)),
          lua_engine_(std::make_shared<LuaGameEngine>()),
              world_(10,10) {

    Log::Info("Server starting on port {}...", Protocol::PORT);
    StartAccept();
    game_loop_thread_ = std::thread(&GameServer::GameLogicThread, this);
}

GameServer::~GameServer() {
        Shutdown();
}

void GameServer::Shutdown() {
    if (shutdown_requested_.exchange(true)) return;

    Log::Info("Shutting down server...");
    server_running_ = false;

    // Stop accepting connections
    std::error_code ec;
    acceptor_.close(ec);

    // Close all client connections
    clients_.CloseAll();

    // Wait for game loop to finish
    if (game_loop_thread_.joinable()) {
        game_loop_thread_.join();
    }

    io_context_.stop();

    Log::Info("Server shutdown complete");
}

void GameServer::ReloadScripts() {
    if (lua_engine_) {
        lua_engine_->ReloadScripts();
    }
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

                auto client = std::make_shared<ClientConnection>(
                        std::move(socket),
                        this,
                        client_id);

                // Start the session's async read chain and handshake timer.
                // The session keeps itself alive via shared_from_this() in its async callbacks.
                // If the handshake fails or times out, the session will clean itself up.
                client->Start();
            }
        }
        else if ( ec && server_running_) {
            // Only log if it's not a shutdown
            Log::Error("Accept failed: {}", ec.message());
        }
        // Only restart if still running
        if (server_running_) {
            StartAccept();
        }
    });
}

void GameServer::GameLogicThread() {
    const int TICKS_PER_SECOND = 20;
    const std::chrono::milliseconds TICK_RATE(1000 / TICKS_PER_SECOND); // 50ms

    Log::Info("Game loop started ({} ticks/sec)", TICKS_PER_SECOND);

    while (server_running_) {
        auto start_time = std::chrono::steady_clock::now();

        // 1. Process Logic
        ProcessTick();
        BandwidthMonitor::Instance().Tick();

        // 2. Sleep until next tick
        //auto end_time = std::chrono::steady_clock::now();
        //auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        auto elapsed = std::chrono::steady_clock::now() - start_time;
        if (elapsed < TICK_RATE) {
            std::this_thread::sleep_for(TICK_RATE - elapsed);
        }
    }
    Log::Info("Game Loop Ended.");
}

// TODO Overhaul
void GameServer::ProcessTick() {
    // Process all queued commands from network thread
    auto updated_players = players_.ProcessCommands(world_.GetMap(), clients_);
    if(updated_players.empty()) return;

    ///
    /// Authorize Movement
    ///

    // TODO: Implement view-based broadcasting
    // Broadcast updates to all clients
    for (const auto& player : updated_players) {
        uint64_t player_id = player->GetID();
        int16_t x = player->GetX();
        int16_t y = player->GetY();
        uint8_t facing = player->GetFacing();

        /* I want to send a batch update here similar to the commented out code below
         * thing is, anything about the player could of changed. do I send one large batch or
         * movement batch
         * status batch
         * appearance batch
         *
         * and how would I know what batch to send? switch player->is_movement_dirty?
         *
        clients_.BroadcastToAll([=](const auto& conn) {
            Packets::Send::PlayerUpdate(conn, player_id, x, y, facing);
        });*/
    }

    // Call Lua hooks
    if (lua_engine_) {
        for (const auto& player : updated_players) {
            lua_engine_->OnPlayerMoved(
                    player->GetID(),
                    player->GetX(),
                    player->GetY(),
                    player->GetFacing());
        }
    }


    /* Packet Batch Example:

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
     */
}

void GameServer::OnClientLogin(std::shared_ptr<ClientConnection> client) {
    // Register with client manager
    clients_.AddClient(client);

    // Create player in registry
    auto player = players_.CreatePlayer(client->GetClientID());

    Log::Info("Client {} logged in as player {}", client->GetClientID(), player->GetID());

    // Send welcome packet to this client
    Packets::PacketSender::Welcome(client,player);

    // Send existing players to this client
    auto all_players = players_.GetAllPlayers();
    for (const auto &other: all_players) {
        if (other->GetID() == player->GetID()) continue;

        Packets::PacketSender::PlayerJoined(client,
                                            other->GetID(),
                                            other->GetX(),
                                            other->GetY(),
                                            other->GetFacing());
    }
    // Broadcast this player joined to everyone else
    clients_.BroadcastToOthers(
            client->GetClientID(),
           [&](const std::shared_ptr<ClientConnection> &conn) {
               Packets::PacketSender::PlayerJoined(conn,
               player->GetID(),
               player->GetX(),
               player->GetY(),
               player->GetFacing());
               });
}
