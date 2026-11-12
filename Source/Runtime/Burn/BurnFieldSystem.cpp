#include "BurnFieldSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/WorldView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BurnFieldComponent.h"
#include "BurnFieldRules.h"
#include "BurnFieldVisuals.h"
#include "../Gourd/GourdRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

constexpr float kRadiansPerDegree = 3.14159265f / 180.0f;
// ヒョウタンの空きがこれより少なければ火は吸えない（飛ぶ斬撃を吸う時と同じ値。BossSystem の kAbsorbRoomEpsilon）。
constexpr float kAbsorbRoomEpsilon = 0.01f;
// 1/60 秒を足し続けた時の丸め誤差で、手傷の始まりが 1 tick 遅れないための余裕 (秒)。
constexpr float kContactSlack = 1e-4f;

struct PlayerOnField {
    bool found = false;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 facing{0.0f, 0.0f, 1.0f};
};

// 火の上に立っている間、damageDelay 秒たってから毎 tick HP を減らす。プレイヤーは 1 人（ボスと同じ探し方）。
PlayerOnField StepFire(HIKARI::WorldView& world, BurnFieldComponent& field, float dt) {
    PlayerOnField seen;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            HIKARI::GameProjectTransform transform{};
            if (seen.found || !object.GetWorldTransform(transform))
                return;
            seen.found = true;
            seen.position = transform.position;
            seen.facing = PlayerRules::GetFacing(player);
            if (!PlayerRules::IsAlive(player) || !field.map.IsBurning(transform.position)) {
                field.contactSeconds = 0.0f;
                return;
            }
            // 無敵の間（ローリングなど）も立っている時間は数え続け、明けた tick から減らす。
            field.contactSeconds += dt;
            if (field.contactSeconds + kContactSlack < field.damageDelay || field.damagePerSecond <= 0.0f)
                return;
            const float before = PlayerRules::GetHealth(player);
            if (PlayerRules::ApplyDamageOverTime(player, field.damagePerSecond * dt))
                field.fireDamageTotal += static_cast<double>(before - PlayerRules::GetHealth(player));
        });
    if (!seen.found)
        field.contactSeconds = 0.0f;
    return seen;
}

// 吸い込み中で煙が入るヒョウタンがあれば、プレイヤーの前の扇の火を近い順に消して、その面積ぶんの煙を渡す。
void StepAbsorb(HIKARI::WorldView& world, BurnFieldComponent& field, const PlayerOnField& player, float dt) {
    bool handled = false;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            if (handled)
                return;
            handled = true;
            const float room = GourdRules::GetCapacity(gourd) - GourdRules::GetSmoke(gourd);
            if (!player.found || !GourdRules::IsAbsorbing(gourd) || room <= kAbsorbRoomEpsilon) {
                field.absorbBudget = 0.0f;
                return;
            }
            const float rate = std::max(0.0f, field.absorbAreaPerSecond) * dt;
            // 1 tick の量が 1 セルに満たなくても、ためて丸ごと消す。吸う相手が無い間にためすぎない。
            field.absorbBudget = std::min(field.absorbBudget + rate, field.map.CellArea() + rate);
            const float perArea = std::max(0.0f, field.smokePerArea);
            const float roomArea = perArea > 0.0f ? room / perArea : std::numeric_limits<float>::max();
            const float cleared = field.map.ClearFan(player.position, player.facing, field.absorbRange,
                                                     field.absorbFanDegrees * kRadiansPerDegree, field.absorbNear,
                                                     std::min(field.absorbBudget, roomArea));
            if (cleared <= 0.0f)
                return;
            field.absorbBudget = std::max(0.0f, field.absorbBudget - cleared);
            field.absorbedAreaTotal += static_cast<double>(cleared);
            field.smokeGivenTotal += static_cast<double>(GourdRules::AddSmoke(gourd, cleared * perArea));
        });
    if (!handled)
        field.absorbBudget = 0.0f;
}

void Trace(const BurnFieldComponent& field) {
    if (field.traceEveryFrames <= 0 || field.tick % static_cast<uint64_t>(field.traceEveryFrames) != 0)
        return;
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "Trace tick=%llu cells=%zu area=%.3f contact=%.3f fireDamage=%.3f absorbedArea=%.3f smokeGiven=%.3f "
                  "painters=%d fluidAccepted=%d",
                  static_cast<unsigned long long>(field.tick), field.map.BurningCells(),
                  static_cast<double>(field.map.BurningArea()), static_cast<double>(field.contactSeconds),
                  field.fireDamageTotal, field.absorbedAreaTotal, field.smokeGivenTotal, field.activePainters,
                  static_cast<int>(field.fluidAccepted));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Burn", message);
}

} // namespace

void BurnFieldSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused || frame.fixedDt <= 0.0f)
        return;
    bool done = false;
    world.ForEachComponent<BurnFieldComponent>(HIKARI::MakeComponentTypeId(kBurnFieldType),
                                               [&](HIKARI::ObjectView object, BurnFieldComponent& field) {
                                                   if (done || !field.enabled)
                                                       return;
                                                   BurnFieldRules::Initialize(object, field);
                                                   if (!field.map.IsReady())
                                                       return;
                                                   done = true;
                                                   ++field.tick;
                                                   field.map.Step(frame.fixedDt);
                                                   const PlayerOnField player = StepFire(world, field, frame.fixedDt);
                                                   StepAbsorb(world, field, player, frame.fixedDt);
                                                   Trace(field);
                                               });
}

void BurnFieldSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr)
        return;
    bool done = false;
    world.ForEachComponent<BurnFieldComponent>(HIKARI::MakeComponentTypeId(kBurnFieldType),
                                               [&](HIKARI::ObjectView object, BurnFieldComponent& field) {
                                                   // 規則と同じく、最初の有効な 1 つだけを見せる。
                                                   if (done || !field.enabled || !field.map.IsReady())
                                                       return;
                                                   done = true;
                                                   BurnFieldVisuals::Render(world, object, field, frame.gameDt);
                                               });
}

} // namespace Project_TD2_2
