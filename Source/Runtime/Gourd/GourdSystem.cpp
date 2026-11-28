#include "GourdSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "GourdAbsorbField.h"
#include "GourdAbsorbVisuals.h"
#include "GourdRules.h"
#include "../Common/ComponentLookup.h"
#include "../Common/GameSession.h"
#include "../Common/TraceInterval.h"
#include "../Common/VectorMath.h"
#include "../Player/PlayerRules.h"
#include "HIKARI/Extend/World.h"

namespace Project_TD2_2 {
namespace {

using ComponentLookup::ForEachOnObject;

using VectorMath::Difference;
using VectorMath::Normalize;
using Vec3 = HIKARI::GameProjectFloat3;
// 外形の基準は正式シーンのメッシュ契約。
constexpr float kMeshHeight = 0.5f;
constexpr float kMeshRadius = 0.18f;
using VectorMath::kTau;
// 吸えている速さをならす時間 (秒)。回読は 1〜3 フレーム遅れて塊で届くので、そのままでは演出がちらつく。
constexpr float kIntakeSmoothSeconds = 0.2f;

using GameSession::IsPlaying;

// カメラの正面（水平）を out に書く。読めなければ false（out は変えない）。
bool CameraForward(HIKARI::WorldView& world, const Gourd& gourd, Vec3& out) {
    HIKARI::GameProjectCameraPose pose{};
    if (world.GetCamera().GetControlPose(pose)) {
        out = PlayerRules::FlatNormalize(Difference(pose.target, pose.eye), out);
        return true;
    }
    const auto camera = HIKARI::Extend::ObjectOf(gourd.camera);
    HIKARI::GameProjectTransform transform{};
    if (camera.IsValid() && camera.GetWorldTransform(transform)) {
        const auto q = transform.rotation;
        out = PlayerRules::FlatNormalize({2.0f * (q.x * q.z + q.w * q.y), 0.0f, 1.0f - 2.0f * (q.x * q.x + q.y * q.y)},
                                         out);
        return true;
    }
    return false;
}

// 口の向きの元になる水平の向き：吸い込み（構え・後隙も）はプレイヤーの向き、ロック中は対象へ、ほかはカメラの正面。
Vec3 Horizontal(Vec3 from, Vec3 facing, Vec3 camera, bool absorbPose, bool hasLock, Vec3 lockPosition) {
    if (absorbPose)
        return facing;
    if (hasLock)
        return PlayerRules::FlatNormalize(Difference(lockPosition, from), facing);
    return camera;
}

template <class Callback>
bool WithPlayer(const Gourd& gourd, Callback&& callback) {
    const auto playerObject = HIKARI::Extend::ObjectOf(gourd.player);
    if (!playerObject.IsValid())
        return false;
    bool found = false;
    ForEachOnObject<Player>(playerObject, [&](HIKARI::ObjectView object, Player& player) {
        found = true;
        callback(object, player);
    });
    return found;
}

void EnterAbsorbPhase(Gourd& gourd, GourdAbsorbPhase phase) {
    gourd.absorbPhase = phase;
    gourd.absorbPhaseFrames = 0;
}

// 後隙が 0 F なら、吸い終わった F から動ける（後隙の段階を 1 F も挟まない）。
void FinishAbsorb(Gourd& gourd) {
    EnterAbsorbPhase(gourd, gourd.absorbRecoveryFrames > 0 ? GourdAbsorbPhase::Recovery : GourdAbsorbPhase::Idle);
}

// 固定 tick ごとに一度だけ進める。構え → 吸う → 後隙。どの段階も動作ロックは掛けず、歩きを遅くするだけにする
// （ロックするとローリングで打ち切れない）。ローリング・被弾などで通常の姿勢でなくなったら、後隙なしで打ち切る。
void StepAbsorb(Gourd& gourd) {
    gourd.playerValid = WithPlayer(gourd, [&](HIKARI::ObjectView, Player& player) {
        // ローリングの移動が終わった後なら、吸い込み（押し続け・先行入力）でローリングの残りを打ち切って構えへ移る。
        if (gourd.enabled && gourd.absorbPhase == GourdAbsorbPhase::Idle && GourdRules::WantsAbsorb(gourd))
            PlayerRules::TryEndRollForAction(player);
        // 演出の間（ボスの登場）は使えない。
        const bool usable = gourd.enabled && PlayerRules::IsAlive(player) &&
                            PlayerRules::GetState(player) == PlayerState::Normal &&
                            !PlayerRules::IsCinematicHeld(player);
        if (!usable)
            EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
        switch (gourd.absorbPhase) {
        case GourdAbsorbPhase::Idle:
            // 砲撃の後隙などの動作ロック中は始めない。先行入力が残っていれば、ロックが明けた F に始まる。
            if (usable && GourdRules::WantsAbsorb(gourd) && PlayerRules::CanStartAction(player)) {
                gourd.absorbBuffer = 0;
                EnterAbsorbPhase(gourd, gourd.absorbWindupFrames > 0 ? GourdAbsorbPhase::Windup
                                                                      : GourdAbsorbPhase::Absorbing);
            }
            break;
        case GourdAbsorbPhase::Windup:
            // 軽く押しただけでも最低の長さは吸うので、離していても吸う段階へ進む。
            if (++gourd.absorbPhaseFrames >= gourd.absorbWindupFrames) {
                if (gourd.absorbHeld || gourd.absorbMinFrames > 0)
                    EnterAbsorbPhase(gourd, GourdAbsorbPhase::Absorbing);
                else
                    FinishAbsorb(gourd);
            }
            break;
        case GourdAbsorbPhase::Absorbing:
            // 増やした後の値は、前の F までに吸った F 数（入った F から数える）。
            // 最低の長さを吸い終えて、離していれば止める。
            if (++gourd.absorbPhaseFrames >= gourd.absorbMinFrames && !gourd.absorbHeld)
                FinishAbsorb(gourd);
            break;
        case GourdAbsorbPhase::Recovery:
            if (++gourd.absorbPhaseFrames >= gourd.absorbRecoveryFrames)
                EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
            break;
        }
        if (gourd.absorbPhase != GourdAbsorbPhase::Idle) {
            // 歩きは遅くなるが、入力した方向へ向きは変えられる。吸う向きはこの向きについて行く。
            PlayerRules::SetMovementSpeedScale(player, gourd.absorbMoveScale);
            PlayerRules::SetTurnOverride(player, gourd.absorbTurnSpeed);
        }
        if (GourdRules::IsAbsorbCommitted(gourd))
            PlayerRules::BlockHeal(player);
    });
    if (!gourd.playerValid)
        EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
    gourd.absorbing = gourd.absorbPhase == GourdAbsorbPhase::Absorbing;
    // 砲撃（実行順が前）も同じ tick に先行入力を見るので、使い終わってから減らす。
    gourd.absorbBuffer = std::max(0, gourd.absorbBuffer - 1);
}

// 満杯で吸えない「入らない」の tick に、ヒョウタンを口の向きと逆へ小さく跳ねさせ、押し戻す噴きを始める
// （跳ねは同じ tick の StepRecoil が一歩目を進める。振動は GourdAbsorbVisuals が回数の差を見て出す）。
void StepBlocked(Gourd& gourd, float dt) {
    gourd.blockedPuffLeft = std::max(0, gourd.blockedPuffLeft - 1);
    if (!GourdRules::StepBlocked(gourd, dt))
        return;
    GourdRules::AddRecoil(gourd, VectorMath::Scale(gourd.aimDirection, -1.0f), gourd.blockedKickDistance,
                          gourd.blockedKickFrames);
    gourd.blockedPuffLeft = std::max(0, gourd.blockedPuffFrames);
}

void SpringAxis(float& value, float& velocity, float target, float frequency, float dt) {
    // 臨界減衰の解析解なので、描画のフレーム間隔が変わってもばねが発散しない。
    const float offset = value - target;
    const float impulse = (velocity + frequency * offset) * dt;
    const float decay = std::exp(-frequency * dt);
    value = target + (offset + impulse) * decay;
    velocity = (velocity - frequency * impulse) * decay;
}

void StepRecoil(Gourd& gourd, float dt) {
    if (gourd.recoilFramesLeft > 0) {
        gourd.recoilOffset.x += gourd.recoilPerFrame.x;
        gourd.recoilOffset.y += gourd.recoilPerFrame.y;
        gourd.recoilOffset.z += gourd.recoilPerFrame.z;
        --gourd.recoilFramesLeft;
        return;
    }
    // 押し戻す区間は固定 F、回位は既存の追従速度を使った臨界減衰にする。
    SpringAxis(gourd.recoilOffset.x, gourd.recoilVelocity.x, 0.0f, gourd.followFrequency, dt);
    SpringAxis(gourd.recoilOffset.y, gourd.recoilVelocity.y, 0.0f, gourd.followFrequency, dt);
    SpringAxis(gourd.recoilOffset.z, gourd.recoilVelocity.z, 0.0f, gourd.followFrequency, dt);
}

void Follow(HIKARI::WorldView& world, HIKARI::ObjectView object, Gourd& gourd, float dt, float alpha) {
    WithPlayer(gourd, [&](HIKARI::ObjectView playerObject, Player& player) {
        HIKARI::GameProjectTransform playerTransform{};
        if (!playerObject.GetWorldTransform(playerTransform))
            return;
        // 体が描かれる位置（tick の間の補間）を追う（tick ごとの段の位置を追うと、速く動く時にひょうたんががくつく）。
        playerTransform.position = PlayerRules::PresentedPosition(player, playerTransform.position, alpha);
        const auto facing = PlayerRules::GetFacing(player);
        const Vec3 right{facing.z, 0.0f, -facing.x};
        const auto offset = gourd.shoulderOffset;
        const Vec3 desired{playerTransform.position.x + right.x * offset.x + facing.x * offset.z,
                           playerTransform.position.y + offset.y,
                           playerTransform.position.z + right.z * offset.x + facing.z * offset.z};
        const auto error = Difference(desired, gourd.followPosition);
        const float distanceSquared = error.x * error.x + error.y * error.y + error.z * error.z;
        if (!gourd.poseInitialized || distanceSquared > gourd.teleportDistance * gourd.teleportDistance) {
            gourd.followPosition = desired;
            gourd.followVelocity = {};
            gourd.poseInitialized = true;
        } else {
            SpringAxis(gourd.followPosition.x, gourd.followVelocity.x, desired.x, gourd.followFrequency, dt);
            SpringAxis(gourd.followPosition.y, gourd.followVelocity.y, desired.y, gourd.followFrequency, dt);
            SpringAxis(gourd.followPosition.z, gourd.followVelocity.z, desired.z, gourd.followFrequency, dt);
        }
        // 描画のヒョウタン（見た目と流体の注入だけ。規則の置き場所は StepRulePose）。
        gourd.drawPosition = {gourd.followPosition.x + gourd.recoilOffset.x,
                              gourd.followPosition.y + gourd.recoilOffset.y,
                              gourd.followPosition.z + gourd.recoilOffset.z};
        Vec3 camera = facing;
        if (!CameraForward(world, gourd, camera))
            camera = facing;
        const auto lockTarget = world.FindObject(PlayerRules::GetLockTarget(player));
        HIKARI::GameProjectTransform targetTransform{};
        const bool hasLock = lockTarget.IsValid() && lockTarget.GetWorldTransform(targetTransform);
        // 吸い込み（構え・後隙も）の口は、カメラやロック対象ではなくプレイヤーの向きへ向ける。
        const bool absorbPose = gourd.absorbPhase != GourdAbsorbPhase::Idle;
        const Vec3 horizontal =
            Horizontal(gourd.drawPosition, facing, camera, absorbPose, hasLock, targetTransform.position);
        gourd.drawSinkPosition = {gourd.drawPosition.x + horizontal.x * gourd.absorbForwardDistance,
                                  playerTransform.position.y + gourd.absorbHeightOffset,
                                  gourd.drawPosition.z + horizontal.z * gourd.absorbForwardDistance};
        gourd.drawAimDirection =
            gourd.absorbing            ? Normalize(Difference(gourd.drawSinkPosition, gourd.drawPosition), horizontal)
            : absorbPose               ? horizontal
            : gourd.aimOverridePending ? gourd.aimOverrideDirection
            : hasLock                  ? Normalize(Difference(targetTransform.position, gourd.drawPosition), horizontal)
                                       : horizontal;
        gourd.drawMouthPosition = {gourd.drawPosition.x + gourd.drawAimDirection.x * gourd.height * 0.5f,
                                   gourd.drawPosition.y + gourd.drawAimDirection.y * gourd.height * 0.5f,
                                   gourd.drawPosition.z + gourd.drawAimDirection.z * gourd.height * 0.5f};
        HIKARI::GameProjectTransform transform{};
        transform.position = gourd.drawPosition;
        transform.rotation = VectorMath::RotationFromYAxis(gourd.drawAimDirection);
        // ボスの斬撃を吸い取った時は、飲み込んだように一度大きく膨らんで戻る（止めの間は時間が遅いので、膨らんだまま見える）。
        if (gourd.catchPulseObserved != gourd.catchCount) {
            gourd.catchPulseObserved = gourd.catchCount;
            gourd.catchPulseLeft = gourd.catchPulseSeconds;
        }
        gourd.catchPulseLeft = std::max(0.0f, gourd.catchPulseLeft - dt);
        const float gulp = gourd.catchPulseSeconds > 0.0f ? gourd.catchPulseLeft / gourd.catchPulseSeconds : 0.0f;
        // 膨らんで、弾んで戻る（c-e0a9c3）：始めが最大。catchPulseBounce の割合だけ、戻りながら逆へ縮んでから落ち着く。
        const float bounce = std::clamp(gourd.catchPulseBounce, 0.0f, 1.0f);
        const float swell = (1.0f - bounce) * gulp * gulp +
                            bounce * std::exp(-4.0f * (1.0f - gulp)) * std::cos(1.25f * kTau * (1.0f - gulp)) * gulp;
        // 吸い込み中は脈を打つように膨らむ（見た目だけ。口の位置と吸い込みの規則は変えない）。
        const float pulse =
            (1.0f + gourd.absorbPulseScale * gourd.absorbBlend *
                        (0.5f + 0.5f * std::sin(kTau * gourd.absorbPulseFrequency * gourd.absorbSeconds))) *
            (1.0f + gourd.catchPulseScale * (gourd.catchSmall ? gourd.catchSmallScale : 1.0f) * swell);
        transform.scale = {gourd.radius / kMeshRadius * pulse, gourd.height / kMeshHeight,
                           gourd.radius / kMeshRadius * pulse};
        gourd.transformAccepted = object.SetWorldTransform(transform).Accepted();
    });
}

// 規則のヒョウタンの置き場所（c-164cab）：固定 tick の終わりに、プレイヤーのその tick の位置・向き、吸い込みの段階、
// ロック対象と、Update で覚えたカメラの正面から決める。ばね・補間は使わない（描画のヒョウタンは Follow が別に持つ）。
void StepRulePose(HIKARI::WorldView& world, Gourd& gourd) {
    WithPlayer(gourd, [&](HIKARI::ObjectView playerObject, Player& player) {
        HIKARI::GameProjectTransform playerTransform{};
        if (!playerObject.GetWorldTransform(playerTransform))
            return;
        const auto facing = PlayerRules::GetFacing(player);
        const Vec3 right{facing.z, 0.0f, -facing.x};
        const auto offset = gourd.shoulderOffset;
        gourd.position = {playerTransform.position.x + right.x * offset.x + facing.x * offset.z + gourd.recoilOffset.x,
                          playerTransform.position.y + offset.y + gourd.recoilOffset.y,
                          playerTransform.position.z + right.z * offset.x + facing.z * offset.z + gourd.recoilOffset.z};
        const auto lockTarget = world.FindObject(PlayerRules::GetLockTarget(player));
        HIKARI::GameProjectTransform targetTransform{};
        const bool hasLock = lockTarget.IsValid() && lockTarget.GetWorldTransform(targetTransform);
        const bool absorbPose = gourd.absorbPhase != GourdAbsorbPhase::Idle;
        const Vec3 camera = gourd.inputCameraForwardValid ? gourd.inputCameraForward : facing;
        const Vec3 horizontal =
            Horizontal(gourd.position, facing, camera, absorbPose, hasLock, targetTransform.position);
        gourd.sinkPosition = {gourd.position.x + horizontal.x * gourd.absorbForwardDistance,
                              playerTransform.position.y + gourd.absorbHeightOffset,
                              gourd.position.z + horizontal.z * gourd.absorbForwardDistance};
        gourd.aimDirection = gourd.absorbing ? Normalize(Difference(gourd.sinkPosition, gourd.position), horizontal)
                             : absorbPose    ? horizontal
                             : hasLock ? Normalize(Difference(targetTransform.position, gourd.position), horizontal)
                                       : horizontal;
        gourd.mouthPosition = {gourd.position.x + gourd.aimDirection.x * gourd.height * 0.5f,
                               gourd.position.y + gourd.aimDirection.y * gourd.height * 0.5f,
                               gourd.position.z + gourd.aimDirection.z * gourd.height * 0.5f};
        gourd.rulePoseReady = true;
    });
}

void Trace(HIKARI::ObjectView object, const Gourd& gourd) {
    // ゲーム全体の値（Params）の読み戻し。部品の Update との順序で、現在値より 1 フレーム遅れることがある。
    const float varSmoke = HIKARI::Params::GetFloat(kGourdSmokeVariable, -1.0f);
    const float varCapacity = HIKARI::Params::GetFloat(kGourdCapacityVariable, -1.0f);
    const float varFireShot = HIKARI::Params::GetFloat(kGourdFireShotVariable, -1.0f);
    const bool varsRead = varSmoke >= 0.0f && varCapacity >= 0.0f && varFireShot >= 0.0f;
    char message[1024]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu frame=%llu object=%llu smoke=%.6f capacity=%.3f "
        "removedTotal=%.9f delta=%.9f "
        "absorbing=%d playerValid=%d position=(%.6f,%.6f,%.6f) "
        "sinkPosition=(%.6f,%.6f,%.6f) "
        "mouth=(%.6f,%.6f,%.6f) transformAccepted=%d submitAccepted=%d "
        "readAccepted=%d "
        "recoilOffset=(%.6f,%.6f,%.6f) recoilFramesLeft=%d fluidSubmissions=%u "
        "gatherStrength=%.3f intake=%.3f phase=%d phaseFrames=%d buffer=%d "
        "varsRead=%d varSmoke=%.3f varCapacity=%.3f fireShot=%.3f varFireShot=%.3f blocked=%d blockedSeconds=%.3f",
        static_cast<unsigned long long>(gourd.processedTick), static_cast<unsigned long long>(gourd.renderFrameCount),
        static_cast<unsigned long long>(object.GetSceneObjectId()), static_cast<double>(GourdRules::GetSmoke(gourd)),
        static_cast<double>(GourdRules::GetCapacity(gourd)), gourd.lastRemovedTotal, gourd.removedDelta,
        static_cast<int>(gourd.absorbing), static_cast<int>(gourd.playerValid), static_cast<double>(gourd.position.x),
        static_cast<double>(gourd.position.y), static_cast<double>(gourd.position.z),
        static_cast<double>(gourd.sinkPosition.x), static_cast<double>(gourd.sinkPosition.y),
        static_cast<double>(gourd.sinkPosition.z), static_cast<double>(gourd.mouthPosition.x),
        static_cast<double>(gourd.mouthPosition.y), static_cast<double>(gourd.mouthPosition.z),
        static_cast<int>(gourd.transformAccepted), static_cast<int>(gourd.fluidAccepted),
        static_cast<int>(gourd.readAccepted), static_cast<double>(gourd.recoilOffset.x),
        static_cast<double>(gourd.recoilOffset.y), static_cast<double>(gourd.recoilOffset.z), gourd.recoilFramesLeft,
        gourd.fluidSubmissions, static_cast<double>(gourd.gatherStrength), static_cast<double>(gourd.intakeRate),
        static_cast<int>(gourd.absorbPhase), gourd.absorbPhaseFrames, gourd.absorbBuffer, static_cast<int>(varsRead),
        static_cast<double>(varSmoke), static_cast<double>(varCapacity),
        static_cast<double>(GourdRules::GetFireShotSeconds(gourd)), static_cast<double>(varFireShot),
        static_cast<int>(GourdRules::IsBlocked(gourd)), static_cast<double>(gourd.blockedSeconds));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Gourd", message);
}

} // namespace

void GourdSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    HIKARI::GameProjectInputActionState input{};
    const bool valid = world.GetInput().GetAction("Gameplay.Absorb", input);
    // 先行入力は押した F の終わりにも 1 減るので、1 足して「押した後に覚えておく F 数」にする。
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView, Gourd& gourd) {
        GourdRules::Initialize(gourd);
        gourd.absorbHeld = valid && input.down;
        if (valid && input.pressed)
            gourd.absorbBuffer = std::max(0, gourd.absorbBufferFrames) + 1;
        // カメラの正面は入力の基準：ここで覚え、固定 tick が使う（c-164cab）。
        gourd.inputCameraForwardValid = CameraForward(world, gourd, gourd.inputCameraForward);
    });
}

void GourdSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView, Gourd& gourd) {
        gourd.processedTick = frame.fixedTickIndex;
        GourdRules::Initialize(gourd);
        // 前の描画のフレームで読み戻した霧の増えを、ここで煙に入れる。
        GourdRules::ApplyPendingSmoke(gourd);
        StepAbsorb(gourd);
        StepBlocked(gourd, frame.fixedDt);
        if (gourd.enabled && frame.fixedDt > 0.0f)
            StepRecoil(gourd, frame.fixedDt);
        GourdRules::StepFireShot(gourd, frame.fixedDt);
        GourdRules::StepSwallow(gourd);
    });
}

void GourdSystem::PostFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    // この tick の終わりのプレイヤー（移動の後）に合わせて、規則のヒョウタンの置き場所を決める（次の tick の砲撃の口・
    // 同じ tick の後で動くボスの捕まえる口が読む）。
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView, Gourd& gourd) {
        if (gourd.enabled && gourd.playerValid)
            StepRulePose(world, gourd);
    });
}

void GourdSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView object, Gourd& gourd) {
        if (gourd.enabled && gourd.playerValid)
            Follow(world, object, gourd, std::max(0.0f, frame.gameDt), frame.fixedInterpolationAlpha);
        gourd.aimOverridePending = false;
        GourdAbsorbVisuals::Update(world, object, gourd, frame);
    });
}

void GourdSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView object, Gourd& gourd) {
        // 口元の吸入・口の上の回収・扇の中で消した霧を足して、煙に数える（どれも累計値）。
        double total = 0.0;
        gourd.readAccepted = GourdAbsorbField::ReadRemoved(world, object, gourd, total);
        // 松開後の遅れた回読も回収する。容量超過時も累計の基準は必ず更新する。
        if (gourd.readAccepted)
            GourdRules::ReceiveRemovedSmoke(gourd, total);
        else
            gourd.removedDelta = 0.0;
        if (frame.gameDt > 0.0f) {
            const float rate =
                static_cast<float>(gourd.removedDelta) * GourdRules::GetSmokePerVolume(gourd) / frame.gameDt;
            gourd.intakeRate += (rate - gourd.intakeRate) * VectorMath::ExpFollow(frame.gameDt, kIntakeSmoothSeconds);
        }
        gourd.fluidAccepted = true;
        gourd.fluidSubmissions = 0;
        if (gourd.absorbing && gourd.poseInitialized)
            GourdAbsorbField::Submit(world, object, gourd, gourd.absorbSeconds);
        if (GourdRules::IsSwallowing(gourd) && gourd.poseInitialized)
            GourdAbsorbField::SubmitSwallow(world, object, gourd);
        if (gourd.blockedPuffLeft > 0 && gourd.poseInitialized)
            GourdAbsorbField::SubmitPushBack(world, object, gourd);
        ++gourd.renderFrameCount;
        if (TraceInterval::Due(gourd.traceEveryFrames, gourd.renderFrameCount))
            Trace(object, gourd);
    });
}

void GourdSystem::OnWorldDetached(HIKARI::WorldView& world) {
    ComponentLookup::ForEachGame<Gourd>(
        [&](HIKARI::ObjectView, Gourd& gourd) { GourdAbsorbVisuals::Stop(world, gourd); });
}

} // namespace Project_TD2_2
