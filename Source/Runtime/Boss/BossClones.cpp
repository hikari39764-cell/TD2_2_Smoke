#include "BossClones.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BossCloneComponent.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossSpecialMoves.h"
#include "../Combat/DamageableRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// ロック対象の候補：ボス＋出ている分身。
constexpr size_t kTargetCapacity = kBossCloneCapacity + 1;

struct Targets {
    std::array<HIKARI::GameProjectObjectHandle, kTargetCapacity> handles{};
    std::array<Vec3, kTargetCapacity> positions{};
    size_t count = 0;

    void Add(HIKARI::GameProjectObjectHandle handle, Vec3 position) {
        if (count >= handles.size())
            return;
        handles[count] = handle;
        positions[count] = position;
        ++count;
    }
    int IndexOf(HIKARI::GameProjectObjectHandle handle) const {
        for (size_t i = 0; i < count; ++i) {
            if (handles[i].value == handle.value)
                return static_cast<int>(i);
        }
        return -1;
    }
};

void Log(const BossComponent& boss, const char* event, const char* extra) {
    char message[384]{};
    std::snprintf(message, sizeof(message), "%s tick=%llu %s", event, static_cast<unsigned long long>(boss.tick),
                  extra);
    BossLog::Write(message);
}

uint64_t HitCountOf(HIKARI::WorldView& world, HIKARI::ObjectView object) {
    uint64_t hits = 0;
    world.ForEachComponent<DamageableComponent>(HIKARI::MakeComponentTypeId(kDamageableType),
                                                [&](HIKARI::ObjectView other, DamageableComponent& health) {
                                                    if (other.GetHandle().value == object.GetHandle().value)
                                                        hits = DamageableRules::GetHitCount(health);
                                                });
    return hits;
}

void Remove(BossComponent& boss, BossCloneComponent& clone, bool puff, const char* why) {
    if (!clone.active)
        return;
    clone.active = false;
    if (puff)
        BossSpecialMoves::Puff(boss, clone.ground);
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "why=%s position=(%.3f,%.3f)", why, static_cast<double>(clone.ground.x),
                  static_cast<double>(clone.ground.z));
    Log(boss, "CloneRemove", extra);
}

// 分身の体：出ている間はボスと同じ高さに立ってプレイヤーを向き、霧から浮かび上がる。しまう時は地面の下へ。
void Pose(HIKARI::ObjectView object, const BossComponent& boss, BossCloneComponent& clone, Vec3 playerCenter,
          bool hasPlayer) {
    HIKARI::GameProjectTransform transform{};
    transform.scale = {1.0f, 1.0f, 1.0f};
    if (!clone.active) {
        transform.position = {clone.ground.x, boss.ground.y - clone.parkDepth, clone.ground.z};
        transform.rotation = YawLeanSway(clone.yaw, 0.0f, 0.0f);
        object.SetWorldTransform(transform);
        return;
    }
    ++clone.age;
    if (hasPlayer)
        clone.yaw = YawOf(Sub({playerCenter.x, clone.ground.y, playerCenter.z}, clone.ground));
    const float rise =
        std::clamp(static_cast<float>(clone.age) / static_cast<float>(std::max(1, boss.appearRiseFrames)), 0.0f, 1.0f);
    const float sink = boss.hidePreSinkDepth * (1.0f - rise);
    transform.position = {clone.ground.x, clone.ground.y + boss.floatMin - sink + boss.bodyHalfHeight, clone.ground.z};
    transform.rotation = YawLeanSway(clone.yaw, 0.0f, 0.0f);
    object.SetWorldTransform(transform);
}

// 画面の中央（プレイヤーが向いている先）に一番近い候補。ロック中のプレイヤーは対象を向いているので、向きで代える。
size_t Nearest(const Targets& targets, const PlayerComponent& player, Vec3 playerCenter) {
    const Vec3 facing = FlatNormalize(PlayerRules::GetFacing(player));
    size_t best = 0;
    float bestDot = -2.0f;
    for (size_t i = 0; i < targets.count; ++i) {
        const float d = Dot(FlatNormalize(Sub(targets.positions[i], playerCenter)), facing);
        if (d > bestDot) {
            bestDot = d;
            best = i;
        }
    }
    return best;
}

void SwitchLock(BossComponent& boss, PlayerComponent& player, const Targets& targets, bool spawned, bool lostClone,
                Vec3 playerCenter) {
    const HIKARI::GameProjectObjectHandle current = PlayerRules::GetLockTarget(player);
    const bool cycle = PlayerRules::TakeLockSwitch(player);
    if (current.value == 0 || targets.count == 0)
        return;
    int next = -1;
    if (lostClone)
        next = 0;
    if (spawned)
        next = static_cast<int>(Nearest(targets, player, playerCenter));
    if (cycle) {
        const int index = targets.IndexOf(current);
        next = (index < 0 ? 0 : index + 1) % static_cast<int>(targets.count);
    }
    if (next < 0 || targets.handles[static_cast<size_t>(next)].value == current.value)
        return;
    PlayerRules::SetLockTarget(player, targets.handles[static_cast<size_t>(next)]);
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "to=%s index=%d targets=%zu why=%s", next == 0 ? "boss" : "clone", next,
                  targets.count,
                  cycle     ? "switch"
                  : spawned ? "spawn"
                            : "lost");
    Log(boss, "LockTarget", extra);
}

} // namespace

void BossClones::Place(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    const int count =
        std::clamp(static_cast<int>(std::floor(move.size + 0.5f)), 1, static_cast<int>(kBossCloneCapacity));
    const Vec3 player = {frame.playerCenter.x, boss.ground.y, frame.playerCenter.z};
    const float distance = std::clamp(FlatLength(Sub(boss.ground, player)), boss.cloneMinDistance,
                                      std::max(boss.cloneMinDistance, boss.cloneMaxDistance));
    const float base = YawOf(Sub(boss.ground, player));
    std::array<Vec3, kBossCloneCapacity + 1> spots{};
    for (int i = 0; i <= count; ++i) {
        const float angle = base + (static_cast<float>(i) - static_cast<float>(count) * 0.5f) *
                                       (boss.cloneSpreadRadians + BossRandom(boss) * boss.cloneSpreadJitter);
        Vec3 spot = Add(player, Mul(Forward(angle), distance));
        BossMoveExecutor::ClampToArena(boss, spot);
        spots[static_cast<size_t>(i)] = spot;
    }
    // どれが本体か分からないよう、並びを混ぜてから最初の場所へ本体を移す。
    for (int i = count; i > 0; --i)
        std::swap(spots[static_cast<size_t>(i)],
                  spots[static_cast<size_t>(std::min(i, static_cast<int>(BossRandom(boss) * (i + 1))))]);
    BossSpecialMoves::Puff(boss, boss.ground, false);
    boss.ground = boss.lastBodyPosition = spots[0];
    boss.yaw = YawOf(Sub(player, boss.ground));
    BossSpecialMoves::Puff(boss, boss.ground, false);
    for (int i = 0; i < count; ++i) {
        boss.cloneSpots[static_cast<size_t>(i)] = spots[static_cast<size_t>(i + 1)];
        BossSpecialMoves::Puff(boss, spots[static_cast<size_t>(i + 1)], false);
    }
    boss.cloneSpotCount = count;
    boss.cloneSpawnPending = true;
    boss.brain.cloneReadySeconds = boss.clockSeconds + boss.cloneCooldownSeconds;
    char extra[192]{};
    std::snprintf(extra, sizeof(extra), "count=%d distance=%.3f boss=(%.3f,%.3f) readyAt=%.3f", count,
                  static_cast<double>(distance), static_cast<double>(boss.ground.x), static_cast<double>(boss.ground.z),
                  static_cast<double>(boss.brain.cloneReadySeconds));
    Log(boss, "ClonePlace", extra);
}

void BossClones::Step(HIKARI::WorldView& world, HIKARI::ObjectView bossObject, BossComponent& boss,
                      PlayerComponent* player, Vec3 playerCenter) {
    Targets targets;
    targets.Add(bossObject.GetHandle(), boss.ground);
    const HIKARI::GameProjectObjectHandle locked =
        player != nullptr ? PlayerRules::GetLockTarget(*player) : HIKARI::GameProjectObjectHandle{};
    const bool spawning = boss.cloneSpawnPending;
    const bool bossDown = boss.state == BossState::Dead;
    bool lostClone = false;
    int index = 0;
    int active = 0;
    world.ForEachComponent<BossCloneComponent>(
        HIKARI::MakeComponentTypeId(kBossCloneType), [&](HIKARI::ObjectView object, BossCloneComponent& clone) {
            const uint64_t hits = HitCountOf(world, object);
            if (!clone.initialized) {
                clone.initialized = true;
                clone.seenHits = hits;
                clone.ground = boss.ground;
            }
            if (spawning) {
                // 前の分身は霞を残さずに消し、新しい場所へ出し直す。
                Remove(boss, clone, false, "replaced");
                if (index < boss.cloneSpotCount) {
                    clone.active = true;
                    clone.ground = boss.cloneSpots[static_cast<size_t>(index)];
                    clone.untilSeconds = boss.clockSeconds + boss.cloneLifeSeconds;
                    clone.age = 0;
                    clone.seenHits = hits;
                }
                ++index;
            }
            if (clone.active && hits > clone.seenHits) {
                // 撃たれた分身は霞になって消え、本体が突進系で来る（行動ツリーの「分身を撃たれた」）。
                Remove(boss, clone, true, "shot");
                boss.brain.cloneStruck = true;
            } else if (clone.active && (bossDown || boss.clockSeconds > clone.untilSeconds)) {
                Remove(boss, clone, true, bossDown ? "bossDefeated" : "expired");
            }
            clone.seenHits = hits;
            if (!clone.active && locked.value == object.GetHandle().value)
                lostClone = true;
            Pose(object, boss, clone, playerCenter, player != nullptr);
            if (clone.active) {
                targets.Add(object.GetHandle(), clone.ground);
                ++active;
            }
        });
    if (spawning) {
        char extra[96]{};
        std::snprintf(extra, sizeof(extra), "wanted=%d placed=%d", boss.cloneSpotCount, active);
        Log(boss, "CloneSpawn", extra);
    }
    boss.cloneSpawnPending = false;
    boss.activeClones = active;
    if (player == nullptr)
        return;
    SwitchLock(boss, *player, targets, spawning && active > 0, lostClone, playerCenter);
    // ロックしていない時の照準補助も本体と分身を同じ土俵で比べられるよう、今 tick の候補を渡す（毎 tick 作り直す）。
    PlayerRules::SetLockCandidates(*player, targets.handles.data(), targets.count);
    // 霧へ沈んだボスをロックしている間は、沈む前の高さを見続ける（カメラが地面の下を向かないように）。
    const bool lockingBoss = PlayerRules::GetLockTarget(*player).value == bossObject.GetHandle().value;
    PlayerRules::SetLockPivotOffset(*player, {0.0f, lockingBoss ? boss.sink : 0.0f, 0.0f});
}

} // namespace Project_TD2_2
