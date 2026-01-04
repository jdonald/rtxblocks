#include "PerlinNoise.h"
#include <algorithm>
#include <numeric>

PerlinNoise::PerlinNoise(unsigned int seed) {
    p.resize(256);
    std::iota(p.begin(), p.end(), 0);

    std::default_random_engine engine(seed);
    std::shuffle(p.begin(), p.end(), engine);

    p.insert(p.end(), p.begin(), p.end());
}

float PerlinNoise::Fade(float t) const {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::Lerp(float t, float a, float b) const {
    return a + t * (b - a);
}

float PerlinNoise::Grad(int hash, float x, float y, float z) const {
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float PerlinNoise::Noise(float x, float y, float z) const {
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    float u = Fade(x);
    float v = Fade(y);
    float w = Fade(z);

    int A = p[X] + Y;
    int AA = p[A] + Z;
    int AB = p[A + 1] + Z;
    int B = p[X + 1] + Y;
    int BA = p[B] + Z;
    int BB = p[B + 1] + Z;

    return Lerp(w, Lerp(v, Lerp(u, Grad(p[AA], x, y, z),
                                     Grad(p[BA], x - 1, y, z)),
                             Lerp(u, Grad(p[AB], x, y - 1, z),
                                     Grad(p[BB], x - 1, y - 1, z))),
                     Lerp(v, Lerp(u, Grad(p[AA + 1], x, y, z - 1),
                                     Grad(p[BA + 1], x - 1, y, z - 1)),
                             Lerp(u, Grad(p[AB + 1], x, y - 1, z - 1),
                                     Grad(p[BB + 1], x - 1, y - 1, z - 1))));
}

float PerlinNoise::Noise(float x, float y) const {
    return Noise(x, y, 0.0f);
}

float PerlinNoise::OctaveNoise(float x, float y, int octaves, float persistence) const {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float maxValue = 0;

    for (int i = 0; i < octaves; i++) {
        total += Noise(x * frequency, y * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }

    return total / maxValue;
}

float PerlinNoise::OctaveNoise(float x, float y, float z, int octaves, float persistence) const {
    float total = 0;
    float frequency = 1;
    float amplitude = 1;
    float maxValue = 0;

    for (int i = 0; i < octaves; i++) {
        total += Noise(x * frequency, y * frequency, z * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= 2;
    }

    return total / maxValue;
}
