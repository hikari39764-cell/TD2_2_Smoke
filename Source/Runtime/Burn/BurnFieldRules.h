#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// 引爆の前線が 1 tick に通った輪の結果。
struct BurnExplosion {
    // 爆ぜて消えたセルの数。
    uint32_t cells = 0;
    // 爆ぜたセルのどれかから hitRadius 以内にプレイヤーの足元があった。
    bool playerCaught = false;
    // 見た目へ渡せた塊の数と、受け渡しの枠があふれて捨てた塊の数。
    uint32_t blasts = 0;
    uint32_t blastsDropped = 0;
    // 爆ぜたセルの中心の、origin からの最小・最大の距離 (m。爆ぜたセルが無ければ負)。
    float nearest = -1.0f;
    float farthest = -1.0f;
};

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
    // 燃えている面積 (m²) と、燃えているセルの数。
    static float GetBurningArea(HIKARI::WorldView& world);
    static size_t GetBurningCells(HIKARI::WorldView& world);
    // 燃えている面積 ÷ 闘技場の円の面積（π × arenaRadius²）。
    static float GetBurningFraction(HIKARI::WorldView& world);
    // 全部消す。消した面積 (m²) を返す。
    static float ClearAll(HIKARI::WorldView& world);
    // origin から一番遠い燃えているセルの中心までの水平距離 (m)。燃えていなければ負。
    static float GetFarthestBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin);
    // 引爆の前線の 1 tick：origin からの距離が innerRadius 以上 outerRadius 未満の燃えているセルを爆ぜさせて消す。
    // player（足元。nullptr はプレイヤーなし）が爆ぜたセルのどれかから hitRadius 以内なら playerCaught。
    // 爆ぜたセルは見た目への受け渡し（BurnFieldComponent::blasts）へ、blastBlockSize の升ごとに 1 塊で足す。
    // 燃える地面が無ければ false（何も爆ぜない）。
    static bool Explode(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin, float innerRadius,
                        float outerRadius, float hitRadius, const HIKARI::GameProjectFloat3* player,
                        BurnExplosion& result);
    // 引爆の溜めの進み（0〜1）を見た目へ渡す（BurnFieldComponent::charge）。規則は読まない。
    static void SetCharge(HIKARI::WorldView& world, float charge);
    // 見た目だけの塊を、場の火の域へ 1 つ出す（当たった所の火球・煙の塊）。火の格子や当たりは変えない。
    // 待ちがいっぱいなら捨てる。
    static void AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                         BurnBlastKind kind);
    // 格子が出来ていなければ、持ち主の物体の位置で作る（BurnFieldSystem が最初の固定 tick に呼ぶ）。
    static void Initialize(HIKARI::ObjectView object, BurnFieldComponent& field);
};

} // namespace Project_TD2_2
