#pragma once
#include <cstdint>
#include "GameMap.h" // Player needs to know about the Map to move

class Player {
public:
    Player(uint32_t id, int start_x, int start_y);

    // Returns TRUE if move succeeded
    bool AttemptMove(uint8_t direction, const GameMap& map);

    uint32_t GetID() const { return id_; }
    int GetX() const { return x_; }
    int GetY() const { return y_; }

private:
    uint32_t id_;
    int x_;
    int y_;
};