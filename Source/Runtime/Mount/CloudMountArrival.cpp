#include "CloudMountArrival.h"

#include <algorithm>
#include <cmath>

#include "../Player/Feedback.h"
#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;
using namespace VectorMath;

using VectorMath::kPi;
using VectorMath::kRadiansPerDegree;
// 曲がりを決める点を、飛び始めの水平の位置のどれだけの所に置くか。足元の手前で道が水平（受け止めでは上向き）になり、
// 雲が足元へ滑り込む。
constexpr float kBendReach = 0.3f;
// 飛ぶ長さの下限（秒）。summonFrames × 割合が 0 に近い時に、割り算が大きくなりすぎないように。
constexpr float kMinimumFlySeconds = 1.0f / Feedback::kFramesPerSecond;
// 上り下りの角度を測る時の水平の長さの下限（真上・真下の向きで角度が飛ばないように）。
constexpr float kMinimumHorizontal = 1e-4f;

} // namespace

void CloudMountArrival::Begin(const CloudMountComponent& mount, CloudMountArrivalState& state, Vec3 facing,
                              bool catching) {
    const Vec3 forward = Normalize({facing.x, 0.0f, facing.z}, {0.0f, 0.0f, 1.0f});
    // 右（体の右＝局所 −X）。真後ろから右へ side 度回した向きが、飛び始めの水平の向き。
    const Vec3 right{-forward.z, 0.0f, forward.x};
    const float side = mount.summonFromSideDegrees * kRadiansPerDegree;
    const Vec3 back = Add(Scale(forward, -std::cos(side)), Scale(right, std::sin(side)));
    Vec3 from = Add(Scale(back, std::max(0.0f, mount.summonFromDistance)), {0.0f, mount.summonFromHeight, 0.0f});
    Vec3 bend = Add(Scale({from.x, 0.0f, from.z}, kBendReach),
                    {0.0f, catching ? -std::max(0.0f, mount.summonCatchDip) : mount.summonApproachHeight, 0.0f});
    state.flySeconds = std::max(kMinimumFlySeconds, static_cast<float>(std::max(1, mount.summonFrames)) *
                                                        std::clamp(mount.summonArriveFraction, 0.0f, 1.0f) /
                                                        Feedback::kFramesPerSecond);
    // 道の長さの見積もり（弦と、曲がりの点を通る折れ線の平均）。出だしの速さは 平均 × 進みの曲線の傾き（ease の冪）。
    const float length = 0.5f * (Length(from) + Length(Difference(from, bend)) + Length(bend));
    const float peak = std::max(1.0f, mount.summonFlyEasePower) * length / state.flySeconds;
    if (mount.summonFlySpeedLimit > 0.0f && peak > mount.summonFlySpeedLimit) {
        const float shrink = mount.summonFlySpeedLimit / peak;
        from = Scale(from, shrink);
        bend = Scale(bend, shrink);
    }
    state.from = from;
    state.bend = bend;
    state.catching = catching;
    state.flying = true;
    state.age = 0.0f;
    state.flyFrames = 0;
    state.hasLastRoot = false;
    state.landedAge = -1.0f;
    state.scatterAge = -1.0f;
    state.trail = 0.0f;
    const Vec3 direction = PathDirection(state, 0.0f);
    state.yaw = YawOf(direction);
    state.pitch = PitchOf(direction, mount.cloudPitchMaxDegrees);
}

float CloudMountArrival::Progress(const CloudMountComponent& mount, const CloudMountArrivalState& state) {
    const float x = std::clamp(state.age / std::max(state.flySeconds, kMinimumFlySeconds), 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - x, std::max(1.0f, mount.summonFlyEasePower));
}

Vec3 CloudMountArrival::PathOffset(const CloudMountArrivalState& state, float u) {
    const float rest = 1.0f - u;
    return Add(Scale(state.from, rest * rest), Scale(state.bend, 2.0f * u * rest));
}

Vec3 CloudMountArrival::PathDirection(const CloudMountArrivalState& state, float u) {
    // 終点は足元（0）なので、B'(u) = 2(1−u)(bend − from) + 2u(0 − bend)。
    return Add(Scale(Difference(state.bend, state.from), 2.0f * (1.0f - u)), Scale(state.bend, -2.0f * u));
}

float CloudMountArrival::Sink(const CloudMountComponent& mount, const CloudMountArrivalState& state) {
    if (state.landedAge < 0.0f || mount.summonSinkSeconds <= 0.0f || state.landedAge >= mount.summonSinkSeconds)
        return 0.0f;
    // 平方根で時間を縮め、沈みを速く（0.25 の所で一番深い）、戻りをゆっくりにする。
    return state.sinkDepth * std::sin(kPi * std::sqrt(state.landedAge / mount.summonSinkSeconds));
}

float CloudMountArrival::Afterglow(const CloudMountComponent& mount, const CloudMountArrivalState& state) {
    if (state.landedAge < 0.0f || mount.summonPuffSeconds <= 0.0f || state.landedAge >= mount.summonPuffSeconds)
        return 0.0f;
    const float rest = 1.0f - state.landedAge / mount.summonPuffSeconds;
    return rest * rest;
}

float CloudMountArrival::Settle(const CloudMountComponent& mount, const CloudMountArrivalState& state) {
    if (state.landedAge < 0.0f || mount.summonSettleSeconds <= 0.0f)
        return 1.0f;
    return Smoothstep(state.landedAge / mount.summonSettleSeconds);
}

float CloudMountArrival::YawOf(Vec3 direction) { return std::atan2(direction.x, direction.z) / kRadiansPerDegree; }

float CloudMountArrival::PitchOf(Vec3 direction, float limitDegrees) {
    const float horizontal = std::hypot(direction.x, direction.z);
    const float degrees = std::atan2(-direction.y, std::max(horizontal, kMinimumHorizontal)) / kRadiansPerDegree;
    return std::clamp(degrees, -limitDegrees, limitDegrees);
}

float CloudMountArrival::BlendDegrees(float a, float b, float t) {
    const float difference = std::remainder(b - a, 360.0f);
    return a + difference * std::clamp(t, 0.0f, 1.0f);
}

} // namespace Project_TD2_2
