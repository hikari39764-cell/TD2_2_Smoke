#pragma once

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

// 発射規則から独立した見た目と手応え。GourdShot がデータとして所有する。
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
    // 火弾を撃った時だけ掛ける倍率（c-5d738d）。カメラは後退幅と視野角だけを強め、揺れ幅には掛けない
    // （8F 間隔の連射で重なっても震え続けないように）。
    float fireCameraScale = 1.6f;
    float fireChromaScale = 1.5f;
    float fireRumbleScale = 1.4f;
    float dryFireKickDistance = 0.05f;
    int dryFireKickFrames = 3;
    int dryFireRumbleFrames = 5;
    float dryFireRumble = 0.2f;

    uint64_t observedShots = 0;
    uint64_t observedDryFires = 0;
    float ageSeconds = 0.0f;
    bool active = false;
    HIKARI::GameProjectFloat3 direction{};
    // いまの発射の、カメラ（後退幅・視野角）と色ずれの倍率。火弾なら fire〜Scale、ふだんの弾は 1。
    float cameraScale = 1.0f;
    float chromaScale = 1.0f;
    // 前のフレームにヒョウタンが火弾の状態だったか（始まりの合図を 1 回だけ出すため）。
    bool observedFireShot = false;
};

} // namespace Project_TD2_2
