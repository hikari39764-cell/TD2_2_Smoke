#include "BurnFieldSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/WorldView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BurnFieldComponent.h"
#include "BurnFieldRules.h"
#include "BurnFieldVisuals.h"
#include "../Common/GameSession.h"
#include "../Common/TraceInterval.h"
#include "../Common/VectorMath.h"
#include "../Gourd/GourdRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::kRadiansPerDegree;
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
            // 床から fireReachHeight より高い所（筋斗雲で飛んでいる時など）は触れていない。
            if (!PlayerRules::IsAlive(player) || PlayerRules::IsInvulnerable(player) ||
                transform.position.y - field.map.FloorY() > field.fireReachHeight ||
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

// 吸い込み中で物を取り込めるヒョウタンがあれば、プレイヤーの前の扇の火を近い順に消して、その面積ぶんの煙を渡し、火弾の
// 元として数える。満杯で吸えない間は火を消さない（火弾にもならない）。空きが少しでもあれば 1 tick の量を消し、
// 入りきらない煙は捨てる。ヒョウタンの「満杯でも吸える」（Gourd.absorbWhenFull）を入にすると、満杯でも消せる。
// 吸っている tick には、プレイヤーの体の火も消える（触れている時間も 0 に戻し、足元の火を吸い切るまで燃え移り直さない）。
void StepAbsorb(HIKARI::WorldView& world, BurnFieldComponent& field, const PlayerOnField& player, float dt,
                uint64_t tick) {
    bool handled = false;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            if (handled)
                return;
            handled = true;
            if (!player.found || !GourdRules::CanTakeIn(gourd)) {
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
            const float cleared =
                field.map.ClearFan(player.position, player.facing, field.absorbRange,
                                   field.absorbFanDegrees * kRadiansPerDegree, field.absorbNear, field.absorbBudget);
            if (cleared <= 0.0f)
                return;
            field.absorbBudget = std::max(0.0f, field.absorbBudget - cleared);
            field.absorbedAreaTotal += static_cast<double>(cleared);
            field.smokeGivenTotal += static_cast<double>(GourdRules::AddSmoke(gourd, cleared * perArea));
            // 火を吸った：消した面積だけ火弾へ近づける（入りきらずに捨てた煙の分も、消した火は数える）。
            GourdRules::AddAbsorbedFireArea(gourd, cleared);
        });
    if (!handled)
        field.absorbBudget = 0.0f;
}

void Trace(const BurnFieldComponent& field) {
    if (!TraceInterval::Due(field.traceEveryFrames, field.tick))
        return;
    // 爆発のエフェクトは累計（出した・均等に間引いた・窓の上限で止めた）と、窓の中に同時にあった数の最大。
    char message[576]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu cells=%zu area=%.3f contact=%.3f fireDamage=%.3f absorbedArea=%.3f smokeGiven=%.3f "
        "emitters=%d tiles=%d stride=%d fuelLength=%.2f fuelWidth=%.2f fluidAccepted=%d charge=%.3f maxGap=%d blasts=%u "
        "blastPlan=%u effects=%u thinned=%u held=%u effectPeak=%u",
        static_cast<unsigned long long>(field.tick), field.map.BurningCells(),
        static_cast<double>(field.map.BurningArea()), static_cast<double>(field.contactSeconds), field.fireDamageTotal,
        field.absorbedAreaTotal, field.smokeGivenTotal, field.activeEmitters, field.burningTiles, field.emitterStride,
        static_cast<double>(field.traceAlong), static_cast<double>(field.traceAcross),
        static_cast<int>(field.fluidAccepted), static_cast<double>(field.charge), field.maxFeedGap, field.blastCount,
        field.blastPlan, field.blastEffectsPlayed, field.blastEffectsThinned, field.blastEffectsHeld,
        field.blastEffectPeak);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Burn", message);
}

// 描画フレームの、プレイヤーの体の炎の様子（プレイヤーは 1 人）。
BurnBody SeeBody(HIKARI::WorldView& world) {
    BurnBody body;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            HIKARI::GameProjectTransform transform{};
            if (body.found || !object.GetWorldTransform(transform))
                return;
            body.found = true;
            body.position = transform.position;
            body.remaining = player.burnRemaining;
            body.endings = player.burnEndings;
            body.scatter = player.burnEndReason == PlayerBurnEnd::Roll || player.burnEndReason == PlayerBurnEnd::Absorb;
        });
    return body;
}

} // namespace

void BurnFieldSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!GameSession::IsPlayingFixed(world, frame))
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
                                                   BurnFieldVisuals::Render(world, object, field, SeeBody(world),
                                                                            frame.gameDt);
                                               });
}

} // namespace Project_TD2_2
