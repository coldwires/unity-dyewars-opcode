#pragma once
#include <vector>
#include <cstdint>
#include <string>
using std::vector;
// --- PACKET STRUCTURE ---
struct Packet {
    uint8_t header[2] = {0x11, 0x68};
    uint16_t size;
    vector<uint8_t> payload;

    vector<uint8_t> ToBytes() const {
        vector<uint8_t> bytes;
        bytes.reserve(4 + payload.size());
        bytes.push_back(header[0]);
        bytes.push_back(header[1]);
        bytes.push_back((size >> 8) & 0xFF);
        bytes.push_back(size & 0xFF);
        bytes.insert(bytes.end(), payload.begin(), payload.end());
        return bytes;
    }
};

// --- PLAYER DATA ---
struct PlayerData {
    uint32_t player_id;
    int x;
    int y;
};