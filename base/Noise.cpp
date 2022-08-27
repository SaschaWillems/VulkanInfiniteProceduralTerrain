#include "Noise.h"

#include <stdint.h>

#pragma once

float PerlinNoise::fade(float t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::lerp(float t, float a, float b)
{
    return a + t * (b - a);
}

float PerlinNoise::grad(int hash, float x, float y)
{
    return ((hash & 1) == 0 ? x : -x) + ((hash & 2) == 0 ? y : -y);
}

float PerlinNoise::noise(float x, float y)
{
    int32_t X = (int32_t)floor(x) & 255;
    int32_t Y = (int32_t)floor(y) & 255;
    x -= floor(x);
    y -= floor(y);
    float u = fade(x);
    float v = fade(y);
    uint32_t A = (permutations[X] + Y) & 0xff;
    uint32_t B = (permutations[X + 1] + Y) & 0xff;
    return lerp(v, lerp(u, grad(permutations[A], x, y), grad(permutations[B], x - 1, y)), lerp(u, grad(permutations[A + 1], x, y - 1), grad(permutations[B + 1], x - 1, y - 1)));
}
