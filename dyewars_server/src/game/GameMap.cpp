#include "game/GameMap.h"

GameMap::GameMap(int width, int height) : width_(width), height_(height) {
    // Initialize empty grid
    walls_.resize(width * height, false);

    // EXAMPLE: Create a hardcoded wall at (5,5)
    SetWall(5, 5, true);
}

void GameMap::SetWall(int x, int y, bool is_wall) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        walls_[y * width_ + x] = is_wall;
    }
}

bool GameMap::IsWalkable(int x, int y) const {
    // 1. Bounds Check
    if (x < 0 || x >= width_ || y < 0 || y >= height_) return false;

    // 2. Wall Check
    return !walls_[y * width_ + x];
}