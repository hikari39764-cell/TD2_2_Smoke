#include "BurnFieldVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者の中で注入を識別する。筆 1 本につき燃料 2 つ（毎フレーム番号を替える。同じ番号を続けて出すと、
// エンジンは前の位置からの線分に沿って注入を広げるので、離れた 2 か所の間まで燃えてしまう）と、霧を焼く力 1 つ。
constexpr uint64_t kFuelInstanceBase = 100;
constexpr uint64_t kFogInstanceBase = 140;
// 引爆で爆ぜた塊：燃料と、外へ押す衝撃。こちらも毎フレーム番号を替える。
constexpr uint64_t kBlastFuelBase = 160;
constexpr uint64_t kBlastPushBase = 180;
constexpr uint64_t kBlastSmokeBase = 200;
// 1 フレームに見せる塊の数。火の域が受け取れる注入は 32 個で、筆・吸い込みの力と分け合う。残りは次のフレームへ回す。
constexpr uint32_t kBlastsPerFrame = 3;
// 燃料の球（中心から (1 − r/R)² で弱まる）を床の上の半分だけ積分した体積の係数：2π／15 の半分。
constexpr float kHalfSphereShare = 0.2094f;
constexpr float kNeverFueled = 1000.0f;
// これ以上待ったセルは冷えきっているとみなし、着火用の温度で入れる。
constexpr float kColdSeconds = 0.25f;
constexpr float kMaximumStrength = 10000.0f;

struct Cell {
    int x = -1;
    int z = -1;
    bool Valid() const { return x >= 0; }
};

float Lerp(float a, float b, float t) { return a + (b - a) * t; }

float& Ago(BurnFieldComponent& field, int x, int z) {
    return field.fueledAgo[static_cast<size_t>(z) * static_cast<size_t>(field.map.Width()) + static_cast<size_t>(x)];
}

// いちばん長く燃料をもらっていない燃えているセル。
Cell Stalest(BurnFieldComponent& field) {
    Cell best{};
    float bestAgo = -1.0f;
    field.map.ForEachBurning([&](int x, int z, float) {
        const float ago = Ago(field, x, z);
        if (ago > bestAgo) {
            best = {x, z};
            bestAgo = ago;
        }
    });
    return best;
}

// center の周りのセルを「いま燃料をもらった」ことにする。
void MarkFueled(BurnFieldComponent& field, Cell at, HIKARI::GameProjectFloat3 center, float radius) {
    const BurnMap& map = field.map;
    const int span = static_cast<int>(std::ceil(radius / map.CellSize()));
    for (int z = std::max(0, at.z - span); z <= std::min(map.Height() - 1, at.z + span); ++z)
        for (int x = std::max(0, at.x - span); x <= std::min(map.Width() - 1, at.x + span); ++x) {
            const HIKARI::GameProjectFloat3 cell = map.CellCenter(x, z);
            if (std::hypot(cell.x - center.x, cell.z - center.z) <= radius)
                Ago(field, x, z) = 0.0f;
        }
}

// 塊を、場の火の域の流体で見せる。引爆で爆ぜた火（規則の BurnFieldRules::Explode が積む）は火球と爆発のエフェクト、
// 当たった所や刀の点火は小さな火球、煙の刃が当たった所や霞隠れの出入りは暗い煙の塊（ボスの演出が AddBlast で積む）。
void RenderBlasts(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field) {
    const uint32_t count = std::min<uint32_t>(field.blastCount, kBlastsPerFrame);
    if (count == 0)
        return;
    const HIKARI::FluidView fluid = world.GetFluid();
    field.blastFlip ^= 1u;
    for (uint32_t index = 0; index < count; ++index) {
        const BurnBlast& blast = field.blasts[index];
        const bool detonation = blast.kind == BurnBlastKind::Detonation;
        const bool smoke = blast.kind == BurnBlastKind::Smoke;
        const float radius = std::max(0.3f, blast.radius * (detonation ? field.blastRadiusScale : 1.0f));
        // 引爆の塊は床の上の位置で来るので、火球の中心を少し浮かせる。ほかは渡された位置のまま。
        const HIKARI::GameProjectFloat3 center{
            blast.position.x, detonation ? field.map.FloorY() + radius * 0.4f : blast.position.y, blast.position.z};
        const uint64_t slot = static_cast<uint64_t>(index) * 2u + field.blastFlip;
        const float fuelStrength = smoke ? field.smokeBlastStrength * field.smokeBlastFuelShare
                                         : field.blastFuel * (detonation ? 1.0f : field.hitFuelScale);
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = center;
        fuel.extents = {radius, std::max(0.0f, field.blastTemperature), 0.0f};
        fuel.direction = {0.0f, field.blastRise, 0.0f};
        fuel.strength = std::clamp(fuelStrength, 0.0f, kMaximumStrength);
        fuel.channels = FluidChannels::kFire;
        if (fuel.strength > 0.0f)
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastFuelBase + slot, fuel).Accepted() && field.fluidAccepted;
        if (smoke && field.smokeBlastStrength > 0.0f) {
            // 冷たい煙：火の域の暗い散乱色で、濃い煙の塊になる。
            HIKARI::GameProjectFluidInteraction puff{};
            puff.kind = Kind::Emit;
            puff.position = center;
            puff.extents = {radius, 0.0f, 0.0f};
            puff.direction = {0.0f, field.blastRise * 0.5f, 0.0f};
            puff.strength = std::min(kMaximumStrength, field.smokeBlastStrength);
            puff.channels = FluidChannels::kFire;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastSmokeBase + slot, puff).Accepted() && field.fluidAccepted;
        }
        // 向きの無い衝撃：火の域では塊を外へ膨らませ、引爆では場の霧も吹き飛ばす（当たった所の霧はボスの側が押す）。
        const float pushSpeed = smoke ? field.smokeBlastPush : field.blastPush * (detonation ? 1.0f : 0.6f);
        if (pushSpeed > 0.0f) {
            HIKARI::GameProjectFluidInteraction push{};
            push.kind = Kind::Impulse;
            push.position = center;
            push.extents = {radius * 1.5f, 0.0f, 0.0f};
            push.strength = pushSpeed;
            push.channels = detonation ? (FluidChannels::kFire | FluidChannels::kFog) : FluidChannels::kFire;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kBlastPushBase + slot, push).Accepted() && field.fluidAccepted;
        }
        if (detonation && !field.blastEffect.IsEmpty()) {
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = field.blastEffect;
            play.transform.position = center;
            const float scale = radius * field.blastEffectScale;
            play.transform.scale = {scale, scale, scale};
            play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            field.vfxAccepted = world.GetVfx().Play(play).IsValid() && field.vfxAccepted;
        }
    }
    // 見せた分を前から詰める。
    std::move(field.blasts.begin() + count, field.blasts.begin() + field.blastCount, field.blasts.begin());
    field.blastCount -= count;
}

} // namespace

void BurnFieldVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field,
                              float seconds) {
    const BurnMap& map = field.map;
    const size_t cells = static_cast<size_t>(map.Width()) * static_cast<size_t>(map.Height());
    if (field.fueledAgo.size() != cells)
        field.fueledAgo.assign(cells, kNeverFueled);
    field.fluidAccepted = true;
    field.vfxAccepted = true;
    field.activePainters = 0;
    if (seconds <= 0.0f)
        return;
    // 爆ぜた塊は、火のセルが全部消えた後のフレームにも残っているので、先に見せる。
    RenderBlasts(world, object, field);
    if (map.BurningCells() == 0)
        return;
    // 燃えているセルは待った時間を足し、燃えていないセルは「まだもらっていない」に戻す（塗られた直後がいちばん先になる）。
    for (int z = 0; z < map.Height(); ++z)
        for (int x = 0; x < map.Width(); ++x) {
            float& ago = Ago(field, x, z);
            ago = map.Remaining(x, z) > 0.0f ? std::min(kNeverFueled, ago + seconds) : kNeverFueled;
        }

    // 引爆の溜め：火そのものが熱く・細く・低くなる（白く締まって、内側へ縮んで見える）。地面や画面の予兆は出さない。
    const float charge = std::clamp(field.charge, 0.0f, 1.0f);
    // 筆の数は燃えている広さで決める（狭い火に何本も重ねない）。
    const float area = map.BurningArea();
    const int limit = std::clamp(field.painterCount, 1, static_cast<int>(field.painterFlip.size()));
    const int count = std::clamp(static_cast<int>(std::ceil(area / std::max(1.0f, field.areaPerPainter))), 1, limit);
    const float radius = std::max(0.2f, field.fuelRadius * Lerp(1.0f, field.chargeShrink, charge));
    // 筆が何本でも、火の広さあたりの燃料が同じになるように、球 1 つの強さを決める。
    const float perArea = std::max(0.0f, field.fuelPerArea) * Lerp(1.0f, field.chargeFuelScale, charge);
    const float strength = std::min(
        kMaximumStrength, perArea * area / (static_cast<float>(count) * kHalfSphereShare * radius * radius * radius));
    const float rise = field.fuelRise * Lerp(1.0f, field.chargeRiseScale, charge);
    const HIKARI::FluidView fluid = world.GetFluid();
    for (int index = 0; index < count; ++index) {
        // 前の筆が配った所は待ち時間が 0 になっているので、筆どうしは自然に離れる。
        const Cell next = Stalest(field);
        if (!next.Valid())
            break;
        const float waited = Ago(field, next.x, next.z);
        const HIKARI::GameProjectFloat3 center = map.CellCenter(next.x, next.z);
        MarkFueled(field, next, center, field.fuelMarkRadius);
        ++field.activePainters;

        // 消えかけのセルは燃料を絞る（残り fadeSeconds から細くなる）。
        const float fade = std::clamp(map.Remaining(next.x, next.z) / std::max(0.01f, field.fadeSeconds), 0.0f, 1.0f);
        // 注入は 1 フレームで、そこの温度を少ししか上げない。冷えきった所へは熱い燃料を入れて着火させ、
        // 続けて配っている所（狭い火）はふつうの温度にする。溜めの間は、溜めるほど熱くする。
        const float cold = std::clamp(waited / kColdSeconds, 0.0f, 1.0f);
        const float steady = field.fuelTemperature + (field.fuelIgniteTemperature - field.fuelTemperature) * cold;
        const float temperature = Lerp(steady, std::max(steady, field.chargeTemperature), charge);
        uint8_t& flip = field.painterFlip[static_cast<size_t>(index)];
        flip ^= 1u;
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = {center.x, map.FloorY() + field.fuelHeight, center.z};
        fuel.extents = {radius, std::max(0.0f, temperature), 0.0f};
        fuel.direction = {0.0f, rise, 0.0f};
        fuel.strength = strength * fade;
        fuel.channels = FluidChannels::kFire;
        const uint64_t id = kFuelInstanceBase + static_cast<uint64_t>(index) * 2u + flip;
        field.fluidAccepted = fluid.Submit(object.GetHandle(), id, fuel).Accepted() && field.fluidAccepted;

        // 火の下の霧を焼く：場の霧（Default）だけに届く、吸収だけの力。
        if (field.fogBurnAbsorption > 0.0f && field.fogBurnRadius > 0.0f) {
            HIKARI::GameProjectFluidInteraction burn{};
            burn.kind = Kind::Force;
            burn.position = fuel.position;
            burn.extents = {field.fogBurnRadius, 0.0f, 0.0f};
            burn.absorption = field.fogBurnAbsorption;
            burn.channels = FluidChannels::kFog;
            field.fluidAccepted =
                fluid.Submit(object.GetHandle(), kFogInstanceBase + static_cast<uint64_t>(index), burn).Accepted() &&
                field.fluidAccepted;
        }
    }
}

} // namespace Project_TD2_2
