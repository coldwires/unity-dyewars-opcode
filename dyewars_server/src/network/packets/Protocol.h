#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// REMOVED: "using std::vector;" was here
// WHY: "using" directives in headers pollute the global namespace for EVERY file
// that includes this header. If another file defines its own "vector" class or
// uses a different library with a vector type, you get name collisions.
// Rule: Never put "using" in headers. Always use fully-qualified names (std::vector).

// ============================================================================
// PROTOCOL CONSTANTS - The ONE place for all protocol definitions
// ============================================================================


namespace Protocol {
    constexpr uint32_t PORT = 8081;
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

    // Leniency / Hacking
    constexpr uint8_t MAX_HEADER_VIOLATIONS = 3;

    struct Packet {
        uint8_t header[2] = {Protocol::MAGIC_1, Protocol::MAGIC_2};
        uint16_t size;
        std::vector<uint8_t> payload;

        std::vector<uint8_t> ToBytes() const {
            std::vector<uint8_t> bytes;
            bytes.reserve(4 + payload.size());
            bytes.push_back(header[0]);
            bytes.push_back(header[1]);
            bytes.push_back((size >> 8) & 0xFF);
            bytes.push_back(size & 0xFF);
            bytes.insert(bytes.end(), payload.begin(), payload.end());
            return bytes;
        }
    };

    namespace PacketWriter {
        inline void WriteByte(std::vector<uint8_t> &buffer, const uint8_t value) {
            buffer.push_back(value);
        }

        inline void WriteShort(std::vector<uint8_t> &buffer, const uint16_t value) {
            buffer.push_back((value >> 8) & 0xFF);
            buffer.push_back(value & 0xFF);
        }

        inline void WriteUInt(std::vector<uint8_t> &buffer, const uint32_t value) {
            buffer.push_back((value >> 24) & 0xFF);
            buffer.push_back((value >> 16) & 0xFF);
            buffer.push_back((value >> 8) & 0xFF);
            buffer.push_back(value & 0xFF);
        }

        // CHANGE: Use int32_t instead of int
        // WHY: `int` is platform-dependent (could be 16-bit on some embedded systems,
        // though realistically 32-bit everywhere you'll run this). Using int32_t
        // makes the wire format explicit and guarantees 4 bytes always.
        inline void WriteInt32(std::vector<uint8_t> &buffer, const int32_t value) {
            WriteUInt(buffer, static_cast<uint32_t>(value));
        }

        inline void WriteUInt64(std::vector<uint8_t> &buffer, const uint64_t value) {
            buffer.push_back((value >> 56) & 0xFF);
            buffer.push_back((value >> 48) & 0xFF);
            buffer.push_back((value >> 40) & 0xFF);
            buffer.push_back((value >> 32) & 0xFF);
            buffer.push_back((value >> 24) & 0xFF);
            buffer.push_back((value >> 16) & 0xFF);
            buffer.push_back((value >> 8) & 0xFF);
            buffer.push_back(value & 0xFF);
        }

        inline void WriteInt64(std::vector<uint8_t> &buffer, const int64_t value) {
            WriteUInt64(buffer, static_cast<uint64_t>(value));
        }
    }

    namespace PacketReader {
        inline uint8_t ReadByte(const std::vector<uint8_t> &buffer, size_t &offset) {
            // BOUNDS CHECK: Use subtraction instead of addition to prevent overflow.
            // If offset is SIZE_MAX, "offset + 1" wraps to 0, bypassing the check!
            // "buffer.size() - offset < 1" is safe because we check offset <= size first.
            if (offset >= buffer.size()) {
                throw std::out_of_range("Buffer too small for ReadByte");
            }
            return buffer[offset++];
        }

        inline uint16_t ReadShort(const std::vector<uint8_t> &buffer, size_t &offset) {
            // Check if there's room for 2 bytes without overflow
            if (offset > buffer.size() || buffer.size() - offset < 2) {
                throw std::out_of_range("Buffer too small for ReadShort");
            }
            // CHANGE: Added static_cast<uint16_t>
            // WHY: buffer[offset] is uint8_t. When you use << on it, C++ performs
            // "integer promotion" and converts it to `int` first. The result is `int`.
            // Then you OR two ints together, get an int, and return it as uint16_t.
            // This works fine in practice, but the cast makes intent explicit and
            // silences any compiler warnings about implicit narrowing.
            const uint16_t value = static_cast<uint16_t>(
                    (buffer[offset] << 8) | buffer[offset + 1]
            );
            offset += 2;
            return value;
        }

        inline uint32_t ReadUInt(const std::vector<uint8_t> &buffer, size_t &offset) {
            // Check if there's room for 4 bytes without overflow
            if (offset > buffer.size() || buffer.size() - offset < 4) {
                throw std::out_of_range("Buffer too small for ReadUInt");
            }
            // CHANGE: Cast BEFORE shifting, not after
            // WHY: buffer[offset] is uint8_t, promoted to int (typically 32-bit signed).
            // Shifting int << 24 works, but if the high bit of the byte is set, you're
            // shifting into the sign bit of a signed int, which is undefined behavior
            // in C++ (until C++20 where it's well-defined but still not what you want).
            //
            // Example of the bug:
            //   buffer[0] = 0x80 (128)
            //   0x80 << 24 = 0x80000000 as int = -2147483648 (negative!)
            //   OR-ing that with other values gives wrong results
            //
            // By casting to uint32_t first, we ensure unsigned arithmetic throughout.
            const uint32_t value =
                    (static_cast<uint32_t>(buffer[offset]) << 24) |
                    (static_cast<uint32_t>(buffer[offset + 1]) << 16) |
                    (static_cast<uint32_t>(buffer[offset + 2]) << 8) |
                    static_cast<uint32_t>(buffer[offset + 3]);
            offset += 4;
            return value;
        }

        // CHANGE: Renamed to ReadInt32, delegates to ReadUInt
        // WHY: Same platform-independence reason as WriteInt32.
        // Also, the original had the same shift bug as ReadUInt.
        // Reinterpreting the bits via static_cast is correct for two's complement
        // (which is guaranteed as of C++20, and universal in practice).
        inline int32_t ReadInt32(const std::vector<uint8_t> &buffer, size_t &offset) {
            return static_cast<int32_t>(ReadUInt(buffer, offset));
        }

        inline uint64_t ReadUInt64(const std::vector<uint8_t> &buffer, size_t &offset) {
            // Check if there's room for 8 bytes without overflow
            if (offset > buffer.size() || buffer.size() - offset < 8) {
                throw std::out_of_range("Buffer too small for ReadUInt64");
            }
            // WHY static_cast on every byte: Without it, uint8_t promotes to int.
            // Shifting int by 32+ bits is undefined behavior (shift amount >= width of type).
            // Even shifting by 24 has the sign bit issue mentioned above.
            // Casting to uint64_t first means we're shifting a 64-bit value, so shifts
            // up to 63 are valid.
            const uint64_t value =
                    (static_cast<uint64_t>(buffer[offset]) << 56) |
                    (static_cast<uint64_t>(buffer[offset + 1]) << 48) |
                    (static_cast<uint64_t>(buffer[offset + 2]) << 40) |
                    (static_cast<uint64_t>(buffer[offset + 3]) << 32) |
                    (static_cast<uint64_t>(buffer[offset + 4]) << 24) |
                    (static_cast<uint64_t>(buffer[offset + 5]) << 16) |
                    (static_cast<uint64_t>(buffer[offset + 6]) << 8) |
                    static_cast<uint64_t>(buffer[offset + 7]);
            offset += 8;
            return value;
        }

        inline int64_t ReadInt64(const std::vector<uint8_t> &buffer, size_t &offset) {
            return static_cast<int64_t>(ReadUInt64(buffer, offset));
        }
    }
}