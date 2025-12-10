/// =======================================
/// DyeWarsServer - TileMap
/// Pure tile data container - no entity tracking
///
/// This is a "dumb" data structure that holds:
/// - Tile types (grass, water, stone, etc.)
/// - Blocking data (walls, obstacles)
/// - Serialization for client sync
///
/// Created by Anonymous on Dec 05, 2025
/// =======================================
#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>

/// ============================================================================
/// TILE TYPES
///
/// Each tile type is a single byte, allowing efficient network streaming.
/// Client and server share these values to stay in sync.
///
/// Future: These can be loaded from Lua config files.
/// ============================================================================
namespace TileTypes {
    constexpr uint8_t Void = 0x00; // Empty/out of bounds
    constexpr uint8_t Default = 0x01; // Walkable
    constexpr uint8_t Wall = 0x02;
    constexpr uint8_t Grass = 0x03;

    inline bool IsBlocking(uint8_t type) {
        switch (type) {
            case Void:
            case Wall:
                return true;
            default:
                return false;
        }
    }

}
/// ============================================================================
/// TILEMAP
///
/// Pure data container for a single map's tile information.
/// World can own multiple TileMaps for different zones/instances.
///
/// Storage: 1D vector for cache efficiency
/// Index formula: y * width + x
/// ============================================================================
class TileMap {
public:
    /// ========================================================================
    /// CONSTRUCTION
    /// ========================================================================

    /// Create a new tilemap with default grass tiles
    explicit TileMap(
            int16_t width,
            int16_t height,
            uint8_t default_tile = TileTypes::Grass)
            : width_(width),
              height_(height),
              map_id_(0) {
        // Initialize all tiles to default type
        tiles_.resize(width * height, default_tile);

        // Initialize blocking based on tile types
        blocking_.resize(width * height, false);
        RecalculateBlocking();
    }

    /// Create a tilemap from raw byte data (loaded from file or Lua)
    TileMap(int16_t width, int16_t height, const std::vector<uint8_t> &tile_data)
            : width_(width),
              height_(height),
              map_id_(0) {
        if (tile_data.size() != static_cast<size_t>(width * height)) {
            throw std::invalid_argument("Tile data size doesn't match dimensions.");
        }
        tiles_ = tile_data;
        blocking_.resize(width * height, false);
        RecalculateBlocking();

    }

    /// ========================================================================
    /// MAP IDENTITY
    /// ========================================================================
    void SetMapID(uint32_t id) { map_id_ = id; }

    uint32_t GetMapID() const { return map_id_; }

    void SetMapName(const std::string &name) { map_name_ = name; }

    const std::string &GetMapName() const { return map_name_; }

    /// ========================================================================
    /// DIMENSIONS
    /// ========================================================================

    int16_t GetWidth() const { return width_; }

    int16_t GetHeight() const { return height_; }

    bool InBounds(int16_t x, int16_t y) const {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    }

    /// ========================================================================
    /// TILE ACCESS
    /// ========================================================================

    /// Get tile type at position
    /// Returns TileTypes::Void if out of bounds
    uint8_t GetTile(int16_t x, int16_t y) const {
        if (!InBounds(x, y)) return TileTypes::Void;
        return tiles_[Index(x, y)];
    }

    /// Set tile type at position
    /// Also updates blocking state based on tile type
    void SetTile(int16_t x, int16_t y, uint8_t type) {
        if (!InBounds(x, y)) return;
        size_t idx = Index(x, y);
        tiles_[idx] = type;
        blocking_[idx] = TileTypes::IsBlocking(type);
    }

    /// ========================================================================
    /// BLOCKING / COLLISION
    /// ========================================================================

    /// Check if a tile is blocked (out of bounds = blocked)
    bool IsTileBlocked(int16_t x, int16_t y) const {
        if (!InBounds(x, y)) return true;  // Out of bounds is blocked
        return blocking_[Index(x, y)];
    }

    /// Manually set blocking state (for dynamic obstacles, doors, etc.)
    /// This overrides the natural blocking of the tile type
    void SetTileBlocked(int16_t x, int16_t y, bool blocked) {
        if (!InBounds(x, y)) return;
        blocking_[Index(x, y)] = blocked;
    };

    /// Recalculate all blocking from tile types
    /// Call after bulk tile changes
    void RecalculateBlocking() {
        for (size_t i = 0; i < tiles_.size(); ++i) {
            blocking_[i] = TileTypes::IsBlocking(tiles_[i]);
        }
    }

    /// ========================================================================
    /// SERIALIZATION - For Client Sync
    /// ========================================================================

    /// Get raw tile data for entire map
    /// Use for initial map load or full sync
    // TODO the WORLD should contain a map of tilemaps. each tilemap is a 'map'
    // maps are loaded from .map files on the server.
    // Loaded if a player is on them, unloaded when not in use.
    // Psuedo Code:
    // World.current_map_ = TileMap map;
    // World.maps_in_use_ = std::unordered_map<TileMap> or something similar
    // This function will incorporate that.
    const std::vector<uint8_t> &GetRawTileData() const {
        return tiles_;
    }

    /// Get tile data for a rectangular region
    /// For streaming visible area to client
    /// Returns: vector of tile bytes, row by row
    std::vector<uint8_t> GetRegionTiles(
            int16_t start_x,
            int16_t start_y,
            int16_t region_width,
            int16_t region_height) const {
        std::vector<uint8_t> region;
        region.reserve(region_width * region_height);

        for (int16_t y = start_y; y < start_y + region_height; y++) {
            for (int16_t x = start_x; x < start_x + region_width; x++) {
                region.push_back(GetTile(x, y));
            }
        }
        return region;
    }

/// Get tiles in a view centered on a position
    /// Convenience for "send player what they can see"
    /// view_radius: tiles in each direction (total width = 2*radius + 1)
    std::vector<uint8_t> GetViewTiles(int16_t center_x, int16_t center_y, int16_t view_radius) const {
        int16_t size = view_radius * 2 + 1;
        int16_t start_x = center_x - view_radius;
        int16_t start_y = center_y - view_radius;
        return GetRegionTiles(start_x, start_y, size, size);
    }

    /// ========================================================================
    /// BULK OPERATIONS - For Lua / Editor
    /// ========================================================================

    /// Fill a rectangular region with a tile type
    void FillRegion(int16_t start_x, int16_t start_y,
                    int16_t region_width, int16_t region_height, uint8_t type) {
        for (int16_t y = start_y; y < start_y + region_height; y++) {
            for (int16_t x = start_x; x < start_x + region_width; x++) {
                SetTile(x, y, type);
            }
        }
    }

    /// Load tile data from raw bytes (from file or Lua)
    void LoadFromBytes(const std::vector<uint8_t> &data) {
        if (data.size() != static_cast<size_t>(width_ * height_)) {
            throw std::invalid_argument("Data size doesn't match map dimensions");
        }
        tiles_ = data;
        RecalculateBlocking();
    }

    /// Create a wall border around the map
    void CreateBorder() {
        // Top and bottom walls
        for (int16_t x = 0; x < width_; x++) {
            SetTile(x, 0, TileTypes::Wall);
            SetTile(x, height_ - 1, TileTypes::Wall);
        }
        // Left and right walls
        for (int16_t y = 0; y < height_; y++) {
            SetTile(0, y, TileTypes::Wall);
            SetTile(width_ - 1, y, TileTypes::Wall);
        }
    }

private:
    /// ========================================================================
    /// INTERNAL
    /// ========================================================================

    /// Convert 2D coordinates to 1D index
    size_t Index(int16_t x, int16_t y) const {
        return static_cast<size_t>(y * width_ + x);
    }
    /// ========================================================================
    /// DATA
    /// ========================================================================

    int16_t width_;
    int16_t height_;
    uint32_t map_id_;
    std::string map_name_;

    std::vector<uint8_t> tiles_;    // Tile type at each position
    std::vector<bool> blocking_;    // 1D vector is faster than 2D
};