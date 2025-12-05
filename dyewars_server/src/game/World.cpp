/// =======================================
/// Created by Anonymous on Dec 05, 2025
/// =======================================
//
#include "World.h"

World::World(int width, int height)
    : tilemap_(make_unique<TileMap>(width,height))
{}