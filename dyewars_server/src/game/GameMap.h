#pragma once
#include <vector>
#include <mutex>

class GameMap {
public:
    GameMap(int width, int height);

    bool IsWalkable(int x, int y) const;
    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

    // Toggle a wall (useful for dynamic events later)
    void SetWall(int x, int y, bool is_wall);

private:
    int width_;
    int height_;
    std::vector<bool> walls_; // 1D vector is faster than 2D vector
};