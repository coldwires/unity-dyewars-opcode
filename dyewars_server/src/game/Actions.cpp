#include "Actions.h"
#include "core/Common.h"
#include "PlayerRegistry.h"
#include "TileMap.h"

namespace Actions {

    std::shared_ptr<Player> MoveCommand::Execute(GameContext& ctx) const {
        auto player = ctx.players.GetByID(player_id);
        if (!player) return nullptr;

        if (direction == facing && direction == player->GetFacing()) {
            if (player->AttemptMove(direction, ctx.map)) {
                player->SetDirty(true);
                return player;
            }
        } else {
            player->SetFacing(direction);
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }

    std::shared_ptr<Player> TurnCommand::Execute(GameContext& ctx) const {
        auto player = ctx.players.GetByID(player_id);
        if (!player) return nullptr;

        player->SetFacing(direction);
        player->SetDirty(true);
        return player;
    }

    std::shared_ptr<Player> AttackCommand::Execute(GameContext& ctx) const {
        auto player = ctx.players.GetByID(player_id);
        if (!player) return nullptr;

        // TODO: Combat logic
        return nullptr;
    }

    std::shared_ptr<Player> ChatCommand::Execute(GameContext& ctx) const {
        // TODO: Implement chat
        return nullptr;
    }

    std::shared_ptr<Player> WarpCommand::Execute(GameContext& ctx) const {
        // TODO: Implement warp
        return nullptr;
    }

    std::shared_ptr<Player> SkillCommand::Execute(GameContext& ctx) const {
        // TODO: Implement skills
        return nullptr;
    }

}