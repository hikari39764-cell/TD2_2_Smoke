#include "CloudMountSpeedFx.h"

#include <algorithm>
#include <cmath>

#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;

// 速さの割り算の下限（巡航・ブーストの速さを同じにした時に割らない）。
constexpr float kMinimumSpan = 0.01f;
// これより弱い強さは 0 にする（ならしの尾を切り、効果を出さない＝止まっている時は一切掛からない）。強さ 0.01 の
// ぼかしは画面の隅でも 1 画素に届かない。
constexpr float kOffStrength = 0.01f;
// カメラの後ろへ向かう時に強さを弱める幅：進む向きとカメラの前の cos が 0（真横）で弱めず、−この値で 0。
// 後ろへ飛ぶ時は流れが画面の内へ向かうので、外へ流す効果は掛けない。
constexpr float kBackwardFadeCos = 0.5f;
// 進む向きの前の成分がこれより小さい時は、写した点が遠すぎるので限度の所へ置く。
constexpr float kMinimumForward = 1.0e-3f;
// 体を画面へ写す時の、カメラからの奥行きの下限 (m)。
constexpr float kMinimumDepth = 0.05f;

using VectorMath::Clamp01;
using VectorMath::Dot;
using VectorMath::Lerp;

// 目標へ seconds の時定数で近づく（0 以下で一瞬）。
using VectorMath::Approach;

} // namespace

float CloudMountSpeedFx::TargetStrength(const CloudMountComponent& mount) {
    if (mount.state == CloudMountState::None)
        return 0.0f;
    const float speed = std::max(mount.flight.speed, 0.0f);
    const float cruise = std::max(mount.cruiseSpeed, kMinimumSpan);
    const float boost = std::max(mount.boostSpeed, cruise);
    const float diveMax = std::max(mount.diveMaxSpeed, boost);
    const float start = std::clamp(mount.speedFxStartSpeed, 0.0f, cruise);
    // 速さの曲線：speedFxStartSpeed まで 0 → 巡航の速さで speedFxCruise（なめらかに立ち上がる）→ ブーストの速さで
    // speedFxBoost → 急降下の上限の速さで speedFxDive（間は線形）。
    float curve = 0.0f;
    if (speed <= cruise)
        curve = mount.speedFxCruise * VectorMath::Smoothstep((speed - start) / std::max(cruise - start, kMinimumSpan));
    else if (speed <= boost)
        curve = Lerp(mount.speedFxCruise, mount.speedFxBoost, (speed - cruise) / std::max(boost - cruise, kMinimumSpan));
    else
        curve = Lerp(mount.speedFxBoost, mount.speedFxDive, Clamp01((speed - boost) / std::max(diveMax - boost, kMinimumSpan)));
    // ブースト中と急降下の度合いは、速さが巡航へ届く前から強める（止まって押しているだけの時は掛けない）。
    const float moving = Clamp01(speed / cruise);
    const float boosting = mount.flight.boosting ? mount.speedFxBoost * moving : 0.0f;
    const float diving = mount.speedFxDive * Clamp01(mount.flight.dive) * moving;
    return std::max({curve, boosting, diving, 0.0f});
}

float CloudMountSpeedFx::StepStrength(CloudMountComponent& mount, float dt) {
    const float target = TargetStrength(mount);
    const float seconds = target > mount.speedFxStrength ? mount.speedFxRiseSeconds : mount.speedFxFallSeconds;
    mount.speedFxStrength = std::max(0.0f, Approach(mount.speedFxStrength, target, seconds, dt));
    if (target <= 0.0f && mount.speedFxStrength < kOffStrength)
        mount.speedFxStrength = 0.0f;
    const float strength = std::min(Clamp01(mount.speedFxStrength), Clamp01(mount.speedFxLimit));
    return strength < kOffStrength ? 0.0f : strength;
}

float CloudMountSpeedFx::HeadingOnScreen(const CloudMountComponent& mount, const CloudMountSpeedFxView& view,
                                         Vec3 direction, float& outU, float& outV) {
    const float x = Dot(direction, view.right) / std::max(view.tanHalfFov * view.aspect, kMinimumSpan);
    const float y = Dot(direction, view.up) / std::max(view.tanHalfFov, kMinimumSpan);
    const float z = Dot(direction, view.forward);
    const float limit = std::max(mount.speedFxCenterLimit, 0.0f);
    const float planar = std::hypot(x, y);
    float ndcX = 0.0f;
    float ndcY = 0.0f;
    if (z > kMinimumForward && planar <= limit * z) {
        ndcX = x / z;
        ndcY = y / z;
    } else if (planar > 0.0f) {
        // 横・後ろへ向かう時と遠すぎる時は、画面の上でその向きのまま限度の所へ置く（流れはほぼ平行になる）。
        ndcX = x / planar * limit;
        ndcY = y / planar * limit;
    }
    outU = 0.5f + 0.5f * ndcX;
    outV = 0.5f - 0.5f * ndcY;
    return Clamp01((z + kBackwardFadeCos) / kBackwardFadeCos);
}

void CloudMountSpeedFx::StepCenter(CloudMountComponent& mount, float u, float v, bool first, float dt) {
    if (first) {
        mount.speedFxCenterU = u;
        mount.speedFxCenterV = v;
        return;
    }
    mount.speedFxCenterU = Approach(mount.speedFxCenterU, u, mount.speedFxCenterSeconds, dt);
    mount.speedFxCenterV = Approach(mount.speedFxCenterV, v, mount.speedFxCenterSeconds, dt);
}

bool CloudMountSpeedFx::SphereOnScreen(const CloudMountSpeedFxView& view, Vec3 center, float radius, float& outU,
                                       float& outV, float& outRadius) {
    const Vec3 relative{center.x - view.eye.x, center.y - view.eye.y, center.z - view.eye.z};
    const float depth = Dot(relative, view.forward);
    if (depth < kMinimumDepth || view.tanHalfFov <= 0.0f || view.aspect <= 0.0f)
        return false;
    const float scale = 1.0f / (depth * view.tanHalfFov);
    outU = 0.5f + 0.5f * Dot(relative, view.right) * scale / view.aspect;
    outV = 0.5f - 0.5f * Dot(relative, view.up) * scale;
    // 画面の高さを 1 とした半径（NDC の縦 2 が画面の高さ 1）。
    outRadius = 0.5f * std::max(radius, 0.0f) * scale;
    return std::isfinite(outU) && std::isfinite(outV) && std::isfinite(outRadius);
}

} // namespace Project_TD2_2
