/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#pragma once
#include <memory>
#include <vector>
class ClientConnection;
class GameServer;
using namespace std;

namespace PacketHandler {
    void Handle(
            std::shared_ptr<ClientConnection> client,
            const std::vector<uint8_t> &data,
            GameServer *server
                );
};
