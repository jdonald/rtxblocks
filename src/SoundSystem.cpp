#include "SoundSystem.h"
#include <cmath>

#pragma comment(lib, "winmm.lib")

const std::string SoundSystem::SOUND_BLOCK_HIT = "hit";
const std::string SoundSystem::SOUND_BLOCK_BREAK = "break";
const std::string SoundSystem::SOUND_BLOCK_PLACE = "place";
const std::string SoundSystem::SOUND_LAND = "land";

SoundSystem::SoundSystem()
    : m_initialized(false) {
}

SoundSystem::~SoundSystem() {
}

void SoundSystem::Initialize() {
    m_initialized = true;
}

void SoundSystem::PlaySound(const std::string& soundName) {
    if (!m_initialized) return;

    // Use simple beep sounds for now
    // Different frequencies for different actions
    if (soundName == SOUND_BLOCK_HIT) {
        // Quick high-pitched click for hitting
        Beep(800, 50);
    }
    else if (soundName == SOUND_BLOCK_BREAK) {
        // Breaking sound - descending tone
        Beep(600, 80);
        Beep(400, 60);
    }
    else if (soundName == SOUND_BLOCK_PLACE) {
        // Placing sound - soft click
        Beep(500, 60);
    }
    else if (soundName == SOUND_LAND) {
        // Landing sound - thud
        Beep(300, 100);
    }
}
