#include "PlayerSystem.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "PlayerCamera.h"
#include "PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。
constexpr uint64_t kMotionSource = 0x5444325001ull;
constexpr uint64_t kHitStopSource = 0x5444325003ull;
constexpr uint64_t kRollFogInstance = 1;
constexpr int kPlayerPriority = 300;
constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 向きを変える入力として扱う最小の倒し量。スティックの遊びで向きが震えないようにする。
constexpr float kTurnInputDeadZone = 0.1f;

using Vec3 = HIKARI::GameProjectFloat3;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

HIKARI::GameProjectInputActionState ReadAction(HIKARI::InputView input, std::string_view id) {
    HIKARI::GameProjectInputActionState action{};
    if (!input.GetAction(id, action)) {
        return {};
    }
    return action;
}

void ReadInput(HIKARI::WorldView& world, PlayerComponent& player) {
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

void CompleteState(PlayerComponent& player) {
    const int duration = StateDuration(player);
    if (duration <= 0 || player.stateFrame < duration) {
        return;
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

void StartBufferedAction(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player) {
    // 外部の動作（吸い込み）の最中は、ローリングも回復も始められない。先行入力は残り、明けた時に通る。
    if (player.externalActionBlock)
        return;
    const bool canRoll =
        PlayerRules::CanStartAction(player) ||
        (player.state == PlayerState::Down && player.stateFrame >= std::min(player.downRollFrame, player.downFrames) &&
         player.actionLockFrames == 0);
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
        PlayerRules::EnterState(player, PlayerState::Roll);
    } else if (player.healBuffer > 0 && PlayerRules::CanStartAction(player) && player.healCharges > 0 &&
               player.health < player.maxHealth) {
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

// 歩く速度を、入力の向き × speed へ寄せる。歩き出しと止まる時だけ少し時間を掛け、体の重さを出す。
// 寄せる速さは「moveSpeed に届くまでの時間」で決める（0 なら一瞬）。
Vec3 WalkVelocity(PlayerComponent& player, float speed, float dt) {
    const Vec3 wanted{player.moveInput.x * speed, 0.0f, player.moveInput.z * speed};
    const Vec3 gap{wanted.x - player.moveVelocity.x, 0.0f, wanted.z - player.moveVelocity.z};
    const float distance = std::hypot(gap.x, gap.z);
    const bool slowing = std::hypot(wanted.x, wanted.z) < std::hypot(player.moveVelocity.x, player.moveVelocity.z);
    const float seconds = slowing ? player.moveDecelSeconds : player.moveAccelSeconds;
    const float step = seconds > 0.0f ? player.moveSpeed / seconds * dt : distance;
    if (distance <= step || distance <= 0.0f) {
        player.moveVelocity = wanted;
    } else {
        player.moveVelocity.x += gap.x / distance * step;
        player.moveVelocity.z += gap.z / distance * step;
    }
    return player.moveVelocity;
}

Vec3 StateVelocity(PlayerComponent& player, float dt) {
    if (player.state == PlayerState::Normal && player.actionLockFrames == 0) {
        return WalkVelocity(player, player.moveSpeed * std::clamp(player.externalMoveScale, 0.0f, 1.0f), dt);
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
        distance = player.knockbackDistance / static_cast<float>(player.knockbackFrames);
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
    if (player.externalTurnSpeed > 0.0f) {
        // 外部の要求（吸い込み）：ロックオン中でも、入力した方向へ決まった速さで向きを変える。入力が無ければそのまま。
        if (hasInput)
            TurnToward(player, player.moveInput, player.externalTurnSpeed, dt);
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
    if (hasTarget)
        TurnToward(player, toTarget, player.turnSpeed, dt);
    else if (hasInput)
        TurnToward(player, player.moveInput, player.turnSpeed, dt);
}

// 吹っ飛びの放物線。knockbackFrames（N F、T 秒）の半分で knockbackHeight（h）に届き、終わりに元の高さへ戻る。
// 下向きの加速度は 8h/T²。運動学ソルバーは F ごとに速度を先に減らしてから進めるので、初速を (N+1)/N 倍して
// 頂点と着地を F の刻みにそろえる（4h/T のままだと頂点が 1 割ほど低くなる）。
void KnockbackArc(const PlayerComponent& player, float& outLaunchSpeed, float& outGravity) {
    const float frames = static_cast<float>(std::max(1, player.knockbackFrames));
    const float seconds = frames / kFramesPerSecond;
    const float height = std::max(0.0f, player.knockbackHeight);
    outGravity = 8.0f * height / (seconds * seconds);
    outLaunchSpeed = 4.0f * height / seconds * (frames + 1.0f) / frames;
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
    if (frozen || player.state != PlayerState::Knockback || player.knockbackHeight <= 0.0f) {
        player.motionAccepted = world.GetMotion().SubmitKinematicMove(move);
        return;
    }
    // 吹っ飛びの間だけ重力を差し替え、最初の F に跳ね上げて放物線を描かせる。着地の判定は運動学ソルバーに任せる。
    float launchSpeed = 0.0f, gravity = 0.0f;
    KnockbackArc(player, launchSpeed, gravity);
    if (player.stateFrame == 0) {
        move.jumpRequested = 1;
        move.jumpSpeed = launchSpeed;
        move.allowJumpWithoutGroundContact = 1;
    }
    HIKARI::GameProjectKinematicMoveOptions options{};
    options.overrideVerticalGravity = 1;
    options.verticalGravityAcceleration = -gravity;
    player.motionAccepted = world.GetMotion().SubmitKinematicMoveWithOptions(move, options);
}

void StepPlayer(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                const HIKARI::FrameContext& frame) {
    PlayerRules::Initialize(player);
    if (player.pendingHitStopFrames > 0) {
        HIKARI::GameProjectTimeDilationRequest request{};
        request.sourceId = kHitStopSource;
        request.priority = kPlayerPriority;
        request.scale = 0.0f;
        request.durationUnscaledSeconds = static_cast<float>(player.pendingHitStopFrames) / kFramesPerSecond;
        player.hitStopAccepted = world.GetTimeDilation().Request(request) != 0;
        player.pendingHitStopFrames = 0;
        // ヒットストップを出す tick は止まるだけにし、吹っ飛びの跳ね上げは止まった後の最初の F に回す。
        SubmitMovement(world, object, player, frame, {}, true);
        return;
    }
    UpdateFacing(world, object, player, frame.fixedDt);
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
    if (PlayerRules::IsAlive(player)) {
        if (player.state != PlayerState::Roll && player.staminaAge >= player.staminaDelayFrames) {
            player.stamina = std::min(player.maxStamina, player.stamina + player.staminaPerSecond * frame.fixedDt);
        }
        player.staminaAge = std::min(player.staminaAge + 1, player.staminaDelayFrames);
    }
    player.advanceStateNextTick = StateDuration(player) > 0;
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
                                                // 毎 tick 失効させ、外部システムが消えても減速を残さない。
                                                PlayerRules::SetMovementSpeedScale(player, 1.0f);
                                                PlayerRules::SetTurnOverride(player, 0.0f);
                                                // 全 FixedUpdate の前に境界を決め、被弾側の実行順で無敵が一 F
                                                // ずれないようにする。
                                                if (player.advanceStateNextTick)
                                                    ++player.stateFrame;
                                                player.advanceStateNextTick = false;
                                                CompleteState(player);
                                                StartBufferedAction(world, object, player);
                                                // 行動の停止は前の tick の要求。使ったら消し、
                                                // 要求が途切れた次の tick から動けるようにする。
                                                player.externalActionBlock = false;
                                            });
}

void PlayerSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType),
        [&](HIKARI::ObjectView object, PlayerComponent& player) { StepPlayer(world, object, player, frame); });
}

void PlayerSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            PlayerCamera::Update(world, object, player, std::max(0.0f, frame.unscaledDt));
        });
}

void PlayerSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
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
            player.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), kRollFogInstance, impulse).Accepted();
        });
}

} // namespace Project_TD2_2
