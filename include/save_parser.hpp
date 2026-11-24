#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "types.hpp"

namespace PkBank::Parser {

// Parses Gen 1-7 saves (32KB VC, 128KB GBA + .AGBSAVE, 512KB NDS + .dsv, 3DS main)
// including authentic binary progression flags (Pokedex, National Dex, FRLG Celio quest, Pal Park, etc.)
bool loadSaveFile(const std::string& path, uint32_t payloadOffset, SaveSession& outSession);

// Writes modified save buffer back with atomic pre-write backup in sdmc:/3ds/PkBank3DS/backups/
bool commitSaveWithBackup(const SaveSession& session);

// Local Bank storage (Bank 1 .. Bank 16) at sdmc:/3ds/PkBank3DS/bank.bnk
bool loadLocalBank(const std::string& bankPath, std::vector<BoxGrid>& outBankBoxes);
bool saveLocalBank(const std::string& bankPath, const std::vector<BoxGrid>& bankBoxes);

// Sort all boxes globally by Regional Dex No., National Dex No., Name (A-Z), or Name (Z-A)
void sortAllBoxes(std::vector<BoxGrid>& boxes, SortMode mode);

// Gen 3-7 Pokerus status byte decoder
inline PokerusState decodePokerus(uint8_t pkrsByte) {
    if (pkrsByte == 0) return PokerusState::None;
    return (pkrsByte & 0x0F) > 0 ? PokerusState::Infected : PokerusState::Cured;
}

// Single-version origin resolver (never grouped strings like "Ruby/Sapphire")
const char* mapOriginVersionByte(uint8_t originByte, Gen fallbackGen);

} // namespace PkBank::Parser
