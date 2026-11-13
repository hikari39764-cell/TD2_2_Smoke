#pragma once

#include <cstdint>

#include "HIKARI/Project/ActionTypes.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2 {

// 手応え（ヒットストップ・カメラの揺れ・手柄の振動）を出す共通の小さな関数。
// 数値は呼び出し側のコンポーネントの項目から渡す。ここには調整値を持たない。
namespace Feedback {

constexpr float kFramesPerSecond = 60.0f;

// 時間倍率・カメラの競合の優先度。大きいほど勝つ。プレイヤー 300 < 吸い込みの寄り 305 < 発射 310
// < 吸い取り 315 < 命中 320 < 崩し・気絶 330。
constexpr int kCatchPriority = 315;
constexpr int kBreakPriority = 330;

// 時間倍率を frames F の間 scale にする。frames が 0 以下、または scale が 1 以上なら何もせず 0 を返す。
// 返す値は解除用の token（時間が過ぎれば自然に失効する）。
uint64_t RequestHitStop(HIKARI::WorldView& world, uint64_t sourceId, int priority, int frames, float scale);

// 手柄を frames F 振動させる。どちらのモーターも 0、または frames が 0 以下なら何もしない。受け付けたら true。
bool Rumble(HIKARI::WorldView& world, float low, float high, int frames);

struct CameraShakeParams {
    uint64_t sourceId = 0;
    int priority = 0;
    int frames = 0;
    float amplitude = 0.0f;
    float frequency = 24.0f;
    float kickDistance = 0.0f;
    HIKARI::GameProjectFloat3 kickDirection{};
    float fovDegrees = 0.0f;
};

// 一回分のカメラの揺れ。出した瞬間から frames F の間、実時間で二乗に弱まる。毎フレーム SubmitCameraShake を呼ぶ。
struct CameraShake {
    bool active = false;
    float ageSeconds = 0.0f;
    float durationSeconds = 0.0f;
    CameraShakeParams params{};
};

// 揺れの始まり。動きが何も無い（揺れ幅・押し・視野角がすべて 0）か frames が 0 なら始めない。
void StartCameraShake(CameraShake& shake, const CameraShakeParams& params);

// 揺れが続いている間、このフレームの分をカメラへ出して時間を進める。
// 出した変化量（ローカルの位置のずれ）を offsetOut へ返す（検証の記録用。null 可）。出せたら true。
bool SubmitCameraShake(HIKARI::WorldView& world, CameraShake& shake, uint64_t preferredCameraId, float unscaledDt,
                       HIKARI::GameProjectFloat3* offsetOut = nullptr);

} // namespace Feedback

} // namespace Project_TD2_2
