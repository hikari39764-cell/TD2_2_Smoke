#include "BurnFieldRules.h"

#include "HIKARI/Runtime/ComponentTypeId.h"

namespace Project_TD2_2 {
namespace {

constexpr float kPi = 3.14159265f;

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

} // namespace Project_TD2_2
