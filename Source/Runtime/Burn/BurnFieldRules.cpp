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

// 引爆で同じ tick に爆ぜたセルを足し合わせる升の番号（origin から block m ごと）。
int BlockIndex(float offset, float block) { return static_cast<int>(std::floor(offset / block)); }

// 引爆の前線が最初の輪を通る tick に、この引爆で出来る塊の数を見込む（見た目が爆発のエフェクトを均等に間引くため）。
// 前線は 1 tick に ringWidth ずつ広がり、Explode は tick ごとに爆ぜたセルを升でまとめるので、燃えているセルを
// 「何番目の輪か」と「升」の組で分けた、違う組の数が塊の数になる。前線が届く前に燃え尽きるセルの分は多めに出る。
// reach が 0 以上なら、origin から reach 未満のセルだけを数える（一か所だけの爆発）。
uint32_t PlanBlasts(BurnFieldComponent& field, HIKARI::GameProjectFloat3 origin, float ringWidth, float block,
                    float reach) {
    std::vector<uint64_t>& keys = field.blastPlanKeys;
    keys.clear();
    field.map.ForEachBurning([&](int ix, int iz, float) {
        const HIKARI::GameProjectFloat3 c = field.map.CellCenter(ix, iz);
        const float distance = std::hypot(c.x - origin.x, c.z - origin.z);
        if (reach >= 0.0f && distance >= reach)
            return;
        const uint64_t ring = static_cast<uint64_t>(distance / ringWidth);
        // 升の番号は 20 bit ずつに詰める（格子の一辺 200 m ÷ 升 0.5 m でも収まる）。
        const uint64_t bx = static_cast<uint32_t>(BlockIndex(c.x - origin.x, block)) & 0xFFFFFu;
        const uint64_t bz = static_cast<uint32_t>(BlockIndex(c.z - origin.z, block)) & 0xFFFFFu;
        keys.push_back((ring << 40) | (bx << 20) | bz);
    });
    std::sort(keys.begin(), keys.end());
    return static_cast<uint32_t>(std::unique(keys.begin(), keys.end()) - keys.begin());
}

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
    field.blastPlanKeys.reserve(static_cast<size_t>(field.map.Width()) * static_cast<size_t>(field.map.Height()));
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
    return ExplodeBand(world, origin, innerRadius, outerRadius, hitRadius, player, -1.0f, 1.0f, result);
}

bool BurnFieldRules::ExplodeDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius,
                                 float hitRadius, const HIKARI::GameProjectFloat3* player, float power,
                                 BurnExplosion& result) {
    return ExplodeBand(world, center, 0.0f, radius, hitRadius, player, radius, power, result);
}

bool BurnFieldRules::ExplodeBand(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin, float innerRadius,
                                 float outerRadius, float hitRadius, const HIKARI::GameProjectFloat3* player,
                                 float planReach, float power, BurnExplosion& result) {
    result = {};
    return WithField(world, [&](BurnFieldComponent& field) {
        std::array<BlastBlock, kBlastBlockWork> blocks{};
        size_t blockCount = 0;
        const float block = std::max(field.map.CellSize(), field.blastBlockSize);
        const float reach = std::max(0.0f, hitRadius);
        if (innerRadius <= 0.0f && outerRadius > 0.0f) {
            field.blastPlan = PlanBlasts(field, origin, outerRadius, block, planReach);
            field.blastOrder = 0;
        }
        const size_t cleared =
            field.map.ClearBand(origin, std::max(0.0f, innerRadius), outerRadius, [&](HIKARI::GameProjectFloat3 c) {
                const float distance = std::hypot(c.x - origin.x, c.z - origin.z);
                result.nearest = result.nearest < 0.0f ? distance : std::min(result.nearest, distance);
                result.farthest = std::max(result.farthest, distance);
                // 当たりは高さを見ない：プレイヤーの足元と、爆ぜたセルの中心の水平距離。
                if (player != nullptr && !result.playerCaught &&
                    (c.x - player->x) * (c.x - player->x) + (c.z - player->z) * (c.z - player->z) <= reach * reach)
                    result.playerCaught = true;
                const int bx = BlockIndex(c.x - origin.x, block);
                const int bz = BlockIndex(c.z - origin.z, block);
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
            // 捨てる塊にも番号を振る（見込みの数と同じ数え方にして、間引きの間隔をずらさない）。
            ++field.blastOrder;
            if (field.blastCount >= field.blasts.size()) {
                ++result.blastsDropped;
                continue;
            }
            const float n = static_cast<float>(b.cells);
            // 半径は、塊のセルを合わせた面積の円。
            field.blasts[field.blastCount++] = {{b.sumX / n, field.map.FloorY(), b.sumZ / n},
                                                std::sqrt(n * field.map.CellArea() / kPi),
                                                BurnBlastKind::Detonation,
                                                field.blastOrder,
                                                std::max(0.0f, power)};
            ++result.blasts;
        }
    });
}

void BurnFieldRules::SetCharge(HIKARI::WorldView& world, float charge) {
    WithField(world, [&](BurnFieldComponent& field) { field.charge = std::clamp(charge, 0.0f, 1.0f); });
}

void BurnFieldRules::SetHotspot(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius,
                                float charge) {
    WithField(world, [&](BurnFieldComponent& field) {
        field.hotspotCenter = center;
        field.hotspotRadius = std::max(0.0f, radius);
        field.hotspotCharge = std::clamp(charge, 0.0f, 1.0f);
    });
}

void BurnFieldRules::AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                              BurnBlastKind kind) {
    WithField(world, [&](BurnFieldComponent& field) {
        if (field.blastCount < field.blasts.size() && radius > 0.0f)
            field.blasts[field.blastCount++] = {position, radius, kind};
    });
}

} // namespace Project_TD2_2
