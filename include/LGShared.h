//
// Created by Antoine on 12/11/2025.
//

#ifndef LAYING_GAME_LGSHARED_H
#define LAYING_GAME_LGSHARED_H
#pragma once
#include <unordered_map>
#include <utility>
#include <vector>

// Global shared state to avoid changing Board.h signatures:
// - start cell per player symbol
// - whether placements must touch the start-connected territory (disabled during the start phase)

inline std::unordered_map<char, std::pair<int,int>> LG_START_CELL; // symbol -> {r,c}
inline bool LG_REQUIRE_TOUCH_TO_START = false;

// Bonus types (board overlay stored in Game; Board stays unaware)
enum class LGBonus { None, Coupon, Stone, Robbery };

// Simple placed-tile record so we can implement "robbery"
struct LGPlacedTile {
    char owner = '?';
    int shapeIndex = -1; // index into Tiles
    std::vector<std::vector<int>> shape; // normalized shape matrix used when stealing
    std::vector<std::pair<int,int>> cells; // absolute board cells covered
};

#endif //LAYING_GAME_LGSHARED_H
