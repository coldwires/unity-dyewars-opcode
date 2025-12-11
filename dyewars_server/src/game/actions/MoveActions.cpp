#include "Actions.h"
#include "server/GameServer.h"
#include "network/packets/outgoing/PacketSender.h"
#include "core/Log.h"

namespace Actions::Movement {
    void Move(GameServer *server, uint64_t client_id, uint8_t direction, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            // Get client connection once - used for ping and potential correction
            auto conn = server->Clients().GetClient(client_id);
            uint32_t ping_ms = conn ? conn->GetPing() : 0;

            // Occupancy check: is another player at (x, y)?
            uint64_t player_id = player->GetID();
            auto is_occupied = [server, player_id](int16_t x, int16_t y) {
                return server->GetWorld().IsPositionOccupied(x, y, player_id);
            };

            auto result = player->AttemptMove(direction, facing, server->GetWorld().GetMap(), ping_ms, is_occupied);

            if (result == MoveResult::Success) {
                server->GetWorld().UpdatePlayerPosition(
                        player->GetID(),
                        player->GetX(),
                        player->GetY());
                server->Players().MarkDirty(player);

                // ============================================================
                // VISIBILITY UPDATE (two parts)
                // 1. Update mover's view: who entered/left MY view?
                // 2. Update observers: who can no longer see ME?
                // ============================================================

                // Part 1: Update mover's own visibility
                if (conn) {
                    auto visible = server->GetWorld().GetPlayersInRange(
                            player->GetX(), player->GetY());

                    auto diff = server->GetWorld().Visibility().Update(player_id, visible);

                    // Send S_Player_Spatial for players who entered mover's view
                    if (!diff.entered.empty()) {
                        Packets::PacketSender::BatchPlayerSpatial(conn, diff.entered);
                    }

                    // Send S_Left_Game for players who left mover's view
                    for (uint64_t left_id : diff.left) {
                        Packets::PacketSender::PlayerLeft(conn, left_id);
                    }
                }

                // Part 2: Notify observers who lost sight of the mover
                // (When B walks away from A, A needs to know B left their view)
                auto get_player_pos = [server](uint64_t id) -> std::pair<int16_t, int16_t> {
                    auto p = server->GetWorld().GetPlayer(id);
                    return p ? std::make_pair(p->GetX(), p->GetY())
                             : std::make_pair<int16_t, int16_t>(0, 0);
                };

                auto observers_who_lost_sight = server->GetWorld().Visibility()
                        .NotifyObserversOfDeparture(
                                player_id,
                                player->GetX(),
                                player->GetY(),
                                World::VIEW_RANGE,
                                get_player_pos);

                // Send S_Left_Game to each observer who can no longer see the mover
                for (uint64_t observer_id : observers_who_lost_sight) {
                    auto observer = server->GetWorld().GetPlayer(observer_id);
                    if (!observer) continue;

                    auto observer_conn = server->Clients().GetClient(observer->GetClientID());
                    if (!observer_conn) continue;

                    Packets::PacketSender::PlayerLeft(observer_conn, player_id);
                }
            } else {
                // Move failed (collision, cooldown, etc.) - rubber band client back
                Log::Trace("Player {} move attempt failed: dir={}, facing={}, result={}",
                           player->GetID(), direction, facing, static_cast<int>(result));
                if (conn) {
                    Packets::PacketSender::PositionCorrection(
                            conn,
                            player->GetX(),
                            player->GetY(),
                            player->GetFacing());
                }
            }
        });
    }

    void Turn(GameServer *server, uint64_t client_id, uint8_t facing) {
        server->QueueAction([=]() {
            auto player = server->Players().GetByClientID(client_id);
            if (!player) return;

            player->SetFacing(facing);
            server->Players().MarkDirty(player);
        });
    }
}