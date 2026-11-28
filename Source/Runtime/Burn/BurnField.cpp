#include "BurnField.h"

#include <cmath>

#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BurnFieldRules.h"
#include "../Common/VectorMath.h"
#include "../Common/ComponentLookup.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::kPi;

// 最初の有効な BurnField に visit(BurnField&) を 1 回だけ呼ぶ。格子がまだなら先に作る
// （ボスは燃える地面のシステムより先に動くので、最初の tick に塗る時もここで作られる）。
template <class Visitor>
bool WithField(HIKARI::WorldView&, Visitor&& visit) {
    bool done = false;
    ComponentLookup::ForEachGame<BurnField>([&](HIKARI::ObjectView object, BurnField& field) {
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

void BurnFieldAccess::PaintSegment(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                             float width, float seconds) {
    WithField(world, [&](BurnField& field) { field.map.PaintSegment(a, b, width, seconds); });
}

void BurnFieldAccess::PaintRing(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float width,
                          float seconds) {
    WithField(world, [&](BurnField& field) { field.map.PaintRing(center, radius, width, seconds); });
}

void BurnFieldAccess::PaintDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float seconds) {
    WithField(world, [&](BurnField& field) { field.map.PaintDisc(center, radius, seconds); });
}

bool BurnFieldAccess::IsBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point) {
    bool burning = false;
    WithField(world, [&](BurnField& field) { burning = field.map.IsBurning(point); });
    return burning;
}

float BurnFieldAccess::GetBurningFraction(HIKARI::WorldView& world) {
    float fraction = 0.0f;
    WithField(world, [&](BurnField& field) {
        const float arena = kPi * field.arenaRadius * field.arenaRadius;
        fraction = arena > 0.0f ? field.map.BurningArea() / arena : 0.0f;
    });
    return fraction;
}

void BurnFieldAccess::AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                         BurnBlastKind kind) {
    WithField(world, [&](BurnField& field) {
        if (field.blastCount < field.blasts.size() && radius > 0.0f)
            field.blasts[field.blastCount++] = {position, radius, kind};
    });
}

void BurnFieldAccess::AddJet(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position,
                       HIKARI::GameProjectFloat3 direction, float scale) {
    WithField(world, [&](BurnField& field) {
        const float length = std::hypot(direction.x, direction.z);
        if (!(length > 0.0f) || !(field.jetSeconds > 0.0f) || !(scale > 0.0f))
            return;
        const HIKARI::GameProjectFloat3 forward{direction.x / length, 0.0f, direction.z / length};
        // 体の手前（攻撃の来た側）から入れ始め、入れる点が体を通り抜ける。
        const float back = 0.5f * field.jetTravel * field.jetSeconds;
        field.jet = {{position.x - forward.x * back, position.y, position.z - forward.z * back},
                     forward,
                     scale,
                     field.jetSeconds};
    });
}

} // namespace Project_TD2_2
