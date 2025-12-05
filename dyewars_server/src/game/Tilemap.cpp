/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#include "TileMap.h"

TileMap::TileMap(int16_t width, int16_t height)
        : width_(width), height_(height) {
    walls_.resize(width * height, false);

    // Example: Create a hardcoded wall at (5,5)
    SetWall(5, 5, true);
}

void TileMap::SetWall(int16_t x, int16_t y, bool is_wall) {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
        walls_[y * width_ + x] = is_wall;
    }
}

bool TileMap::IsWalkable(int16_t x, int16_t y) const {
    // Bounds check
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return false;
    }

    // Wall check
    return !walls_[y * width_ + x];
}