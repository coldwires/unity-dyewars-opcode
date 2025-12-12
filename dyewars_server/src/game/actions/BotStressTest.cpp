/// =======================================
/// DyeWarsServer - Bot Stress Test
/// =======================================
#include "BotStressTest.h"
#include "server/GameServer.h"
#include "server/FakeClientConnection.h"
#include "network/packets/outgoing/PacketSender.h"
#include "core/Log.h"

namespace Actions::BotStressTest {

    void SpawnBots(GameServer* server, BotManager& manager, size_t count, bool clustered) {
        auto& world = server->GetWorld();
        auto& players = server->Players();

        const int map_width = world.GetMap().GetWidth();
        const int map_height = world.GetMap().GetHeight();

        int min_x, max_x, min_y, max_y;

        if (clustered) {
            // Clustered mode: spawn near first real player (stress test)
            int center_x = map_width / 2;
            int center_y = map_height / 2;

            // Find first real player (client_id < 0x8000000000000000)
            players.ForEachPlayer([&](const std::shared_ptr<Player>& p) {
                if (p->GetClientID() < 0x8000000000000000ULL) {
                    center_x = p->GetX();
                    center_y = p->GetY();
                }
            });

            // Spawn within 50 tiles of center
            min_x = std::max(1, center_x - 50);
            max_x = std::min(map_width - 2, center_x + 50);
            min_y = std::max(1, center_y - 50);
            max_y = std::min(map_height - 2, center_y + 50);
        } else {
            // Spread mode: spawn across entire map (realistic)
            min_x = 1;
            max_x = map_width - 2;
            min_y = 1;
            max_y = map_height - 2;
        }

        std::uniform_int_distribution<int> x_dist(min_x, max_x);
        std::uniform_int_distribution<int> y_dist(min_y, max_y);
        std::uniform_int_distribution<int> facing_dist(0, 3);

        // Spawn in batches to avoid tick lag
        constexpr size_t BATCH_SIZE = 100;
        size_t to_spawn = std::min(BATCH_SIZE, count);
        size_t spawned = 0;
        size_t attempts = 0;
        const size_t max_attempts = to_spawn * 10;

        while (spawned < to_spawn && attempts < max_attempts) {
            attempts++;

            int16_t x = x_dist(manager.rng);
            int16_t y = y_dist(manager.rng);

            // Skip if tile is blocked or occupied
            if (world.GetMap().IsTileBlocked(x, y)) continue;
            if (world.IsPositionOccupied(x, y)) continue;

            // Create bot player
            // Use a unique fake client_id (high bit set to avoid collision with real clients)
            uint64_t fake_client_id = 0x8000000000000000ULL + manager.bot_ids.size();
            uint8_t facing = static_cast<uint8_t>(facing_dist(manager.rng));
            auto bot = players.CreatePlayer(fake_client_id, x, y, facing);
            if (!bot) continue;

            // Create fake connection for this bot to simulate packet building overhead
            auto fake_conn = std::make_shared<FakeClientConnection>(fake_client_id);
            server->Clients().AddFakeClient(fake_conn);

            // Add to world
            world.AddPlayer(bot->GetID(), x, y, bot);

            // Initialize visibility
            auto nearby = world.GetPlayersInRange(x, y);
            std::vector<uint64_t> nearby_ids;
            for (const auto& p : nearby) {
                if (p->GetID() != bot->GetID()) {
                    nearby_ids.push_back(p->GetID());
                }
            }
            world.Visibility().Initialize(bot->GetID(), nearby_ids);

            // Notify nearby real players about the new bot
            for (const auto& viewer : nearby) {
                if (viewer->GetID() == bot->GetID()) continue;
                auto conn = server->Clients().GetClient(viewer->GetClientID());
                if (conn) {
                    Packets::PacketSender::PlayerSpatial(conn, bot->GetID(), x, y, facing);
                    world.Visibility().AddKnown(viewer->GetID(), bot->GetID());
                }
            }

            // Track as bot
            manager.bot_ids.push_back(bot->GetID());
            spawned++;
        }

        size_t remaining = count - spawned;
        if (remaining > 0) {
            Log::Info("Spawning bots ({})... {} so far, {} remaining",
                      clustered ? "clustered" : "spread", manager.bot_ids.size(), remaining);
            // Queue another batch
            server->QueueAction([server, &manager, remaining, clustered] {
                SpawnBots(server, manager, remaining, clustered);
            });
        } else {
            Log::Info("Spawned all bots ({} total, {})", manager.bot_ids.size(),
                      clustered ? "clustered" : "spread");
        }
    }

    void RemoveBots(GameServer* server, BotManager& manager) {
        auto& world = server->GetWorld();
        auto& players = server->Players();

        // Remove in batches to avoid tick lag
        // Remove ~100 per tick max
        constexpr size_t BATCH_SIZE = 100;
        size_t to_remove = std::min(BATCH_SIZE, manager.bot_ids.size());

        for (size_t i = 0; i < to_remove; i++) {
            uint64_t bot_id = manager.bot_ids.back();
            manager.bot_ids.pop_back();

            auto bot = players.GetByID(bot_id);
            if (bot) {
                // Notify everyone who KNOWS about this bot (not just nearby)
                // This fixes ghost players when bots moved out of view before removal
                const auto* known_by = world.Visibility().GetKnownBy(bot_id);
                if (known_by) {
                    for (uint64_t observer_id : *known_by) {
                        auto observer = players.GetByID(observer_id);
                        if (!observer) continue;
                        auto conn = server->Clients().GetClient(observer->GetClientID());
                        if (conn) {
                            Packets::PacketSender::PlayerLeft(conn, bot_id);
                        }
                    }
                }

                world.RemovePlayer(bot_id);
                world.Visibility().RemovePlayer(bot_id);
                players.RemovePlayer(bot_id);

                // Remove fake connection
                server->Clients().RemoveClient(bot->GetClientID());
            }
        }

        if (manager.bot_ids.empty()) {
            Log::Info("Removed all bots");
        } else {
            Log::Info("Removing bots... {} remaining", manager.bot_ids.size());
            // Queue another batch removal
            server->QueueAction([server, &manager] {
                RemoveBots(server, manager);
            });
        }
    }

    void ProcessBotMovement(GameServer* server, BotManager& manager) {
        if (manager.bot_ids.empty()) return;

        auto& world = server->GetWorld();
        auto& players = server->Players();

        std::uniform_int_distribution<size_t> bot_picker(0, manager.bot_ids.size() - 1);
        std::uniform_int_distribution<int> dir_dist(0, 3);

        // Move ~30% of bots per tick (realistic player activity simulation)
        size_t moves_this_tick = std::max(size_t{1}, manager.bot_ids.size() / 3);

        // Timing accumulators
        double spatial_time = 0, visibility_time = 0, departure_time = 0;
        size_t actual_moves = 0;

        for (size_t i = 0; i < moves_this_tick; i++) {
            size_t bot_index = bot_picker(manager.rng);
            uint64_t bot_id = manager.bot_ids[bot_index];

            auto bot = players.GetByID(bot_id);
            if (!bot) continue;

            uint8_t new_facing = static_cast<uint8_t>(dir_dist(manager.rng));

            // Set facing first
            bot->SetFacing(new_facing);

            // Calculate target position
            int16_t new_x = bot->GetX();
            int16_t new_y = bot->GetY();

            switch (new_facing) {
                case 0: new_y++; break;  // North
                case 1: new_x++; break;  // East
                case 2: new_y--; break;  // South
                case 3: new_x--; break;  // West
            }

            // Check if move is valid
            if (world.GetMap().IsTileBlocked(new_x, new_y)) continue;
            if (world.IsPositionOccupied(new_x, new_y, bot_id)) continue;

            actual_moves++;

            // Move the bot
            bot->SetPosition(new_x, new_y);
            world.UpdatePlayerPosition(bot_id, new_x, new_y);
            players.MarkDirty(bot);

            // Update visibility for the bot
            auto t0 = std::chrono::steady_clock::now();
            auto visible = world.GetPlayersInRange(new_x, new_y);
            auto t1 = std::chrono::steady_clock::now();
            spatial_time += std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();

            world.Visibility().Update(bot_id, visible);
            auto t2 = std::chrono::steady_clock::now();
            visibility_time += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            // Notify observers who lost sight of the bot
            auto get_player_pos = [&world](uint64_t id) -> std::pair<int16_t, int16_t> {
                auto p = world.GetPlayer(id);
                return p ? std::make_pair(p->GetX(), p->GetY())
                         : std::make_pair<int16_t, int16_t>(0, 0);
            };

            auto observers_lost = world.Visibility().NotifyObserversOfDeparture(
                    bot_id, new_x, new_y, World::VIEW_RANGE, get_player_pos);
            auto t3 = std::chrono::steady_clock::now();
            departure_time += std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count();

            for (uint64_t observer_id : observers_lost) {
                auto observer = world.GetPlayer(observer_id);
                if (!observer) continue;
                auto conn = server->Clients().GetClient(observer->GetClientID());
                if (conn) {
                    Packets::PacketSender::PlayerLeft(conn, bot_id);  // Tell observer that BOT left their view
                }
            }
        }

        // Record stats for debug dashboard
        server->Stats().RecordBotMovement(
            spatial_time / 1000.0,
            visibility_time / 1000.0,
            departure_time / 1000.0
        );

        // Log detailed timing every 100 ticks (5 sec at 20 TPS)
        static int log_counter = 0;
        if (++log_counter >= 100) {
            log_counter = 0;
            Log::Trace("BotMove breakdown - Moves: {}, Spatial: {:.2f}ms, Visibility: {:.2f}ms, Departure: {:.2f}ms",
                       actual_moves, spatial_time / 1000.0, visibility_time / 1000.0, departure_time / 1000.0);
        }
    }

}
