#pragma once

#include <cstdint>

#include "HIKARI/Project/ActionTypes.h"
#include "HIKARI/Extend/World.h"

namespace Project_TD2_2 {

// 手応え（ヒットストップ・カメラの揺れ・手柄の振動）を出す共通の小さな関数。どのモジュールからも使う。
// 揺らすのはプレイヤーのカメラ、振るわせるのはプレイヤーの手柄なので、それを持つプレイヤーのモジュールに置く
// （c-c1b799。前は Combat にあり、カメラを探すのにプレイヤーを読んで Player と Combat が輪になっていた）。
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
bool SubmitCameraShake(HIKARI::WorldView& world, CameraShake& shake, HIKARI::Entity preferredCamera, float unscaledDt,
                       HIKARI::GameProjectFloat3* offsetOut = nullptr);

struct BlurPulseParams {
    int frames = 0;
    float shutterDegrees = 0.0f;
    float maxPixels = 0.0f;
};

// 一瞬の動きのぼかし（瞬間の反動を、画の流れで伝える。c-e0a9c3）。出した瞬間から frames F の間、実時間で二乗に弱まる。
struct BlurPulse {
    bool active = false;
    float ageSeconds = 0.0f;
    float durationSeconds = 0.0f;
    BlurPulseParams params{};
};

// 一瞬のぼかしを始める。frames が 0 以下か、シャッター角・長さのどちらかが 0 以下なら始めない。いま残っているぼかしの
// 方が強い（シャッター角が大きい）間も始めない（弱い合図で、強い合図の余韻を切らない）。
void StartBlurPulse(BlurPulse& pulse, const BlurPulseParams& params);

// 動きのぼかしの要求を 1 つ、いま画を出しているカメラへ出す。weight は、カメラの元の設定（無効なら 0°）からの補間
// （エンジンは、同じカメラへ出た要求のうち、優先度の一番高い 1 つだけを使う）。シャッター角 (度) と、ぼかしの長さの上限
// (1080p の画素) は範囲へ収める。出せたら true。
bool SubmitMotionBlur(HIKARI::WorldView& world, uint64_t sourceId, int priority, float weight, float shutterDegrees,
                      float maxPixels);

// ふだんの動きのぼかし（rest：シャッター角 (度) と長さの上限 (1080p の画素)。どちらかが 0 なら無し）に、一瞬のぼかしを
// 重ねて、このフレームの分を、いま画を出しているカメラへ出して時間を進める。エンジンは優先度の一番高い 1 つだけを使い、
// カメラの元の設定（無効＝0°）から混ぜるので、ふだんの分と一瞬の分は 1 つにまとめて出す（一瞬の分が弱まると、ふだんの
// 強さへ戻る）。出す物が無ければ何も出さずに false。
bool SubmitBlur(HIKARI::WorldView& world, uint64_t sourceId, int priority, BlurPulse& pulse, float restShutterDegrees,
                float restPixels, float unscaledDt);

// カメラの位置と注視点を、カメラの局所の向きへ一緒にずらす（このフレームだけ）。続けて出す揺れは、呼ぶ側が位相を進めて
// 毎フレーム出す。出せたら true。
bool SubmitCameraOffset(HIKARI::WorldView& world, uint64_t sourceId, int priority,
                        HIKARI::GameProjectFloat3 localOffset);

} // namespace Feedback

} // namespace Project_TD2_2
