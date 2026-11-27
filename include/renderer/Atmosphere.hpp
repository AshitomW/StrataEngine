#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include "core/EngineConstants.hpp"

namespace Atmosphere {

struct State {
    glm::vec3 sunDir;
    glm::vec3 activeLightDir;
    glm::vec3 sunColor;
    glm::vec3 skyZenith;
    glm::vec3 skyHorizon;
    float ambientIntensity = 0.0f;
    float dayFactor = 0.0f;
    float starFactor = 0.0f;
    float sinElevation = 0.0f;
    glm::vec4 skyClearColor{0.0f, 0.0f, 0.0f, 1.0f};
};

inline float smoothstep(float edge0, float edge1, float x) {
    float val = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return val * val * (3.0f - 2.0f * val);
}

inline State compute(float timeOfDay) {
    using namespace EngineConstants::Environment::SkyColors;

    State s;
    float sunAngle = ((timeOfDay - 6.0f) / 12.0f) * 3.141592653589793f;
    float sinElev = std::sin(sunAngle);
    float cosElev = std::cos(sunAngle);
    s.sinElevation = sinElev;

    s.sunDir = glm::normalize(glm::vec3(cosElev * 0.80f, sinElev, 0.35f));
    glm::vec3 moonDir = glm::normalize(glm::vec3(-cosElev * 0.80f, -sinElev, -0.35f));

    float tHorizonToDay = smoothstep(-0.02f, 0.68f, sinElev);
    float tDuskToSunset = smoothstep(-0.06f, 0.14f, sinElev);
    float tNightToDusk = smoothstep(-0.20f, 0.02f, sinElev);

    glm::vec3 zenithDusk   = glm::mix(NIGHT_ZENITH, DUSK_ZENITH, tNightToDusk);
    glm::vec3 zenithSunset = glm::mix(zenithDusk, SUNSET_ZENITH, tDuskToSunset);
    s.skyZenith            = glm::mix(zenithSunset, NOON_ZENITH, tHorizonToDay);

    glm::vec3 horizDusk    = glm::mix(NIGHT_HORIZON, DUSK_HORIZON, tNightToDusk);
    glm::vec3 horizSunset  = glm::mix(horizDusk, SUNSET_HORIZON, tDuskToSunset);
    s.skyHorizon           = glm::mix(horizSunset, NOON_HORIZON, tHorizonToDay);

    float sunWeight = smoothstep(-0.04f, 0.14f, sinElev);
    float moonWeight = smoothstep(-0.05f, -0.22f, sinElev);
    glm::vec3 sunCol = glm::mix(DUSK_SUN * DUSK_SUN_INTENSITY,
                                glm::mix(SUNSET_SUN * SUNSET_SUN_INTENSITY, NOON_SUN * NOON_SUN_INTENSITY, tHorizonToDay),
                                tDuskToSunset);
    glm::vec3 moonCol = NIGHT_MOON * NIGHT_MOON_INTENSITY;
    s.sunColor = sunCol * sunWeight + moonCol * moonWeight;

    float ambTwilight = glm::mix(NIGHT_AMBIENT, DUSK_AMBIENT, tNightToDusk);
    float ambSunset   = glm::mix(ambTwilight, SUNSET_AMBIENT, tDuskToSunset);
    s.ambientIntensity = glm::mix(ambSunset, NOON_AMBIENT, tHorizonToDay);

    s.dayFactor = smoothstep(-0.14f, 0.18f, sinElev);
    s.starFactor = 1.0f - smoothstep(-0.20f, 0.05f, sinElev);

    // Switch shadow/directional light seamlessly while both sunWeight and moonWeight are zero
    s.activeLightDir = (sinElev >= -0.045f) ? s.sunDir : moonDir;

    float clearBlend = smoothstep(-0.22f, 0.25f, sinElev);
    s.skyClearColor = glm::vec4(
        glm::mix(glm::vec3(0.002f, 0.004f, 0.012f), glm::vec3(0.025f, 0.090f, 0.280f), clearBlend),
        1.0f
    );

    return s;
}

}