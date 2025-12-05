#include "Player.h"

Player::Player(uint64_t id, int start_x, int start_y)
        : id_(id), x_(start_x), y_(start_y), facing_(2) {}

void Player::SetFacing(uint8_t direction){
    if (direction <= 3 && direction >= 0) {  // Validate: 0-3 only
        facing_ = direction;
    }
}

void Player::SetPosition(int16_t x, int16_t y) {
    // TODO Bounds check / Density check
    x_ = x;
    y_ = y;
}

bool Player::AttemptMove(uint8_t direction, const TileMap& map) {
    int new_x = x_;
    int new_y = y_;

    // 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT
    switch (direction) {
        case 0: new_y++; break;
        case 1: new_x++; break;
        case 2: new_y--; break;
        case 3: new_x--; break;
        default: return false;
    }

    // Ask the Map: "Is this safe?"
    if (map.IsWalkable(new_x, new_y)) {
        x_ = new_x;
        y_ = new_y;
        return true; // Moved!
    }
    return false; // Hit a wall
}
