#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace PkBank::SD {

enum class SaveSourceKind {
    Official3DS,        // XY / ORAS / SM / USUM via AM + FSUSER_OpenArchive or Checkpoint
    OfficialVC_RBY_GSC, // 3DS VC sav.dat (RBY: 0x171000-1200, GSC: 0x172600-2800)
    HomebrewGbaVC,      // AGB_FIRM agbsave.bin (.AGBSAVE 0x200 hdr), open_agb_firm, NSUI
    TwilightMenuNDS,    // sdmc:/roms/nds/saves/*.sav & sdmc:/_nds/TWiLightMenu/
    NdsRomsFolder       // sdmc:/roms/*.sav or *.dsv
};

struct DetectedSave {
    std::string    path;
    std::string    displayName;   // File extensions stripped in UI
    std::string    trainerName;
    uint16_t       trainerId     = 0;
    uint64_t       titleId       = 0;
    SaveSourceKind kind          = SaveSourceKind::Official3DS;
    uint32_t       payloadOffset = 0; // 0x200 for .AGBSAVE header, 0 for raw saves
    uint32_t       payloadSize   = 0;
};

struct TitleEntry { uint64_t tid; const char* name; SaveSourceKind kind; };

inline constexpr TitleEntry OFFICIAL_TITLES[] = {
    {0x0004000000055D00ULL, "Pokemon X",              SaveSourceKind::Official3DS},
    {0x0004000000055E00ULL, "Pokemon Y",              SaveSourceKind::Official3DS},
    {0x000400000011C400ULL, "Pokemon Omega Ruby",     SaveSourceKind::Official3DS},
    {0x000400000011C500ULL, "Pokemon Alpha Sapphire", SaveSourceKind::Official3DS},
    {0x0004000000164800ULL, "Pokemon Sun",            SaveSourceKind::Official3DS},
    {0x0004000000175E00ULL, "Pokemon Moon",           SaveSourceKind::Official3DS},
    {0x00040000001B5000ULL, "Pokemon Ultra Sun",      SaveSourceKind::Official3DS},
    {0x00040000001B5100ULL, "Pokemon Ultra Moon",     SaveSourceKind::Official3DS},
    {0x0004000000171000ULL, "Pokemon Red",            SaveSourceKind::OfficialVC_RBY_GSC},
    {0x0004000000171100ULL, "Pokemon Blue",           SaveSourceKind::OfficialVC_RBY_GSC},
    {0x0004000000171200ULL, "Pokemon Yellow",         SaveSourceKind::OfficialVC_RBY_GSC},
    {0x0004000000172600ULL, "Pokemon Gold",           SaveSourceKind::OfficialVC_RBY_GSC},
    {0x0004000000172700ULL, "Pokemon Silver",         SaveSourceKind::OfficialVC_RBY_GSC},
    {0x0004000000172800ULL, "Pokemon Crystal",        SaveSourceKind::OfficialVC_RBY_GSC},
};

inline constexpr const char* SD_ROOT_DIR = "sdmc:/";

inline bool isAgbSaveHeader(const uint8_t* buf, size_t len) {
    static constexpr uint8_t MAGIC[8] = {'.','A','G','B','S','A','V','E'};
    if (len < 0x200 + 131072) return false;
    for (int i = 0; i < 8; ++i) if (buf[i] != MAGIC[i]) return false;
    return true;
}

std::vector<DetectedSave> autoScanSdCard();

} // namespace PkBank::SD
