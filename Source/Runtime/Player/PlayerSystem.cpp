#include "PlayerSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../Common/FireObstacle.h"
#include "../Common/GameSession.h"
#include "../Common/VectorMath.h"
#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "PlayerCamera.h"
#include "PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。
constexpr uint64_t kMotionSource = 0x5444325001ull;
constexpr uint64_t kHitStopSource = 0x5444325003ull;
constexpr uint64_t kHitCameraSource = 0x5444325004ull;
constexpr uint64_t kHitFovSource = 0x5444325005ull;
constexpr uint64_t kRollFogInstance = 1;
constexpr uint64_t kRollFireInstance = 2;
constexpr uint64_t kFireObstacleInstance = 3;
constexpr uint64_t kKnockbackLandFogInstance = 4;
constexpr int kPlayerPriority = 300;
using VectorMath::kTau;
using VectorMath::kRadiansPerDegree;
// 向きを変える入力として扱う最小の倒し量。スティックの遊びで向きが震えないようにする。
constexpr float kTurnInputDeadZone = 0.1f;

using Vec3 = HIKARI::GameProjectFloat3;

using GameSession::IsPlaying;
using GameSession::ReadAction;

void ReadInput(HIKARI::WorldView& world, PlayerComponent& player) {
    // 演出の間（ボスの登場）は、入力を受け付けない（動かず、技も出さない）。
    if (player.cinematicHold > 0) {
        player.moveInput = {};
        player.sprintHeld = false;
        return;
    }
    const auto input = world.GetInput();
    // 移動の基準は、カメラの水平の向き（PlayerCamera が毎フレーム入れる）。前を押すと画面の奥へ進む。
    // 揺れ・肩越しのずらしを含んだ「見えている向き」からは取らない（入力の向きが揺れたり、斜めにずれたりする）。
    const auto move = ReadAction(input, "Gameplay.Move");
    const Vec3 right{-player.cameraForward.z, 0.0f, player.cameraForward.x};
    player.moveInput = {right.x * move.x + player.cameraForward.x * move.y, 0.0f,
                        right.z * move.x + player.cameraForward.z * move.y};
    const float magnitude = std::hypot(player.moveInput.x, player.moveInput.z);
    if (magnitude > 1.0f) {
        player.moveInput.x /= magnitude;
        player.moveInput.z /= magnitude;
    }
    if (ReadAction(input, "Gameplay.Dodge").pressed) {
        player.rollBuffer = player.inputBufferFrames;
    }
    player.sprintHeld = ReadAction(input, "Gameplay.Sprint").down != 0u;
    if (ReadAction(input, "Gameplay.Heal").pressed) {
        player.healBuffer = player.inputBufferFrames;
    }
    if (ReadAction(input, "Gameplay.LockOn").pressed && PlayerRules::IsAlive(player)) {
        const auto target = world.FindSceneObject(player.lockTargetId);
        player.lockedOn = !player.lockedOn && target.IsValid();
        player.lockTarget = player.lockedOn ? target.GetHandle() : HIKARI::GameProjectObjectHandle{};
    }
    // 対象の候補（ボスと分身）はプレイヤーは知らないので、押されたことだけを覚えておく。
    if (ReadAction(input, "Gameplay.LockSwitch").pressed && player.lockedOn)
        player.lockSwitchRequested = true;
}

int StateDuration(const PlayerComponent& player) {
    switch (player.state) {
    case PlayerState::Roll:
        return player.rollFrames;
    case PlayerState::Heal:
        return player.healFrames;
    case PlayerState::Stagger:
        return player.staggerFrames;
    case PlayerState::Knockback:
        return player.knockbackFrames;
    case PlayerState::Down:
        return player.downFrames;
    case PlayerState::GetUp:
        return player.getUpFrames;
    default:
        return 0;
    }
}

// 水平に、吹っ飛びを始めた位置からどれだけ離れたか（ログ用）。
float KnockbackTravelled(HIKARI::ObjectView object, const PlayerComponent& player) {
    HIKARI::GameProjectTransform transform{};
    if (!object.GetWorldTransform(transform))
        return 0.0f;
    return std::hypot(transform.position.x - player.knockbackOrigin.x, transform.position.z - player.knockbackOrigin.z);
}

void CompleteState(HIKARI::ObjectView object, PlayerComponent& player, uint64_t tick) {
    const int duration = StateDuration(player);
    if (duration <= 0 || player.stateFrame < duration) {
        return;
    }
    if (player.state == PlayerState::Knockback) {
        // 最後の tick の動きまで物体の位置へ反映された後なので、ここで測る距離が吹っ飛びの総距離になる。
        char message[160]{};
        std::snprintf(message, sizeof(message), "KnockbackEnd tick=%llu travelled=%.3f",
                      static_cast<unsigned long long>(tick), static_cast<double>(KnockbackTravelled(object, player)));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
    }
    const auto next = player.state == PlayerState::Knockback ? PlayerState::Down
                      : player.state == PlayerState::Down    ? PlayerState::GetUp
                                                             : PlayerState::Normal;
    PlayerRules::EnterState(player, next);
}

// ロックオン中の対象への水平方向。対象が無い・位置が取れないときは false。
bool DirectionToLockTarget(HIKARI::WorldView& world, HIKARI::ObjectView object, const PlayerComponent& player,
                           Vec3& outDirection) {
    if (!player.lockedOn)
        return false;
    const auto target = world.FindObject(player.lockTarget);
    HIKARI::GameProjectTransform sourceTransform{}, targetTransform{};
    if (!target.IsValid() || !target.GetWorldTransform(targetTransform) || !object.GetWorldTransform(sourceTransform))
        return false;
    const Vec3 offset{targetTransform.position.x - sourceTransform.position.x, 0.0f,
                      targetTransform.position.z - sourceTransform.position.z};
    if (std::hypot(offset.x, offset.z) <= 0.0f)
        return false;
    outDirection = PlayerRules::FlatNormalize(offset);
    return true;
}

// 吹っ飛び中と、ダウンでローリングを受け付ける前：押したローリングは受け付ける F まで保つ（吹っ飛ばされてすぐ押した
// ローリングが、先行入力の長さで消えて出ないことが無いように）。
bool HoldsRollBuffer(const PlayerComponent& player) {
    return player.state == PlayerState::Knockback ||
           (player.state == PlayerState::Down && player.stateFrame < std::min(player.downRollFrame, player.downFrames));
}

void StartBufferedAction(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player, uint64_t tick) {
    // ローリングは外部の動作（吸い込み）の最中でも出せて、吸い込みの方が打ち切られる。
    // 回復だけは外部の要求で止める。先行入力は残り、明けた時に通る。
    // 砲撃の動作ロック中でも、打ち切れる F からはローリングで抜けられる。
    // 起き上がりもローリングで打ち切れる（起き上がりには無敵が無いので、追い打ちをよけられるように）。
    const bool canRoll =
        PlayerRules::CanStartAction(player) ||
        (player.state == PlayerState::Down && player.stateFrame >= std::min(player.downRollFrame, player.downFrames) &&
         player.actionLockFrames == 0) ||
        (player.state == PlayerState::GetUp && player.stateFrame >= player.getUpRollFrame &&
         player.actionLockFrames == 0) ||
        (player.external.rollGate == PlayerRollGate::Cancel && PlayerRules::IsAlive(player) &&
         player.state == PlayerState::Normal);
    if (player.rollBuffer > 0 && canRoll) {
        const bool moving = std::hypot(player.moveInput.x, player.moveInput.z) > 0.0f;
        // 入力が無ければ、ロックオン中は対象から離れる向き、そうでなければ今の向きの後ろへ下がる。
        Vec3 toTarget{};
        const Vec3 away = DirectionToLockTarget(world, object, player, toTarget)
                              ? Vec3{-toTarget.x, 0.0f, -toTarget.z}
                              : Vec3{-player.facing.x, 0.0f, -player.facing.z};
        player.actionDirection = moving ? PlayerRules::FlatNormalize(player.moveInput) : away;
        // 転がる向きへその場で体を向ける（ロックオン中も）。転がっている間は向きを変えない。
        player.facing = player.actionDirection;
        // 回避だけは残量 0 でも開始できる。射撃用の消費関数とは条件が違う。
        player.stamina = std::max(0.0f, player.stamina - player.rollStamina);
        player.staminaAge = 0;
        player.rollBuffer = 0;
        player.rollFogPending = true;
        // 打ち切った動作のロックを、ローリングの後へ持ち越さない。
        player.actionLockFrames = 0;
        PlayerRules::EnterState(player, PlayerState::Roll);
        // 転がると体の火が消える（ローリングに入った tick に消す）。
        PlayerRules::Extinguish(player, PlayerBurnEnd::Roll, tick);
    } else if (player.healBuffer > 0 && !player.external.healBlocked && PlayerRules::CanStartAction(player) &&
               player.healCharges > 0 && player.health < player.maxHealth) {
        --player.healCharges;
        player.healApplied = false;
        player.healBuffer = 0;
        PlayerRules::EnterState(player, PlayerState::Heal);
    }
}

float StaggerStep(const PlayerComponent& player) {
    // 合計時間を短く調整しても三歩を残し、各距離を失わないようにする。
    const int first = std::clamp(player.staggerStepOneFrames, 1, player.staggerFrames - 2);
    const int second = std::clamp(player.staggerStepTwoFrames, 1, player.staggerFrames - first - 1);
    const int third = player.staggerFrames - first - second;
    if (player.stateFrame < first)
        return player.staggerStepOne / static_cast<float>(first);
    if (player.stateFrame < first + second)
        return player.staggerStepTwo / static_cast<float>(second);
    return player.staggerStepThree / static_cast<float>(third);
}

// 走る：ふだんの歩き（外部の動作が歩きを変えていない・動作ロックが無い）で、入力があり、走る入力を押している間。
// 走っている間はスタミナを使い、回復を止める（止めてから回復待ちの F を数え直す）。尽きたら入力を離すまで走らない。
// 向き（UpdateFacing）と歩き（StateVelocity）より先に、その固定 tick の分を決める。
void StepSprint(PlayerComponent& player, float dt, uint64_t tick) {
    const bool wasSprinting = player.sprinting;
    if (!player.sprintHeld)
        player.sprintExhausted = false;
    const bool hasInput = std::hypot(player.moveInput.x, player.moveInput.z) > kTurnInputDeadZone;
    const bool plainWalk = player.state == PlayerState::Normal && player.actionLockFrames == 0 &&
                           !player.external.walkAllowed && player.external.moveScale >= 1.0f &&
                           player.external.turnSpeed <= 0.0f && !player.aimTurnPending;
    player.sprinting = PlayerRules::IsAlive(player) && player.sprintHeld && !player.sprintExhausted && hasInput &&
                       plainWalk && player.stamina > 0.0f;
    if (player.sprinting) {
        player.stamina = std::max(0.0f, player.stamina - player.sprintStaminaPerSecond * dt);
        player.staminaAge = 0;
        if (player.stamina <= 0.0f)
            player.sprintExhausted = true;
    }
    if (player.sprinting != wasSprinting) {
        char message[128]{};
        std::snprintf(message, sizeof(message), "%s tick=%llu stamina=%.3f exhausted=%d",
                      player.sprinting ? "SprintStart" : "SprintEnd", static_cast<unsigned long long>(tick),
                      static_cast<double>(player.stamina), static_cast<int>(player.sprintExhausted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
    }
}

// 歩く速度を、入力の向き × speed へ寄せる。歩き出しと止まる時だけ少し時間を掛け、体の重さを出す。
// 寄せる速さは「最高速に届くまでの時間」で決める（0 なら一瞬）。最高速は歩き・走り・いまの速さの大きい方
// （走りから止まる時も、決めた時間で止まる）。
Vec3 WalkVelocity(PlayerComponent& player, float speed, float dt) {
    const Vec3 wanted{player.moveInput.x * speed, 0.0f, player.moveInput.z * speed};
    const Vec3 gap{wanted.x - player.moveVelocity.x, 0.0f, wanted.z - player.moveVelocity.z};
    const float distance = std::hypot(gap.x, gap.z);
    const float currentSpeed = std::hypot(player.moveVelocity.x, player.moveVelocity.z);
    const bool slowing = std::hypot(wanted.x, wanted.z) < currentSpeed;
    const float seconds = slowing ? player.moveDecelSeconds : player.moveAccelSeconds;
    const float reference = std::max({player.moveSpeed, speed, currentSpeed});
    const float step = seconds > 0.0f ? reference / seconds * dt : distance;
    if (distance <= step || distance <= 0.0f) {
        player.moveVelocity = wanted;
    } else {
        player.moveVelocity.x += gap.x / distance * step;
        player.moveVelocity.z += gap.z / distance * step;
    }
    return player.moveVelocity;
}

Vec3 StateVelocity(PlayerComponent& player, float dt) {
    if (player.state == PlayerState::Normal && (player.actionLockFrames == 0 || player.external.walkAllowed)) {
        const float speed = player.sprinting ? player.sprintSpeed : player.moveSpeed;
        return WalkVelocity(player, speed * std::clamp(player.external.moveScale, 0.0f, 1.0f), dt);
    }
    if (player.state == PlayerState::Heal) {
        if (!player.healApplied && player.stateFrame + 1 >= std::min(player.healApplyFrame, player.healFrames)) {
            player.health = std::min(player.maxHealth, player.health + player.healAmount);
            player.healApplied = true;
        }
        return WalkVelocity(player, player.actionLockFrames == 0 ? player.healMoveSpeed : 0.0f, dt);
    }
    // 回避・被弾などの動作は決まった距離を進む。終わった後は止まった所から歩き出す。
    player.moveVelocity = {};
    float distance = 0.0f;
    if (player.state == PlayerState::Roll) {
        // 初め速くあとゆっくり。この tick に進む分 = 次の F までの距離 − いまの F までの距離。合計は rollDistance。
        const int moveFrames = std::max(1, std::min(player.rollMoveFrames, player.rollFrames));
        if (player.stateFrame < moveFrames)
            distance = player.rollDistance *
                       (PlayerRules::EaseOutFraction(player.stateFrame + 1, moveFrames, player.rollEasePower) -
                        PlayerRules::EaseOutFraction(player.stateFrame, moveFrames, player.rollEasePower));
    } else if (player.state == PlayerState::Stagger) {
        distance = StaggerStep(player);
    } else if (player.state == PlayerState::Knockback) {
        // 前が速く後ろが遅い（ローリングと同じ形）。合計はちょうど knockbackDistanceNow。
        distance =
            player.knockbackDistanceNow *
            (PlayerRules::EaseOutFraction(player.stateFrame + 1, player.knockbackFrames, player.knockbackEasePower) -
             PlayerRules::EaseOutFraction(player.stateFrame, player.knockbackFrames, player.knockbackEasePower));
    }
    const float speed = distance / dt;
    return {player.actionDirection.x * speed, 0.0f, player.actionDirection.z * speed};
}

// 向きを wanted（水平）へ、毎秒 degreesPerSecond までの速さで回す。
void TurnToward(PlayerComponent& player, Vec3 wanted, float degreesPerSecond, float dt) {
    const float current = std::atan2(player.facing.x, player.facing.z);
    const float target = std::atan2(wanted.x, wanted.z);
    const float limit = degreesPerSecond * kRadiansPerDegree * dt;
    const float yaw = current + std::clamp(std::remainder(target - current, kTau), -limit, limit);
    player.facing = {std::sin(yaw), 0.0f, std::cos(yaw)};
}

void UpdateFacing(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player, float dt) {
    const bool aimTurn = player.aimTurnPending;
    player.aimTurnPending = false;
    if (!PlayerRules::IsAlive(player))
        return;
    Vec3 toTarget{};
    const bool hasTarget = DirectionToLockTarget(world, object, player, toTarget);
    if (player.lockedOn && !hasTarget) {
        player.lockedOn = false;
        player.lockTarget = {};
    }
    const bool hasInput = std::hypot(player.moveInput.x, player.moveInput.z) > kTurnInputDeadZone;
    if (player.external.turnSpeed > 0.0f) {
        // 外部の要求（吸い込み）：ロックオン中でも、入力した方向へ決まった速さで向きを変える。入力が無ければそのまま。
        if (hasInput)
            TurnToward(player, player.moveInput, player.external.turnSpeed, dt);
        return;
    }
    if (aimTurn) {
        // 射撃の構え：撃つ向きへ振り向く。発射の瞬間に射撃側が向きをそろえる。
        TurnToward(player, player.aimTurnDirection, player.aimTurnSpeed, dt);
        return;
    }
    // 回避・被弾などの最中は、その動作を始めた時の向きのまま。
    if (player.state != PlayerState::Normal && player.state != PlayerState::Heal)
        return;
    // 走っている間は、ロックオン中でも進む向きを向く（横歩きではなく、その方向へ走る）。
    if (hasTarget && !player.sprinting)
        TurnToward(player, toTarget, player.turnSpeed, dt);
    else if (hasInput)
        TurnToward(player, player.moveInput, player.turnSpeed, dt);
}

// 吹っ飛びの高さの輪郭。飛行（knockbackFrames）の knockbackApexAt で頂点（高さ knockbackHeight）に届き、
// knockbackLandAt で高さ 0 へ着く。上りと下りを別の放物線でつなぎ、頂点では速さが 0 でそろう。
// 着地より後は地面を滑る（高さの輪郭は 0 で、重力はいつもどおり）。
struct KnockbackProfile {
    float apexFrames = 1.0f;
    float landFrames = 2.0f;
};

KnockbackProfile MakeKnockbackProfile(const PlayerComponent& player) {
    const float frames = static_cast<float>(std::max(1, player.knockbackFrames));
    KnockbackProfile profile;
    profile.apexFrames = std::max(1.0f, player.knockbackApexAt * frames);
    profile.landFrames = std::max(profile.apexFrames + 1.0f, player.knockbackLandAt * frames);
    return profile;
}

// 着地が完了する tick の番号（その tick の終わりで高さが 0 に届く）。
int KnockbackLandTick(const KnockbackProfile& profile) { return static_cast<int>(std::ceil(profile.landFrames)) - 1; }

// 飛び始めから frames F たった時の高さ。
float KnockbackHeightAt(const PlayerComponent& player, const KnockbackProfile& profile, float frames) {
    const float height = std::max(0.0f, player.knockbackHeight);
    if (frames <= 0.0f || frames >= profile.landFrames)
        return 0.0f;
    if (frames < profile.apexFrames) {
        const float left = 1.0f - frames / profile.apexFrames;
        return height * (1.0f - left * left);
    }
    const float fall = (frames - profile.apexFrames) / (profile.landFrames - profile.apexFrames);
    return height * (1.0f - fall * fall);
}

// この tick の縦の動き。運動学ソルバーは F ごとに速度を先に変えてから進めるので、tick k の速度を
// 「k〜k+1 F の高さの差 ÷ dt」にして、k+1 F の高さを輪郭にそろえる。重力は速度の変化として毎 tick 差し替える。
// 最初の tick は跳ね上げの速度だけで、重力は 0。
// 下りは、下向きの加速度を輪郭の下りの加速度以上に保つ（着地の tick は高さが 0 に切られて速度の変化が上向きになるが、
// 地面が輪郭より高い坂で先に着地していても上へ突き上げないために、下向きのままにする。着地はソルバーに任せる）。
void KnockbackVertical(const PlayerComponent& player, float dt, float& outLaunchSpeed, float& outGravity) {
    const KnockbackProfile profile = MakeKnockbackProfile(player);
    const float k = static_cast<float>(player.stateFrame);
    const auto speedAt = [&](float frame) {
        return (KnockbackHeightAt(player, profile, frame + 1.0f) - KnockbackHeightAt(player, profile, frame)) / dt;
    };
    outLaunchSpeed = speedAt(0.0f);
    if (player.stateFrame == 0) {
        outGravity = 0.0f;
        return;
    }
    outGravity = (speedAt(k) - speedAt(k - 1.0f)) / dt;
    if (k >= profile.apexFrames) {
        const float fallSeconds = (profile.landFrames - profile.apexFrames) * dt;
        outGravity = std::min(outGravity, -2.0f * std::max(0.0f, player.knockbackHeight) / (fallSeconds * fallSeconds));
    }
}

// 立ち止まっていて（通常の状態で水平の速度が 0）、前の tick に立てる傾きの地面へ足が着いていた。
// 運動学ソルバーは地面の上でも毎 tick 重力 × dt の下向きの速度を足し、坂ではそれが斜面に沿った動きになって、
// 立っているだけで坂を滑り落ちる（傾き θ で毎秒 g·dt·sinθ·cosθ。12 度の坂で約 3.3 cm）。
bool StandingOnSlope(HIKARI::WorldView& world, HIKARI::ObjectView object, const PlayerComponent& player,
                     Vec3 velocity) {
    if (player.state != PlayerState::Normal || velocity.x != 0.0f || velocity.z != 0.0f)
        return false;
    HIKARI::GameProjectKinematicState state{};
    if (!world.GetMotion().GetKinematicState(object.GetHandle().value, state) ||
        state.groundState != HIKARI::GameProjectGroundState::OnGround)
        return false;
    const Vec3 normal = state.groundNormal;
    const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    return length > 0.0f && normal.y >= length * std::cos(player.standSlopeDegrees * kRadiansPerDegree);
}

// 吹っ飛びの始まりと着地（ログと、着地の手応えの合図）。ヒットストップの tick は呼ばれないので、始まりは 1 回だけ。
// 位置は前の tick までの動きが反映された値なので、着地のログの距離は着地の 1 tick 手前までの値になる。
void StepKnockbackEvents(HIKARI::ObjectView object, PlayerComponent& player, uint64_t tick) {
    if (player.state != PlayerState::Knockback)
        return;
    char message[192]{};
    if (player.stateFrame == 0) {
        HIKARI::GameProjectTransform transform{};
        if (object.GetWorldTransform(transform))
            player.knockbackOrigin = transform.position;
        std::snprintf(message, sizeof(message), "KnockbackStart tick=%llu distance=%.3f direction=(%.3f,%.3f)",
                      static_cast<unsigned long long>(tick), static_cast<double>(player.knockbackDistanceNow),
                      static_cast<double>(player.actionDirection.x), static_cast<double>(player.actionDirection.z));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
    }
    if (player.knockbackHeight <= 0.0f || player.stateFrame != KnockbackLandTick(MakeKnockbackProfile(player)))
        return;
    player.pendingLandFeedback = true;
    player.pendingLandFog = true;
    std::snprintf(message, sizeof(message), "KnockbackLand tick=%llu frame=%d travelled=%.3f",
                  static_cast<unsigned long long>(tick), player.stateFrame,
                  static_cast<double>(KnockbackTravelled(object, player)));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
}

void SubmitMovement(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                    const HIKARI::FrameContext& frame, Vec3 velocity, bool frozen = false) {
    HIKARI::GameProjectKinematicMove move{};
    move.objectHandle = object.GetHandle().value;
    move.sourceId = kMotionSource;
    move.fixedTickIndex = frame.fixedTickIndex;
    move.priority = kPlayerPriority;
    move.horizontalVelocity = velocity;
    move.maximumFallSpeed = player.maximumFallSpeed;
    const float halfYaw = std::atan2(player.facing.x, player.facing.z) * 0.5f;
    move.hasDesiredRotation = 1;
    move.desiredRotation = {0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
    if (StandingOnSlope(world, object, player, velocity)) {
        // 立っている間だけ、この tick の縦の重力を 0 にする（足は地面に着いたままで、坂へ沿って流れない）。
        // 押されて足場を失えば、次の tick は地面に着いていないので、いつもの重力で落ちる。
        HIKARI::GameProjectKinematicMoveOptions options{};
        options.overrideVerticalGravity = 1;
        options.verticalGravityAcceleration = 0.0f;
        player.motionAccepted = world.GetMotion().SubmitKinematicMoveWithOptions(move, options);
        return;
    }
    // 着地の後は、いつもの重力で地面を滑る（着地の判定も、坂や縁の扱いも運動学ソルバーに任せる）。
    if (frozen || player.state != PlayerState::Knockback || player.knockbackHeight <= 0.0f ||
        player.stateFrame > KnockbackLandTick(MakeKnockbackProfile(player))) {
        player.motionAccepted = world.GetMotion().SubmitKinematicMove(move);
        return;
    }
    // 飛んでいる間だけ重力を差し替え、最初の F に跳ね上げて、上り・下りの放物線を描かせる。
    float launchSpeed = 0.0f, gravity = 0.0f;
    KnockbackVertical(player, frame.fixedDt, launchSpeed, gravity);
    if (player.stateFrame == 0) {
        move.jumpRequested = 1;
        move.jumpSpeed = launchSpeed;
        move.allowJumpWithoutGroundContact = 1;
    }
    HIKARI::GameProjectKinematicMoveOptions options{};
    options.overrideVerticalGravity = 1;
    options.verticalGravityAcceleration = gravity;
    player.motionAccepted = world.GetMotion().SubmitKinematicMoveWithOptions(move, options);
}

// 食らった瞬間のカメラの揺れと手柄の振動。止めは固定 tick 側（StepPlayer）が出す。
// 揺れは実時間で進めるので、止めの間も揺れが続いて止まらない。
void StepHitFeedback(HIKARI::WorldView& world, PlayerComponent& player, const HIKARI::FrameContext& frame) {
    if (player.pendingHitFeedback) {
        player.pendingHitFeedback = false;
        const bool knockback = player.pendingHitFeedbackKnockback;
        Feedback::CameraShakeParams shake{};
        shake.sourceId = kHitCameraSource;
        shake.priority = kPlayerPriority;
        shake.frames = knockback ? player.hitCameraFramesKnockback : player.hitCameraFramesStagger;
        shake.amplitude = knockback ? player.hitCameraAmplitudeKnockback : player.hitCameraAmplitudeStagger;
        shake.frequency = player.hitCameraFrequency;
        Feedback::StartCameraShake(player.hitShake, shake);
        const float rumble = knockback ? player.hitRumbleKnockback : player.hitRumbleStagger;
        Feedback::Rumble(world, rumble, rumble, player.hitRumbleFrames);
        if (knockback) {
            // 吹っ飛ばされた瞬間にカメラを少し引き、戻す。
            Feedback::CameraShakeParams fov{};
            fov.sourceId = kHitFovSource;
            fov.priority = kPlayerPriority;
            fov.frames = player.knockbackCameraFovFrames;
            fov.fovDegrees = player.knockbackCameraFov;
            Feedback::StartCameraShake(player.knockbackFov, fov);
        }
    }
    if (player.pendingLandFeedback) {
        // 着地の重み。揺れは食らった時の揺れの機構をそのまま使う（その揺れはもう終わっている長さ）。
        player.pendingLandFeedback = false;
        Feedback::CameraShakeParams shake{};
        shake.sourceId = kHitCameraSource;
        shake.priority = kPlayerPriority;
        shake.frames = player.knockbackLandShakeFrames;
        shake.amplitude = player.knockbackLandShake;
        shake.frequency = player.hitCameraFrequency;
        Feedback::StartCameraShake(player.hitShake, shake);
        Feedback::Rumble(world, player.knockbackLandRumble, player.knockbackLandRumble,
                         player.knockbackLandShakeFrames);
    }
    Feedback::SubmitCameraShake(world, player.knockbackFov, player.cameraId, frame.unscaledDt);
    HIKARI::GameProjectFloat3 offset{};
    const bool wasActive = player.hitShake.active;
    const bool accepted =
        Feedback::SubmitCameraShake(world, player.hitShake, player.cameraId, frame.unscaledDt, &offset);
    if (wasActive && player.cameraTrace) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "HitShake tick=%llu offset=(%.6f,%.6f) length=%.6f accepted=%d",
                      static_cast<unsigned long long>(frame.fixedTickIndex), static_cast<double>(offset.x),
                      static_cast<double>(offset.y), static_cast<double>(std::hypot(offset.x, offset.y)),
                      static_cast<int>(accepted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerFeedback", message);
    }
}

// 自分の足での 1 固定 tick の向きと移動（騎乗中は呼ばない）。
void StepMovement(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                  const HIKARI::FrameContext& frame) {
    UpdateFacing(world, object, player, frame.fixedDt);
    StepKnockbackEvents(object, player, frame.fixedTickIndex);
    Vec3 velocity = StateVelocity(player, frame.fixedDt);
    if (PlayerRules::IsAlive(player) && player.recoilFramesLeft > 0) {
        const int elapsed = player.recoilFrames - player.recoilFramesLeft;
        const float step = player.recoilDistance *
                           (PlayerRules::EaseOutFraction(elapsed + 1, player.recoilFrames, player.recoilEasePower) -
                            PlayerRules::EaseOutFraction(elapsed, player.recoilFrames, player.recoilEasePower));
        velocity.x += player.recoilDirection.x * step / frame.fixedDt;
        velocity.z += player.recoilDirection.z * step / frame.fixedDt;
        --player.recoilFramesLeft;
    }
    SubmitMovement(world, object, player, frame, velocity);
}

void StepPlayer(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                const HIKARI::FrameContext& frame) {
    PlayerRules::Initialize(player);
    if (player.pendingHitStopFrames > 0) {
        player.hitStopAccepted =
            Feedback::RequestHitStop(world, kHitStopSource, kPlayerPriority, player.pendingHitStopFrames, 0.0f) != 0;
        player.pendingHitStopFrames = 0;
        // ヒットストップを出す tick は止まるだけにし、吹っ飛びの跳ね上げは止まった後の最初の F に回す。
        SubmitMovement(world, object, player, frame, {}, true);
        return;
    }
    StepSprint(player, frame.fixedDt, frame.fixedTickIndex);
    // 騎乗中の向きと動きは CloudMountSystem が出す。ここでは移動を提出しない（二つの移動が取り合わない）。
    if (PlayerRules::IsRiding(player))
        PlayerRules::StepRide(player);
    else
        StepMovement(world, object, player, frame);
    if (PlayerRules::IsAlive(player)) {
        if (player.state != PlayerState::Roll && player.staminaAge >= player.staminaDelayFrames) {
            player.stamina = std::min(player.maxStamina, player.stamina + player.staminaPerSecond * frame.fixedDt);
        }
        player.staminaAge = std::min(player.staminaAge + 1, player.staminaDelayFrames);
    }
    player.advanceStateNextTick = StateDuration(player) > 0;
    // まだ打ち切れない動作（砲撃の構え・後隙の前半）の間に押したローリングは、打ち切れる F まで減らさない。
    if (player.external.rollGate != PlayerRollGate::Hold && !HoldsRollBuffer(player))
        player.rollBuffer = std::max(0, player.rollBuffer - 1);
    player.healBuffer = std::max(0, player.healBuffer - 1);
    player.actionLockFrames = std::max(0, player.actionLockFrames - 1);
}

} // namespace

void PlayerSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                PlayerRules::Initialize(player);
                                                ReadInput(world, player);
                                            });
}

void PlayerSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                PlayerRules::Initialize(player);
                                                // 演出の間の止め（演出の側が毎 tick 入れ直す）を 1 tick 進める。
                                                if (player.cinematicHold > 0)
                                                    --player.cinematicHold;
                                                // この tick の始めの体の位置（描画のフレームの補間の起点）。
                                                HIKARI::GameProjectTransform start{};
                                                if (object.GetWorldTransform(start)) {
                                                    player.tickStartPosition = start.position;
                                                    player.tickStartKnown = true;
                                                }
                                                // 全 FixedUpdate の前に境界を決め、被弾側の実行順で無敵が一 F
                                                // ずれないようにする。
                                                if (player.advanceStateNextTick)
                                                    ++player.stateFrame;
                                                player.advanceStateNextTick = false;
                                                CompleteState(object, player, frame.fixedTickIndex);
                                                StartBufferedAction(world, object, player, frame.fixedTickIndex);
                                                // 外部の要求は毎 tick 失効させ、外部システムが消えても減速や停止を
                                                // 残さない（回復の停止・ローリングの扱いは前の tick の要求なので、
                                                // 先行入力が使った後に消す）。
                                                player.external = {};
                                            });
}

void PlayerSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType),
        [&](HIKARI::ObjectView object, PlayerComponent& player) { StepPlayer(world, object, player, frame); });
}

void PlayerSystem::PostFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    // 炎上は、その tick の被弾（ボス）と燃える地面（BurnFieldSystem）が全部済んだ後に進める（点いた tick から減る）。
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                PlayerRules::StepBurn(player, frame.fixedDt, frame.fixedTickIndex);
                                            });
}

void PlayerSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            PlayerCamera::Update(world, object, player, std::max(0.0f, frame.unscaledDt),
                                 frame.fixedInterpolationAlpha);
            StepHitFeedback(world, player, frame);
        });
}

void PlayerSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object,
                                                                                          PlayerComponent& player) {
        player.fluidAccepted = FireObstacle::Submit(world, object, kFireObstacleInstance, player.fireObstacleRadius,
                                                    player.fireObstacleHeight);
        if (player.pendingLandFog) {
            // 着地：足元の霧を外へ 1 回押す（向きの無い押しは、中心から外へ広がる）。
            player.pendingLandFog = false;
            HIKARI::GameProjectTransform landTransform{};
            if (player.knockbackLandPush > 0.0f && object.GetWorldTransform(landTransform)) {
                HIKARI::GameProjectFluidInteraction burst{};
                burst.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
                burst.position = landTransform.position;
                burst.position.y += player.knockbackLandPushHeightOffset;
                burst.extents = {player.knockbackLandPushRadius, 0.0f, 0.0f};
                burst.strength = player.knockbackLandPush;
                burst.channels = FluidChannels::kFog;
                player.fluidAccepted =
                    world.GetFluid().Submit(object.GetHandle(), kKnockbackLandFogInstance, burst).Accepted() &&
                    player.fluidAccepted;
            }
        }
        if (!player.rollFogPending)
            return;
        player.rollFogPending = false;
        HIKARI::GameProjectTransform transform{};
        if (!object.GetWorldTransform(transform))
            return;
        HIKARI::GameProjectFluidInteraction impulse{};
        impulse.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
        impulse.position = transform.position;
        impulse.position.y += player.rollFogHeightOffset;
        impulse.extents = {player.rollFogRadius, 0.0f, 0.0f};
        impulse.direction = player.actionDirection;
        impulse.strength = player.rollFogStrength;
        player.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kRollFogInstance, impulse).Accepted() && player.fluidAccepted;
        // ローリングの風は、場の火も弱めに押す（火のそばを転がると炎がなびく）。
        if (player.rollFireShare > 0.0f) {
            impulse.strength = player.rollFogStrength * player.rollFireShare;
            impulse.channels = FluidChannels::kFire;
            player.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), kRollFireInstance, impulse).Accepted() &&
                                   player.fluidAccepted;
        }
    });
}

} // namespace Project_TD2_2
