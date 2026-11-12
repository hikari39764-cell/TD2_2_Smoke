#include "GourdShotSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "GourdShotFogReturn.h"
#include "GourdShotRules.h"
#include "GourdShotVisuals.h"
#include "GourdRules.h"
#include "../Combat/DamageableRules.h"
#include "../Common/VectorMath.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::Add;
using VectorMath::Difference;
using VectorMath::Normalize;
using VectorMath::Scale;
using Vec3 = HIKARI::GameProjectFloat3;
constexpr float kPi = 3.14159265358979323846f;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

template <class Callback>
bool WithShooter(HIKARI::WorldView& world, HIKARI::ObjectView shotObject, Callback&& callback) {
    bool found = false;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView gourdObject, GourdComponent& gourd) {
            if (gourdObject.GetHandle().value != shotObject.GetHandle().value)
                return;
            const auto playerObject = world.FindSceneObject(gourd.playerId);
            if (!playerObject.IsValid())
                return;
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                        if (object.GetHandle().value != playerObject.GetHandle().value)
                                                            return;
                                                        found = true;
                                                        callback(gourd, object, player);
                                                    });
        });
    return found;
}

float SmokePerVolume(HIKARI::WorldView& world, HIKARI::ObjectView shotObject) {
    float value = 0.0f;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView object, GourdComponent& gourd) {
                                               if (object.GetHandle().value == shotObject.GetHandle().value)
                                                   value = GourdRules::GetSmokePerVolume(gourd);
                                           });
    return value;
}

Vec3 Aim(HIKARI::WorldView& world, const GourdComponent& gourd, const PlayerComponent& player) {
    const auto fallback = PlayerRules::GetFacing(player);
    const auto target = world.FindObject(PlayerRules::GetLockTarget(player));
    HIKARI::GameProjectTransform transform{};
    if (target.IsValid() && target.GetWorldTransform(transform))
        return Normalize(Difference(transform.position, gourd.position), fallback);
    return PlayerRules::FlatNormalize(fallback);
}

void Log(const GourdShotComponent& shot, const char* message) {
    if (shot.traceEveryFrames > 0)
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdShot", message);
}

void LogPhase(const GourdShotComponent& shot, const char* event) {
    char message[384]{};
    std::snprintf(
        message, sizeof(message),
        "%s tick=%llu phase=%d beginTick=%llu nextFireTick=%llu lastShotTick=%llu recoveryEnd=%llu shots=%llu", event,
        static_cast<unsigned long long>(shot.processedTick), static_cast<int>(shot.phase),
        static_cast<unsigned long long>(shot.beginTick), static_cast<unsigned long long>(shot.nextFireTick),
        static_cast<unsigned long long>(shot.lastShotTick), static_cast<unsigned long long>(shot.recoveryEndTick),
        static_cast<unsigned long long>(shot.shotsFired));
    Log(shot, message);
}

int BlockedReason(const GourdShotComponent& shot, const GourdComponent& gourd, const PlayerComponent& player) {
    if (shot.staminaCost > 0.0f && PlayerRules::GetStamina(player) <= 0.0f)
        return 1;
    if (shot.smokeCost > 0.0f && GourdRules::GetSmoke(gourd) < shot.smokeCost)
        return 2;
    const auto free =
        std::count_if(shot.projectiles.begin(), shot.projectiles.end(), [](const GourdProjectile& projectile) {
            return !projectile.active && !projectile.finishTrailPending && !projectile.trail.IsValid();
        });
    return free < std::clamp(shot.pelletsPerShot, 1, static_cast<int>(kGourdProjectileCapacity)) ? 3 : 0;
}

bool CanPay(GourdShotComponent& shot, const GourdComponent& gourd, const PlayerComponent& player) {
    const int reason = BlockedReason(shot, gourd, player);
    if (reason != 0 && reason != shot.blockedReason) {
        char message[256]{};
        std::snprintf(message, sizeof(message), "Blocked tick=%llu reason=%d stamina=%.3f smoke=%.6f",
                      static_cast<unsigned long long>(shot.processedTick), reason,
                      static_cast<double>(PlayerRules::GetStamina(player)),
                      static_cast<double>(GourdRules::GetSmoke(gourd)));
        Log(shot, message);
    }
    shot.blockedReason = reason;
    return reason == 0;
}

void SpawnPellets(GourdShotComponent& shot, HIKARI::GameProjectObjectHandle player, Vec3 origin, Vec3 direction,
                  float groundClearance) {
    const int count = std::clamp(shot.pelletsPerShot, 1, static_cast<int>(kGourdProjectileCapacity));
    int pellet = 0;
    for (auto& projectile : shot.projectiles) {
        if (projectile.active || projectile.finishTrailPending || projectile.trail.IsValid() || pellet >= count)
            continue;
        const float fraction = count > 1 ? static_cast<float>(pellet) / static_cast<float>(count - 1) - 0.5f : 0.0f;
        const float angle = fraction * shot.spreadDegrees * kPi / 180.0f;
        const float cosine = std::cos(angle), sine = std::sin(angle);
        // 冷却中の炎の域が残っていても発射間隔は延ばさない。古い域は表現側が次の更新で止める。
        const auto staleShell = projectile.shell.IsValid() ? projectile.shell : projectile.staleShell;
        projectile = {};
        projectile.staleShell = staleShell;
        projectile.active = true;
        projectile.id = shot.nextProjectileId++;
        projectile.spawnedTick = shot.processedTick;
        projectile.origin = origin;
        projectile.position = origin;
        projectile.direction = {direction.x * cosine + direction.z * sine, direction.y,
                                direction.z * cosine - direction.x * sine};
        projectile.ignoredPlayer = player;
        projectile.speed = shot.initialSpeed;
        projectile.radius = shot.projectileRadius;
        projectile.groundClearance = groundClearance;
        ++pellet;
    }
}

bool Fire(HIKARI::WorldView& world, GourdShotComponent& shot, GourdComponent& gourd, HIKARI::ObjectView playerObject,
          PlayerComponent& player) {
    if (!CanPay(shot, gourd, player))
        return false;
    const float staminaBefore = PlayerRules::GetStamina(player), smokeBefore = GourdRules::GetSmoke(gourd);
    // 両方を先に照合する。煙不足でスタミナだけを失わず、0 の費用は制限そのものを外す。
    if (shot.staminaCost > 0.0f && !PlayerRules::TryConsumeStamina(player, shot.staminaCost, true))
        return false;
    if (shot.smokeCost > 0.0f && !GourdRules::TryConsumeSmoke(gourd, shot.smokeCost))
        return false;
    const Vec3 direction = Aim(world, gourd, player);
    const Vec3 origin = Add(gourd.position, Scale(direction, gourd.height * 0.5f));
    Vec3 ground{};
    const bool grounded =
        GourdShotFogReturn::FindGround(world, origin, shot.impactGroundReach, playerObject.GetHandle().value, ground);
    SpawnPellets(shot, playerObject.GetHandle(), origin, direction, grounded ? origin.y - ground.y : -1.0f);
    GourdShotVisuals::QueueBurst(shot, true, origin, direction, shot.muzzleScale);
    GourdShotFogReturn::MarkFire(shot, origin, shot.muzzleFireFrames);
    PlayerRules::LockActions(player, shot.recoveryFrames);
    PlayerRules::AddRecoil(player, Scale(direction, -1.0f), shot.recoilDistance, shot.recoilFrames);
    GourdRules::AddRecoil(gourd, Scale(direction, -1.0f), shot.gourdRecoilDistance, shot.gourdRecoilFrames);
    shot.phase = GourdShotPhase::Recovery;
    shot.lastShotTick = shot.processedTick;
    shot.nextFireTick = shot.processedTick + static_cast<uint64_t>(shot.repeatFrames);
    shot.recoveryEndTick = shot.processedTick + static_cast<uint64_t>(shot.recoveryFrames);
    ++shot.shotsFired;
    shot.attackBuffer = 0;
    shot.muzzlePending = true;
    shot.muzzlePosition = origin;
    shot.muzzleDirection = direction;
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "Shot tick=%llu ordinal=%llu pellets=%d staminaBefore=%.3f stamina=%.3f smokeBefore=%.6f smoke=%.6f "
                  "nextFireTick=%llu recoveryEnd=%llu position=(%.6f,%.6f,%.6f) direction=(%.6f,%.6f,%.6f)",
                  static_cast<unsigned long long>(shot.processedTick), static_cast<unsigned long long>(shot.shotsFired),
                  shot.pelletsPerShot, static_cast<double>(staminaBefore),
                  static_cast<double>(PlayerRules::GetStamina(player)), static_cast<double>(smokeBefore),
                  static_cast<double>(GourdRules::GetSmoke(gourd)), static_cast<unsigned long long>(shot.nextFireTick),
                  static_cast<unsigned long long>(shot.recoveryEndTick), static_cast<double>(origin.x),
                  static_cast<double>(origin.y), static_cast<double>(origin.z), static_cast<double>(direction.x),
                  static_cast<double>(direction.y), static_cast<double>(direction.z));
    Log(shot, message);
    return true;
}

void StepPhase(HIKARI::WorldView& world, GourdShotComponent& shot, GourdComponent& gourd,
               HIKARI::ObjectView playerObject, PlayerComponent& player) {
    // 自分の動作ロック中は CanStartAction が false になるので、継続は通常状態と生存を確認する。
    if (!shot.enabled || !gourd.enabled || !PlayerRules::IsAlive(player) || player.state != PlayerState::Normal) {
        if (shot.phase != GourdShotPhase::Idle)
            LogPhase(shot, "Interrupted");
        shot.phase = GourdShotPhase::Idle;
        return;
    }
    if (shot.phase == GourdShotPhase::Recovery && shot.processedTick >= shot.recoveryEndTick) {
        shot.phase = GourdShotPhase::Idle;
        LogPhase(shot, "End");
    }
    // 押したのに撃てない時（煙・スタミナ不足）は、無反応にせず空撃ちの手応えを返す。
    if (shot.phase == GourdShotPhase::Idle && shot.attackPressed && PlayerRules::CanStartAction(player) &&
        !GourdRules::IsAbsorbBusy(gourd) && BlockedReason(shot, gourd, player) != 0) {
        ++shot.dryFires;
        GourdRules::AddRecoil(gourd, Scale(Aim(world, gourd, player), -1.0f), shot.feedback.dryFireKickDistance,
                              shot.feedback.dryFireKickFrames);
    }
    if (shot.phase == GourdShotPhase::Idle && (shot.attackHeld || shot.attackBuffer > 0) &&
        PlayerRules::CanStartAction(player) && !GourdRules::IsAbsorbBusy(gourd) && CanPay(shot, gourd, player)) {
        shot.phase = GourdShotPhase::Windup;
        shot.beginTick = shot.processedTick;
        shot.nextFireTick = shot.beginTick + static_cast<uint64_t>(shot.windupFrames);
        shot.attackBuffer = 0;
        // 発射 tick の Player::PreFixedUpdate が先に走るため、その境界までロックをつなぐ。
        // 発射時刻は増やさず、同じ tick の Fire が後隙のロックに置き換える。
        PlayerRules::LockActions(player, shot.windupFrames + 1);
        // 押した瞬間から構えが見えるように、発射までヒョウタンを後ろへ引く。発射の反動がそこから続く。
        GourdRules::AddRecoil(gourd, Scale(Aim(world, gourd, player), -1.0f), shot.gourdWindupPull,
                              shot.windupFrames);
        LogPhase(shot, "Begin");
    }
    if (shot.phase == GourdShotPhase::Windup && shot.processedTick >= shot.nextFireTick) {
        if (!Fire(world, shot, gourd, playerObject, player))
            shot.phase = GourdShotPhase::Idle;
    } else if (shot.phase == GourdShotPhase::Recovery && shot.processedTick >= shot.nextFireTick &&
               (shot.attackHeld || shot.attackBuffer > 0)) {
        Fire(world, shot, gourd, playerObject, player);
    }
}

void AddResidue(HIKARI::WorldView& world, GourdShotComponent& shot, Vec3 position, uint64_t ignoredObject) {
    auto& residue = shot.residues[shot.residueCursor % kGourdResidueCapacity];
    ++shot.residueCursor;
    if (residue.smoke.IsValid() && world.GetVfx().IsActive(residue.smoke))
        shot.vfxAccepted = world.GetVfx().Stop(residue.smoke) && shot.vfxAccepted;
    residue.smoke = {};
    residue.active = true;
    // 霧を押す力は重なる流体の域すべてに効く。爆ぜた点で押すと炎の域の中身まで壁へ吹き飛ぶので、
    // 真下の地面を探して、足元の霧だけを押す（高い所ほど弱く、届かなければ押さない）。
    Vec3 ground{};
    const bool grounded =
        GourdShotFogReturn::FindGround(world, position, shot.impactGroundReach, ignoredObject, ground);
    residue.burstPending = grounded;
    residue.burstScale =
        grounded ? std::clamp(1.0f - (position.y - ground.y) / shot.impactGroundReach, 0.0f, 1.0f) : 0.0f;
    if (grounded)
        position = {ground.x, ground.y + shot.impactGroundOffset, ground.z};
    residue.framesLeft = std::max(0, shot.residueFrames);
    residue.position = position;
}

bool ApplyHit(HIKARI::WorldView& world, uint64_t target, float amount, Vec3 direction, float& health) {
    bool accepted = false;
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType), [&](HIKARI::ObjectView object, DamageableComponent& damageable) {
            if (object.GetHandle().value == target) {
                accepted = DamageableRules::ApplyDamage(damageable, amount, direction);
                health = DamageableRules::GetHealth(damageable);
            }
        });
    return accepted;
}

void LogProjectile(const GourdShotComponent& shot, const GourdProjectile& projectile, const char* event,
                   uint64_t hitObject = 0, bool damageAccepted = false, float health = 0.0f) {
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "%s tick=%llu id=%llu distance=%.6f speed=%.6f radius=%.6f density=%.6f damage=%.6f "
                  "hitObject=%llu damageAccepted=%d health=%.6f position=(%.6f,%.6f,%.6f)",
                  event, static_cast<unsigned long long>(shot.processedTick),
                  static_cast<unsigned long long>(projectile.id), static_cast<double>(projectile.distance),
                  static_cast<double>(projectile.speed), static_cast<double>(projectile.radius),
                  static_cast<double>(projectile.densityScale),
                  static_cast<double>(GourdShotRules::GetDamageAtDistance(shot, projectile.distance)),
                  static_cast<unsigned long long>(hitObject), static_cast<int>(damageAccepted),
                  static_cast<double>(health), static_cast<double>(projectile.position.x),
                  static_cast<double>(projectile.position.y), static_cast<double>(projectile.position.z));
    Log(shot, message);
}

void StepProjectile(HIKARI::WorldView& world, HIKARI::ObjectView owner, GourdShotComponent& shot,
                    GourdProjectile& projectile, float dt) {
    if (projectile.spawnedTick == shot.processedTick) {
        // 同じ描画フレームで砲口に源を作り、次の移動から最初の区間も掃掠する。
        LogProjectile(shot, projectile, "ProjectileSpawn");
        return;
    }
    const float speed = GourdShotRules::GetSpeedAtDistance(shot, projectile.distance);
    const float step = std::min(speed * dt, std::max(0.0f, shot.range - projectile.distance));
    HIKARI::GameProjectPhysicsShapeCastQuery query{};
    query.shape.type = HIKARI::GameProjectPhysicsShapeType::Sphere;
    query.shape.radius = GourdShotRules::GetRadiusAtDistance(shot, projectile.distance);
    query.startPosition = projectile.position;
    query.direction = projectile.direction;
    query.maxDistance = step;
    query.filter.ignoredObjectHandle = projectile.ignoredPlayer.value;
    query.filter.includeTriggers = 0;
    HIKARI::GameProjectPhysicsHit hit{};
    const bool didHit = step > 0.0f && world.GetPhysicsQueries().ShapeCast(query, hit) &&
                        hit.objectHandle != owner.GetHandle().value &&
                        hit.objectHandle != projectile.ignoredPlayer.value;
    const float traveled = didHit ? std::clamp(hit.distance, 0.0f, step) : step;
    projectile.distance = std::min(shot.range, projectile.distance + traveled);
    projectile.position = Add(projectile.position, Scale(projectile.direction, traveled));
    projectile.speed = GourdShotRules::GetSpeedAtDistance(shot, projectile.distance);
    projectile.radius = GourdShotRules::GetRadiusAtDistance(shot, projectile.distance);
    projectile.densityScale = GourdShotRules::GetDensityAtDistance(shot, projectile.distance);
    if (didHit) {
        float health = 0.0f;
        const bool accepted =
            ApplyHit(world, hit.objectHandle, GourdShotRules::GetDamageAtDistance(shot, projectile.distance),
                     projectile.direction, health);
        LogProjectile(shot, projectile, "ProjectileHit", hit.objectHandle, accepted, health);
        projectile.active = false;
        projectile.finishTrailPending = true;
        // 遠くで当たるほど威力が落ちる規則に合わせ、命中の派手さも射程端へ向けて抑える。
        const float farBlend = GourdShotRules::GetFalloffFraction(shot, projectile.distance);
        // 体に重なる所で爆ぜると隠れて小さく暗く見えるので、飛んで来た距離を超えない範囲で手前へ戻す。
        const float standoff = std::min(shot.impactStandoff, projectile.distance);
        const Vec3 burst = Add(projectile.position, Scale(projectile.direction, -standoff));
        GourdShotVisuals::QueueBurst(shot, false, burst, projectile.direction,
                                     shot.impactScale * (1.0f - (1.0f - shot.impactFarScale) * farBlend));
        GourdShotFogReturn::MarkFire(shot, burst, shot.impactFireFrames);
        projectile.flashFramesLeft = shot.shellFlashFrames;
        projectile.fadeFramesLeft = std::max(shot.shellFadeFrames, shot.shellFlashFrames);
        AddResidue(world, shot, projectile.position, hit.objectHandle);
        GourdShotFogReturn::Queue(world, shot, projectile, hit.objectHandle);
    } else if (projectile.distance >= shot.range) {
        LogProjectile(shot, projectile, "Dissipate");
        projectile.active = false;
        projectile.finishTrailPending = true;
        projectile.fadeFramesLeft = shot.shellFadeFrames;
        AddResidue(world, shot, projectile.position, 0);
        GourdShotFogReturn::Queue(world, shot, projectile, 0);
    } else if (shot.traceEveryFrames > 0 && shot.processedTick % static_cast<uint64_t>(shot.traceEveryFrames) == 0) {
        LogProjectile(shot, projectile, "ProjectileStep");
    }
}

} // namespace

void GourdShotSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    HIKARI::GameProjectInputActionState action{};
    const bool valid = world.GetInput().GetAction("Gameplay.Attack", action);
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   shot.attackHeld = valid && action.down;
                                                   if (valid && action.pressed) {
                                                       shot.attackBuffer = shot.inputBufferFrames;
                                                       shot.attackPressed = true;
                                                   }
                                               });
}

void GourdShotSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            shot.processedTick = frame.fixedTickIndex;
            const bool found = WithShooter(
                world, object, [&](GourdComponent& gourd, HIKARI::ObjectView playerObject, PlayerComponent& player) {
                    StepPhase(world, shot, gourd, playerObject, player);
                });
            if (!found)
                shot.phase = GourdShotPhase::Idle;
            shot.attackPressed = false;
            shot.attackBuffer = std::max(0, shot.attackBuffer - 1);
        });
}

void GourdShotSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            GourdShotFogReturn::Step(shot);
            for (auto& residue : shot.residues) {
                if (residue.active) {
                    residue.framesLeft = std::max(0, residue.framesLeft - 1);
                    if (residue.framesLeft <= 0 && !residue.burstPending)
                        residue.active = false;
                }
            }
            for (auto& projectile : shot.projectiles) {
                projectile.flashFramesLeft = std::max(0, projectile.flashFramesLeft - 1);
                projectile.fadeFramesLeft = std::max(0, projectile.fadeFramesLeft - 1);
                if (projectile.active)
                    StepProjectile(world, object, shot, projectile, frame.fixedDt);
            }
        });
}

void GourdShotSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    uint32_t trails = 0;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
            if (GourdShotRules::IsFiring(shot)) {
                WithShooter(world, object, [&](GourdComponent& gourd, HIKARI::ObjectView, PlayerComponent& player) {
                    GourdRules::SetAimOverride(gourd, Aim(world, gourd, player));
                });
            }
            GourdShotVisuals::Update(world, object, shot, trails);
        });
}

void GourdShotSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    uint32_t submitted = 0;
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
                                                   GourdShotVisuals::Render(world, object, shot, submitted);
                                                   GourdShotFogReturn::Render(world, object, shot, submitted,
                                                                              SmokePerVolume(world, object));
                                               });
}

void GourdShotSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   GourdShotVisuals::Stop(world, shot);
                                                   GourdShotFogReturn::Clear(shot);
                                               });
}

} // namespace Project_TD2_2
