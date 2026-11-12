#pragma once

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

// 発射規則から独立した見た目と手応え。GourdShotComponent がデータとして所有する。
struct GourdFeedback {
    void DescribeProperties(HIKARI::PropertyFields& fields);

    // 発射のカメラの手応えは、連射（8F ごと）で重なっても画面が震え続けない強さにする。
    // 自分の通常攻撃でカメラを揺らさない（揺れ幅 0）。後ろへ少し押されて戻るだけ。
    int cameraFrames = 6;
    float cameraKickDistance = 0.04f;
    float cameraShakeAmplitude = 0.0f;
    float cameraShakeFrequency = 22.0f;
    float cameraFovDegrees = 0.3f;
    int chromaFrames = 4;
    float chromaPixels = 1.0f;
    float chromaContrast = 0.04f;
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
