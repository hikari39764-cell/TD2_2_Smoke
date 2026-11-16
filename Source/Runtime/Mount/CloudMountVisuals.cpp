#include "CloudMountVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"
#include "../Combat/Feedback.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxParameters.h"
#include "../Player/PlayerRules.h"
#include "CloudMountArrival.h"
#include "CloudMountComponent.h"
#include "CloudMountRules.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;

// 雲のエフェクト資産の入力（資産側の契約。Tools/Nimbus/make_nimbus_cloud.py・make_nimbus_fx.py と同じ名前）。
constexpr const char* kAppearInput = "input.appear";
constexpr const char* kSpeedInput = "input.speed";
constexpr const char* kBoostInput = "input.boost";
constexpr const char* kScatterInput = "input.scatter";
constexpr const char* kTrailInput = "input.trail";
constexpr const char* kSwirlInput = "input.swirl";
constexpr const char* kRingInput = "input.ring";
constexpr const char* kGlowInput = "input.glow";
// 飛行の飾りの資産の入力（Tools/Nimbus/make_nimbus_trims.py と同じ名前）。衝撃の輪は kRingInput。
constexpr const char* kStreakInput = "input.streak";
constexpr const char* kHazeInput = "input.haze";
constexpr const char* kConeInput = "input.cone";
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 上り下りの角度を測る時の水平の速さの下限 (m/s)。止まっている時に角度が飛ばないように。
constexpr float kMinimumHorizontalSpeed = 0.01f;
// 雲の芯は資産の原点（足元）の下 0.3 m（make_nimbus_cloud.py の Core）。着いた・散る飾りはここに出す。
constexpr float kCloudCoreDepth = 0.3f;
// 軌跡の帯は、飛び始めのこのフレーム数だけ消しておく（根が足元から飛び始めの所へ移る線を帯にしない）。
constexpr int kTrailDelayFrames = 2;
// 着いた瞬間のカメラの沈みの送り手。優先度はプレイヤーの手応え（300）と同じ段（揺れは足し合わされる）。
constexpr uint64_t kArriveDipSource = 0x5444324D04ull;
constexpr int kArriveDipPriority = 300;

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

// 向き（+Y 回り）、上り下り（局所 +X 回り。前が下がる向きが正）、旋回の傾き（局所 +Z 回り。右旋回で右＝局所 −X が
// 下がる向きが正）の順に掛ける。
HIKARI::GameProjectQuaternion Orientation(float yaw, float pitch, float bank) {
    return Multiply(Multiply(AxisRotation(1, yaw), AxisRotation(0, pitch)), AxisRotation(2, bank));
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

// 雲の原点（資産の足元）を置く所：体の足元（揺れを除く）。
Vec3 Feet(const CloudMountComponent& mount, Vec3 bodyCenter) {
    return {bodyCenter.x, bodyCenter.y - mount.standOffset - mount.cloudBelowFeet, bodyCenter.z};
}

// 一度きりの飾りを、雲の芯の所に世界の向きのまま出す。
void PlayAccent(HIKARI::WorldView& world, const HIKARI::GameProjectVfxAssetReference& asset, Vec3 root, float scale,
                const HIKARI::GameProjectVfxParameterOverrides& parameters) {
    if (asset.IsEmpty())
        return;
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform.position = {root.x, root.y - kCloudCoreDepth, root.z};
    play.transform.scale = {scale, scale, scale};
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    play.parameters = parameters;
    world.GetVfx().Play(play);
}

void LogEvent(const char* event, const HIKARI::FrameContext& frame, const char* extra) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "%s frame=%llu tick=%llu %s", event,
                  static_cast<unsigned long long>(frame.frameIndex),
                  static_cast<unsigned long long>(frame.fixedTickIndex), extra);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

// 段階の変わり目：乗り始め（乗っていない・解除中から召喚へ）で雲を飛ばし始め、降りた（召喚中・騎乗中から解除・
// 乗っていないへ）時に、雲をその場に残して散らし始める。雲がまだ見えている間に呼び直した時は飛ばさず、その場で湧かせる。
void OnStateChange(HIKARI::WorldView& world, CloudMountComponent& mount, const PlayerComponent& player,
                   const HIKARI::FrameContext& frame) {
    CloudMountArrivalState& state = mount.arrival;
    const bool wasShown = state.lastState == CloudMountState::Summoning || state.lastState == CloudMountState::Riding;
    const bool shown = mount.state == CloudMountState::Summoning || mount.state == CloudMountState::Riding;
    state.lastState = mount.state;
    char extra[192]{};
    if (shown && !wasShown) {
        const bool stillVisible = mount.cloudAppear > 0.0f;
        state.scatterAge = -1.0f;
        if (stillVisible) {
            state.flying = false;
            state.landedAge = -1.0f;
            std::snprintf(extra, sizeof(extra), "fly=0 appear=%.3f", static_cast<double>(mount.cloudAppear));
        } else {
            // 空中（落下の受け止め・持ち上げの無い召喚）では、足元の下へ回り込んで下から受け止める。
            const bool airborne = mount.catching || mount.summonTargetY <= mount.summonStartY;
            CloudMountArrival::Begin(mount, state, PlayerRules::GetFacing(player), airborne);
            std::snprintf(
                extra, sizeof(extra), "fly=1 catch=%d from=(%.2f,%.2f,%.2f) bend=(%.2f,%.2f,%.2f) seconds=%.3f",
                static_cast<int>(state.catching), static_cast<double>(state.from.x), static_cast<double>(state.from.y),
                static_cast<double>(state.from.z), static_cast<double>(state.bend.x), static_cast<double>(state.bend.y),
                static_cast<double>(state.bend.z), static_cast<double>(state.flySeconds));
        }
        LogEvent("CloudArrive", frame, extra);
    } else if (!shown && wasShown) {
        state.flying = false;
        state.landedAge = -1.0f;
        state.scatterAge = 0.0f;
        state.scatterRoot = state.lastTransform;
        PlayAccent(world, mount.disperseEffect, state.scatterRoot.position, 1.0f, {});
        const Vec3& at = state.scatterRoot.position;
        std::snprintf(extra, sizeof(extra), "root=(%.2f,%.2f,%.2f)", static_cast<double>(at.x),
                      static_cast<double>(at.y), static_cast<double>(at.z));
        LogEvent("CloudScatter", frame, extra);
    }
}

// 雲が足元に着いた：沈み・膨らみ・向きの戻しを始め、空気のゆがみの輪と、カメラの小さな沈みを出す。
void Land(HIKARI::WorldView& world, CloudMountComponent& mount, Vec3 root, const HIKARI::FrameContext& frame) {
    CloudMountArrivalState& state = mount.arrival;
    state.flying = false;
    state.landedAge = 0.0f;
    state.sinkDepth = state.catching ? mount.summonCatchSinkMeters : mount.summonSinkMeters;
    HIKARI::GameProjectVfxParameterOverrides accent{};
    const float strength = std::max(0.0f, mount.arriveRingStrength);
    VfxParameters::SetColor(accent, kRingInput, {strength, strength, strength});
    VfxParameters::SetColor(accent, kGlowInput, {1.0f, 1.0f, 1.0f});
    PlayAccent(world, mount.arriveEffect, root, std::max(0.0f, mount.arriveRingScale), accent);
    if (mount.summonCameraDip > 0.0f && mount.summonCameraDipSeconds > 0.0f) {
        Feedback::CameraShakeParams dip{};
        dip.sourceId = kArriveDipSource;
        dip.priority = kArriveDipPriority;
        dip.frames =
            std::max(1, static_cast<int>(std::ceil(mount.summonCameraDipSeconds * Feedback::kFramesPerSecond)));
        dip.kickDistance = mount.summonCameraDip;
        dip.kickDirection = {0.0f, -1.0f, 0.0f};
        Feedback::StartCameraShake(state.cameraDip, dip);
    }
    char extra[96]{};
    std::snprintf(extra, sizeof(extra), "catch=%d sink=%.3f", static_cast<int>(state.catching),
                  static_cast<double>(state.sinkDepth));
    LogEvent("CloudLand", frame, extra);
}

// 飛んで来る間の根：足元から見た道の位置へ置き、進む向き（体の動きを含む、世界の向き）へ向ける。
HIKARI::GameProjectTransform FlyingPlacement(HIKARI::WorldView& world, CloudMountComponent& mount, Vec3 feet, float dt,
                                             const HIKARI::FrameContext& frame) {
    CloudMountArrivalState& state = mount.arrival;
    state.age += dt;
    ++state.flyFrames;
    const float u = CloudMountArrival::Progress(mount, state);
    const Vec3 root = VectorMath::Add(feet, CloudMountArrival::PathOffset(state, u));
    // 向きは、前のフレームからの根の動き（足元が落ちている分を含む）。飛び始めのフレームは道の向き。
    Vec3 direction = CloudMountArrival::PathDirection(state, u);
    if (state.hasLastRoot && state.flyFrames > 1 && dt > 0.0f) {
        const Vec3 moved = VectorMath::Difference(root, state.lastRoot);
        state.rootSpeed = std::sqrt(moved.x * moved.x + moved.y * moved.y + moved.z * moved.z) / dt;
        if (state.rootSpeed > kMinimumHorizontalSpeed)
            direction = moved;
    }
    state.yaw = CloudMountArrival::YawOf(direction);
    state.pitch = CloudMountArrival::PitchOf(direction, mount.cloudPitchMaxDegrees);
    state.trail = state.flyFrames > kTrailDelayFrames ? 1.0f : 0.0f;
    HIKARI::GameProjectTransform transform{};
    transform.position = root;
    transform.rotation = Orientation(state.yaw, state.pitch, 0.0f);
    transform.scale = {1.0f, 1.0f, 1.0f};
    if (state.age >= state.flySeconds)
        Land(world, mount, root, frame);
    return transform;
}

// 足元に置き、体の向き（水平）へ向け、上り下りと旋回の内側へ傾ける。止まっている時だけ上下に揺らす。着いた直後は
// 雲だけ沈めて戻し、向きを飛んで来た向きから戻す。
HIKARI::GameProjectTransform RidingPlacement(CloudMountComponent& mount, const PlayerComponent& player, Vec3 feet,
                                             float dt) {
    CloudMountArrivalState& state = mount.arrival;
    const float horizontal = std::hypot(mount.velocity.x, mount.velocity.z);
    const float idle = 1.0f - std::clamp(horizontal / mount.cloudIdleSpeed, 0.0f, 1.0f);
    const float bob = mount.cloudBobHeight * idle * std::sin(kTau * mount.cloudBobTime / mount.cloudBobSeconds);
    HIKARI::GameProjectTransform transform{};
    transform.position = feet;
    transform.position.y += bob - CloudMountArrival::Sink(mount, state);
    const HIKARI::GameProjectFloat3 facing = PlayerRules::GetFacing(player);
    float yaw = std::atan2(facing.x, facing.z) / kRadiansPerDegree;
    float pitch = mount.cloudPitch;
    if (state.landedAge >= 0.0f) {
        const float settle = CloudMountArrival::Settle(mount, state);
        yaw = CloudMountArrival::BlendDegrees(state.yaw, yaw, settle);
        pitch = state.pitch + (pitch - state.pitch) * settle;
        state.landedAge += dt;
    }
    state.trail = Ramp(state.trail, 0.0f, mount.summonTrailFadeSeconds, dt);
    transform.rotation = Orientation(yaw, pitch, mount.cloudBank);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

// このフレームの根の変換。散っている間は降りた所に留め、飛んで来る間は道の上、ほかは足元。
HIKARI::GameProjectTransform Placement(HIKARI::WorldView& world, CloudMountComponent& mount,
                                       const PlayerComponent& player, Vec3 bodyCenter, float dt,
                                       const HIKARI::FrameContext& frame) {
    CloudMountArrivalState& state = mount.arrival;
    const Vec3 feet = Feet(mount, bodyCenter);
    HIKARI::GameProjectTransform transform{};
    if (state.scatterAge >= 0.0f) {
        state.scatterAge += dt;
        state.trail = 0.0f;
        if (state.scatterAge < mount.disperseHoldSeconds)
            transform = state.scatterRoot;
        else
            state.scatterAge = -1.0f;
    }
    if (state.scatterAge < 0.0f)
        transform =
            state.flying ? FlyingPlacement(world, mount, feet, dt, frame) : RidingPlacement(mount, player, feet, dt);
    state.lastRoot = transform.position;
    state.hasLastRoot = true;
    state.lastTransform = transform;
    return transform;
}

HIKARI::GameProjectVfxParameterOverrides Parameters(const CloudMountComponent& mount) {
    const CloudMountArrivalState& state = mount.arrival;
    const float tail = std::min(mount.flight.speed / mount.cloudTailFullSpeed, mount.cloudTailLimit);
    // 飛んで来る間は雲の量・尾の量・渦を飛来の値にし、着いた後は膨らみの分を足して、渦を余韻に合わせて戻す。
    const float afterglow = state.flying ? 1.0f : CloudMountArrival::Afterglow(mount, state);
    const float amount = state.flying ? mount.summonFlyAmount : 1.0f + mount.summonPuffAmount * afterglow;
    const bool scattering = state.scatterAge >= 0.0f && state.scatterAge < mount.disperseSeconds;
    const float swirl = scattering ? std::max(0.0f, mount.disperseSwirl)
                                   : 1.0f + (std::max(0.0f, mount.summonFlySwirl) - 1.0f) * afterglow;
    const float tailAmount = state.flying ? mount.summonFlyTail : mount.cloudTailAmount * tail;
    const float trail = state.trail * std::max(0.0f, mount.summonTrailAmount);
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAppearInput, mount.cloudAppear * mount.cloudAmount * amount);
    VfxParameters::SetFloat(parameters, kSpeedInput, mount.cloudAppear * tailAmount);
    VfxParameters::SetFloat(parameters, kBoostInput, mount.cloudAppear * mount.cloudBoost);
    VfxParameters::SetBool(parameters, kScatterInput, scattering);
    VfxParameters::SetColor(parameters, kTrailInput, {trail, trail, trail});
    VfxParameters::SetFloat(parameters, kSwirlInput, swirl);
    return parameters;
}

void Trace(const CloudMountComponent& mount, const HIKARI::GameProjectVfxParameterOverrides& parameters,
           const HIKARI::GameProjectTransform& transform, const HIKARI::FrameContext& frame) {
    if (mount.traceEveryFrames <= 0 || frame.frameIndex % static_cast<uint64_t>(mount.traceEveryFrames) != 0)
        return;
    const CloudMountArrivalState& state = mount.arrival;
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "Cloud frame=%llu tick=%llu playing=%d appear=%.3f speedInput=%.3f boostInput=%.3f scatter=%d "
                  "trail=%.2f bank=%.2f pitch=%.2f fly=%d age=%.3f rootSpeed=%.1f landed=%.3f sink=%.3f "
                  "root=(%.2f,%.2f,%.2f) accepted=%d streak=%.3f haze=%.3f cone=%.3f rings=%d trimAccepted=%d",
                  static_cast<unsigned long long>(frame.frameIndex),
                  static_cast<unsigned long long>(frame.fixedTickIndex), static_cast<int>(mount.cloudVfx.IsValid()),
                  static_cast<double>(parameters.values[0].value.numeric[0]),
                  static_cast<double>(parameters.values[1].value.numeric[0]),
                  static_cast<double>(parameters.values[2].value.numeric[0]),
                  static_cast<int>(parameters.values[3].value.boolean),
                  static_cast<double>(parameters.values[4].value.numeric[0]), static_cast<double>(mount.cloudBank),
                  static_cast<double>(mount.cloudPitch), static_cast<int>(state.flying), static_cast<double>(state.age),
                  static_cast<double>(state.rootSpeed), static_cast<double>(state.landedAge),
                  static_cast<double>(CloudMountArrival::Sink(mount, state)), static_cast<double>(transform.position.x),
                  static_cast<double>(transform.position.y), static_cast<double>(transform.position.z),
                  static_cast<int>(mount.cloudAccepted), static_cast<double>(mount.trimStreak),
                  static_cast<double>(mount.trimHaze), static_cast<double>(mount.trimCone), mount.boostRings,
                  static_cast<int>(mount.trimAccepted && mount.trimVfx.IsValid()));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

// 場面の始めに一度だけ、着いた瞬間の飾りを見えない強さ（輪・光・粒とも 0）で足元に出す。画面のゆがみの合成は初めて
// 使うフレームにシェーダーを作るので（headless で約 150〜180 ms）、最初の召喚で止まらないよう、場面の始めに済ませる。
void Prewarm(HIKARI::WorldView& world, CloudMountComponent& mount, Vec3 root) {
    if (mount.arrival.prewarmed || !mount.arrivePrewarm)
        return;
    mount.arrival.prewarmed = true;
    HIKARI::GameProjectVfxParameterOverrides hidden{};
    VfxParameters::SetColor(hidden, kRingInput, {0.0f, 0.0f, 0.0f});
    VfxParameters::SetColor(hidden, kGlowInput, {0.0f, 0.0f, 0.0f});
    PlayAccent(world, mount.arriveEffect, root, 1.0f, hidden);
}

// 速さが start で 0、full で 1 になる割合（full ≤ start の時は start を超えたら 1）。
float SpeedShare(float speed, float start, float full) {
    if (full <= start)
        return speed > start ? 1.0f : 0.0f;
    return std::clamp((speed - start) / (full - start), 0.0f, 1.0f);
}

// 飛行の飾りの量を 1 描画フレーム進める。騎乗中で、飛んで来ても散ってもいない時だけ速さで出す。ブーストの押し始めに
// 体の中心へ衝撃の輪を出す。
void StepTrims(HIKARI::WorldView& world, CloudMountComponent& mount, Vec3 bodyCenter, float dt,
               const HIKARI::FrameContext& frame) {
    const CloudMountFlightReadout& flight = mount.flight;
    const bool riding = mount.state == CloudMountState::Riding && !mount.arrival.flying && mount.arrival.scatterAge < 0.0f;
    const float speed = riding ? flight.speed : 0.0f;
    const float streakShare = SpeedShare(speed, mount.trimStreakStartSpeed, mount.trimStreakFullSpeed);
    const float streak = std::max(0.0f, riding && flight.boosting ? mount.trimStreakBoostAmount * streakShare
                                                                   : mount.trimStreakAmount * streakShare);
    const float haze = std::max(0.0f, mount.trimHazeAmount) *
                       SpeedShare(speed, mount.trimHazeStartSpeed, mount.trimHazeFullSpeed);
    const float cone = std::max(0.0f, mount.trimConeAmount) *
                       SpeedShare(speed, mount.trimConeStartSpeed, mount.trimConeFullSpeed);
    const auto step = [&](float current, float target) {
        return Smooth(current, target, target > current ? mount.trimRiseSeconds : mount.trimFallSeconds, dt);
    };
    mount.trimStreak = step(mount.trimStreak, streak);
    mount.trimHaze = step(mount.trimHaze, haze);
    mount.trimCone = step(mount.trimCone, cone);
    const bool boosting = riding && flight.boosting;
    if (boosting && !mount.trimWasBoosting && !mount.boostRingEffect.IsEmpty()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = mount.boostRingEffect;
        play.transform.position = bodyCenter;
        const float scale = std::max(0.0f, mount.boostRingScale);
        play.transform.scale = {scale, scale, scale};
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        const float strength = std::max(0.0f, mount.boostRingStrength);
        VfxParameters::SetColor(play.parameters, kRingInput, {strength, strength, strength});
        const bool accepted = world.GetVfx().Play(play).IsValid();
        ++mount.boostRings;
        char extra[96]{};
        std::snprintf(extra, sizeof(extra), "speed=%.1f accepted=%d count=%d", static_cast<double>(flight.speed),
                      static_cast<int>(accepted), mount.boostRings);
        LogEvent("BoostRing", frame, extra);
    }
    mount.trimWasBoosting = boosting;
}

HIKARI::GameProjectVfxParameterOverrides TrimParameters(const CloudMountComponent& mount) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetColor(parameters, kStreakInput, {mount.trimStreak, mount.trimStreak, mount.trimStreak});
    // ゆがみの色：R＝強さの倍率、B＝1（貼図の向きを使う）のまま。
    VfxParameters::SetColor(parameters, kHazeInput, {mount.trimHaze, 1.0f, 1.0f});
    VfxParameters::SetColor(parameters, kConeInput, {mount.trimCone, mount.trimCone, mount.trimCone});
    return parameters;
}

// 飛行の飾りを雲の根と同じ変換で出し続ける（場面の作り直しで消えたら出し直す）。
void UpdateTrims(HIKARI::WorldView& world, CloudMountComponent& mount, const HIKARI::GameProjectTransform& transform) {
    if (mount.trimEffect.IsEmpty())
        return;
    const HIKARI::GameProjectVfxParameterOverrides parameters = TrimParameters(mount);
    if (mount.trimVfx.IsValid()) {
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1u;
        update.updateParameters = 1u;
        update.transform = transform;
        update.parameters = parameters;
        mount.trimAccepted = world.GetVfx().Update(mount.trimVfx, update);
        if (!mount.trimAccepted)
            mount.trimVfx = {};
        return;
    }
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = mount.trimEffect;
    play.transform = transform;
    play.parameters = parameters;
    mount.trimVfx = world.GetVfx().Play(play);
    mount.trimAccepted = mount.trimVfx.IsValid();
}

void Stop(HIKARI::WorldView& world, CloudMountComponent& mount) {
    if (mount.cloudVfx.IsValid())
        world.GetVfx().Stop(mount.cloudVfx);
    mount.cloudVfx = {};
    if (mount.trimVfx.IsValid())
        world.GetVfx().Stop(mount.trimVfx);
    mount.trimVfx = {};
}

} // namespace

void CloudMountVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView body, CloudMountComponent& mount,
                               const PlayerComponent& player, const HIKARI::FrameContext& frame) {
    // 雲の流れ（エフェクト）はゲーム時間で進むので、出し入れ・揺れ・飛来もゲーム時間で進める。
    const float dt = std::max(0.0f, frame.gameDt);
    // 段階の変わり目は、出し入れを進める前に見る（呼び直した時に、雲がまだ見えていたかを前のフレームの値で決める）。
    if (mount.state != mount.arrival.lastState)
        OnStateChange(world, mount, player, frame);
    Step(mount, dt);
    // カメラの沈みは、停止中も実時間で戻す。
    Feedback::SubmitCameraShake(world, mount.arrival.cameraDip, player.cameraId, std::max(0.0f, frame.unscaledDt));
    HIKARI::GameProjectTransform bodyTransform{};
    const bool wanted =
        !mount.cloudEffect.IsEmpty() && (mount.cloudKeepAlive || mount.cloudHiddenSeconds < mount.cloudLingerSeconds ||
                                         mount.arrival.scatterAge >= 0.0f);
    if (!wanted || !body.GetWorldTransform(bodyTransform)) {
        Stop(world, mount);
        return;
    }
    const HIKARI::GameProjectTransform transform = Placement(world, mount, player, bodyTransform.position, dt, frame);
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
        Prewarm(world, mount, transform.position);
    }
    StepTrims(world, mount, bodyTransform.position, dt, frame);
    UpdateTrims(world, mount, transform);
    Trace(mount, parameters, transform, frame);
}

} // namespace Project_TD2_2
