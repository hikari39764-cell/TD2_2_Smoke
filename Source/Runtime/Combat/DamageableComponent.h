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
    int hitStopFrames = 0;
    float hitStopScale = 0.0f;
    int hitCameraFrames = 14;
    float hitCameraAmplitude = 0.16f;
    float hitCameraFrequency = 24.0f;
    float hitCameraKickDistance = 0.12f;
    float hitCameraFovDegrees = 1.2f;
    uint64_t feedbackCameraId = 0;
    int hitRumbleFrames = 9;
    float hitRumbleLow = 0.85f;
    float hitRumbleHigh = 0.6f;
    int hitFlashFrames = 8;
    float hitFlashStrength = 3.0f;
    HIKARI::GameProjectFloat3 hitFlashColor{0.45f, 0.92f, 1.0f};
    int hitChromaFrames = 6;
    float hitChromaPixels = 7.0f;
    float hitChromaContrast = 0.25f;
    int hitMaterialIndex = 0;

    // 実行時の記録は保存しない。敵の種類や被弾後の動作には依存しない。
    bool initialized = false;
    float currentHealth = 0.0f;
    float lastDamage = 0.0f;
    // 受けるダメージの倍率（ボスの気絶中など、持ち主のルールが毎 tick 決める）。
    float incomingDamageScale = 1.0f;
    // 倍率をかける前の、受け付けたダメージの累計。気絶値のように「当てた量」に比例する規則が差分を読む。
    double baseDamageTotal = 0.0;
    HIKARI::GameProjectFloat3 lastDirection{};
    uint64_t hitCount = 0;
    uint64_t feedbackHitCount = 0;
    uint64_t hitStopToken = 0;
    float feedbackAgeSeconds = 0.0f;
    bool feedbackActive = false;
    bool flashActive = false;
    bool flashSourceValid = false;
    bool flashClearAccepted = true;
    uint32_t flashMaterialIndex = 0;
    HIKARI::GameProjectModelMaterialColor flashSource{};
};

} // namespace Project_TD2_2
