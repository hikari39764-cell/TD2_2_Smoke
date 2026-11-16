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
    // Gameplay.Sprint（ブースト）を押している。
    bool boost = false;
};

// 飛行の手応えの値。カメラ（④ c-db5975）と見た目（⑤〜⑧）が読むために毎 tick 書く。読む側は書き換えない。保存しない。
struct CloudMountFlightReadout {
    // いまの速さ (m/s)。騎乗していなければ 0。
    float speed = 0.0f;
    // ブースト中（Gameplay.Sprint を押し、精力が残っている間）。
    bool boosting = false;
    // ブーストの弾き出しの残り（押した瞬間 1、boostKickSeconds で 0）。
    float boostKick = 0.0f;
    // 旋回の強さ（−1〜1、右へ回る時が正）。いまの速さで回れる最大の速さで回っている時が ±1。
    float turn = 0.0f;
    // 急降下の度合い（0〜1）。下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。前後左右の入力がある時だけ。
    float dive = 0.0f;
    // 巡航・ブーストの速さを超えている分 (m/s)。急降下の後に overspeedDecaySeconds かけて 0 へ戻る。
    float overspeed = 0.0f;
    // 地面すれすれの持ち上げをこの tick に掛けた（騎乗中に地面に触れたまま速く進んでいる）。
    bool groundSkim = false;
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
    // どの状態でも超えない速さ (m/s)。⑤ の雲の流体の域は 1 フレームに 60 m/s を超える動きを瞬間移動と見なす。
    float maxFlightSpeed = 45.0f;
    // 地面すれすれの持ち上げ：騎乗中に地面に触れ、水平の速さが groundSkimMinSpeed 以上なら、その tick の縦の速さの
    // 下向きを捨てて groundSkimSpeed (m/s) 上へ出す。地面に触れたまま速く進むと運動学ソルバーが平らに近い所で止まる
    // 件（エンジンへの要望 e-dca15c）を避け、起伏を滑るように越える。groundSkimSpeed 0 で切る。
    // 持ち上げた後 groundSkimHoldSeconds の間は下へ向かわない（地面へ向けて飛び続けても、持ち上げと下降を毎 tick
    // 繰り返して震えず、地面に沿って滑る）。
    float groundSkimSpeed = 3.0f;
    float groundSkimMinSpeed = 6.0f;
    float groundSkimHoldSeconds = 0.2f;
    // ブースト：Gameplay.Sprint を押している間の最高速。押した瞬間は boostKickSeconds で boostKickSpeed まで弾き出す。
    float boostSpeed = 32.0f;
    float boostKickSpeed = 28.0f;
    float boostKickSeconds = 0.15f;
    // ブースト中に使う精力（/秒）。尽きたら入力を離すまでブーストしない。0 で使い放題。
    float boostStaminaPerSecond = 12.0f;
    // 急降下：下向きに進んでいる間、下向きの度合いに応じて速さが伸びる（度合い 1 で diveAccel、上限 diveMaxSpeed）。
    // 度合いは下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。浅い下りでは伸びない（速さが溜まり続け
    // ないように）。前で飛べる一番急な下りは「騎乗中のカメラの見下ろす限度 − levelPitch」（既定 75 − 16 = 59 度）。
    float diveAccel = 12.0f;
    float diveMaxSpeed = 45.0f;
    float diveStartDegrees = 10.0f;
    float diveFullDegrees = 55.0f;
    // 巡航・ブーストより速い分は、急降下をやめてからこの秒数で戻る（その間は速いまま上へ抜けられる）。
    float overspeedDecaySeconds = 1.5f;
    // 旋回：速度の向きを入力の向きへ回す速さ（度/秒）。巡航の速さ以下で turnRateCruise、ブーストの速さ以上で
    // turnRateBoost、間は速さで補間する。旋回の半径＝速さ ÷ 回る速さ（ラジアン/秒）なので速いほど大回りになる。
    float turnRateCruise = 180.0f;
    float turnRateBoost = 90.0f;
    // 落下の受け止め：乗っていない時、足場を離れて catchFallMeters 落ちたら自動で召喚し、下向きの速さを
    // catchBrakeSeconds でならして止める。自分で降りた後の落下は、catchAfterDismiss が true の時だけ受け止める。
    bool autoCatch = true;
    float catchFallMeters = 12.0f;
    float catchBrakeSeconds = 0.4f;
    bool catchAfterDismiss = false;
    // 飛べる範囲：場の原点から水平の半径 boundsRadius、高さ boundsMinY〜boundsMaxY（世界の Y）。境界の手前
    // boundsSoftMeters から外向きの速さを弱め、境界の上で 0 にする（壁に当たったようには止めない）。
    // 高さの既定は NimbusSky の島の床（Y 3400）から −150〜+300 m。
    float boundsRadius = 400.0f;
    float boundsMinY = 3250.0f;
    float boundsMaxY = 3700.0f;
    float boundsSoftMeters = 30.0f;
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
    // 外へ知らせる飛行の値（④〜⑧が読む）。
    CloudMountFlightReadout flight{};
    // ブーストが尽きた（入力を離すまでブーストしない）。
    bool boostExhausted = false;
    // 弾き出しの残り (秒)。
    float boostKickLeft = 0.0f;
    // 速い分を戻す速さ (m/s²)。戻し始めた時の「速い分 ÷ overspeedDecaySeconds」。戻していない間は 0。
    float overspeedDecayRate = 0.0f;
    // 前の固定 tick の体の中心と、そこから実際に動いた速さ（当たって止められた分の速さを持ち越さないために使う）。
    HIKARI::GameProjectFloat3 lastPosition{};
    bool hasLastPosition = false;
    float achievedSpeed = 0.0f;
    // 落下の受け止め：足場を離れてからの一番高い体の中心の高さ。受け止める落下か（自分で降りた後は catchAfterDismiss）。
    float fallPeakY = 0.0f;
    bool catchArmed = true;
    // 受け止めた時の下向きの速さ (m/s、正) と、ならしの残り (秒)。catching は受け止めの召喚の間（縦は持ち上げの輪郭
    // ではなく、落ちる勢いのならしで出す）。
    float catchFallSpeed = 0.0f;
    float catchBrakeLeft = 0.0f;
    bool catching = false;
    // 地面すれすれの持ち上げの後、下へ向かわない残り (秒)。
    float groundSkimHoldLeft = 0.0f;
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
