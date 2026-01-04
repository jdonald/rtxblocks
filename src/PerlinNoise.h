#pragma once
#include <vector>
#include <random>

class PerlinNoise {
public:
    PerlinNoise(unsigned int seed = 0);

    float Noise(float x, float y, float z) const;
    float Noise(float x, float y) const;

    float OctaveNoise(float x, float y, int octaves, float persistence = 0.5f) const;
    float OctaveNoise(float x, float y, float z, int octaves, float persistence = 0.5f) const;

private:
    std::vector<int> p;

    float Fade(float t) const;
    float Lerp(float t, float a, float b) const;
    float Grad(int hash, float x, float y, float z) const;
};
