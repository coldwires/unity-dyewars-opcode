/// =======================================
/// DyeWarsServer
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once
#include <vector>
#include <cstdint>

class TileMap {
public:
    TileMap(int16_t width, int16_t height);

    bool IsWalkable(int16_t x, int16_t y) const;
    int16_t GetWidth() const { return width_; }
    int16_t GetHeight() const { return height_; }

    // Toggle a wall (useful for dynamic events later)
    void SetWall(int16_t x, int16_t y, bool is_wall);

private:
    int16_t width_;
    int16_t height_;
    std::vector<bool> walls_;  // 1D vector is faster than 2D
};