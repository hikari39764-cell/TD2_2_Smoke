#include "BurnFieldRules.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "HIKARI/Runtime/ComponentTypeId.h"

namespace Project_TD2_2 {
namespace {

constexpr float kPi = 3.14159265f;
// 1 tick に爆ぜたセルを升ごとに足し合わせる作業場所の数。前線の輪は細いので、闘技場いっぱいの火でも
// 1 tick に通る升はこれより少ない。あふれたセルは塊にせず、捨てた塊として数える。
constexpr size_t kBlastBlockWork = 64;

// 引爆で同じ tick に爆ぜた、1 つの升の中のセル。
struct BlastBlock {
    int bx = 0;
    int bz = 0;
    float sumX = 0.0f;
    float sumZ = 0.0f;
    uint32_t cells = 0;
};

// 最初の有効な BurnField に visit(BurnFieldComponent&) を 1 回だけ呼ぶ。格子がまだなら先に作る
// （ボスは燃える地面のシステムより先に動くので、最初の tick に塗る時もここで作られる）。
template <class Visitor>
bool WithField(HIKARI::WorldView& world, Visitor&& visit) {
    bool done = false;
    world.ForEachComponent<BurnFieldComponent>(HIKARI::MakeComponentTypeId(kBurnFieldType),
                                               [&](HIKARI::ObjectView object, BurnFieldComponent& field) {
                                                   if (done || !field.enabled)
                                                       return;
                                                   BurnFieldRules::Initialize(object, field);
                                                   if (!field.map.IsReady())
                                                       return;
                                                   done = true;
                                                   visit(field);
                                               });
    return done;
}

} // namespace

void BurnFieldRules::Initialize(HIKARI::ObjectView object, BurnFieldComponent& field) {
    if (field.initialized)
        return;
    field.initialized = true;
    HIKARI::GameProjectTransform transform{};
    if (!object.GetWorldTransform(transform))
        return;
    field.map.Reset(transform.position, transform.position.y, field.fieldSize, field.cellSize);
}

void BurnFieldRules::PaintSegment(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                                  float width, float seconds) {
    WithField(world, [&](BurnFieldComponent& field) { field.map.PaintSegment(a, b, width, seconds); });
}

void BurnFieldRules::PaintRing(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float width,
                               float seconds) {
    WithField(world, [&](BurnFieldComponent& field) { field.map.PaintRing(center, radius, width, seconds); });
}

void BurnFieldRules::PaintDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius,
                               float seconds) {
    WithField(world, [&](BurnFieldComponent& field) { field.map.PaintDisc(center, radius, seconds); });
}

bool BurnFieldRules::IsBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point) {
    bool burning = false;
    WithField(world, [&](BurnFieldComponent& field) { burning = field.map.IsBurning(point); });
    return burning;
}

float BurnFieldRules::GetBurningArea(HIKARI::WorldView& world) {
    float area = 0.0f;
    WithField(world, [&](BurnFieldComponent& field) { area = field.map.BurningArea(); });
    return area;
}

size_t BurnFieldRules::GetBurningCells(HIKARI::WorldView& world) {
    size_t cells = 0;
    WithField(world, [&](BurnFieldComponent& field) { cells = field.map.BurningCells(); });
    return cells;
}

float BurnFieldRules::GetBurningFraction(HIKARI::WorldView& world) {
    float fraction = 0.0f;
    WithField(world, [&](BurnFieldComponent& field) {
        const float arena = kPi * field.arenaRadius * field.arenaRadius;
        fraction = arena > 0.0f ? field.map.BurningArea() / arena : 0.0f;
    });
    return fraction;
}

float BurnFieldRules::ClearAll(HIKARI::WorldView& world) {
    float area = 0.0f;
    WithField(world, [&](BurnFieldComponent& field) { area = field.map.ClearAll(); });
    return area;
}

float BurnFieldRules::GetFarthestBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin) {
    float farthest = -1.0f;
    WithField(world, [&](BurnFieldComponent& field) { farthest = field.map.FarthestFrom(origin); });
    return farthest;
}

bool BurnFieldRules::Explode(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin, float innerRadius,
                             float outerRadius, float hitRadius, const HIKARI::GameProjectFloat3* player,
                             BurnExplosion& result) {
    result = {};
    return WithField(world, [&](BurnFieldComponent& field) {
        std::array<BlastBlock, kBlastBlockWork> blocks{};
        size_t blockCount = 0;
        const float block = std::max(field.map.CellSize(), field.blastBlockSize);
        const float reach = std::max(0.0f, hitRadius);
        const size_t cleared =
            field.map.ClearBand(origin, std::max(0.0f, innerRadius), outerRadius, [&](HIKARI::GameProjectFloat3 c) {
                const float distance = std::hypot(c.x - origin.x, c.z - origin.z);
                result.nearest = result.nearest < 0.0f ? distance : std::min(result.nearest, distance);
                result.farthest = std::max(result.farthest, distance);
                // 当たりは高さを見ない：プレイヤーの足元と、爆ぜたセルの中心の水平距離。
                if (player != nullptr && !result.playerCaught &&
                    (c.x - player->x) * (c.x - player->x) + (c.z - player->z) * (c.z - player->z) <= reach * reach)
                    result.playerCaught = true;
                const int bx = static_cast<int>(std::floor((c.x - origin.x) / block));
                const int bz = static_cast<int>(std::floor((c.z - origin.z) / block));
                auto found = std::find_if(blocks.begin(), blocks.begin() + static_cast<ptrdiff_t>(blockCount),
                                          [&](const BlastBlock& b) { return b.bx == bx && b.bz == bz; });
                if (found == blocks.begin() + static_cast<ptrdiff_t>(blockCount)) {
                    if (blockCount >= blocks.size()) {
                        ++result.blastsDropped;
                        return;
                    }
                    *found = {bx, bz, 0.0f, 0.0f, 0};
                    ++blockCount;
                }
                found->sumX += c.x;
                found->sumZ += c.z;
                ++found->cells;
            });
        result.cells = static_cast<uint32_t>(cleared);
        for (size_t k = 0; k < blockCount; ++k) {
            const BlastBlock& b = blocks[k];
            if (field.blastCount >= field.blasts.size()) {
                ++result.blastsDropped;
                continue;
            }
            const float n = static_cast<float>(b.cells);
            // 半径は、塊のセルを合わせた面積の円。
            field.blasts[field.blastCount++] = {{b.sumX / n, field.map.FloorY(), b.sumZ / n},
                                                std::sqrt(n * field.map.CellArea() / kPi)};
            ++result.blasts;
        }
    });
}

void BurnFieldRules::SetCharge(HIKARI::WorldView& world, float charge) {
    WithField(world, [&](BurnFieldComponent& field) { field.charge = std::clamp(charge, 0.0f, 1.0f); });
}

void BurnFieldRules::AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                              BurnBlastKind kind) {
    WithField(world, [&](BurnFieldComponent& field) {
        if (field.blastCount < field.blasts.size() && radius > 0.0f)
            field.blasts[field.blastCount++] = {position, radius, kind};
    });
}

} // namespace Project_TD2_2
