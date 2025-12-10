/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <cstdint>
#include <variant>
#include <string>
#include <memory>

class GameServer;

class Player;

namespace Actions {

    namespace Movement {
        void Move(GameServer *server, uint64_t client_id, uint8_t direction, uint8_t facing);

        void Turn(GameServer *server, uint64_t client_id, uint8_t facing);

        void Warp(GameServer *server, uint64_t client_id, uint16_t map_id, int16_t x, int16_t y);
    }

    namespace Combat {
        void Attack(GameServer *server, uint64_t client_id, uint64_t target_id);

        void UseSkill(GameServer *server, uint64_t client_id, uint16_t skill_id, int16_t target_x, int16_t target_y);

        void UseItem(GameServer *server, uint64_t client_id, uint8_t slot);
    }

    namespace Social {
        void Say(GameServer *server, uint64_t client_id, const std::string &message);

        void
        Whisper(GameServer *server, uint64_t client_id, const std::string &target_name, const std::string &message);

        void Shout(GameServer *server, uint64_t client_id, const std::string &message);
    }

    namespace Session {
        void Login(GameServer *server, std::shared_ptr<class ClientConnection> client);

        void Logout(GameServer *server, uint64_t client_id);

        void Kick(GameServer *server, uint64_t client_id, const std::string &reason);
    }

    namespace Inventory {
        void PickupItem(GameServer *server, uint64_t client_id, uint64_t entity_id);

        void DropItem(GameServer *server, uint64_t client_id, uint8_t slot, uint16_t quantity);

        void MoveItem(GameServer *server, uint64_t client_id, uint8_t from_slot, uint8_t to_slot);
    }

    namespace Trade {
        void RequestTrade(GameServer *server, uint64_t client_id, uint64_t target_id);

        void AcceptTrade(GameServer *server, uint64_t client_id);

        void CancelTrade(GameServer *server, uint64_t client_id);

        void AddItem(GameServer *server, uint64_t client_id, uint8_t slot, uint16_t quantity);

        void ConfirmTrade(GameServer *server, uint64_t client_id);
    }

}