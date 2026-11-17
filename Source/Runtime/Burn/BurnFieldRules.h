#pragma once

#include <array>

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// 引爆で 1 tick に爆ぜたセルを、塊（blastBlockSize の升）にまとめられる数。前線の輪は細いので、闘技場いっぱいの火でも
// 1 tick に通る升はこれより少ない。あふれたセルは塊にせず、捨てた塊として数える。
inline constexpr size_t kBurnExplosionBlockCapacity = 64;

// 引爆の前線が 1 tick に通った輪の結果。
struct BurnExplosion {
    // 爆ぜて消えたセルの数。
    uint32_t cells = 0;
    // プレイヤーの足元から、この tick に爆ぜたセルの中心までの最小の水平距離 (m。爆ぜたセルかプレイヤーが無ければ負)。
    float playerDistance = -1.0f;
    // その距離で決まる輪：hitRadius × BurnField.blastCoreShare 以内は Core、hitRadius 以内は Edge、その外は None。
    BurnBlastZone zone = BurnBlastZone::None;
    // その輪のダメージの割合（Core は 1、Edge は BurnField.blastEdgeDamageShare、None は 0）。
    float damageShare = 0.0f;
    // 見た目へ渡せた塊の数と、受け渡しの枠があふれて捨てた塊の数。
    uint32_t blasts = 0;
    uint32_t blastsDropped = 0;
    // 爆ぜたセルの中心の、origin からの最小・最大の距離 (m。爆ぜたセルが無ければ負)。
    float nearest = -1.0f;
    float farthest = -1.0f;
    // この tick に出来た塊の中心（床の高さ）と、その塊で爆ぜた火の面積 (m²)。見た目への受け渡しがあふれて捨てた塊も入る
    // （跡に煙を残すのに使う）。
    std::array<HIKARI::GameProjectFloat3, kBurnExplosionBlockCapacity> blockCenters{};
    std::array<float, kBurnExplosionBlockCapacity> blockAreas{};
    uint32_t blockCount = 0;
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
    // player（足元。nullptr はプレイヤーなし）から爆ぜたセルまでの最小距離で、当たった輪（zone）を決める。
    // 爆ぜたセルは見た目への受け渡し（BurnFieldComponent::blasts）へ、blastBlockSize の升ごとに 1 塊で足す。
    // innerRadius が 0 の輪を引爆の始まりと見て、その引爆で出来る塊の数を見込む（BurnFieldComponent::blastPlan。
    // 前線は 1 tick に最初の輪の幅ずつ広がる前提）。燃える地面が無ければ false（何も爆ぜない）。
    static bool Explode(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin, float innerRadius,
                        float outerRadius, float hitRadius, const HIKARI::GameProjectFloat3* player,
                        BurnExplosion& result);
    // center から radius 未満の燃えているセルを、1 tick で全部爆ぜさせて消す（一か所だけの爆発）。当たりと塊の出し方は
    // Explode と同じ。塊の数は、その円の中だけで見込む（場のほかの火が多くても、爆発のエフェクトを間引かない）。
    // power は、塊の燃料と押す速さに掛ける倍率（場全体の引爆を 1 として、爆発の大きさを決める）。
    static bool ExplodeDisc(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float hitRadius,
                            const HIKARI::GameProjectFloat3* player, float power, BurnExplosion& result);
    // いま燃えている火（まだ爆ぜていないセル）のどれかが、point から爆心の近さ（hitRadius × BurnField.blastCoreShare 以内）
    // にあるか。引爆の前線は少しずつ進むので、火の上に立つ相手にも外の輪が先に届く。その時の輪の決め直しに使う。
    static bool IsCoreOfBurning(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point, float hitRadius);
    // ログに書く輪の名前（core／edge／none）。
    static const char* ZoneKey(BurnBlastZone zone);
    // 引爆の溜めの進み（0〜1）を見た目へ渡す（BurnFieldComponent::charge）。規則は読まない。
    static void SetCharge(HIKARI::WorldView& world, float charge);
    // 一か所だけの溜め：center から radius 以内の火だけを、進み charge（0〜1）の溜めの見た目にする。
    // 終わったら charge を 0 にして呼ぶ。規則は読まない。
    static void SetHotspot(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 center, float radius, float charge);
    // 見た目だけの塊を、場の火の域へ 1 つ出す（当たった所の火球・煙の塊）。火の格子や当たりは変えない。
    // 待ちがいっぱいなら捨てる。
    static void AddBlast(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position, float radius,
                         BurnBlastKind kind);
    // 火の刃がプレイヤーに当たった所：position（体の中心）を、direction（水平の向き）へ突き抜ける炎を出す。
    // 見た目だけで、火の格子や当たりは変えない。scale は炎の太さと量の倍率（ナイフのような小さい当たり用）。
    static void AddJet(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 position,
                       HIKARI::GameProjectFloat3 direction, float scale);
    // 格子が出来ていなければ、持ち主の物体の位置で作る（BurnFieldSystem が最初の固定 tick に呼ぶ）。
    static void Initialize(HIKARI::ObjectView object, BurnFieldComponent& field);

private:
    // Explode と ExplodeDisc の中身。planReach が 0 以上なら、塊の数を origin から planReach 未満のセルだけで見込む。
    static bool ExplodeBand(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 origin, float innerRadius,
                            float outerRadius, float hitRadius, const HIKARI::GameProjectFloat3* player,
                            float planReach, float power, BurnExplosion& result);
};

} // namespace Project_TD2_2
