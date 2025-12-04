#pragma once
#include <cstdint>
#include "game/GameMap.h"

class Player
{
public:
    Player(uint32_t id, int start_x, int start_y);

    bool AttemptMove(uint8_t direction, const GameMap &map);
    void SetFacing(uint8_t direction);

    uint32_t GetID() const { return id_; }
    int GetX() const { return x_; }
    int GetY() const { return y_; }
    uint8_t GetFacing() const { return facing_; }

private:
    uint32_t id_;
    int x_;
    int y_;
    uint8_t facing_ = 2; // Default: facing down (0=up, 1=right, 2=down, 3=left)
};