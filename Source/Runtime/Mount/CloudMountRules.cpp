#include "CloudMountRules.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;

constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 飛ぶ向きの上り下りの限度（度）。真上・真下では左右の向きが決まらないので少し手前で止める。
constexpr float kMaximumFlightPitch = 89.0f;
// これより遅い間は体の向きを変えない (m/s)。止まり際に向きが震えないようにする。
constexpr float kTurnMinimumSpeed = 0.5f;

float Length(Vec3 value) { return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z); }

// 召喚の持ち上げの輪郭：frame F たった時の高さ。出だしが速く、終わりで速さが 0 になる。
float SummonHeightAt(const CloudMountComponent& mount, int frame) {
    const int frames = std::max(1, mount.summonFrames);
    const float progress = std::clamp(static_cast<float>(frame) / static_cast<float>(frames), 0.0f, 1.0f);
    const float share = 1.0f - std::pow(1.0f - progress, std::max(mount.summonEasePower, 0.01f));
    return mount.summonStartY + (mount.summonTargetY - mount.summonStartY) * share;
}

} // namespace

void CloudMountRules::Enter(CloudMountComponent& mount, CloudMountState state) {
    mount.state = state;
    mount.stateFrame = 0;
}

CloudMountState CloudMountRules::NextOnToggle(CloudMountState state) {
    switch (state) {
    case CloudMountState::None: return CloudMountState::Summoning;
    case CloudMountState::Dismissing: return CloudMountState::Summoning;
    case CloudMountState::Summoning:
    case CloudMountState::Riding: return CloudMountState::Dismissing;
    }
    return CloudMountState::None;
}

CloudMountState CloudMountRules::TimedNext(const CloudMountComponent& mount) {
    if (mount.state == CloudMountState::Summoning && mount.stateFrame >= std::max(1, mount.summonFrames))
        return CloudMountState::Riding;
    if (mount.state == CloudMountState::Dismissing && mount.stateFrame >= std::max(0, mount.dismissFrames))
        return CloudMountState::None;
    return mount.state;
}

void CloudMountRules::AdvanceFrame(CloudMountComponent& mount) {
    if (mount.state != CloudMountState::None)
        ++mount.stateFrame;
}

void CloudMountRules::BeginSummon(CloudMountComponent& mount, float startY, bool hasGround, float groundY) {
    mount.summonStartY = startY;
    mount.summonTargetY =
        hasGround ? std::max(startY, groundY + mount.standOffset + std::max(0.0f, mount.rideHeight)) : startY;
    // 落下の勢いは止め、横の勢いは残して（歩いていた速さから）飛び始める。
    mount.velocity.y = 0.0f;
    Enter(mount, CloudMountState::Summoning);
}

float CloudMountRules::SummonVerticalSpeed(const CloudMountComponent& mount, float dt) {
    if (dt <= 0.0f)
        return 0.0f;
    return (SummonHeightAt(mount, mount.stateFrame + 1) - SummonHeightAt(mount, mount.stateFrame)) / dt;
}

Vec3 CloudMountRules::WantedVelocity(const CloudMountComponent& mount, Vec3 cameraLook) {
    if (mount.state == CloudMountState::Dismissing || mount.state == CloudMountState::None)
        return {};
    const float lookLength = Length(cameraLook);
    const Vec3 look = lookLength > 0.0f
                          ? Vec3{cameraLook.x / lookLength, cameraLook.y / lookLength, cameraLook.z / lookLength}
                          : Vec3{0.0f, 0.0f, 1.0f};
    const float yaw = std::atan2(look.x, look.z);
    // 見下ろしが正。levelPitch だけ見下ろしたカメラで水平に飛ぶ。
    const float limit = kMaximumFlightPitch * kRadiansPerDegree;
    float pitch = std::asin(std::clamp(-look.y, -1.0f, 1.0f)) - mount.levelPitch * kRadiansPerDegree;
    pitch = std::clamp(pitch, -limit, limit);
    const bool summoning = mount.state == CloudMountState::Summoning;
    if (summoning)
        pitch = 0.0f;
    const Vec3 forward{std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    // 右は PlayerSystem の歩きと同じ取り方（水平の前を回した向き）。
    const Vec3 right{-std::cos(yaw), 0.0f, std::sin(yaw)};
    Vec3 direction{right.x * mount.input.strafe + forward.x * mount.input.forward, forward.y * mount.input.forward,
                   right.z * mount.input.strafe + forward.z * mount.input.forward};
    const float magnitude = Length(direction);
    if (magnitude > 1.0f)
        direction = {direction.x / magnitude, direction.y / magnitude, direction.z / magnitude};
    const float lift =
        summoning ? 0.0f
                  : static_cast<float>(mount.input.ascend ? 1 : 0) - static_cast<float>(mount.input.descend ? 1 : 0);
    return {direction.x * mount.cruiseSpeed, direction.y * mount.cruiseSpeed + lift * mount.verticalSpeed,
            direction.z * mount.cruiseSpeed};
}

void CloudMountRules::StepVelocity(CloudMountComponent& mount, Vec3 wanted, float dt) {
    const Vec3 gap{wanted.x - mount.velocity.x, wanted.y - mount.velocity.y, wanted.z - mount.velocity.z};
    const float distance = Length(gap);
    const float currentSpeed = Length(mount.velocity);
    const float wantedSpeed = Length(wanted);
    const float seconds = wantedSpeed < currentSpeed ? mount.brakeSeconds : mount.accelSeconds;
    // 最高速は巡航・上下・いまの速さ・欲しい速さの大きい方（斜めに上る時や、速い所から止まる時も、決めた時間で届く）。
    const float reference = std::max({mount.cruiseSpeed, mount.verticalSpeed, currentSpeed, wantedSpeed});
    const float step = seconds > 0.0f ? reference / seconds * dt : distance;
    if (distance <= step || distance <= 0.0f) {
        mount.velocity = wanted;
        return;
    }
    mount.velocity.x += gap.x / distance * step;
    mount.velocity.y += gap.y / distance * step;
    mount.velocity.z += gap.z / distance * step;
}

float CloudMountRules::VerticalSpeed(const CloudMountComponent& mount, float dt) {
    return mount.state == CloudMountState::Summoning ? SummonVerticalSpeed(mount, dt) : mount.velocity.y;
}

float CloudMountRules::GravityForVerticalSpeed(float wanted, float baseVertical, float dt) {
    return dt > 0.0f ? (wanted - baseVertical) / dt : 0.0f;
}

Vec3 CloudMountRules::TurnFacing(Vec3 facing, Vec3 velocity, float degreesPerSecond, float dt) {
    if (std::hypot(velocity.x, velocity.z) < kTurnMinimumSpeed)
        return facing;
    const float current = std::atan2(facing.x, facing.z);
    const float target = std::atan2(velocity.x, velocity.z);
    const float limit = std::max(0.0f, degreesPerSecond) * kRadiansPerDegree * dt;
    const float yaw = current + std::clamp(std::remainder(target - current, kTau), -limit, limit);
    return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

bool CloudMountRules::IsMounted(const CloudMountComponent& mount) { return mount.state != CloudMountState::None; }

const char* CloudMountRules::StateName(CloudMountState state) {
    switch (state) {
    case CloudMountState::None: return "None";
    case CloudMountState::Summoning: return "Summoning";
    case CloudMountState::Riding: return "Riding";
    case CloudMountState::Dismissing: return "Dismissing";
    }
    return "Unknown";
}

} // namespace Project_TD2_2
