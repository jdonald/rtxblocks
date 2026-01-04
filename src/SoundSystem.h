#pragma once
#include <windows.h>
#include <mmsystem.h>
#include <string>
#include <map>

class SoundSystem {
public:
    SoundSystem();
    ~SoundSystem();

    void Initialize();
    void PlaySound(const std::string& soundName);

    // Sound types
    static const std::string SOUND_BLOCK_HIT;
    static const std::string SOUND_BLOCK_BREAK;
    static const std::string SOUND_BLOCK_PLACE;
    static const std::string SOUND_LAND;

private:
    void GenerateTone(const std::string& filename, int frequency, int duration);
    bool m_initialized;
};
