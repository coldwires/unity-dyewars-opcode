#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <stdexcept>

using std::vector;

// ============================================================================
// PROTOCOL CONSTANTS - The ONE place for all protocol definitions
// ============================================================================


namespace Protocol {
    constexpr uint32_t PORT = 8080;
    constexpr const char *ADDRESS = "0.0.0.0";

    // Packet framing
    constexpr uint8_t MAGIC_1 = 0x11;
    constexpr uint8_t MAGIC_2 = 0x68;
    constexpr size_t HEADER_SIZE = 4;
    constexpr size_t MAX_PAYLOAD_SIZE = 4096;

    // Handshake validation
    constexpr uint16_t VERSION = 0x0001;
    constexpr uint32_t CLIENT_MAGIC = 0x44594557;  // "DYEW" in ASCII

    // Timing
    constexpr int HANDSHAKE_TIMEOUT_SECONDS = 5;

    struct Packet {
        uint8_t header[2] = {Protocol::MAGIC_1, Protocol::MAGIC_2};
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
            WriteUInt(buffer, static_cast<uint32_t>(value));
        }

    }
    namespace PacketReader{
        inline uint8_t ReadByte(const std::vector<uint8_t>& buffer, size_t& offset) {
            if (offset + 1 > buffer.size()) {
                throw std::out_of_range("Buffer too small for ReadByte");
            }
            return buffer[offset++];
        }

        inline uint16_t ReadShort(const std::vector<uint8_t>& buffer, size_t& offset) {
            if (offset + 2 > buffer.size()) {
                throw std::out_of_range("Buffer too small for ReadShort");
            }
            uint16_t value = (buffer[offset] << 8) | buffer[offset + 1];
            offset += 2;
            return value;
        }

        inline uint32_t ReadUInt(const std::vector<uint8_t>& buffer, size_t& offset) {
            if (offset + 4 > buffer.size()) {
                throw std::out_of_range("Buffer too small for ReadUInt");
            }
            uint32_t value = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                             (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;
            return value;
        }
        inline int ReadInt(const std::vector<uint8_t>& buffer, size_t& offset) {
            if (offset + 4 > buffer.size()) {
                throw std::out_of_range("Buffer too small for ReadInt");
            }
            int value = (buffer[offset] << 24) | (buffer[offset + 1] << 16) |
                        (buffer[offset + 2] << 8) | buffer[offset + 3];
            offset += 4;
            return value;
        }

    }
}

