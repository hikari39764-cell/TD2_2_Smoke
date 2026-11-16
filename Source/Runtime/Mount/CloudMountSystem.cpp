#include "CloudMountSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "../Combat/Feedback.h"
#include "../Combat/FeedbackCamera.h"
#include "../Player/PlayerRules.h"
#include "CloudMountCamera.h"
#include "CloudMountComponent.h"
#include "CloudMountRules.h"
#include "CloudMountVisuals.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。
constexpr uint64_t kMotionSource = 0x5444324D01ull;
// プレイヤーの移動（300）より上。
constexpr int kMotionPriority = 310;
// ブースト中の震えと動きのぼかしの送り手。優先度はプレイヤーの手応え（300）と同じ段（揺れは足し合わされ、ぼかしは
// この送り手だけ）。
constexpr uint64_t kShakeSource = 0x5444324D02ull;
constexpr uint64_t kBlurSource = 0x5444324D03ull;
constexpr int kCameraEffectPriority = 300;
// 動きのぼかしの、エンジンが受け付ける範囲（シャッター角は度、ぼかしの長さは 1080p の画素）。
constexpr float kMaximumShutterDegrees = 360.0f;
constexpr float kMaximumBlurPixels = 64.0f;
// 震えを続ける間、弱まりを割り戻す時の包絡の下限（割り算が大きくなりすぎないように）。
constexpr float kMinimumShakeEnvelope = 0.05f;
// 騎乗中は毎 tick ソルバーの跳躍として出す時の初速 (m/s)。初速は同じ tick の重力の差し替えで打ち消すので、
// 正の値なら何でもよい（縦の速さは変わらない）。
constexpr float kRideJumpSpeed = 1.0f;
// 提出する落下の速さの上限を、下降の速さよりこれだけ大きくする (m/s)。上限で縦の速さが切られないようにする。
constexpr float kFallSpeedMargin = 1.0f;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

HIKARI::GameProjectInputActionState ReadAction(HIKARI::InputView input, std::string_view id) {
    HIKARI::GameProjectInputActionState action{};
    if (!input.GetAction(id, action))
        return {};
    return action;
}

// CloudMount と、同じ物体の Player の組ごとに callback(object, mount, player) を呼ぶ。Player が無ければ何もしない。
template <class Callback>
void ForEachRider(HIKARI::WorldView& world, Callback&& callback) {
    world.ForEachComponent<CloudMountComponent>(
        HIKARI::MakeComponentTypeId(kCloudMountType), [&](HIKARI::ObjectView object, CloudMountComponent& mount) {
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView owner, PlayerComponent& player) {
                                                        if (owner.GetHandle() == object.GetHandle())
                                                            callback(object, mount, player);
                                                    });
        });
}

void Log(HIKARI::ObjectView object, const CloudMountComponent& mount, const char* event, uint64_t tick,
         const char* extra = "") {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    char message[256]{};
    std::snprintf(message, sizeof(message), "%s tick=%llu state=%s position=(%.3f,%.3f,%.3f) %s", event,
                  static_cast<unsigned long long>(tick), CloudMountRules::StateName(mount.state),
                  static_cast<double>(transform.position.x), static_cast<double>(transform.position.y),
                  static_cast<double>(transform.position.z), extra);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

// 足元の地面を真下へ探し、見つかれば高さを返す。自分の体は除く。
bool FindGroundBelow(HIKARI::WorldView& world, HIKARI::ObjectView object, Vec3 center, float reach, float& outY) {
    HIKARI::GameProjectPhysicsRaycastQuery query{};
    query.origin = center;
    query.direction = {0.0f, -1.0f, 0.0f};
    query.maxDistance = reach;
    query.filter.ignoredObjectHandle = object.GetHandle().value;
    query.filter.includeTriggers = 0;
    HIKARI::GameProjectPhysicsHit hit{};
    if (reach <= 0.0f || !world.GetPhysicsQueries().Raycast(query, hit))
        return false;
    outY = hit.position.y;
    return true;
}

// 召喚を始める（乗っていない時と、解除中に押し直した時）。持ち上げる高さは足元の地面から決める。
void StartSummon(HIKARI::WorldView& world, HIKARI::ObjectView object, CloudMountComponent& mount, uint64_t tick) {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    float groundY = 0.0f;
    const bool hasGround = FindGroundBelow(world, object, transform.position,
                                           mount.standOffset + std::max(0.0f, mount.summonGroundReach), groundY);
    CloudMountRules::BeginSummon(mount, transform.position.y, hasGround, groundY);
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "from=%.3f target=%.3f ground=%d groundY=%.3f standOffset=%.3f",
                  static_cast<double>(mount.summonStartY), static_cast<double>(mount.summonTargetY),
                  static_cast<int>(hasGround), static_cast<double>(groundY), static_cast<double>(mount.standOffset));
    Log(object, mount, "Summon", tick, extra);
}

// Mount が押された：乗る・降りるを切り替える。
void Toggle(HIKARI::WorldView& world, HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player,
            uint64_t tick) {
    const CloudMountState next = CloudMountRules::NextOnToggle(mount.state);
    if (next == CloudMountState::Dismissing) {
        CloudMountRules::Enter(mount, CloudMountState::Dismissing);
        Log(object, mount, "Dismiss", tick);
        return;
    }
    if (mount.state == CloudMountState::None) {
        // 歩いていた横の勢いは残して飛び始める（乗る時に歩きの速度は消える）。
        const Vec3 walk = player.moveVelocity;
        if (!PlayerRules::BeginRide(player)) {
            char extra[64]{};
            std::snprintf(extra, sizeof(extra), "playerState=%d", static_cast<int>(player.state));
            Log(object, mount, "MountRefused", tick, extra);
            return;
        }
        mount.velocity = {walk.x, 0.0f, walk.z};
    }
    StartSummon(world, object, mount, tick);
}

// 落下を受け止める：乗っていない時、足場を離れて catchFallMeters 落ちたら自動で召喚する。プレイヤーが通常の状態で
// なければ（吹っ飛びで縁から落ちた時の吹っ飛び・ダウン・起き上がりの間など）乗れないので、次の tick にまた試す。
void TryCatch(HIKARI::WorldView& world, HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player,
              uint64_t tick) {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    if (!CloudMountRules::ShouldCatch(mount, transform.position.y))
        return;
    HIKARI::GameProjectKinematicState state{};
    const float fallSpeed =
        world.GetMotion().GetKinematicState(object.GetHandle().value, state) ? std::max(0.0f, -state.velocity.y) : 0.0f;
    // 空中の横の勢いは残して受け止める（乗る時に歩きの速度は消える）。
    const Vec3 drift = player.moveVelocity;
    if (!PlayerRules::BeginRide(player))
        return;
    const float fallen = mount.fallPeakY - transform.position.y;
    CloudMountRules::BeginCatch(mount, transform.position.y, fallSpeed, drift);
    char extra[128]{};
    std::snprintf(extra, sizeof(extra), "fallen=%.3f fallSpeed=%.3f brake=%.3f", static_cast<double>(fallen),
                  static_cast<double>(fallSpeed), static_cast<double>(mount.catchBrakeLeft));
    Log(object, mount, "Catch", tick, extra);
}

// 乗る・降りるを決める。PlayerSystem の PreFixedUpdate（状態の F を進める・先行入力）の後、FixedUpdate の前。
void StepState(HIKARI::WorldView& world, HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player,
               uint64_t tick) {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    // 被弾・死亡で Ride でなくなった：雲は消え、いつもの被弾の動きは PlayerSystem が出す。この後の落下は受け止める。
    if (CloudMountRules::IsMounted(mount) && !PlayerRules::IsRiding(player)) {
        char extra[64]{};
        std::snprintf(extra, sizeof(extra), "playerState=%d", static_cast<int>(player.state));
        CloudMountRules::Enter(mount, CloudMountState::None);
        CloudMountRules::OnDismounted(mount, transform.position.y, false);
        Log(object, mount, "Interrupted", tick, extra);
    }
    const CloudMountState next = CloudMountRules::TimedNext(mount);
    if (next != mount.state) {
        CloudMountRules::Enter(mount, next);
        if (next == CloudMountState::None) {
            // 降りた：この tick から PlayerSystem がいつもの重力で動かす。
            PlayerRules::EndRide(player);
            CloudMountRules::OnDismounted(mount, transform.position.y, true);
            Log(object, mount, "End", tick);
        } else {
            Log(object, mount, "Ride", tick);
        }
    }
    if (mount.mountPressed) {
        mount.mountPressed = false;
        Toggle(world, object, mount, player, tick);
    }
    TryCatch(world, object, mount, player, tick);
}

void Trace(HIKARI::ObjectView object, const CloudMountComponent& mount, const HIKARI::GameProjectKinematicState& state,
           uint64_t tick) {
    if (mount.traceEveryFrames <= 0 || tick % static_cast<uint64_t>(mount.traceEveryFrames) != 0)
        return;
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    char message[512]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu state=%s frame=%d position=(%.4f,%.4f,%.4f) velocity=(%.4f,%.4f,%.4f) "
        "solverVelocityY=%.4f vertical=%.4f gravity=%.3f grounded=%d groundNormalY=%.3f hitWall=%d maxHits=%d "
        "accepted=%d speed=%.4f boost=%d kick=%.3f turn=%.4f dive=%.4f overspeed=%.4f achieved=%.4f exhausted=%d "
        "skim=%d",
        static_cast<unsigned long long>(tick), CloudMountRules::StateName(mount.state), mount.stateFrame,
        static_cast<double>(transform.position.x), static_cast<double>(transform.position.y),
        static_cast<double>(transform.position.z), static_cast<double>(mount.velocity.x),
        static_cast<double>(mount.velocity.y), static_cast<double>(mount.velocity.z),
        static_cast<double>(state.velocity.y), static_cast<double>(mount.submittedVertical),
        static_cast<double>(mount.submittedGravity), static_cast<int>(state.grounded),
        static_cast<double>(state.groundNormal.y), static_cast<int>(state.hitWall),
        static_cast<int>(state.maximumHitsExceeded), static_cast<int>(mount.motionAccepted),
        static_cast<double>(mount.flight.speed), static_cast<int>(mount.flight.boosting),
        static_cast<double>(mount.flight.boostKick), static_cast<double>(mount.flight.turn),
        static_cast<double>(mount.flight.dive), static_cast<double>(mount.flight.overspeed),
        static_cast<double>(mount.achievedSpeed), static_cast<int>(mount.boostExhausted),
        static_cast<int>(mount.flight.groundSkim));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

// 乗っていない間：立った時の体の中心から足元までの高さを測る（召喚の高さと雲の位置に使う）。
void MeasureStandOffset(CloudMountComponent& mount, const HIKARI::GameProjectKinematicState& state) {
    if (state.groundState != HIKARI::GameProjectGroundState::OnGround)
        return;
    const float offset = state.pose.position.y - state.groundPosition.y;
    if (std::isfinite(offset) && offset > 0.0f)
        mount.standOffset = offset;
}

// ブーストの 1 固定 tick：精力の残りでブーストできるかを決め、ブースト中は精力を使う（PlayerRules を通す）。
// 尽きた tick まではブーストし、入力を離すまでブーストしない（走りと同じ決まり）。
void StepBoost(HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player,
               const HIKARI::FrameContext& frame) {
    const bool unlimited = mount.boostStaminaPerSecond <= 0.0f;
    const bool changed = CloudMountRules::StepBoost(mount, unlimited || PlayerRules::GetStamina(player) > 0.0f);
    if (mount.flight.boosting && !unlimited &&
        !PlayerRules::DrainStamina(player, mount.boostStaminaPerSecond, frame.fixedDt))
        CloudMountRules::ExhaustBoost(mount);
    if (!changed)
        return;
    char extra[96]{};
    std::snprintf(extra, sizeof(extra), "stamina=%.3f exhausted=%d",
                  static_cast<double>(PlayerRules::GetStamina(player)), static_cast<int>(mount.boostExhausted));
    Log(object, mount, mount.flight.boosting ? "BoostStart" : "BoostEnd", frame.fixedTickIndex, extra);
}

// 騎乗中の 1 固定 tick の運動。縦の速さは、跳躍の初速とこの tick の重力の差し替えで決める（ソルバーは提出した速度の
// 縦を使わない）。
void StepFlight(HIKARI::WorldView& world, HIKARI::ObjectView object, CloudMountComponent& mount,
                PlayerComponent& player, const HIKARI::FrameContext& frame) {
    HIKARI::GameProjectKinematicState state{};
    const bool hasState = world.GetMotion().GetKinematicState(object.GetHandle().value, state);
    const float dt = frame.fixedDt;
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    CloudMountRules::RecordPosition(mount, transform.position, dt);
    if (!CloudMountRules::IsMounted(mount)) {
        if (hasState) {
            MeasureStandOffset(mount, state);
            CloudMountRules::TrackFall(mount, state.grounded != 0u, transform.position.y);
        }
        return;
    }
    if (!PlayerRules::IsRiding(player))
        return;
    StepBoost(object, mount, player, frame);
    CloudMountRules::StepFlight(mount, PlayerRules::GetCameraLook(player), transform.position, dt);
    PlayerRules::FaceDirection(
        player, CloudMountRules::TurnFacing(PlayerRules::GetFacing(player), mount.velocity, mount.turnSpeed, dt));
    // 毎 tick 跳躍として出すので、ソルバーが重力を足す前の縦の速さは「初速＋（地面に着いていれば）地面の縦の速さ」。
    // 前の tick の速さには寄らない。
    const bool grounded = hasState && state.grounded != 0u;
    const float vertical =
        CloudMountRules::ApplyGroundSkim(mount, CloudMountRules::VerticalSpeed(mount, dt), grounded, dt);
    const float baseVertical = kRideJumpSpeed + (grounded ? state.groundVelocity.y : 0.0f);
    const float gravity = CloudMountRules::GravityForVerticalSpeed(vertical, baseVertical, dt);

    HIKARI::GameProjectKinematicMove move{};
    move.objectHandle = object.GetHandle().value;
    move.sourceId = kMotionSource;
    move.fixedTickIndex = frame.fixedTickIndex;
    move.priority = kMotionPriority;
    move.horizontalVelocity = {mount.velocity.x, 0.0f, mount.velocity.z};
    move.maximumFallSpeed = std::max(player.maximumFallSpeed, -vertical + kFallSpeedMargin);
    const Vec3 facing = PlayerRules::GetFacing(player);
    const float halfYaw = std::atan2(facing.x, facing.z) * 0.5f;
    move.hasDesiredRotation = 1;
    move.desiredRotation = {0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
    // 跳躍の間は、地面の上を歩く時の決まり（段差を上る・床へ吸い付く・縁から落ちない）が切れる。地面すれすれを
    // 飛んでも崖の縁で止められず、そのまま外へ飛び出せる。地形・物との当たりはそのまま。
    move.jumpRequested = 1;
    move.jumpSpeed = kRideJumpSpeed;
    move.allowJumpWithoutGroundContact = 1;
    HIKARI::GameProjectKinematicMoveOptions options{};
    options.overrideVerticalGravity = 1;
    options.verticalGravityAcceleration = gravity;
    mount.motionAccepted = world.GetMotion().SubmitKinematicMoveWithOptions(move, options);
    mount.submittedVertical = vertical;
    mount.submittedGravity = gravity;
    Trace(object, mount, state, frame.fixedTickIndex);
    CloudMountRules::AdvanceFrame(mount);
}

// ブースト中だけの細かい震え（Combat/Feedback の揺れ）。続ける間は、位相が飛ばないよう 1 周期ずつ時間を巻き戻し、
// 弱まった分を揺れ幅で割り戻して一定に保つ。離すと、そのまま cameraBoostShakeFadeSeconds で消えていく。
// 出した揺れのずれの大きさ (m) を返す（出していなければ 0）。
float StepBoostShake(HIKARI::WorldView& world, CloudMountComponent& mount, const PlayerComponent& player, float dt) {
    Feedback::CameraShake& shake = mount.cameraShake;
    if (mount.flight.boosting && mount.cameraBoostShake > 0.0f && mount.cameraBoostShakeFrequency > 0.0f) {
        const float period = 1.0f / mount.cameraBoostShakeFrequency;
        // 1 周期より短いと、巻き戻す前に消えてしまう。
        const float fade = std::max(mount.cameraBoostShakeFadeSeconds, period);
        if (!shake.active) {
            Feedback::CameraShakeParams params{};
            params.sourceId = kShakeSource;
            params.priority = kCameraEffectPriority;
            params.frames = std::max(1, static_cast<int>(std::ceil(fade * Feedback::kFramesPerSecond)));
            params.amplitude = mount.cameraBoostShake;
            params.frequency = mount.cameraBoostShakeFrequency;
            Feedback::StartCameraShake(shake, params);
        }
        shake.ageSeconds = std::fmod(shake.ageSeconds, period);
        const float envelope = 1.0f - shake.ageSeconds / shake.durationSeconds;
        shake.params.amplitude = mount.cameraBoostShake / std::max(envelope * envelope, kMinimumShakeEnvelope);
        shake.params.frequency = mount.cameraBoostShakeFrequency;
    }
    HIKARI::GameProjectFloat3 offset{};
    if (!Feedback::SubmitCameraShake(world, shake, player.cameraId, dt, &offset))
        return 0.0f;
    return std::hypot(offset.x, offset.y);
}

// 動きのぼかしと震え。姿勢には触れず（姿勢は PlayerCamera が出す）、その上に掛ける。降りた後も消え終わるまで出す。
void SubmitCameraEffects(HIKARI::WorldView& world, CloudMountComponent& mount, const PlayerComponent& player,
                         const HIKARI::FrameContext& frame) {
    const float dt = std::max(0.0f, frame.unscaledDt);
    const float weight = CloudMountCamera::StepBlur(mount, dt);
    bool blurAccepted = false;
    if (weight > 0.0f && mount.cameraBlurShutterDegrees > 0.0f) {
        HIKARI::GameProjectCameraMotionBlurRequest blur{};
        blur.cameraObjectHandle = FeedbackCamera::Resolve(world, player.cameraId);
        blur.sourceId = kBlurSource;
        blur.priority = kCameraEffectPriority;
        // 重みはカメラの元の設定（無効なら 0°）からのシャッター角と長さの補間。
        blur.weight = weight;
        blur.settings.enabled = 1;
        blur.settings.shutterAngleDegrees = std::clamp(mount.cameraBlurShutterDegrees, 0.0f, kMaximumShutterDegrees);
        blur.settings.maxBlur1080pPixels = std::clamp(mount.cameraBlurMaxPixels, 0.0f, kMaximumBlurPixels);
        blurAccepted = blur.cameraObjectHandle != 0 && world.GetCamera().SubmitMotionBlur(blur);
    }
    const float shake = StepBoostShake(world, mount, player, dt);
    if (mount.traceEveryFrames <= 0 || (weight <= 0.0f && shake <= 0.0f))
        return;
    char message[160]{};
    std::snprintf(message, sizeof(message), "CameraFx frame=%llu blur=%.4f blurAccepted=%d shake=%.5f",
                  static_cast<unsigned long long>(frame.frameIndex), static_cast<double>(weight),
                  static_cast<int>(blurAccepted), static_cast<double>(shake));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "CloudMount", message);
}

} // namespace

void CloudMountSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    const auto input = world.GetInput();
    ForEachRider(world, [&](HIKARI::ObjectView, CloudMountComponent& mount, PlayerComponent& player) {
        if (ReadAction(input, "Gameplay.Mount").pressed)
            mount.mountPressed = true;
        const auto move = ReadAction(input, "Gameplay.Move");
        mount.input.strafe = std::isfinite(move.x) ? move.x : 0.0f;
        mount.input.forward = std::isfinite(move.y) ? move.y : 0.0f;
        mount.input.ascend = ReadAction(input, "Gameplay.Jump").down != 0u;
        mount.input.descend = ReadAction(input, "Gameplay.Descend").down != 0u;
        mount.input.boost = ReadAction(input, "Gameplay.Sprint").down != 0u;
        // 騎乗中のカメラ：上下の遊びを無くして素早く追い、見上げ・見下ろしを広げ、速さで引く・画角・傾きなどの見せ方を
        // 要求する。同じフレームの LateUpdate（PlayerCamera）が使う。降りると要求をやめ、PlayerCamera が普段へ戻す。
        if (CloudMountRules::IsMounted(mount))
            PlayerRules::RequestCamera(player, CloudMountCamera::BuildRequest(mount, std::max(0.0f, frame.unscaledDt)));
        else
            CloudMountCamera::Reset(mount);
    });
}

void CloudMountSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    ForEachRider(world, [&](HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player) {
        StepState(world, object, mount, player, frame.fixedTickIndex);
    });
}

void CloudMountSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    ForEachRider(world, [&](HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player) {
        StepFlight(world, object, mount, player, frame);
    });
}

void CloudMountSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    ForEachRider(world, [&](HIKARI::ObjectView object, CloudMountComponent& mount, PlayerComponent& player) {
        CloudMountVisuals::Update(world, object, mount, player, frame);
        SubmitCameraEffects(world, mount, player, frame);
    });
}

} // namespace Project_TD2_2
