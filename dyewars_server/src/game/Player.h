/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once
#include <cstdint>
#include <atomic>
#include "game/TileMap.h"

class Player
{
public:
    Player(uint64_t id, int start_x, int start_y);

    bool AttemptMove(uint8_t direction, const TileMap &map);
    void SetFacing(uint8_t direction);
    void SetPosition(int16_t x, int16_t y);

    // Getters
    uint64_t GetID() const { return id_; }
    int GetX() const { return x_; }
    int GetY() const { return y_; }
    uint8_t GetFacing() const { return facing_; }

    // Dirty flag - marks player as needing broadcast
    bool IsDirty() const { return is_dirty_.load(); }
    void SetDirty(bool val) { is_dirty_.store(val); }

    // Client link (which network connection owns this player)
    uint64_t GetClientID() const { return client_id_; }
    void SetClientID(uint64_t id) { client_id_ = id; }

private:
    uint64_t id_;
    uint32_t client_id_ = 0;
    int x_;
    int y_;
    uint8_t facing_ = 2; // Default: facing down (0=up, 1=right, 2=down, 3=left)
    std::atomic<bool> is_dirty_{false};
};