#pragma once

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/PresentationTypes.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kDamageableType = "Project_TD2_2.Damageable";
inline constexpr std::string_view kDamageableSystem = "Project_TD2_2.DamageableSystem";

class DamageableComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    float maxHealth = 1000.0f;
    bool invulnerable = false;
    bool traceEveryHit = false;
    // 命中の止め。連射（8F ごと）でも止めが積み上がらないよう、止めている間の次の命中では延ばさない（c-d330c0。元は 0 F / 0.0）。
    int hitStopFrames = 2;
    float hitStopScale = 0.05f;
    // 命中のカメラの手応え。連射が当たり続けても画面が震え続けない強さにする（当たった重さは、体の光と振動で出す）。
    // 揺れ幅 0.015、押し 0.03、視野角 0 から強めた（c-d330c0）。
    int hitCameraFrames = 6;
    float hitCameraAmplitude = 0.03f;
    float hitCameraFrequency = 24.0f;
    float hitCameraKickDistance = 0.05f;
    float hitCameraFovDegrees = 0.4f;
    // この量以上の弾が当たった時を、カメラの揺れの最大とする。距離で威力が減った弾は同じ割合で弱く揺らす。
    float hitFeedbackFullDamage = 40.0f;
    // 威力が小さい弾でも、カメラの揺れが消えきらないための下限（最大に対する割合）。
    float hitFeedbackMinScale = 0.3f;
    uint64_t feedbackCameraId = 0;
    int hitRumbleFrames = 9;
    float hitRumbleLow = 0.85f;
    float hitRumbleHigh = 0.6f;
    int hitFlashFrames = 8;
    float hitFlashStrength = 3.0f;
    HIKARI::GameProjectFloat3 hitFlashColor{0.45f, 0.92f, 1.0f};
    int hitChromaFrames = 4;
    float hitChromaPixels = 1.5f;
    float hitChromaContrast = 0.06f;
    int hitMaterialIndex = 0;

    // 実行時の記録は保存しない。敵の種類や被弾後の動作には依存しない。
    bool initialized = false;
    float currentHealth = 0.0f;
    float lastDamage = 0.0f;
    // 受けた時の、倍率をかける前の量。命中の手応えの強さ（hitFeedbackFullDamage との比）に使う。
    float lastRawDamage = 0.0f;
    // 受けるダメージの倍率（ボスの気絶中など、持ち主のルールが毎 tick 決める）。
    float incomingDamageScale = 1.0f;
    // 倍率をかける前の、受け付けたダメージの累計。気絶値のように「当てた量」に比例する規則が差分を読む。
    double baseDamageTotal = 0.0;
    HIKARI::GameProjectFloat3 lastDirection{};
    uint64_t hitCount = 0;
    uint64_t feedbackHitCount = 0;
    uint64_t hitStopToken = 0;
    float feedbackAgeSeconds = 0.0f;
    // 光りの経過と長さ。命中の光りと、持ち主が頼む光り（ボスの崩し・気絶）で長さが違うので、手応えの経過とは別に持つ。
    float flashAgeSeconds = 0.0f;
    float flashDurationSeconds = 0.0f;
    int pendingFlashFrames = 0;
    // 命中のカメラの揺れの強さ（0〜1）。命中ごとに決める。
    float hitCameraScale = 1.0f;
    bool feedbackActive = false;
    bool flashActive = false;
    bool flashSourceValid = false;
    bool flashClearAccepted = true;
    uint32_t flashMaterialIndex = 0;
    HIKARI::GameProjectModelMaterialColor flashSource{};
};

} // namespace Project_TD2_2
