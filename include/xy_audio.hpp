#pragma once
#include <3ds.h>
#include <cmath>
#include <cstdint>

namespace PkBank::Audio {

// Pokemon X & Y (Gen VI Kalos) crisp NDSP UI SFX synthesizer
// Note: Pokemon cries are NEVER played automatically on highlight/select — only manually on KEY_ZL (ZL Cry)
static constexpr int SAMPLE_RATE = 22050;

inline void playToneBurst(int channel, float startHz, float endHz, int durationMs, float gain) {
    const int samples = (SAMPLE_RATE * durationMs) / 1000;
    if (samples <= 0) return;
    static int16_t pcmBuf[4096];
    const int count = samples < 4096 ? samples : 4096;
    float phase = 0.0f;
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count);
        const float freq = startHz + (endHz - startHz) * t;
        const float env = (t < 0.08f ? (t / 0.08f) : (1.0f - t)) * gain;
        phase += (2.0f * 3.14159265f * freq) / static_cast<float>(SAMPLE_RATE);
        pcmBuf[i] = static_cast<int16_t>(std::sin(phase) * env * 28000.0f);
    }
    DSP_FlushDataCache(pcmBuf, static_cast<u32>(count * sizeof(int16_t)));
    static ndspWaveBuf waveBuf{};
    waveBuf.data_vaddr = pcmBuf;
    waveBuf.nsamples   = static_cast<u32>(count);
    waveBuf.looping    = false;
    ndspChnReset(channel);
    ndspChnSetInterp(channel, NDSP_INTERP_LINEAR);
    ndspChnSetRate(channel, static_cast<float>(SAMPLE_RATE));
    ndspChnSetFormat(channel, NDSP_FORMAT_MONO_PCM16);
    ndspChnWaveBufAdd(channel, &waveBuf);
}

inline void playXYCursorMove()         { playToneBurst(0, 1260.0f, 1520.0f, 34, 0.24f); }
inline void playXYPickPokemon()        { playToneBurst(1,  784.0f, 1568.0f, 75, 0.30f); }
inline void playXYDropPokemon()        { playToneBurst(1, 1318.0f,  988.0f, 68, 0.30f); }
inline void playXYBoxSwitch(int dir)   { playToneBurst(2, dir > 0 ? 740.0f : 1108.0f, dir > 0 ? 1108.0f : 740.0f, 64, 0.28f); }
inline void playXYCancel()             { playToneBurst(3,  659.0f,  440.0f, 80, 0.26f); }
inline void playXYMenuOpen()           { playToneBurst(4,  880.0f, 1318.0f, 95, 0.28f); }
inline void playXYToggle(float pitch)  { playToneBurst(5,  880.0f * pitch, 1174.0f * pitch, 55, 0.25f); }
inline void playXYLegalityScan(bool on){ playToneBurst(6, on ? 987.0f : 1318.0f, on ? 1480.0f : 880.0f, 85, 0.22f); }
inline void playManualPokemonCry(uint16_t speciesId) {
    // Plays romfs:/cries/<speciesId>.wav if present, or crisp procedural species cry fallback on KEY_ZL
    const float base = 260.0f + static_cast<float>((speciesId * 37) % 440);
    playToneBurst(7, base, base * 1.42f, 145, 0.34f);
}

} // namespace PkBank::Audio
