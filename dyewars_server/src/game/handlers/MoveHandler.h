#pragma once
#include <memory>
#include "game/Player.h"
#include "game/Actions.h"

class TileMap;
class PlayerRegistry;

namespace Handlers {

// Returns player if updated, nullptr if not
    std::shared_ptr<Player> HandleMove(
            const Actions::MoveCommand& cmd,
            PlayerRegistry& registry,
            TileMap& map);

    std::shared_ptr<Player> HandleTurn(
            const Actions::TurnCommand& cmd,
            PlayerRegistry& registry);

}