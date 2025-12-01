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
    uint8_t facing; //0=up, 1=right, 2=down, 3=left
};

namespace PacketWriter{
    inline void WriteByte(std::vector<uint8_t>& buffer, uint8_t value)
    {buffer.push_back(value);}

    inline void WriteShort(std::vector<uint8_t> & buffer, uint16_t value) {
            buffer.push_back((value >> 8) & 0xFF);
            buffer.push_back(value & 0xFF);
    }
    inline void WriteUInt(std::vector<uint8_t>& buffer, uint32_t value) {
            buffer.push_back((value >> 24) & 0xFF);
            buffer.push_back((value >> 16) & 0xFF);
            buffer.push_back((value >> 8) & 0xFF);
            buffer.push_back(value & 0xFF);
    }
    inline void WriteInt(std::vector<uint8_t>& buffer, int value) {
            WriteUInt(buffer, static_cast<uint16_t>(value));
    }

}
namespace PacketReader{
    inline uint8_t ReadByte(const std::vector<uint8_t>& buffer, size_t& offset) {
            return buffer[offset++];
    }

    inline uint16_t ReadShort(const std::vector<uint8_t>& buffer, size_t& offset) {
            uint16_t value = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;
            return value;
    }

    inline uint32_t ReadUInt(const std::vector<uint8_t>& buffer, size_t& offset) {
            uint32_t value = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                             (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;
            return value;
    }
    inline int ReadInt(const std::vector<uint8_t>& buffer, size_t& offset) {
            int value = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                             (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;
            return value;
    }

}