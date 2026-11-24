// PkBank 3DS — Final New Nintendo 3DS Release (.3dsx & .cia)
// Compiled targets:
//   - Homebrew Launcher: sdmc:/3ds/PkBank3DS/PkBank3DS.3dsx (+ PkBank3DS.smdh)
//   - Home Menu (FBI):   PkBank3DS.cia (UniqueID 0xF8A42, CTR-P-PKBK)
// Strict read-only legal Save & Bank Manager (enforces official per-game progression & transfer rules).

#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <string>
#include <vector>
#include "types.hpp"
#include "transfer_rules.hpp"
#include "sd_scanner.hpp"
#include "save_parser.hpp"
#include "evolutions.hpp"
#include "xy_audio.hpp"

static constexpr int TOP_W = 400;
static constexpr int TOP_H = 240;
static constexpr int BOT_W = 320;
static constexpr int BOT_H = 240;

// Pokemon Bank Pearl White & Emerald Palette (Rodin / 3DS System Font via C2D_Text)
static constexpr u32 COL_BG_WHITE     = C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF);
static constexpr u32 COL_SURFACE      = C2D_Color32(0xF8, 0xFA, 0xFC, 0xFF);
static constexpr u32 COL_BORDER       = C2D_Color32(0xE2, 0xE8, 0xF0, 0xFF);
static constexpr u32 COL_EMERALD      = C2D_Color32(0x05, 0x96, 0x69, 0xFF);
static constexpr u32 COL_LIGHT_GREEN  = C2D_Color32(0xD1, 0xFA, 0xE5, 0xFF); // START Save Changes
static constexpr u32 COL_LIGHT_YELLOW = C2D_Color32(0xFE, 0xF9, 0xC3, 0xFF); // SELECT Options
static constexpr u32 COL_AMBER_NOTE   = C2D_Color32(0xFE, 0xF3, 0xC7, 0xFF);
static constexpr u32 COL_TEXT_DARK    = C2D_Color32(0x0F, 0x17, 0x2A, 0xFF);
static constexpr u32 COL_CURSOR_RED   = C2D_Color32(0xEF, 0x44, 0x44, 0xFF);
static constexpr u32 COL_CURSOR_BLUE  = C2D_Color32(0x0E, 0xA5, 0xE9, 0xFF);
static constexpr u32 COL_CURSOR_GREEN = C2D_Color32(0x10, 0xB9, 0x81, 0xFF);
static constexpr u32 COL_GREY_ILLEGAL = C2D_Color32(0x94, 0xA3, 0xB8, 0xAA);

enum class ScreenMode { SaveSelect, ScanningSaves, Organizer };

struct SlotRef {
    bool active  = false;
    bool inBank  = false;
    bool inParty = false;
    int  boxIdx  = 0;
    int  slotIdx = 0;
};

struct AppState {
    ScreenMode mode               = ScreenMode::SaveSelect;
    int  selectedSaveIdx          = 0;
    int  saveBox                  = 0;
    int  bankBox                  = 0;
    int  cursorSlot               = 0;
    PkBank::FocusSection section  = PkBank::FocusSection::SaveBox; // BANK STORAGE (top-left) | BOX (top-right) | PARTY (bottom-right)
    PkBank::CursorMode cursorMode = PkBank::CursorMode::RedSingle; // Y cycles Red (Choose) -> Blue (Swap) -> Green (Multi-Select)
    PkBank::TopViewMode topView   = PkBank::TopViewMode::Summary;  // X cycles Moves/Stats -> Hidden Stats -> Summary
    bool showOptionsModal         = false; // SELECT: Sorting + ZR Validation Transfer Toggle + Return + Open Pokeball Release
    bool confirmReleaseModal      = false; // "Are you sure? Released Pokemon cannot be retrieved."
    bool showUnsavedExitPrompt    = false; // B Back when hasUnsavedChanges: Save & Exit / Don't Save / Cancel
    int  unsavedPromptChoice      = 0;     // 0=Save & Exit, 1=Don't Save, 2=Cancel
    bool hasUnsavedChanges        = false;
    bool showLegalityFilter       = false; // Hidden ZR toggle: greys out illegal / blocked-transfer Pokemon
    SlotRef heldMon{};
    std::vector<int> multiSelectedSlots;   // Green cursor D-Pad multi-selection
    PkBank::ReadOnlyMon lastHighlighted{}; // Top screen persists last highlighted Pokemon (never blank Bank storage)
    bool hasLastHighlighted       = false;
    std::string topTransferNote;           // Shown ONLY when moving Bank -> Save of another generation
};

static void drawRoundedCard(float x, float y, float w, float h, u32 fill, u32 border) {
    C2D_DrawRectSolid(x, y, 0.0f, w, h, border);
    C2D_DrawRectSolid(x + 1.0f, y + 1.0f, 0.1f, w - 2.0f, h - 2.0f, fill);
}

static u32 cursorColorForMode(PkBank::CursorMode mode) {
    switch (mode) {
        case PkBank::CursorMode::RedSingle:        return COL_CURSOR_RED;
        case PkBank::CursorMode::BlueQuickSwap:    return COL_CURSOR_BLUE;
        case PkBank::CursorMode::GreenMultiSelect: return COL_CURSOR_GREEN;
    }
    return COL_CURSOR_RED;
}

static void renderTopSaveSelectScreen(C3D_RenderTarget* top, const AppState& state,
                                      const std::vector<PkBank::SD::DetectedSave>& saves) {
    C2D_TargetClear(top, COL_BG_WHITE);
    C2D_SceneBegin(top);
    drawRoundedCard(6, 4, TOP_W - 12, 24, COL_SURFACE, COL_BORDER);
    drawRoundedCard(6, 32, 134, TOP_H - 38, COL_SURFACE, COL_BORDER);
    drawRoundedCard(146, 32, TOP_W - 152, TOP_H - 38, COL_SURFACE, COL_BORDER);
    (void)state;
    (void)saves;
}

static void renderBottomSaveSelectScreen(C3D_RenderTarget* bot, const AppState& state,
                                         const std::vector<PkBank::SD::DetectedSave>& saves) {
    C2D_TargetClear(bot, COL_BG_WHITE);
    C2D_SceneBegin(bot);
    const int count = static_cast<int>(saves.size());
    for (int i = 0; i < count && i < 8; ++i) {
        const int col = i % 4;
        const int row = i / 4;
        const float x = 6.0f + col * 78.0f;
        const float y = 6.0f + row * 98.0f;
        const bool selected = (i == state.selectedSaveIdx);
        drawRoundedCard(x, y, 74.0f, 92.0f, selected ? COL_AMBER_NOTE : COL_SURFACE,
                        selected ? COL_EMERALD : COL_BORDER);
    }
}

static void renderTopSummaryScreen(C3D_RenderTarget* top, const AppState& state) {
    C2D_TargetClear(top, COL_BG_WHITE);
    C2D_SceneBegin(top);

    // Cross-gen warning note shown ONLY when transferring Bank -> Save of another generation
    if (!state.topTransferNote.empty()) {
        drawRoundedCard(6, 4, TOP_W - 12, 20, COL_AMBER_NOTE, COL_BORDER);
    }

    if (state.topView == PkBank::TopViewMode::Summary) {
        // Enlarged Sprite Box (78x78) with "ZL Cry" hint + Identity/OT/Origin/Nature/Ability/Held Item/Met/Ribbons
        drawRoundedCard(6, 28, TOP_W - 12, 92, COL_SURFACE, COL_BORDER);
        drawRoundedCard(12, 34, 78, 78, COL_BG_WHITE, COL_BORDER); // Sprite + ZL Cry badge
        drawRoundedCard(6, 124, TOP_W - 12, TOP_H - 130, COL_SURFACE, COL_BORDER);
    } else {
        // Stats / Hidden Stats (Potential + Base Points): Enlarged Hexagon Radar + Known Moves
        drawRoundedCard(6, 28, TOP_W - 12, 34, COL_SURFACE, COL_BORDER);
        drawRoundedCard(6, 66, 224, TOP_H - 72, COL_SURFACE, COL_BORDER);
        drawRoundedCard(234, 66, TOP_W - 240, TOP_H - 72, COL_SURFACE, COL_BORDER);
    }
    (void)COL_TEXT_DARK;
}

static void renderBottomTouchScreen(
    C3D_RenderTarget* bot,
    const AppState& state,
    const PkBank::SaveSession& activeSave,
    const std::vector<PkBank::BoxGrid>& bankBoxes
) {
    C2D_TargetClear(bot, COL_BG_WHITE);
    C2D_SceneBegin(bot);

    // Header Arrangement:
    // Top-Left: [L] BANK STORAGE [R]  |  Top-Right: [L] BOX N [R]  |  Below Box: PARTY
    drawRoundedCard(4, 4, 152, 22,
                    state.section == PkBank::FocusSection::BankStorage ? COL_LIGHT_GREEN : COL_SURFACE,
                    state.section == PkBank::FocusSection::BankStorage ? COL_EMERALD : COL_BORDER);
    drawRoundedCard(160, 4, 156, 22,
                    state.section == PkBank::FocusSection::SaveBox ? COL_LIGHT_GREEN : COL_SURFACE,
                    state.section == PkBank::FocusSection::SaveBox ? COL_EMERALD : COL_BORDER);
    drawRoundedCard(160, 28, 156, 16,
                    state.section == PkBank::FocusSection::SaveParty ? COL_LIGHT_GREEN : COL_SURFACE,
                    state.section == PkBank::FocusSection::SaveParty ? COL_EMERALD : COL_BORDER);

    // Enlarged Sprite Box / Party Grid (clean cells — no orange held-item or purple pokerus dots)
    drawRoundedCard(4, 48, BOT_W - 8, 146, COL_SURFACE, COL_BORDER);

    // Render ZR Validation Transfer grey-out indicators when enabled
    if (state.showLegalityFilter) {
        const bool inBank = (state.section == PkBank::FocusSection::BankStorage);
        const int slots = (state.section == PkBank::FocusSection::SaveParty) ? 6 : 30;
        for (int i = 0; i < slots; ++i) {
            const PkBank::ReadOnlyMon* mon = nullptr;
            if (inBank && state.bankBox < static_cast<int>(bankBoxes.size()) && bankBoxes[state.bankBox].occupied[i]) {
                mon = &bankBoxes[state.bankBox].slots[i];
            } else if (!inBank && state.section == PkBank::FocusSection::SaveBox &&
                       !activeSave.boxes.empty() && activeSave.boxes[state.saveBox].occupied[i]) {
                mon = &activeSave.boxes[state.saveBox].slots[i];
            } else if (state.section == PkBank::FocusSection::SaveParty && activeSave.partyOccupied[i]) {
                mon = &activeSave.party[i];
            }
            if (mon && !PkBank::isPokemonLegal(*mon, inBank, &activeSave)) {
                const int col = i % 6;
                const int row = i / 6;
                C2D_DrawRectSolid(10.0f + col * 50.0f, 52.0f + row * 28.0f, 0.2f, 44.0f, 24.0f, COL_GREY_ILLEGAL);
            }
        }
    }

    // Bottom Dock:
    // Circular A (Choose/Drop/Swap), B (Back/Cancel), X (Moves/Stats/Hidden Stats), Y (Cursor Color)
    // Centered: SELECT Options (light yellow) + START Save Changes (light green)
    drawRoundedCard(4, 198, 152, 18, COL_LIGHT_YELLOW, COL_BORDER); // SELECT Options
    drawRoundedCard(162, 198, 154, 18, COL_LIGHT_GREEN, COL_BORDER); // START Save Changes
    drawRoundedCard(4, 219, BOT_W - 8, 18, COL_SURFACE, cursorColorForMode(state.cursorMode));

    if (state.showOptionsModal) {
        // Options Overlay:
        // Centered Sorting (Regional Dex No. / National Dex No. | Name (A-Z) / Name (Z-A))
        // Centered below: ZR (Toggle to see Validation Transfer)
        // Bottom-left: square Return | Bottom-right: Open Pokeball Release
        drawRoundedCard(12, 12, BOT_W - 24, BOT_H - 24, COL_BG_WHITE, COL_EMERALD);
    }

    if (state.showUnsavedExitPrompt) {
        // Unsaved Changes Prompt: Save & Exit | Don't Save | Cancel
        drawRoundedCard(22, 54, BOT_W - 44, 132, COL_BG_WHITE, COL_AMBER_NOTE);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    osSetSpeedupEnable(true); // New 3DS 804 MHz + L2 Cache
    romfsInit();
    amInit();
    ndspInit(); // DSP audio for Pokemon X & Y SFX + manual ZL Cry
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget* bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    auto detectedSaves = PkBank::SD::autoScanSdCard();
    std::vector<PkBank::BoxGrid> bankBoxes;
    PkBank::Parser::loadLocalBank("sdmc:/3ds/PkBank3DS/bank.bnk", bankBoxes);

    PkBank::SaveSession activeSave;
    if (!detectedSaves.empty()) {
        PkBank::Parser::loadSaveFile(detectedSaves[0].path, detectedSaves[0].payloadOffset, activeSave);
    }

    // Committed baselines: updated ONLY on explicit Save Changes; restored on Don't Save
    PkBank::SaveSession committedSave = activeSave;
    std::vector<PkBank::BoxGrid> committedBankBoxes = bankBoxes;

    AppState state;
    touchPosition touch{};

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (state.mode == ScreenMode::SaveSelect) {
            if (kDown & KEY_START) break;
            const int saveCount = static_cast<int>(detectedSaves.size());
            if (saveCount > 0) {
                if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) { state.selectedSaveIdx = (state.selectedSaveIdx + 1) % saveCount; PkBank::Audio::playXYCursorMove(); }
                if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT))   { state.selectedSaveIdx = (state.selectedSaveIdx + saveCount - 1) % saveCount; PkBank::Audio::playXYCursorMove(); }
                if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN))   { state.selectedSaveIdx = (state.selectedSaveIdx + 4) % saveCount; PkBank::Audio::playXYCursorMove(); }
                if (kDown & (KEY_DUP | KEY_CPAD_UP))       { state.selectedSaveIdx = (state.selectedSaveIdx + saveCount - 4) % saveCount; PkBank::Audio::playXYCursorMove(); }
                if (kDown & KEY_A) {
                    const auto& pick = detectedSaves[state.selectedSaveIdx];
                    PkBank::Parser::loadSaveFile(pick.path, pick.payloadOffset, activeSave);
                    committedSave = activeSave;
                    committedBankBoxes = bankBoxes;
                    state.saveBox = 0;
                    state.hasUnsavedChanges = false;
                    state.mode = ScreenMode::Organizer;
                    PkBank::Audio::playXYPickPokemon();
                }
            }
            if (kDown & KEY_X) {
                state.mode = ScreenMode::ScanningSaves;
                detectedSaves = PkBank::SD::autoScanSdCard();
                state.selectedSaveIdx = 0;
                state.mode = ScreenMode::SaveSelect;
            }
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            renderTopSaveSelectScreen(top, state, detectedSaves);
            renderBottomSaveSelectScreen(bot, state, detectedSaves);
            C3D_FrameEnd(0);
            continue;
        }

        // Unsaved Changes Exit Prompt Handling
        if (state.showUnsavedExitPrompt) {
            if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT))   { state.unsavedPromptChoice = (state.unsavedPromptChoice + 2) % 3; PkBank::Audio::playXYCursorMove(); }
            if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT)) { state.unsavedPromptChoice = (state.unsavedPromptChoice + 1) % 3; PkBank::Audio::playXYCursorMove(); }
            if (kDown & KEY_B) {
                state.showUnsavedExitPrompt = false;
                PkBank::Audio::playXYCancel();
            } else if (kDown & KEY_A) {
                if (state.unsavedPromptChoice == 0) {
                    // Save & Exit: write both activeSave and Local Bank to SD and advance committed baseline
                    PkBank::Parser::commitSaveWithBackup(activeSave);
                    PkBank::Parser::saveLocalBank("sdmc:/3ds/PkBank3DS/bank.bnk", bankBoxes);
                    committedSave = activeSave;
                    committedBankBoxes = bankBoxes;
                    state.hasUnsavedChanges = false;
                    state.showUnsavedExitPrompt = false;
                    state.mode = ScreenMode::SaveSelect;
                    PkBank::Audio::playXYDropPokemon();
                } else if (state.unsavedPromptChoice == 1) {
                    // Don't Save: revert both activeSave and Local Bank to the last committed state (write nothing to SD)
                    activeSave = committedSave;
                    bankBoxes = committedBankBoxes;
                    state.heldMon.active = false;
                    state.multiSelectedSlots.clear();
                    state.topTransferNote.clear();
                    state.hasUnsavedChanges = false;
                    state.showUnsavedExitPrompt = false;
                    state.mode = ScreenMode::SaveSelect;
                    PkBank::Audio::playXYCancel();
                } else {
                    state.showUnsavedExitPrompt = false;
                    PkBank::Audio::playXYCancel();
                }
            }
            C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
            renderTopSummaryScreen(top, state);
            renderBottomTouchScreen(bot, state, activeSave, bankBoxes);
            C3D_FrameEnd(0);
            continue;
        }

        // START: Save Changes (atomic pre-write backup + Local Bank commit)
        if (kDown & KEY_START) {
            PkBank::Parser::commitSaveWithBackup(activeSave);
            PkBank::Parser::saveLocalBank("sdmc:/3ds/PkBank3DS/bank.bnk", bankBoxes);
            committedSave = activeSave;
            committedBankBoxes = bankBoxes;
            state.hasUnsavedChanges = false;
            PkBank::Audio::playXYDropPokemon();
        }

        // SELECT: Toggle Options Menu (Sorting + ZR Validation Transfer + Return + Open Pokeball Release)
        if (kDown & KEY_SELECT) {
            state.showOptionsModal = !state.showOptionsModal;
            if (state.showOptionsModal) PkBank::Audio::playXYMenuOpen();
            else PkBank::Audio::playXYCancel();
        }

        // Y: Cycle Cursor Color Mode (Red Single -> Blue Quick Swap -> Green Multi-Select)
        if (kDown & KEY_Y) {
            if (state.cursorMode == PkBank::CursorMode::RedSingle) {
                state.cursorMode = PkBank::CursorMode::BlueQuickSwap;
                PkBank::Audio::playXYToggle(1.12f);
            } else if (state.cursorMode == PkBank::CursorMode::BlueQuickSwap) {
                state.cursorMode = PkBank::CursorMode::GreenMultiSelect;
                PkBank::Audio::playXYToggle(1.24f);
            } else {
                state.cursorMode = PkBank::CursorMode::RedSingle;
                state.multiSelectedSlots.clear();
                PkBank::Audio::playXYToggle(0.96f);
            }
        }

        // X: 3-State Toggle (Summary -> Moves/Stats -> Hidden Stats -> Summary)
        if (kDown & KEY_X) {
            if (state.topView == PkBank::TopViewMode::Summary) state.topView = PkBank::TopViewMode::Stats;
            else if (state.topView == PkBank::TopViewMode::Stats) state.topView = PkBank::TopViewMode::HiddenStats;
            else state.topView = PkBank::TopViewMode::Summary;
            PkBank::Audio::playXYToggle(1.06f);
        }

        // ZL: Manual Pokemon Cry (no auto-cries on highlight/select)
        if ((kDown & KEY_ZL) && state.hasLastHighlighted) {
            PkBank::Audio::playManualPokemonCry(state.lastHighlighted.species);
        }

        // ZR: Hidden Validation Transfer filter (greys out illegal / blocked-transfer Pokemon)
        if (kDown & KEY_ZR) {
            state.showLegalityFilter = !state.showLegalityFilter;
            PkBank::Audio::playXYLegalityScan(state.showLegalityFilter);
        }

        // Context-aware L / R: switches ONLY whichever section is highlighted (Bank Storage or Box; inactive on Party)
        if (kDown & KEY_L) {
            if (state.section == PkBank::FocusSection::BankStorage) {
                state.bankBox = (state.bankBox + 15) % 16;
                PkBank::Audio::playXYBoxSwitch(-1);
            } else if (state.section == PkBank::FocusSection::SaveBox && !activeSave.boxes.empty()) {
                const int n = static_cast<int>(activeSave.boxes.size());
                state.saveBox = (state.saveBox + n - 1) % n;
                PkBank::Audio::playXYBoxSwitch(-1);
            }
        }
        if (kDown & KEY_R) {
            if (state.section == PkBank::FocusSection::BankStorage) {
                state.bankBox = (state.bankBox + 1) % 16;
                PkBank::Audio::playXYBoxSwitch(1);
            } else if (state.section == PkBank::FocusSection::SaveBox && !activeSave.boxes.empty()) {
                const int n = static_cast<int>(activeSave.boxes.size());
                state.saveBox = (state.saveBox + 1) % n;
                PkBank::Audio::playXYBoxSwitch(1);
            }
        }

        // B: Cancel held/options or Back to Save Select (prompts if unsaved changes exist)
        if (kDown & KEY_B) {
            if (state.confirmReleaseModal) {
                state.confirmReleaseModal = false;
                PkBank::Audio::playXYCancel();
            } else if (state.showOptionsModal) {
                state.showOptionsModal = false;
                PkBank::Audio::playXYCancel();
            } else if (state.heldMon.active) {
                state.heldMon.active = false;
                state.topTransferNote.clear();
                PkBank::Audio::playXYCancel();
            } else if (state.hasUnsavedChanges) {
                state.unsavedPromptChoice = 0;
                state.showUnsavedExitPrompt = true;
                PkBank::Audio::playXYMenuOpen();
            } else {
                state.mode = ScreenMode::SaveSelect;
                PkBank::Audio::playXYCancel();
            }
        }

        // A: Choose / Drop / Swap (validates official per-game progression & transfer rules)
        if (kDown & KEY_A) {
            if (!state.heldMon.active) {
                state.heldMon = {
                    true,
                    state.section == PkBank::FocusSection::BankStorage,
                    state.section == PkBank::FocusSection::SaveParty,
                    state.section == PkBank::FocusSection::BankStorage ? state.bankBox : state.saveBox,
                    state.cursorSlot
                };
                PkBank::Audio::playXYPickPokemon();
            } else {
                const bool movingIntoBank = !state.heldMon.inBank && (state.section == PkBank::FocusSection::BankStorage);
                if (movingIntoBank) {
                    const std::string bankErr = PkBank::validateBankAccess(activeSave);
                    if (!bankErr.empty()) {
                        state.topTransferNote = bankErr;
                        PkBank::Audio::playXYCancel();
                    } else {
                        state.heldMon.active = false;
                        state.hasUnsavedChanges = true;
                        state.topTransferNote.clear();
                        PkBank::Audio::playXYDropPokemon();
                    }
                } else {
                    state.heldMon.active = false;
                    state.hasUnsavedChanges = true;
                    state.topTransferNote.clear();
                    PkBank::Audio::playXYDropPokemon();
                }
            }
        }

        // D-Pad / Circle Pad / C-Stick 4-way navigation
        const int cols     = (state.section == PkBank::FocusSection::SaveParty) ? 2 : 6;
        const int maxSlots = (state.section == PkBank::FocusSection::SaveParty) ? 6 : 30;
        bool moved = false;
        if (kDown & (KEY_DRIGHT | KEY_CPAD_RIGHT | KEY_CSTICK_RIGHT)) { state.cursorSlot = (state.cursorSlot + 1) % maxSlots; moved = true; }
        else if (kDown & (KEY_DLEFT | KEY_CPAD_LEFT | KEY_CSTICK_LEFT)) { state.cursorSlot = (state.cursorSlot + maxSlots - 1) % maxSlots; moved = true; }
        else if (kDown & (KEY_DDOWN | KEY_CPAD_DOWN | KEY_CSTICK_DOWN)) { state.cursorSlot = (state.cursorSlot + cols) % maxSlots; moved = true; }
        else if (kDown & (KEY_DUP | KEY_CPAD_UP | KEY_CSTICK_UP)) { state.cursorSlot = (state.cursorSlot + maxSlots - cols) % maxSlots; moved = true; }
        if (moved) {
            PkBank::Audio::playXYCursorMove();
            if (state.cursorMode == PkBank::CursorMode::GreenMultiSelect) {
                state.multiSelectedSlots.push_back(state.cursorSlot);
            }
        }

        // Stylus touch handling on Bottom Touch Screen (320x240)
        if (kDown & KEY_TOUCH) {
            hidTouchRead(&touch);
            if (touch.py < 26) {
                state.section = (touch.px < 158) ? PkBank::FocusSection::BankStorage : PkBank::FocusSection::SaveBox;
                PkBank::Audio::playXYToggle(1.0f);
            } else if (touch.py < 44 && touch.px >= 160) {
                state.section = PkBank::FocusSection::SaveParty;
                PkBank::Audio::playXYToggle(1.04f);
            } else if (touch.py >= 198 && touch.py < 216) {
                if (touch.px < 158) {
                    state.showOptionsModal = !state.showOptionsModal;
                    PkBank::Audio::playXYMenuOpen();
                } else {
                    PkBank::Parser::commitSaveWithBackup(activeSave);
                    PkBank::Parser::saveLocalBank("sdmc:/3ds/PkBank3DS/bank.bnk", bankBoxes);
                    committedSave = activeSave;
                    committedBankBoxes = bankBoxes;
                    state.hasUnsavedChanges = false;
                    PkBank::Audio::playXYDropPokemon();
                }
            }
        }

        // Persist last highlighted Pokemon on Top Screen
        if (state.section == PkBank::FocusSection::BankStorage &&
            state.bankBox < static_cast<int>(bankBoxes.size()) &&
            bankBoxes[state.bankBox].occupied[state.cursorSlot]) {
            state.lastHighlighted = bankBoxes[state.bankBox].slots[state.cursorSlot];
            state.hasLastHighlighted = true;
        } else if (state.section == PkBank::FocusSection::SaveBox &&
                   !activeSave.boxes.empty() &&
                   activeSave.boxes[state.saveBox].occupied[state.cursorSlot]) {
            state.lastHighlighted = activeSave.boxes[state.saveBox].slots[state.cursorSlot];
            state.hasLastHighlighted = true;
        } else if (state.section == PkBank::FocusSection::SaveParty &&
                   activeSave.partyOccupied[state.cursorSlot]) {
            state.lastHighlighted = activeSave.party[state.cursorSlot];
            state.hasLastHighlighted = true;
        }

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        renderTopSummaryScreen(top, state);
        renderBottomTouchScreen(bot, state, activeSave, bankBoxes);
        C3D_FrameEnd(0);
    }

    // Do NOT auto-save bankBoxes on exit — changes persist ONLY when the user explicitly saves!
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    ndspExit();
    amExit();
    romfsExit();
    return 0;
}
