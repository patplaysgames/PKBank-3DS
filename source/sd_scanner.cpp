#include "sd_scanner.hpp"
#include <3ds.h>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_set>

namespace PkBank::SD {

static std::string stripExtension(const std::string& name) {
    const size_t dot = name.find_last_of('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

static bool hasSaveExtension(const std::string& name) {
    if (name == "main" || name == "sav.dat" || name == "agbsave.bin") return true;
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::string ext = name.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == ".sav" || ext == ".srm" || ext == ".dsv" || ext == ".dat" || ext == ".bin";
}

static bool shouldSkipRootSubdir(const char* name) {
    return std::strcmp(name, "DCIM") == 0 ||
           std::strcmp(name, "cias") == 0 ||
           std::strcmp(name, "backups") == 0 ||
           std::strcmp(name, "extdata") == 0 ||
           std::strcmp(name, "Private") == 0 ||
           std::strcmp(name, "luma") == 0 ||
           std::strcmp(name, ".Trash") == 0 ||
           std::strcmp(name, "System Volume Information") == 0;
}

static void scanDirectoryRecursive(const std::string& dirPath, int depth, std::vector<DetectedSave>& out, std::unordered_set<std::string>& seen) {
    if (depth > 7) return;
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return;

    const bool endsWithSlash = !dirPath.empty() && dirPath.back() == '/';
    struct dirent* ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
        std::string full = endsWithSlash ? (dirPath + ent->d_name) : (dirPath + "/" + ent->d_name);
        struct stat st{};
        if (stat(full.c_str(), &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            if (shouldSkipRootSubdir(ent->d_name)) continue;
            scanDirectoryRecursive(full, depth + 1, out, seen);
        } else if (S_ISREG(st.st_mode) && hasSaveExtension(ent->d_name)) {
            if (seen.count(full)) continue;
            seen.insert(full);

            DetectedSave item;
            item.path = full;
            item.displayName = stripExtension(ent->d_name);
            item.payloadSize = static_cast<uint32_t>(st.st_size);

            if (std::strcmp(ent->d_name, "agbsave.bin") == 0) {
                FILE* f = std::fopen(full.c_str(), "rb");
                if (f) {
                    uint8_t hdr[8]{};
                    if (std::fread(hdr, 1, 8, f) == 8 && std::memcmp(hdr, ".AGBSAVE", 8) == 0) {
                        item.payloadOffset = 0x200;
                        item.payloadSize = 131072;
                    }
                    std::fclose(f);
                }
                item.kind = SaveSourceKind::HomebrewGbaVC;
            } else if (full.find("TWiLightMenu") != std::string::npos || full.find("/nds/") != std::string::npos) {
                item.kind = SaveSourceKind::TwilightMenuNDS;
            } else {
                item.kind = SaveSourceKind::NdsRomsFolder;
            }
            out.push_back(std::move(item));
        }
    }
    closedir(dir);
}

std::vector<DetectedSave> autoScanSdCard() {
    std::vector<DetectedSave> results;
    std::unordered_set<std::string> seen;

    // 1. Probe installed Official 3DS & Virtual Console titles via AM service
    u32 titleCount = 0;
    if (R_SUCCEEDED(AM_GetTitleCount(MEDIATYPE_SD, &titleCount)) && titleCount > 0) {
        std::vector<u64> tids(titleCount);
        u32 readCount = 0;
        if (R_SUCCEEDED(AM_GetTitleList(&readCount, MEDIATYPE_SD, titleCount, tids.data()))) {
            for (u32 i = 0; i < readCount; ++i) {
                for (const auto& known : OFFICIAL_TITLES) {
                    if (tids[i] == known.tid) {
                        DetectedSave item;
                        item.titleId = known.tid;
                        item.displayName = known.name;
                        item.path = "title://" + std::to_string(known.tid);
                        item.kind = known.kind;
                        if (seen.insert(item.path).second) {
                            results.push_back(std::move(item));
                        }
                    }
                }
            }
        }
    }

    // 2. Automatically scan the entire 3DS root SD card folder recursively (sdmc:/)
    scanDirectoryRecursive(SD_ROOT_DIR, 0, results, seen);

    return results;
}

} // namespace PkBank::SD
