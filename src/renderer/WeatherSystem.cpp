#include "renderer/WeatherSystem.hpp"
#include <algorithm>
#include <cmath>

namespace {

float lerpF(float a, float b, float t) {
    return a + (b - a) * std::clamp(t, 0.0f, 1.0f);
}

} // namespace

void WeatherSystem::update(TerrainConfig& config, float time, float dt) {
    float safeDt = std::clamp(dt, 0.001f, 0.1f);
    WeatherType wt = static_cast<WeatherType>(std::clamp(config.weatherType, 0, 6));

    if (config.weatherAutoTransition) {
        float targetCloud = 0.42f;
        float targetWet = 0.0f;
        float targetSnow = 0.0f;
        float targetPrecip = 0.0f;
        float targetPrecipType = 0.0f;
        float targetFog = 1.0f;
        float targetWind = 9.0f;

        switch (wt) {
            case WeatherType::Clear:
                targetCloud = 0.14f; targetWet = 0.0f; targetSnow = 0.0f;
                targetPrecip = 0.0f; targetPrecipType = 0.0f; targetFog = 0.75f; targetWind = 5.0f;
                break;
            case WeatherType::PartlyCloudy:
                targetCloud = 0.42f; targetWet = 0.0f; targetSnow = 0.0f;
                targetPrecip = 0.0f; targetPrecipType = 0.0f; targetFog = 1.0f; targetWind = 9.0f;
                break;
            case WeatherType::Overcast:
                targetCloud = 0.78f; targetWet = 0.15f; targetSnow = 0.0f;
                targetPrecip = 0.0f; targetPrecipType = 0.0f; targetFog = 1.35f; targetWind = 11.0f;
                break;
            case WeatherType::Rain:
                targetCloud = 0.86f; targetWet = 0.88f; targetSnow = 0.0f;
                targetPrecip = 0.75f; targetPrecipType = 1.0f; targetFog = 1.65f; targetWind = 14.0f;
                break;
            case WeatherType::Thunderstorm:
                targetCloud = 0.96f; targetWet = 1.0f; targetSnow = 0.0f;
                targetPrecip = 1.0f; targetPrecipType = 1.0f; targetFog = 1.90f; targetWind = 21.0f;
                break;
            case WeatherType::Snow:
                targetCloud = 0.82f; targetWet = 0.05f; targetSnow = 0.95f;
                targetPrecip = 0.85f; targetPrecipType = 2.0f; targetFog = 1.75f; targetWind = 10.0f;
                break;
            case WeatherType::Foggy:
                targetCloud = 0.65f; targetWet = 0.35f; targetSnow = 0.0f;
                targetPrecip = 0.0f; targetPrecipType = 0.0f; targetFog = 3.20f; targetWind = 4.0f;
                break;
        }

        float rate = 1.0f - std::exp(-safeDt * 1.2f);
        config.cloudCoverage = lerpF(config.cloudCoverage, targetCloud, rate);
        config.wetness = lerpF(config.wetness, targetWet, rate);
        config.snowAccumulation = lerpF(config.snowAccumulation, targetSnow, rate);
        config.precipitationIntensity = lerpF(config.precipitationIntensity, targetPrecip, rate);
        config.fogMultiplier = lerpF(config.fogMultiplier, targetFog, rate);
        config.windSpeed = lerpF(config.windSpeed, targetWind, rate);
        config.precipitationType = (targetPrecipType > 0.5f) ? targetPrecipType : (config.precipitationIntensity > 0.05f ? config.precipitationType : 0.0f);
    }

    if (wt == WeatherType::Thunderstorm) {
        if (time >= m_nextLightningTime) {
            m_flashDecay = 1.0f;
            float interval = 3.5f + std::fmod(std::abs(std::sin(time * 12.9898f) * 43758.5453f), 5.5f);
            m_nextLightningTime = time + interval;
        }
        m_flashDecay *= std::exp(-safeDt * 6.5f);
        config.lightningFlash = m_flashDecay;
    } else {
        m_flashDecay = 0.0f;
        config.lightningFlash = 0.0f;
    }
}
