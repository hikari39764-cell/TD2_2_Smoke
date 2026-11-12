#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// ほかの機能（ボスの技）が燃える地面に触る入口。どれも最初の有効な BurnField を使い、
// シーンに無ければ何もしない（燃えない・燃えていない・0 を返す）。点と線分は世界座標で、高さは見ない。
class BurnFieldRules final {
public:
    static void PaintSegment(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                             float width, float seconds);
    static void PaintRing(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float width,
                          float seconds);
    static void PaintDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float seconds);
    static bool IsBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point);
    // 燃えている面積 (m²)。
    static float GetBurningArea(HIKARI::WorldView& world);
    // 燃えている面積 ÷ 闘技場の円の面積（π × arenaRadius²）。
    static float GetBurningFraction(HIKARI::WorldView& world);
    // 全部消す。消した面積 (m²) を返す。
    static float ClearAll(HIKARI::WorldView& world);
    // 格子が出来ていなければ、持ち主の物体の位置で作る（BurnFieldSystem が最初の固定 tick に呼ぶ）。
    static void Initialize(HIKARI::ObjectView object, BurnFieldComponent& field);
};

} // namespace Project_TD2_2
