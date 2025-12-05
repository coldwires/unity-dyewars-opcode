/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#pragma once
#include <memory>
#include "TileMap.h"

class World {
public:
    World(int width, int height);

    TileMap &GetMap() { return *tilemap_; }
    const TileMap &GetMap() const {return *tilemap_;}

private:
    std::unique_ptr<TileMap> tilemap_;
};
