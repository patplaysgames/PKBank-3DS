#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include "types.hpp"

namespace PkBank {

struct TransferCheck {
    bool allowed;
    bool isOneWay;
    bool stripsHeldItem;
    bool requiresSequentialStep;
    std::string reason;
};

inline uint16_t maxNationalDexForGame(GameVer v) {
    switch (v) {
        case GameVer::RB:
        case GameVer::Y:    return 151;
        case GameVer::GS:
        case GameVer::C:    return 251;
        case GameVer::RS:
        case GameVer::E:
        case GameVer::FRLG: return 386;
        case GameVer::DP:
        case GameVer::Pt:
        case GameVer::HGSS: return 493;
        case GameVer::BW:
        case GameVer::B2W2: return 649;
        case GameVer::XY:
        case GameVer::ORAS: return 721;
        case GameVer::SM:   return 802;
        case GameVer::USUM: return 807;
    }
    return 807;
}

inline bool isHmForGen(uint16_t moveId, Gen g) {
    switch (moveId) {
        case 15:  return g <= Gen::G6; // Cut
        case 19:  return g <= Gen::G6; // Fly
        case 57:  return g <= Gen::G6; // Surf
        case 70:  return g <= Gen::G6; // Strength
        case 148: return g <= Gen::G3; // Flash
        case 249: return g == Gen::G2 || g == Gen::G3 || g == Gen::G4 || g == Gen::G6; // Rock Smash
        case 127: return g >= Gen::G2 && g <= Gen::G6; // Waterfall
        case 250: return g == Gen::G2 || g == Gen::G4; // Whirlpool
        case 291: return g == Gen::G3 || g == Gen::G5 || g == Gen::G6; // Dive
        case 432: return g == Gen::G4; // Defog
        case 431: return g == Gen::G4; // Rock Climb
        default:  return false;
    }
}

// Official gate: every mainline game must obtain the Pokedex before using Pokemon Bank / Poke Transporter
inline std::string validateBankAccess(const SaveSession& save) {
    if (!save.progression.hasPokedex) {
        return "Official rule: " + save.versionName + " must obtain the Pokedex before using Pokemon Bank.";
    }
    return "";
}

inline TransferCheck validateTransfer(
    const ReadOnlyMon& mon,
    GameVer src,
    GameVer dst,
    Gen srcGen,
    Gen dstGen,
    const SaveProgressionFlags* srcProg = nullptr,
    const SaveProgressionFlags* dstProg = nullptr
) {
    if (mon.isShadow) return {false, false, false, false, "Unpurified Shadow Pokemon cannot transfer."};
    if (mon.isEgg && srcGen != dstGen) return {false, false, false, false, "Eggs cannot transfer across generations."};
    if (mon.species == 172 && mon.form == 1) return {false, false, false, false, "Spiky-eared Pichu cannot leave HGSS."};
    if (mon.species == 25 && src == GameVer::ORAS && mon.form >= 1 && mon.form <= 5) {
        return {false, false, false, false, "Cosplay Pikachu cannot leave ORAS or enter Bank."};
    }
    if (mon.isFused && srcGen != dstGen) return {false, false, false, false, "Fused Pokemon must be separated first."};
    if (mon.species > maxNationalDexForGame(dst)) {
        return {false, false, false, false, "Species does not exist in target game."};
    }

    // Per-game official story/unlock constraints (prevents cheating around in-game transfer unlocks)
    if (srcProg && !srcProg->hasPokedex) {
        return {false, false, false, false, "Official rule: Source save must obtain the Pokedex first."};
    }
    if (dstProg && !dstProg->hasPokedex) {
        return {false, false, false, false, "Official rule: Target save must obtain the Pokedex first."};
    }

    if (srcGen == Gen::G1 || srcGen == Gen::G2) {
        if (dstGen == Gen::G7) {
            return {true, true, true, false, "Poke Transporter VC -> Gen 7 (Nature=EXP%25, HA, >=3x31 Potential)."};
        }
        if ((srcGen == Gen::G1 && dstGen == Gen::G2) || (srcGen == Gen::G2 && dstGen == Gen::G1)) {
            const SaveProgressionFlags* g2Prog = (srcGen == Gen::G2) ? srcProg : dstProg;
            if (g2Prog && !g2Prog->timeCapsuleUnlocked) {
                return {false, false, false, false, "Official rule: Meet Bill at Ecruteak Pokemon Center to unlock Time Capsule."};
            }
        }
        if (dstGen == Gen::G1 && srcGen == Gen::G2 && mon.species > 151) {
            return {false, false, false, false, "Gen 2 species #152+ barred from Gen 1 Time Capsule."};
        }
        if (dstGen >= Gen::G3 && dstGen <= Gen::G6) {
            return {false, false, false, false, "VC Gen 1/2 can only transfer to Gen 7 via Transporter."};
        }
        return {true, false, dstGen == Gen::G1, false, "Time Capsule compatible."};
    }

    if (dstGen < srcGen || mon.currentGen > dstGen) {
        return {false, false, false, false, "Backwards transfer is not allowed."};
    }

    // Gen 3 Kanto (FRLG) <-> Hoenn (RS/Emerald) official unlock checks
    if (srcGen == Gen::G3 && dstGen == Gen::G3) {
        const bool involvesFrlg = (src == GameVer::FRLG || dst == GameVer::FRLG);
        const bool involvesHoenn = (src == GameVer::RS || src == GameVer::E || dst == GameVer::RS || dst == GameVer::E);
        if (involvesFrlg && involvesHoenn) {
            const SaveProgressionFlags* frlgProg = (src == GameVer::FRLG) ? srcProg : dstProg;
            if (frlgProg && !frlgProg->celioQuestCompleted) {
                return {false, false, false, false, "Official rule: Complete Celio's Network Machine quest in FRLG before trading with Hoenn."};
            }
            if (src == GameVer::E || dst == GameVer::E) {
                const SaveProgressionFlags* emProg = (src == GameVer::E) ? srcProg : dstProg;
                if (emProg && !emProg->hasNationalDex) {
                    return {false, false, false, false, "Official rule: Emerald must obtain the National Pokedex before trading with FRLG."};
                }
            }
        }
        if (dst == GameVer::FRLG && dstProg && !dstProg->hasNationalDex && mon.species > 151) {
            return {false, false, false, false, "Official rule: FRLG must obtain the National Pokedex before receiving species #152+."};
        }
    }

    // Strict sequential progression for Gen 3 -> 4 -> 5 -> 6 -> 7
    if (static_cast<uint8_t>(dstGen) - static_cast<uint8_t>(srcGen) > 1) {
        return {
            false, true, true, true,
            "Cannot skip generation transfer (Gen " + std::to_string(static_cast<int>(srcGen)) +
            " -> Gen " + std::to_string(static_cast<int>(dstGen)) + "). Please transfer to Gen " +
            std::to_string(static_cast<int>(srcGen) + 1) + " first."
        };
    }

    if (srcGen == Gen::G3 && dstGen == Gen::G4) {
        if (dstProg && !dstProg->palParkUnlocked) {
            return {false, true, false, false, "Official rule: Unlock Pal Park (National Dex / Fuchsia City) before Gen 3 -> Gen 4 transfer."};
        }
        for (const auto& mv : mon.moves) {
            if (isHmForGen(mv.moveId, Gen::G3)) {
                return {false, true, false, false, "Pal Park blocks Gen 3 HM moves."};
            }
        }
        return {true, true, false, false, "Pal Park Gen 3 -> Gen 4 verified."};
    }
    if (srcGen == Gen::G4 && dstGen == Gen::G5) {
        if (dstProg && !dstProg->pokeTransferLabUnlocked) {
            return {false, true, true, false, "Official rule: Clear Hall of Fame to unlock Route 15 Poke Transfer Lab before Gen 4 -> Gen 5 transfer."};
        }
        for (const auto& mv : mon.moves) {
            if (isHmForGen(mv.moveId, Gen::G4)) {
                return {false, true, true, false, "Poke Transfer blocks Gen 4 HM moves."};
            }
        }
        return {true, true, true, false, "Poke Transfer Lab Gen 4 -> Gen 5 (held item returned to Bag)."};
    }
    if (dst == GameVer::SM && mon.species >= 803) {
        return {false, false, false, false, "USUM-exclusive species barred from Sun/Moon."};
    }
    bool strips = (dstGen > srcGen) && !(srcGen == Gen::G3 && dstGen == Gen::G4);
    return {true, dstGen > srcGen, strips, false, "Legal transfer verified."};
}

// Used by KEY_ZR (Validation Transfer filter) to grey out illegal or non-transferable Pokemon
inline bool isPokemonLegal(
    const ReadOnlyMon& mon,
    bool inBank,
    const SaveSession* activeSave
) {
    const Gen effectiveGen = inBank
        ? (mon.currentGen > mon.bankStoredFromGen ? mon.currentGen : mon.bankStoredFromGen)
        : mon.currentGen;

    if (mon.species < 1 || mon.species > 809) return false;
    if (mon.level < 1 || mon.level > 100) return false;
    if (mon.originGen > effectiveGen) return false;
    if (mon.isShadow) return false;

    const uint16_t maxIv = (effectiveGen <= Gen::G2) ? 15 : 31;
    if (mon.potential.hp > maxIv || mon.potential.atk > maxIv || mon.potential.def > maxIv ||
        mon.potential.spa > maxIv || mon.potential.spd > maxIv || mon.potential.spe > maxIv) {
        return false;
    }
    if (effectiveGen >= Gen::G3) {
        if (mon.basePoints.hp > 252 || mon.basePoints.atk > 252 || mon.basePoints.def > 252 ||
            mon.basePoints.spa > 252 || mon.basePoints.spd > 252 || mon.basePoints.spe > 252) {
            return false;
        }
        const uint32_t evSum = mon.basePoints.hp + mon.basePoints.atk + mon.basePoints.def +
                               mon.basePoints.spa + mon.basePoints.spd + mon.basePoints.spe;
        if (evSum > 510) return false;
    }
    if (!mon.isEgg) {
        int moveCount = 0;
        for (int i = 0; i < 4; ++i) {
            if (mon.moves[i].moveId == 0) continue;
            ++moveCount;
            if (mon.moves[i].introducedGen > effectiveGen) return false;
            for (int j = i + 1; j < 4; ++j) {
                if (mon.moves[j].moveId == mon.moves[i].moveId) return false;
            }
        }
        if (moveCount == 0) return false;
    }

    if (activeSave) {
        if (!inBank && mon.species > maxNationalDexForGame(activeSave->gameVersion)) return false;
        if (inBank) {
            const auto check = validateTransfer(
                mon,
                mon.bankStoredFromGame,
                activeSave->gameVersion,
                effectiveGen,
                activeSave->generation,
                &mon.bankStoredFromProg,
                &activeSave->progression
            );
            if (!check.allowed) return false;
        }
    }
    return true;
}

// Official Gen IV-VII Characteristic (displayed below Nature for Gen 4+)
inline const char* getCharacteristic(const Stats6& ivs, uint32_t pid) {
    static constexpr const char* TABLE[6][5] = {
        {"Loves to eat", "Takes plenty of siestas", "Nods off a lot", "Scatters things often", "Likes to relax"},
        {"Proud of its power", "Likes to thrash about", "A little quick tempered", "Likes to fight", "Quick tempered"},
        {"Sturdy body", "Capable of taking hits", "Highly persistent", "Good endurance", "Good perseverance"},
        {"Likes to run", "Alert to sounds", "Impetuous and silly", "Somewhat of a clown", "Quick to flee"},
        {"Highly curious", "Mischievous", "Thoroughly cunning", "Often lost in thought", "Very finicky"},
        {"Strong willed", "Somewhat vain", "Strongly defiant", "Hates to lose", "Somewhat stubborn"},
    };
    const uint16_t order[6] = {ivs.hp, ivs.atk, ivs.def, ivs.spe, ivs.spa, ivs.spd};
    uint16_t best = 0;
    for (uint16_t v : order) if (v > best) best = v;
    const uint32_t start = pid % 6;
    for (uint32_t i = 0; i < 6; ++i) {
        const uint32_t idx = (start + i) % 6;
        if (order[idx] == best) return TABLE[idx][best % 5];
    }
    return TABLE[0][0];
}

// Origin label: "Gen 3" or "Gen 5 (Transferred from Gen 3)"
inline std::string formatOriginGenStatus(const ReadOnlyMon& mon, Gen effectiveGen) {
    if (effectiveGen > mon.originGen) {
        return "Gen " + std::to_string(static_cast<int>(effectiveGen)) +
               " (Transferred from Gen " + std::to_string(static_cast<int>(mon.originGen)) + ")";
    }
    return "Gen " + std::to_string(static_cast<int>(mon.originGen));
}

// OT arranged as: Name (ID No.) · Single Version
inline std::string formatOtLine(const ReadOnlyMon& mon, Gen effectiveGen) {
    char idBuf[16]{};
    if (effectiveGen >= Gen::G7) {
        uint32_t fullId = (static_cast<uint32_t>(mon.sid) << 16) | mon.tid;
        std::snprintf(idBuf, sizeof(idBuf), "%06u", fullId % 1000000U);
    } else {
        std::snprintf(idBuf, sizeof(idBuf), "%05u", static_cast<unsigned>(mon.tid));
    }
    return mon.otName + " (" + std::string(idBuf) + ") · " + mon.originVersionName;
}

inline const char* getOriginRegion(Gen g, GameVer ver) {
    switch (g) {
        case Gen::G1: return "Kanto";
        case Gen::G2: return "Johto";
        case Gen::G3: return ver == GameVer::FRLG ? "Kanto" : "Hoenn";
        case Gen::G4: return ver == GameVer::HGSS ? "Johto" : "Sinnoh";
        case Gen::G5: return "Unova";
        case Gen::G6: return ver == GameVer::ORAS ? "Hoenn" : "Kalos";
        case Gen::G7: return "Alola";
    }
    return "Hoenn";
}

// Official Pokémon Trainer Memo / Met entry sentence
inline std::string formatOfficialMetEntry(const ReadOnlyMon& mon, Gen effectiveGen, bool isOwnTrainer) {
    if (mon.isEgg) return "A mysterious Pokemon Egg.";
    const std::string region = getOriginRegion(mon.originGen, mon.originGame);
    const int metLv = mon.metLevel > 0 ? mon.metLevel : mon.level;
    const std::string datePrefix = mon.metDate.empty() ? "" : (mon.metDate + " · ");

    if (effectiveGen > mon.originGen) {
        if (mon.originGen <= Gen::G2 && effectiveGen >= Gen::G7) {
            return "Seems to have traveled across both space and time to reach you from the " +
                   region + " region in the good old days.";
        }
        if (mon.originGen == Gen::G3 && effectiveGen == Gen::G4) {
            return region + ". Arrived at Lv. " + std::to_string(metLv) + ".";
        }
        if (effectiveGen == Gen::G5) {
            return region + ". Apparently arrived at Lv. " + std::to_string(metLv) +
                   " after a long travel through time.";
        }
        return "Seems to have traveled across both space and time to reach you from the " +
               region + " region.";
    }

    if (mon.originGen == Gen::G3 && mon.metLevel == 0) {
        const std::string loc = mon.metLocationName.empty() ? region : mon.metLocationName;
        return "Hatched at " + loc + " at Lv. 5.";
    }
    if (mon.originGen <= Gen::G2 && mon.metLocationName.empty()) {
        return "Met in the " + region + " region.";
    }
    const std::string verb = isOwnTrainer ? "Met" : "Apparently met";
    const std::string loc = mon.metLocationName.empty() ? ("the " + region + " region") : mon.metLocationName;
    return datePrefix + verb + " at " + loc + " at Lv. " + std::to_string(metLv) + ".";
}

} // namespace PkBank
