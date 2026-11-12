#include "BurnFieldVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using Kind = HIKARI::GameProjectFluidInteractionKind;

// 同じ所有者の中で注入を識別する。筆 1 本につき燃料 2 つ（毎フレーム番号を替える。同じ番号を続けて出すと、
// エンジンは前の位置からの線分に沿って注入を広げるので、離れた 2 か所の間まで燃えてしまう）と、霧を焼く力 1 つ。
constexpr uint64_t kFuelInstanceBase = 100;
constexpr uint64_t kFogInstanceBase = 140;
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

} // namespace

void BurnFieldVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field,
                              float seconds) {
    const BurnMap& map = field.map;
    const size_t cells = static_cast<size_t>(map.Width()) * static_cast<size_t>(map.Height());
    if (field.fueledAgo.size() != cells)
        field.fueledAgo.assign(cells, kNeverFueled);
    field.fluidAccepted = true;
    field.activePainters = 0;
    if (map.BurningCells() == 0 || seconds <= 0.0f)
        return;
    // 燃えているセルは待った時間を足し、燃えていないセルは「まだもらっていない」に戻す（塗られた直後がいちばん先になる）。
    for (int z = 0; z < map.Height(); ++z)
        for (int x = 0; x < map.Width(); ++x) {
            float& ago = Ago(field, x, z);
            ago = map.Remaining(x, z) > 0.0f ? std::min(kNeverFueled, ago + seconds) : kNeverFueled;
        }

    // 筆の数は燃えている広さで決める（狭い火に何本も重ねない）。
    const float area = map.BurningArea();
    const int limit = std::clamp(field.painterCount, 1, static_cast<int>(field.painterFlip.size()));
    const int count = std::clamp(static_cast<int>(std::ceil(area / std::max(1.0f, field.areaPerPainter))), 1, limit);
    const float radius = std::max(0.2f, field.fuelRadius);
    // 筆が何本でも、火の広さあたりの燃料が同じになるように、球 1 つの強さを決める。
    const float strength =
        std::min(kMaximumStrength, std::max(0.0f, field.fuelPerArea) * area /
                                       (static_cast<float>(count) * kHalfSphereShare * radius * radius * radius));
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
        // 続けて配っている所（狭い火）はふつうの温度にする。
        const float cold = std::clamp(waited / kColdSeconds, 0.0f, 1.0f);
        const float temperature = field.fuelTemperature + (field.fuelIgniteTemperature - field.fuelTemperature) * cold;
        uint8_t& flip = field.painterFlip[static_cast<size_t>(index)];
        flip ^= 1u;
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = Kind::Fuel;
        fuel.position = {center.x, map.FloorY() + field.fuelHeight, center.z};
        fuel.extents = {radius, std::max(0.0f, temperature), 0.0f};
        fuel.direction = {0.0f, field.fuelRise, 0.0f};
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
