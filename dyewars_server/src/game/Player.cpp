#include "Player.h"

Player::Player(uint32_t id, int start_x, int start_y)
        : id_(id), x_(start_x), y_(start_y), facing_(2) {}


void Player::SetFacing(uint8_t direction){
    if (direction <= 3 && direction >= 0) {  // Validate: 0-3 only
        facing_ = direction;
    }
}

bool Player::AttemptMove(uint8_t direction, const GameMap& map) {
    int new_x = x_;
    int new_y = y_;

    // 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT
    if (direction == 0) new_y++;
    else if (direction == 1) new_x++;
    else if (direction == 2) new_y--;
    else if (direction == 3) new_x--;

    // Ask the Map: "Is this safe?"
    if (map.IsWalkable(new_x, new_y)) {
        x_ = new_x;
        y_ = new_y;
        return true; // Moved!
    }

    return false; // Hit a wall
}