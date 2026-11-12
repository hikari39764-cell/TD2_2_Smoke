#pragma once

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

// 発射規則から独立した見た目と手応え。GourdShotComponent がデータとして所有する。
struct GourdFeedback {
    void DescribeProperties(HIKARI::PropertyFields& fields);

    int cameraFrames = 10;
    float cameraKickDistance = 0.2f;
    float cameraShakeAmplitude = 0.035f;
    float cameraShakeFrequency = 22.0f;
    float cameraFovDegrees = 1.6f;
    int chromaFrames = 5;
    float chromaPixels = 5.0f;
    float chromaContrast = 0.15f;
    int rumbleFrames = 10;
    float rumbleLow = 0.6f;
    float rumbleHigh = 0.35f;
    float dryFireKickDistance = 0.05f;
    int dryFireKickFrames = 3;
    int dryFireRumbleFrames = 5;
    float dryFireRumble = 0.2f;

    uint64_t observedShots = 0;
    uint64_t observedDryFires = 0;
    float ageSeconds = 0.0f;
    bool active = false;
    HIKARI::GameProjectFloat3 direction{};
};

} // namespace Project_TD2_2
