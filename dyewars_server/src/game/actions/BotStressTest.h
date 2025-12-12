/// =======================================
/// DyeWarsServer - Bot Stress Test
///
/// Spawns and manages stress test bots for performance testing.
/// Bots are fake players that move randomly to simulate load.
///
/// Created by Anonymous on Dec 11, 2025
/// =======================================
#pragma once

#include <vector>
#include <random>
#include <cstdint>

class GameServer;

namespace Actions::BotStressTest {

    /// Bot manager state (lives in GameServer, passed to actions)
    struct BotManager {
        std::vector<uint64_t> bot_ids;
        std::mt19937 rng{std::random_device{}()};
    };

    /// Spawn count bots at random non-occupied positions
    /// If clustered=true, spawns near first real player (stress test)
    /// If clustered=false, spawns across entire map (realistic)
    void SpawnBots(GameServer* server, BotManager& manager, size_t count, bool clustered = true);

    /// Remove all bots
    void RemoveBots(GameServer* server, BotManager& manager);

    /// Move one random bot (call once per tick)
    void ProcessBotMovement(GameServer* server, BotManager& manager);

}