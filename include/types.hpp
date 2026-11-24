#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace PkBank {

enum class Gen : uint8_t { G1 = 1, G2 = 2, G3 = 3, G4 = 4, G5 = 5, G6 = 6, G7 = 7 };

enum class GameVer : uint8_t {
    RB, Y, GS, C, RS, E, FRLG, DP, Pt, HGSS, BW, B2W2, XY, ORAS, SM, USUM
};

enum class OriginMark : uint8_t { None, Pentagon, Clover, GameBoy };

enum class PokerusState : uint8_t { None, Infected, Cured };

enum class Gender : uint8_t { Male, Female, Genderless };

enum class CursorMode : uint8_t { RedSingle, BlueQuickSwap, GreenMultiSelect };

enum class TopViewMode : uint8_t { Summary, Stats, HiddenStats };

enum class FocusSection : uint8_t { BankStorage, SaveBox, SaveParty };

enum class SortMode : uint8_t { RegionalDex, NationalDex, NameAsc, NameDesc };

// Official per-game story & unlock flags parsed directly from Gen 1-7 save binaries
// Prevents bypassing official trading/transfer gates (e.g., Pokedex, FRLG Celio quest, Pal Park)
struct SaveProgressionFlags {
    bool hasPokedex              = false; // Required by all Gen 1-7 games for trading / Bank / Transporter
    bool hasNationalDex          = false; // Required for FRLG #152+ & Emerald <-> FRLG trades
    bool hallOfFameCleared       = false; // League cleared
    bool timeCapsuleUnlocked     = false; // Gen 2 (GSC): Met Bill at Ecruteak Pokemon Center
    bool celioQuestCompleted     = false; // Gen 3 (FRLG): FLAG_SYS_CAN_LINK_WITH_RS (Ruby & Sapphire plates)
    bool palParkUnlocked         = false; // Gen 4 (DP/Pt/HGSS): National Dex Route 221 / Fuchsia City
    bool pokeTransferLabUnlocked = false; // Gen 5 (BW/B2W2): Hall of Fame Route 15 Transfer Lab
};

struct Stats6 {
    uint16_t hp  = 0;
    uint16_t atk = 0;
    uint16_t def = 0;
    uint16_t spa = 0;
    uint16_t spd = 0;
    uint16_t spe = 0;
};

struct MoveSlot {
    uint16_t moveId        = 0;
    uint8_t  pp            = 0;
    uint8_t  ppUps         = 0;
    Gen      introducedGen = Gen::G1;
};

// Strict read-only Pokémon record (no IV/EV/PID/Shiny/Species mutation APIs exist in PkBank 3DS)
struct ReadOnlyMon {
    uint16_t     species           = 0;
    uint8_t      form              = 0;
    std::string  nickname;
    std::string  speciesName;
    uint8_t      level             = 1;
    uint32_t     exp               = 0;
    uint32_t     pid               = 0;
    uint16_t     tid               = 0;
    uint16_t     sid               = 0;
    std::string  otName;
    Gender       gender            = Gender::Male;
    uint8_t      natureId          = 0;
    uint16_t     abilityId         = 0;
    uint16_t     heldItem          = 0;
    uint8_t      ballId            = 4; // Poké Ball
    bool         isShiny           = false;
    bool         isEgg             = false;
    bool         isShadow          = false;
    bool         isFused           = false;
    PokerusState pokerus           = PokerusState::None;
    uint64_t     ribbonBits        = 0; // Rendered as icon-only ribbon badges on Top Screen
    std::array<MoveSlot, 4> moves{};
    Stats6       stats{};
    Stats6       potential{};           // Official IVs (shown on 2nd KEY_X press: Hidden Stats)
    Stats6       basePoints{};          // Official EVs (shown on 2nd KEY_X press: Hidden Stats)
    Gen          originGen         = Gen::G3;
    GameVer      originGame        = GameVer::E;
    std::string  originVersionName;     // Single version only: e.g. "Ruby", "Sapphire", "Emerald", "FireRed", "LeafGreen"
    Gen          currentGen        = Gen::G3;
    GameVer      bankStoredFromGame = GameVer::E;
    Gen          bankStoredFromGen  = Gen::G3;
    SaveProgressionFlags bankStoredFromProg{};
    OriginMark   originMark        = OriginMark::None;
    uint16_t     metLocationId     = 0;
    std::string  metLocationName;
    uint8_t      metLevel          = 0; // 0 in Gen 3 indicates Hatched Egg (Lv. 5)
    std::string  metDate;               // YYYY-MM-DD for Gen 4-7 when present
    std::vector<uint8_t> rawBytes;      // Preserved intact for exact binary round-trip
};

struct BoxGrid {
    std::string name;
    std::array<bool, 30> occupied{};
    std::array<ReadOnlyMon, 30> slots{};
};

struct SaveSession {
    std::string path;
    std::string trainerName;
    uint16_t    trainerId     = 0;
    uint8_t     badges        = 0;
    uint16_t    pokedexCaught = 0;
    Gen         generation    = Gen::G3;
    GameVer     gameVersion   = GameVer::E;
    std::string versionName;            // Single specific version name
    SaveProgressionFlags progression{}; // Authentic story/unlock gates parsed from binary save
    std::array<bool, 6> partyOccupied{};
    std::array<ReadOnlyMon, 6> party{};
    std::vector<BoxGrid> boxes;
    std::vector<uint8_t> rawSaveBuffer;
};

} // namespace PkBank
