#include "CloudMountVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"
#include "../Common/VfxParameters.h"
#include "../Player/PlayerRules.h"
#include "CloudMountComponent.h"
#include "CloudMountRules.h"

namespace Project_TD2_2 {
namespace {

// 雲のエフェクト資産の入力（資産側の契約。Tools/Nimbus/make_nimbus_cloud.py と同じ名前）。
constexpr const char* kAppearInput = "input.appear";
constexpr const char* kSpeedInput = "input.speed";
constexpr const char* kBoostInput = "input.boost";
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 上り下りの角度を測る時の水平の速さの下限 (m/s)。止まっている時に角度が飛ばないように。
constexpr float kMinimumHorizontalSpeed = 0.01f;

HIKARI::GameProjectQuaternion Multiply(HIKARI::GameProjectQuaternion a, HIKARI::GameProjectQuaternion b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// axis（0＝X、1＝Y、2＝Z）回りに degrees 回す。
HIKARI::GameProjectQuaternion AxisRotation(int axis, float degrees) {
    const float half = degrees * kRadiansPerDegree * 0.5f;
    HIKARI::GameProjectQuaternion rotation{0.0f, 0.0f, 0.0f, std::cos(half)};
    (axis == 0 ? rotation.x : axis == 1 ? rotation.y : rotation.z) = std::sin(half);
    return rotation;
}

// target へ seconds の時定数で寄せる（0 は一瞬）。描画フレームの長さに寄らない。
float Smooth(float current, float target, float seconds, float dt) {
    if (seconds <= 0.0f)
        return target;
    return target + (current - target) * std::exp(-dt / seconds);
}

// 一定の速さで target へ寄せる（0〜1 を seconds で動く。0 は一瞬）。
float Ramp(float current, float target, float seconds, float dt) {
    if (seconds <= 0.0f)
        return target;
    const float step = dt / seconds;
    return current < target ? std::min(target, current + step) : std::max(target, current - step);
}

// 進む向きの上り下りの角度（度、下りが正）× 傾ける割合。水平の速さが遅いほど弱める（真上・真下だけの動きでは 0）。
float PathPitch(const CloudMountComponent& mount) {
    const HIKARI::GameProjectFloat3& velocity = mount.velocity;
    const float horizontal = std::hypot(velocity.x, velocity.z);
    const float weight = std::clamp(horizontal / mount.cloudIdleSpeed, 0.0f, 1.0f);
    const float degrees = std::atan2(-velocity.y, std::max(horizontal, kMinimumHorizontalSpeed)) / kRadiansPerDegree;
    const float limit = mount.cloudPitchMaxDegrees;
    return std::clamp(degrees * mount.cloudPitchFollow * weight, -limit, limit);
}

// 出し入れ・吹き出し・傾き・揺れの時間を 1 描画フレーム進める。召喚中と騎乗中は雲を出し、解除中と乗っていない間は消す。
void Step(CloudMountComponent& mount, float dt) {
    const bool shown = mount.state == CloudMountState::Summoning || mount.state == CloudMountState::Riding;
    mount.cloudAppear = Ramp(mount.cloudAppear, shown ? 1.0f : 0.0f,
                             shown ? mount.cloudAppearSeconds : mount.cloudVanishSeconds, dt);
    const CloudMountFlightReadout& flight = mount.flight;
    const float boost = (flight.boosting ? mount.cloudBoostAmount : 0.0f) +
                        flight.boostKick * mount.cloudBoostKickAmount + flight.dive * mount.cloudDiveAmount;
    mount.cloudBoost = Smooth(mount.cloudBoost, shown ? boost : 0.0f, mount.cloudBoostSeconds, dt);
    mount.cloudBank = Smooth(mount.cloudBank, std::clamp(flight.turn, -1.0f, 1.0f) * mount.cloudBankDegrees,
                             mount.cloudBankSeconds, dt);
    mount.cloudPitch = Smooth(mount.cloudPitch, shown ? PathPitch(mount) : 0.0f, mount.cloudPitchSeconds, dt);
    mount.cloudBobTime = std::fmod(mount.cloudBobTime + dt, std::max(mount.cloudBobSeconds, dt));
    mount.cloudHiddenSeconds = mount.cloudAppear > 0.0f ? 0.0f : mount.cloudHiddenSeconds + dt;
}

// 足元に置き、体の向き（水平）へ向け、上り下りと旋回の内側へ傾ける。止まっている時だけ上下に揺らす。
HIKARI::GameProjectTransform Placement(const CloudMountComponent& mount, const PlayerComponent& player,
                                       HIKARI::GameProjectFloat3 bodyCenter) {
    const float horizontal = std::hypot(mount.velocity.x, mount.velocity.z);
    const float idle = 1.0f - std::clamp(horizontal / mount.cloudIdleSpeed, 0.0f, 1.0f);
    const float bob = mount.cloudBobHeight * idle * std::sin(kTau * mount.cloudBobTime / mount.cloudBobSeconds);
    HIKARI::GameProjectTransform transform{};
    transform.position = bodyCenter;
    transform.position.y += bob - mount.standOffset - mount.cloudBelowFeet;
    // 向き（+Y 回り）、上り下り（局所 +X 回り。前が下がる向きが正）、旋回の傾き（局所 +Z 回り。右旋回で右＝局所 −X が
    // 下がる向きが正）の順に掛ける。
    const HIKARI::GameProjectFloat3 facing = PlayerRules::GetFacing(player);
    const float yaw = std::atan2(facing.x, facing.z) / kRadiansPerDegree;
    transform.rotation = Multiply(Multiply(AxisRotation(1, yaw), AxisRotation(0, mount.cloudPitch)),
                                  AxisRotation(2, mount.cloudBank));
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

HIKARI::GameProjectVfxParameterOverrides Parameters(const CloudMountComponent& mount) {
    const float tail = std::min(mount.flight.speed / mount.cloudTailFullSpeed, mount.cloudTailLimit);
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAppearInput, mount.cloudAppear * mount.cloudAmount);
    VfxParameters::SetFloat(parameters, kSpeedInput, mount.cloudAppear * mount.cloudTailAmount * tail);
    VfxParameters::SetFloat(parameters, kBoostInput, mount.cloudAppear * mount.cloudBoost);
    return parameters;
}

void Trace(const CloudMountComponent& mount, const HIKARI::GameProjectVfxParameterOverrides& parameters,
           const HIKARI::FrameContext& frame) {
    if (mount.traceEveryFrames <= 0 || frame.frameIndex % static_cast<uint64_t>(mount.traceEveryFrames) != 0)
        return;
    char message[192]{};
    std::snprintf(message, sizeof(message),
                  "Cloud frame=%llu playing=%d appear=%.3f speedInput=%.3f boostInput=%.3f bank=%.2f pitch=%.2f "
                  "accepted=%d",
                  static_cast<unsigned long long>(frame.frameIndex), static_cast<int>(mount.cloudVfx.IsValid()),
                  static_cast<double>(parameters.values[0].value.numeric[0]),
                  static_cast<double>(parameters.values[1].value.numeric[0]),
                  static_cast<double>(parameters.values[2].value.numeric[0]), static_cast<double>(mount.cloudBank),
                  static_cast<double>(mount.cloudPitch), static_cast<int>(mount.cloudAccepted));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

void Stop(HIKARI::WorldView& world, CloudMountComponent& mount) {
    if (mount.cloudVfx.IsValid())
        world.GetVfx().Stop(mount.cloudVfx);
    mount.cloudVfx = {};
}

} // namespace

void CloudMountVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView body, CloudMountComponent& mount,
                               const PlayerComponent& player, const HIKARI::FrameContext& frame) {
    // 雲の流れ（エフェクト）はゲーム時間で進むので、出し入れ・揺れもゲーム時間で進める。
    Step(mount, std::max(0.0f, frame.gameDt));
    HIKARI::GameProjectTransform bodyTransform{};
    const bool wanted = !mount.cloudEffect.IsEmpty() &&
                        (mount.cloudKeepAlive || mount.cloudHiddenSeconds < mount.cloudLingerSeconds);
    if (!wanted || !body.GetWorldTransform(bodyTransform)) {
        Stop(world, mount);
        return;
    }
    const HIKARI::GameProjectTransform transform = Placement(mount, player, bodyTransform.position);
    const HIKARI::GameProjectVfxParameterOverrides parameters = Parameters(mount);
    if (mount.cloudVfx.IsValid()) {
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1u;
        update.updateParameters = 1u;
        update.transform = transform;
        update.parameters = parameters;
        mount.cloudAccepted = world.GetVfx().Update(mount.cloudVfx, update);
        // 受け付けられない再生（場面の作り直しなどで消えた）は手放し、次のフレームに出し直す。
        if (!mount.cloudAccepted)
            mount.cloudVfx = {};
    } else {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = mount.cloudEffect;
        play.transform = transform;
        play.parameters = parameters;
        mount.cloudVfx = world.GetVfx().Play(play);
        mount.cloudAccepted = mount.cloudVfx.IsValid();
    }
    Trace(mount, parameters, frame);
}

} // namespace Project_TD2_2
