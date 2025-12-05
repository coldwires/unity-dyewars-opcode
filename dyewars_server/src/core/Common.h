#pragma once
class PlayerRegistry;
class TileMap;
class ClientManager;

// Everything handlers might need
struct GameContext {
    PlayerRegistry& players;
    TileMap& map;
    ClientManager& clients;
    // Add more as needed: CombatSystem&, LuaEngine&, etc.
};