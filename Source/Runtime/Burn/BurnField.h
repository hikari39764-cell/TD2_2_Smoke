#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// ほかの機能（ボスの技）が燃える地面に触る入口。場面の最初の有効な BurnField を探して使うので、WorldView を取る
// （部品だけを相手にする規則は BurnFieldRules）。シーンに無ければ何もしない（燃えない・燃えていない・0 を返す）。
// 点と線分は世界座標で、高さは見ない。
class BurnField final {
public:
    static void PaintSegment(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                             float width, float seconds);
    static void PaintRing(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float width,
                          float seconds);
    static void PaintDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float seconds);
    static bool IsBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point);
    // 燃えている面積 ÷ 闘技場の円の面積（π × arenaRadius²）。
    static float GetBurningFraction(HIKARI::WorldView& world);
    // 見た目だけの塊を、場の火の域へ 1 つ出す（当たった所の火球・煙の塊）。火の格子や当たりは変えない。
    // 待ちがいっぱいなら捨てる。
    static void AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                         BurnBlastKind kind);
    // 火の刃がプレイヤーに当たった所：position（体の中心）を、direction（水平の向き）へ突き抜ける炎を出す。
    // 見た目だけで、火の格子や当たりは変えない。scale は炎の太さと量の倍率（ナイフのような小さい当たり用）。
    static void AddJet(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position,
                       HIKARI::GameProjectFloat3 direction, float scale);
};

} // namespace Project_TD2_2
