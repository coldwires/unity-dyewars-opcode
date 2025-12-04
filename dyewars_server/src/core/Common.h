#pragma once
#include <cstdint>

// --- PLAYER DATA ---
struct PlayerData {
    uint32_t player_id;
    int x;
    int y;
    uint8_t facing; //0=up, 1=right, 2=down, 3=left
};
