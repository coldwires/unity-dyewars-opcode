#include "MoveHandler.h"
#include "game/PlayerRegistry.h"
#include "game/TileMap.h"

namespace Handlers {

    std::shared_ptr<Player> HandleMove(
            const Actions::MoveCommand& cmd,
            PlayerRegistry& registry,
            TileMap& map)
    {
        auto player = registry.GetByID(cmd.player_id);
        if (!player) return nullptr;

        if (cmd.direction == cmd.facing && cmd.direction == player->GetFacing()) {
            if (player->AttemptMove(cmd.direction, map)) {
                player->SetDirty(true);
                return player;
            }
        } else {
            player->SetFacing(cmd.direction);
            player->SetDirty(true);
            return player;
        }
        return nullptr;
    }

    std::shared_ptr<Player> HandleTurn(
            const Actions::TurnCommand& cmd,
            PlayerRegistry& registry)
    {
        auto player = registry.GetByID(cmd.player_id);
        if (!player) return nullptr;

        player->SetFacing(cmd.direction);
        player->SetDirty(true);
        return player;
    }

}