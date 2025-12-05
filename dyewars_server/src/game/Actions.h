/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once
#include <cstdint>
#include <variant>
#include <string>
#include <memory>

class Player;
struct GameContext;

namespace Actions {
    struct MoveCommand {
        uint64_t player_id;
        uint8_t direction;
        uint8_t facing;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    struct TurnCommand {
        uint64_t player_id;
        uint8_t direction;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    struct ChatCommand {
        uint64_t player_id;
        std::string message;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    struct WarpCommand {
        uint64_t player_id;
        uint16_t map_id;
        int16_t x;
        int16_t y;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    struct AttackCommand {
        uint64_t player_id;
        uint64_t target_id;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    struct SkillCommand {
        uint32_t player_id;
        uint16_t skill_id;
        int16_t target_x;
        int16_t target_y;

        std::shared_ptr<Player> Execute(GameContext &ctx) const;
    };

    using Action = std::variant<
            MoveCommand,
            TurnCommand,
            ChatCommand,
            WarpCommand,
            AttackCommand,
            SkillCommand
    >;
}