#pragma once

#include <cstdint>

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kCloudMountType = "Project_TD2_2.CloudMount";
inline constexpr std::string_view kCloudMountSystem = "Project_TD2_2.CloudMountSystem";

// 筋斗雲の段階。Gameplay.Mount を押すたびに、乗る（Summoning → Riding）と降りる（Dismissing → None）を切り替える。
enum class CloudMountState { None, Summoning, Riding, Dismissing };

// 飛行の入力。描画フレームの入力から入れ、固定 tick が使う。
struct CloudMountInput {
    // Gameplay.Move の左右（右が正）と前後（前が正）。
    float strafe = 0.0f;
    float forward = 0.0f;
    // Gameplay.Jump（上昇）と Gameplay.Descend（下降）を押している。
    bool ascend = false;
    bool descend = false;
};

// プレイヤーの筋斗雲。Player と同じ物体に付ける。状態と飛行の運動の規則は CloudMountRules、入力を読んで運動を出すのは
// CloudMountSystem。見た目（いまは仮の雲）とカメラの要求は差し替えられるよう別の所に置く。ここは調整値と実行時の状態だけ。
class CloudMountComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 召喚：summonFrames F で、足元を地面から rideHeight 浮いた高さへ持ち上げる（今より低くはしない）。
    int summonFrames = 30;
    float rideHeight = 1.2f;
    // 持ち上げの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）。
    float summonEasePower = 2.0f;
    // 召喚の時に足元の地面を探す距離 (m)。これより下に地面が無ければ、その場の高さで乗る（落下中の召喚）。
    float summonGroundReach = 4.0f;
    // 解除：dismissFrames F その場で止まってから、いつもの重力で落ちる。
    int dismissFrames = 12;
    // 飛行：入力が無ければその場に浮く。前後はカメラの見ている向き、左右は水平の横、上昇・下降は真上・真下。
    float cruiseSpeed = 16.0f;
    float verticalSpeed = 10.0f;
    float accelSeconds = 0.35f;
    float brakeSeconds = 0.5f;
    // 体を水平の進む向きへ回す速さ（度/秒）。
    float turnSpeed = 540.0f;
    // このカメラの見下ろし角度（度）を水平の飛行とみなす。0 でカメラの向きのまま（始めのカメラは 16 度見下ろす）。
    float levelPitch = 16.0f;
    // 騎乗中のカメラの要求（Player の「カメラ」の値の代わりに使う）。
    float cameraVerticalSeconds = 0.08f;
    float cameraVerticalDeadZone = 0.0f;
    float cameraMinPitch = -70.0f;
    float cameraMaxPitch = 75.0f;
    // 仮の雲（流体の雲ができるまで）。シーンの物体（Procedural Mesh の球）を足元へ動かし、乗っていない間は地面の下へしまう。
    uint64_t cloudObjectId = 0;
    float cloudDiameter = 2.2f;
    float cloudThickness = 0.6f;
    // 雲の中心を足元からどれだけ下げるか (m)。
    float cloudBelowFeet = 0.15f;
    // しまう深さ（体の中心から下へ m）。
    float cloudParkDepth = 30.0f;
    // 確かめ用：この F ごとに位置・速度をログに出す（0 は出さない）。
    int traceEveryFrames = 0;

    // 実行時のデータは保存しない。
    CloudMountState state = CloudMountState::None;
    // いまの段階に入ってから進んだ固定 tick の数。
    int stateFrame = 0;
    // Mount が押された（固定 tick が使ったら消す）。
    bool mountPressed = false;
    CloudMountInput input{};
    // 飛行の速度（世界 m/s）。縦はこの値をソルバーの重力の差し替えで出す。
    HIKARI::GameProjectFloat3 velocity{};
    // 召喚の始めと終わりの体の中心の高さ。
    float summonStartY = 0.0f;
    float summonTargetY = 0.0f;
    // 立った時の、体の中心から足元までの高さ。地面に立っている間に測る（測る前は Player のカプセルの半分）。
    float standOffset = 0.85f;
    // ログ用：この tick に提出した縦の速さと重力の差し替え、提出が受け入れられたか。
    float submittedVertical = 0.0f;
    float submittedGravity = 0.0f;
    bool motionAccepted = true;
    bool cloudAccepted = true;
};

} // namespace Project_TD2_2
