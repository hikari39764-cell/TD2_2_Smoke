#include "BossFlameRunVisuals.h"

#include <algorithm>

#include "HIKARI/Project/FluidView.h"
#include "BossPose.h"
#include "BossRules.h"
#include "../Burn/BurnFieldRules.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者（ボス）の中で注入を識別する。前線 1 本につき、燃料 1 つと、霧を割る力 1 つ。
constexpr uint64_t kFrontFuelBase = 1200;
constexpr uint64_t kFrontPushBase = 1210;
constexpr uint64_t kDragFuelId = 1220;
// 前線・引きずる剣先の燃料を入れる、地面からの高さ (m)。
constexpr float kFuelLift = 0.2f;
// 引きずっているとみなす、剣先の地面からの高さの上限 (m)。これより上（斬り上げた後）では、剣先の炎を出さない
// （見えない剣の炎が空中に浮いて見えないように）。
constexpr float kDragHeight = 0.8f;

void Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
            const HIKARI::GameProjectFluidInteraction& interaction) {
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, interaction).Accepted() && boss.fluidAccepted;
}

HIKARI::GameProjectFloat3 Lifted(HIKARI::GameProjectFloat3 ground) {
    return {ground.x, ground.y + kFuelLift, ground.z};
}

// 走っている前線：波頭の燃料と、霧を割る力。
void RenderFront(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, size_t index,
                 const BossFlameRunFront& front) {
    if (boss.flameRunFuel > 0.0f && boss.flameRunFuelRadius > 0.0f) {
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = Lifted(front.front);
        fuel.extents = {boss.flameRunFuelRadius, std::max(0.0f, boss.flameRunFuelTemperature), 0.0f};
        // 上へ噴き、走る向きへ少し傾ける（波頭が前へ倒れ込む）。
        fuel.direction = {front.direction.x * boss.flameRunFuelLean, boss.flameRunFuelRise,
                          front.direction.z * boss.flameRunFuelLean};
        fuel.strength = boss.flameRunFuel;
        fuel.channels = FluidChannels::kFire;
        Submit(world, object, boss, kFrontFuelBase + index, fuel);
    }
    if (boss.flameRunFogPush > 0.0f && boss.flameRunFogPushRadius > 0.0f) {
        HIKARI::GameProjectFluidInteraction push{};
        push.kind = Kind::Force;
        push.position = Lifted(front.front);
        push.extents = {boss.flameRunFogPushRadius, 0.0f, 0.0f};
        push.direction = front.direction;
        push.strength = boss.flameRunFogPush;
        push.channels = FluidChannels::kFog;
        Submit(world, object, boss, kFrontPushBase + index, push);
    }
}

// 前隙：地面を引きずる剣先から、地を這う小さな炎。
void RenderDrag(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    if (!(boss.flameRunDragFuel > 0.0f) || boss.state != BossState::Act || boss.phase != BossActPhase::Pre)
        return;
    const BossMove* move = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
    if (move == nullptr || move->type != BossMoveType::FlameRun)
        return;
    const HIKARI::GameProjectFloat3 tip = BossPose::SwordPoints(boss, boss.swordPose).outer;
    if (tip.y - boss.ground.y > kDragHeight)
        return;
    HIKARI::GameProjectFluidInteraction fuel{};
    fuel.kind = Kind::Fuel;
    fuel.position = {tip.x, boss.ground.y + kFuelLift, tip.z};
    fuel.extents = {std::max(0.1f, boss.flameRunDragRadius), std::max(0.0f, boss.flameRunFuelTemperature), 0.0f};
    fuel.strength = boss.flameRunDragFuel;
    fuel.channels = FluidChannels::kFire;
    Submit(world, object, boss, kDragFuelId, fuel);
}

} // namespace

void BossFlameRunVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    const BossFlameRunState& run = boss.flameRun;
    BossFlameRunVisualState& look = boss.flameRunVisual;
    RenderDrag(world, object, boss);
    // 放った瞬間：走り始めの所に小さな火球（技を始め直すと released は 0 へ戻る）。
    look.seenReleased = std::min(look.seenReleased, run.released);
    for (; look.seenReleased < run.released; ++look.seenReleased) {
        if (boss.flameRunReleaseBlast > 0.0f)
            BurnFieldRules::AddBlast(world, Lifted(run.fronts[static_cast<size_t>(look.seenReleased)].origin),
                                     boss.flameRunReleaseBlast, BurnBlastKind::Hit);
    }
    for (size_t index = 0; index < run.fronts.size(); ++index) {
        const BossFlameRunFront& front = run.fronts[index];
        if (front.running)
            RenderFront(world, object, boss, index, front);
        else if (look.wasRunning[index] && boss.flameRunEndBlast > 0.0f)
            // 走り切った瞬間：先端に火球。
            BurnFieldRules::AddBlast(world, Lifted(front.front), boss.flameRunEndBlast, BurnBlastKind::Hit);
        look.wasRunning[index] = front.running;
    }
}

} // namespace Project_TD2_2
