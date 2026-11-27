#pragma once

#include "terrain/TerrainTypes.hpp"

enum class WeatherType : int {
    Clear = 0,
    PartlyCloudy = 1,
    Overcast = 2,
    Rain = 3,
    Thunderstorm = 4,
    Snow = 5,
    Foggy = 6
};

class WeatherSystem {
public:
    WeatherSystem() = default;

    void update(TerrainConfig& config, float time, float dt);

private:
    float m_nextLightningTime = 4.0f;
    float m_flashDecay = 0.0f;
};

