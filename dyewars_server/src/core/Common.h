#pragma once

class PlayerRegistry;

class TileMap;

class ClientManager;

// TODO Clear out
// Everything handlers might need
struct GameContext {
    PlayerRegistry &players;
    TileMap &map;
    ClientManager &clients;
    // Add more as needed: CombatSystem&, LuaEngine&, etc.
};