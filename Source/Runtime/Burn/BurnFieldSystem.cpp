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
// ヒョウタンの空きがこれより少なければ満杯と見る（飛ぶ斬撃を吸う時と同じ値。BossSystem の kAbsorbRoomEpsilon）。
constexpr float kAbsorbRoomEpsilon = 0.01f;
// 1/60 秒を足し続けた時の丸め誤差で、手傷の始まりが 1 tick 遅れないための余裕 (秒)。
constexpr float kContactSlack = 1e-4f;

struct PlayerOnField {
    bool found = false;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 facing{0.0f, 0.0f, 1.0f};
    // この FixedUpdate の中だけ使う（次の tick へ持ち越さない）。
    PlayerComponent* component = nullptr;
};

// 体（足元から contactRadius）が火に触れ続けて damageDelay 秒たつと、プレイヤーを炎上させ、火の中にいる間は
// その tick の炎上の減り方を damagePerSecond にする（減らすのは PlayerSystem の炎上）。プレイヤーは 1 人（ボスと同じ探し方）。
PlayerOnField StepFire(HIKARI::WorldView& world, BurnFieldComponent& field, float dt, uint64_t tick) {
    PlayerOnField seen;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            HIKARI::GameProjectTransform transform{};
            if (seen.found || !object.GetWorldTransform(transform))
                return;
            seen.found = true;
            seen.position = transform.position;
            seen.facing = PlayerRules::GetFacing(player);
            seen.component = &player;
            field.fireDamageTotal = player.burnDamageTotal;
            // 無敵の間（ローリングの無敵・吹っ飛び・ダウン）は触れている時間を数えず 0 に戻す：転がって火を抜ければ燃え移らない。
            if (!PlayerRules::IsAlive(player) || PlayerRules::IsInvulnerable(player) ||
                !field.map.AnyBurningWithin(transform.position, field.contactRadius)) {
                field.contactSeconds = 0.0f;
                return;
            }
            field.contactSeconds += dt;
            if (field.contactSeconds + kContactSlack < field.damageDelay)
                return;
            PlayerRules::Ignite(player, PlayerBurnCause::Ground, tick);
            player.burnInGroundFire = true;
            player.burnGroundDamagePerSecond = field.damagePerSecond;
        });
    if (!seen.found)
        field.contactSeconds = 0.0f;
    return seen;
}

// 吸い込み中のヒョウタンがあれば、プレイヤーの前の扇の火を近い順に消して、その面積ぶんの煙を渡す。
// 満杯でも火は消せる（煙は増えない）。ヒョウタンの「満杯でも吸う」（Gourd.absorbWhenFull）を切ると、満杯では消せない。
// 吸っている tick には、プレイヤーの体の火も消える（触れている時間も 0 に戻し、足元の火を吸い切るまで燃え移り直さない）。
void StepAbsorb(HIKARI::WorldView& world, BurnFieldComponent& field, const PlayerOnField& player, float dt,
                uint64_t tick) {
    bool handled = false;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            if (handled)
                return;
            handled = true;
            const float room = GourdRules::GetCapacity(gourd) - GourdRules::GetSmoke(gourd);
            const bool full = room <= kAbsorbRoomEpsilon;
            if (!player.found || !GourdRules::IsAbsorbing(gourd) || (full && !gourd.absorbWhenFull)) {
                field.absorbBudget = 0.0f;
                return;
            }
            if (PlayerRules::IsBurning(*player.component)) {
                PlayerRules::Extinguish(*player.component, PlayerBurnEnd::Absorb, tick);
                field.contactSeconds = 0.0f;
            }
            const float rate = std::max(0.0f, field.absorbAreaPerSecond) * dt;
            // 1 tick の量が 1 セルに満たなくても、ためて丸ごと消す。吸う相手が無い間にためすぎない。
            field.absorbBudget = std::min(field.absorbBudget + rate, field.map.CellArea() + rate);
            const float perArea = std::max(0.0f, field.smokePerArea);
            // 満杯でも吸う設定なら、空きに関わらず消す（入り切らない煙は捨てる）。
            const float roomArea =
                perArea > 0.0f && !gourd.absorbWhenFull ? room / perArea : std::numeric_limits<float>::max();
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
    // 燃料の点のうち最初の 1 つの、受け持つかたまりの長さと幅（炎の形の確かめ用）。
    BurnEmitter first{};
    for (const BurnEmitter& emitter : field.emitters)
        if (emitter.active) {
            first = emitter;
            break;
        }
    // 爆発のエフェクトは累計（出した・均等に間引いた・窓の上限で止めた）と、窓の中に同時にあった数の最大。
    char message[576]{};
    std::snprintf(message, sizeof(message),
                  "Trace tick=%llu cells=%zu area=%.3f contact=%.3f fireDamage=%.3f absorbedArea=%.3f smokeGiven=%.3f "
                  "emitters=%d cover=%.2f fuelLength=%.2f fuelWidth=%.2f fluidAccepted=%d charge=%.3f blasts=%u "
                  "blastPlan=%u effects=%u thinned=%u held=%u effectPeak=%u",
                  static_cast<unsigned long long>(field.tick), field.map.BurningCells(),
                  static_cast<double>(field.map.BurningArea()), static_cast<double>(field.contactSeconds),
                  field.fireDamageTotal, field.absorbedAreaTotal, field.smokeGivenTotal, field.activeEmitters,
                  static_cast<double>(field.cover), static_cast<double>(first.along), static_cast<double>(first.across),
                  static_cast<int>(field.fluidAccepted), static_cast<double>(field.charge), field.blastCount,
                  field.blastPlan, field.blastEffectsPlayed, field.blastEffectsThinned, field.blastEffectsHeld,
                  field.blastEffectPeak);
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
                                                   const PlayerOnField player =
                                                       StepFire(world, field, frame.fixedDt, frame.fixedTickIndex);
                                                   StepAbsorb(world, field, player, frame.fixedDt,
                                                              frame.fixedTickIndex);
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
