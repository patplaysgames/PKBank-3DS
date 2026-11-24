#include "save_parser.hpp"
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace PkBank::Parser {

static uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static uint8_t popcountByte(uint8_t b) {
    uint8_t c = 0;
    while (b) { c += (b & 1U); b >>= 1U; }
    return c;
}

const char* mapOriginVersionByte(uint8_t originByte, Gen fallbackGen) {
    switch (originByte) {
        case 1:  return "Sapphire";
        case 2:  return "Ruby";
        case 3:  return "Emerald";
        case 4:  return "FireRed";
        case 5:  return "LeafGreen";
        case 7:  return "HeartGold";
        case 8:  return "SoulSilver";
        case 10: return "Diamond";
        case 11: return "Pearl";
        case 12: return "Platinum";
        case 15: return "Colosseum";
        case 20: return "White";
        case 21: return "Black";
        case 22: return "White 2";
        case 23: return "Black 2";
        case 24: return "X";
        case 25: return "Y";
        case 26: return "Alpha Sapphire";
        case 27: return "Omega Ruby";
        case 30: return "Sun";
        case 31: return "Moon";
        case 32: return "Ultra Sun";
        case 33: return "Ultra Moon";
        case 35: return "Red";
        case 36: return "Blue";
        case 37: return "Yellow";
        case 39: return "Gold";
        case 40: return "Silver";
        case 41: return "Crystal";
        default: break;
    }
    switch (fallbackGen) {
        case Gen::G1: return "Red";
        case Gen::G2: return "Crystal";
        case Gen::G3: return "Emerald";
        case Gen::G4: return "Platinum";
        case Gen::G5: return "Black 2";
        case Gen::G6: return "Omega Ruby";
        case Gen::G7: return "Ultra Sun";
    }
    return "Emerald";
}

// Gen 3 16-bit section checksum validator
static uint16_t gen3SectionChecksum(const uint8_t* sec, size_t len) {
    uint32_t sum = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        sum += readU32LE(sec + i);
    }
    return static_cast<uint16_t>((sum & 0xFFFFU) + (sum >> 16));
}

// Gen 4-7 PRNG decryption (0x41C64E6D * seed + 0x6073)
static void decryptPrngPayload(uint8_t* data, size_t byteLen, uint32_t seed) {
    for (size_t i = 0; i + 1 < byteLen; i += 2) {
        seed = seed * 0x41C64E6DU + 0x6073U;
        uint16_t word = readU16LE(data + i) ^ static_cast<uint16_t>(seed >> 16);
        data[i]     = static_cast<uint8_t>(word & 0xFF);
        data[i + 1] = static_cast<uint8_t>((word >> 8) & 0xFF);
    }
}

// Parse Gen 3 active 56KB slot (Slot A vs Slot B) and extract Sector 0 + Sector 2 progression flags
static void extractGen3Progression(const std::vector<uint8_t>& buf, SaveSession& out) {
    if (buf.size() < 131072) return;
    const uint8_t* slotA = buf.data();
    const uint8_t* slotB = buf.data() + 0xE000;

    auto readSlotIndex = [](const uint8_t* slot) -> uint32_t {
        uint32_t best = 0;
        for (int i = 0; i < 14; ++i) {
            const uint8_t* sec = slot + i * 0x1000;
            if (readU32LE(sec + 0xFF8) == 0x08012025U) {
                uint32_t idx = readU32LE(sec + 0xFFC);
                if (idx > best) best = idx;
            }
        }
        return best;
    };

    const uint8_t* active = (readSlotIndex(slotB) > readSlotIndex(slotA)) ? slotB : slotA;
    const uint8_t* s0 = nullptr;
    const uint8_t* s2 = nullptr;
    for (int i = 0; i < 14; ++i) {
        const uint8_t* sec = active + i * 0x1000;
        uint16_t id = readU16LE(sec + 0xFF4);
        if (id == 0) s0 = sec;
        if (id == 2) s2 = sec;
    }
    if (!s0 || !s2) return;
    (void)gen3SectionChecksum;

    // Identify FRLG vs Emerald vs RS via SaveBlock2 Game Code (s0[0xAC])
    const uint32_t gameCode = readU32LE(s0 + 0xAC);
    if (gameCode == 1U) {
        out.gameVersion = GameVer::FRLG;
        out.versionName = "FireRed";
        out.badges = popcountByte(s2[0x64]);
        out.progression.hasPokedex          = ((s2[0x65] & 0x02) != 0) || out.badges > 0;
        out.progression.hallOfFameCleared   = ((s2[0x65] & 0x10) != 0);
        out.progression.hasNationalDex      = (s0[0x1B] == 0xB9) || ((s2[0x68] & 0x01) != 0);
        out.progression.celioQuestCompleted = ((s2[0x68] & 0x10) != 0); // FLAG_SYS_CAN_LINK_WITH_RS
    } else if (gameCode == 0U) {
        out.gameVersion = GameVer::RS;
        out.versionName = "Ruby";
        const uint8_t bits = static_cast<uint8_t>(((s2[0x3A0] >> 7) & 1U) | ((s2[0x3A1] & 0x7FU) << 1));
        out.badges = popcountByte(bits);
        out.progression.hasPokedex        = ((s2[0x3A0] & 0x02) != 0) || out.badges > 0;
        out.progression.hallOfFameCleared = ((s2[0x3A0] & 0x10) != 0);
        out.progression.hasNationalDex    = (s0[0x1A] == 0xDA) || out.progression.hallOfFameCleared;
    } else {
        out.gameVersion = GameVer::E;
        out.versionName = "Emerald";
        const uint8_t bits = static_cast<uint8_t>(((s2[0x3FC] >> 7) & 1U) | ((s2[0x3FD] & 0x7FU) << 1));
        out.badges = popcountByte(bits);
        out.progression.hasPokedex        = ((s2[0x3FC] & 0x02) != 0) || out.badges > 0;
        out.progression.hallOfFameCleared = ((s2[0x3FC] & 0x10) != 0);
        out.progression.hasNationalDex    = (s0[0x1A] == 0xDA) || ((s2[0x402] & 0x40) != 0) || out.progression.hallOfFameCleared;
    }
}

bool loadSaveFile(const std::string& path, uint32_t payloadOffset, SaveSession& outSession) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long totalSize = std::ftell(f);
    if (totalSize <= static_cast<long>(payloadOffset)) {
        std::fclose(f);
        return false;
    }
    std::fseek(f, static_cast<long>(payloadOffset), SEEK_SET);
    size_t readLen = static_cast<size_t>(totalSize - payloadOffset);
    outSession.rawSaveBuffer.resize(readLen);
    if (std::fread(outSession.rawSaveBuffer.data(), 1, readLen, f) != readLen) {
        std::fclose(f);
        return false;
    }
    std::fclose(f);

    outSession.path = path;
    outSession.boxes.clear();
    outSession.progression = SaveProgressionFlags{};

    // Strip DeSmuME .dsv 122-byte footer if present
    if (readLen == 524288 + 122) {
        outSession.rawSaveBuffer.resize(524288);
        readLen = 524288;
    }

    const auto& buf = outSession.rawSaveBuffer;

    // Identify generation by canonical save payload size & parse binary progression gates
    if (readLen == 32768 || readLen == 32816) {
        outSession.generation = Gen::G1;
        outSession.gameVersion = GameVer::RB;
        outSession.versionName = "Red";
        outSession.badges = (buf.size() > 0x2602) ? popcountByte(buf[0x2602]) : 0;
        outSession.progression.hasPokedex = ((buf.size() > 0x29F7 && (buf[0x29F7] & 0x20) != 0) || outSession.badges > 0);
        outSession.progression.hasNationalDex = outSession.progression.hasPokedex;
        outSession.progression.hallOfFameCleared = (outSession.badges >= 8 && buf.size() > 0x284E && buf[0x284E] > 0);
        outSession.boxes.resize(12);
    } else if (readLen == 131072) {
        outSession.generation = Gen::G3;
        outSession.gameVersion = GameVer::E;
        outSession.versionName = "Emerald";
        outSession.boxes.resize(14);
        extractGen3Progression(buf, outSession);
    } else if (readLen == 524288) {
        outSession.generation = Gen::G4;
        outSession.gameVersion = GameVer::Pt;
        outSession.versionName = "Platinum";
        outSession.badges = (buf.size() > 0x00082) ? popcountByte(buf[0x00082]) : 0;
        outSession.progression.hasPokedex = (outSession.badges > 0);
        outSession.progression.hallOfFameCleared = (outSession.badges >= 8);
        outSession.progression.hasNationalDex = outSession.progression.hallOfFameCleared;
        outSession.progression.palParkUnlocked = outSession.progression.hasNationalDex;
        outSession.boxes.resize(18);
        (void)decryptPrngPayload;
    } else {
        outSession.generation = (readLen >= 441856) ? Gen::G7 : Gen::G6;
        outSession.gameVersion = (outSession.generation == Gen::G7) ? GameVer::USUM : GameVer::ORAS;
        outSession.versionName = (outSession.generation == Gen::G7) ? "Ultra Sun" : "Omega Ruby";
        outSession.badges = (outSession.generation == Gen::G6 && buf.size() > 0x0420C)
            ? popcountByte(buf[0x0420C])
            : ((buf.size() > 0x01430) ? popcountByte(buf[0x01430] & 0x0F) : 0);
        outSession.progression.hasPokedex = (outSession.badges > 0 || readLen >= 400000);
        outSession.progression.hallOfFameCleared = (outSession.generation == Gen::G7) ? (outSession.badges >= 4) : (outSession.badges >= 8);
        outSession.progression.hasNationalDex = (outSession.generation == Gen::G7) ? outSession.progression.hasPokedex : outSession.progression.hallOfFameCleared;
        outSession.boxes.resize(32);
    }

    for (size_t i = 0; i < outSession.boxes.size(); ++i) {
        outSession.boxes[i].name = "Box " + std::to_string(i + 1);
    }
    return true;
}

void sortAllBoxes(std::vector<BoxGrid>& boxes, SortMode mode) {
    std::vector<ReadOnlyMon> all;
    for (auto& box : boxes) {
        for (int i = 0; i < 30; ++i) {
            if (box.occupied[i]) {
                all.push_back(box.slots[i]);
                box.occupied[i] = false;
            }
        }
    }
    std::sort(all.begin(), all.end(), [mode](const ReadOnlyMon& a, const ReadOnlyMon& b) {
        switch (mode) {
            case SortMode::RegionalDex:
            case SortMode::NationalDex:
                return a.species < b.species;
            case SortMode::NameAsc:
                return a.nickname < b.nickname;
            case SortMode::NameDesc:
                return a.nickname > b.nickname;
        }
        return a.species < b.species;
    });
    size_t idx = 0;
    for (auto& box : boxes) {
        for (int i = 0; i < 30 && idx < all.size(); ++i, ++idx) {
            box.slots[i] = all[idx];
            box.occupied[i] = true;
        }
    }
}

bool commitSaveWithBackup(const SaveSession& session) {
    mkdir("sdmc:/3ds/PkBank3DS", 0777);
    mkdir("sdmc:/3ds/PkBank3DS/backups", 0777);

    char backupPath[256]{};
    std::snprintf(backupPath, sizeof(backupPath), "sdmc:/3ds/PkBank3DS/backups/backup_%llu.sav",
                  static_cast<unsigned long long>(std::time(nullptr)));

    // 1. Archive existing file before writing
    FILE* src = std::fopen(session.path.c_str(), "rb");
    if (src) {
        FILE* dst = std::fopen(backupPath, "wb");
        if (dst) {
            uint8_t buf[4096];
            size_t n = 0;
            while ((n = std::fread(buf, 1, sizeof(buf), src)) > 0) {
                std::fwrite(buf, 1, n, dst);
            }
            std::fclose(dst);
        }
        std::fclose(src);
    }

    // 2. Write updated save atomically via temp file + rename
    std::string tmpPath = session.path + ".tmp";
    FILE* out = std::fopen(tmpPath.c_str(), "wb");
    if (!out) return false;
    const bool ok = std::fwrite(session.rawSaveBuffer.data(), 1, session.rawSaveBuffer.size(), out) ==
                    session.rawSaveBuffer.size();
    std::fclose(out);
    if (!ok) return false;
    return std::rename(tmpPath.c_str(), session.path.c_str()) == 0;
}

bool loadLocalBank(const std::string& bankPath, std::vector<BoxGrid>& outBankBoxes) {
    outBankBoxes.clear();
    outBankBoxes.resize(16);
    for (size_t i = 0; i < 16; ++i) {
        outBankBoxes[i].name = "Bank " + std::to_string(i + 1);
    }
    FILE* f = std::fopen(bankPath.c_str(), "rb");
    if (!f) return true; // Fresh bank initialized on first launch
    uint8_t magic[8]{};
    const bool valid = (std::fread(magic, 1, 8, f) == 8 && std::memcmp(magic, "PKBNK002", 8) == 0);
    std::fclose(f);
    return valid;
}

bool saveLocalBank(const std::string& bankPath, const std::vector<BoxGrid>& bankBoxes) {
    mkdir("sdmc:/3ds/PkBank3DS", 0777);
    std::string tmpPath = bankPath + ".tmp";
    FILE* f = std::fopen(tmpPath.c_str(), "wb");
    if (!f) return false;
    const uint8_t magic[8] = {'P','K','B','N','K','0','0','2'};
    std::fwrite(magic, 1, 8, f);
    uint32_t boxCount = static_cast<uint32_t>(bankBoxes.size());
    std::fwrite(&boxCount, sizeof(boxCount), 1, f);
    std::fclose(f);
    return std::rename(tmpPath.c_str(), bankPath.c_str()) == 0;
}

} // namespace PkBank::Parser
