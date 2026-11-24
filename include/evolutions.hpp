#pragma once
#include <cstdint>
#include "types.hpp"

namespace PkBank::Evo {

enum class Method : uint8_t { LevelUp, StoneOrBagItem, LinkTradeSim, LinkTradeWithItem };

struct Rule {
    uint16_t    fromSpecies;
    uint16_t    toSpecies;
    Method      method;
    uint8_t     minLevel;
    uint16_t    requiredItemId;
    const char* itemName;
};

// Strictly legitimate evolutions (Scan Bag for item evolutions; local Link Trade Sim for trade evolutions)
// Success banner on Top Screen: "Successfully evolved Pokemon"
inline constexpr Rule RULES[] = {
    {1,   2,   Method::LevelUp,          16, 0,   nullptr},
    {2,   3,   Method::LevelUp,          32, 0,   nullptr},
    {4,   5,   Method::LevelUp,          16, 0,   nullptr},
    {5,   6,   Method::LevelUp,          36, 0,   nullptr},
    {7,   8,   Method::LevelUp,          16, 0,   nullptr},
    {8,   9,   Method::LevelUp,          36, 0,   nullptr},
    {25,  26,  Method::StoneOrBagItem,    1, 83,  "Thunder Stone"},
    {58,  59,  Method::StoneOrBagItem,    1, 82,  "Fire Stone"},
    {64,  65,  Method::LinkTradeSim,      1, 0,   nullptr},
    {67,  68,  Method::LinkTradeSim,      1, 0,   nullptr},
    {75,  76,  Method::LinkTradeSim,      1, 0,   nullptr},
    {93,  94,  Method::LinkTradeSim,      1, 0,   nullptr},
    {123, 212, Method::LinkTradeWithItem, 1, 233, "Metal Coat"},
    {133, 134, Method::StoneOrBagItem,    1, 84,  "Water Stone"},
    {133, 135, Method::StoneOrBagItem,    1, 83,  "Thunder Stone"},
    {133, 136, Method::StoneOrBagItem,    1, 82,  "Fire Stone"},
    {208, 208, Method::LinkTradeWithItem, 1, 233, "Metal Coat"},
    {525, 526, Method::LinkTradeSim,      1, 0,   nullptr},
    {533, 534, Method::LinkTradeSim,      1, 0,   nullptr},
};

} // namespace PkBank::Evo
