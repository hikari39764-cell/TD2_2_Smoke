#pragma once

#include <algorithm>
#include <cmath>

#include "BossBodyRig.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossPose.h"
#include "../Boss/BossRules.h"

// 体に付く効果（刀光・兜・手・胴）の置き場所の計算で共通に使う小さな道具。見た目だけが使い、規則は使わない。
namespace Project_TD2_2::BossBodyMath {

using namespace BossMath;

// 割る数・時間の下限（0 で割らない）。
inline constexpr float kTiny = 1e-4f;

inline Vec3 ClampLength(Vec3 v, float maximum) {
    const float length = Length(v);
    return length > maximum && length > kTiny ? Mul(v, maximum / length) : v;
}

using VectorMath::ExpFollow;

// axis に直交する向き。直交する成分が無ければ、axis に直交する何かの向きを返す。
inline Vec3 Perpendicular(Vec3 v, Vec3 axis) {
    const Vec3 fallback = std::abs(axis.y) < 0.9f ? Vec3{0.0f, -1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f};
    return Normalize(ProjectOut(v, axis), Normalize(ProjectOut(fallback, axis)));
}

// いま出している技（技の最中でなければ nullptr）。
inline const BossMove* CurrentMove(const Boss& boss) {
    return BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
}

// フェーズ 2 の見た目（燃える胴・灼けた鎧・場の色・待機の構え・火の目）を出すか。見た目の切り替えの入口はここ 1 か所
// （c-857ae3）。変わり目の演出（c-8dd296）をする時は、その閃光（爆の、刀が地面を離れた tick。飛ばした時はその tick）で
// 切り替わる。演出を切った時は、規則がフェーズ 2 に入った tick から。刀の火は規則の IsBladeLit が決めるので別（演出の
// 間の刀身は BossPhaseShiftVisuals が決める）。
inline bool PhaseTwoLook(const Boss& boss) { return boss.lastPhaseNumber >= 2 && !boss.phaseTwoLookPending; }

// 見える刀の、剣の付け根から剣先までの長さ。当たりの線（剣の長さ＋剣先の霞）より先へは伸ばさない。
inline float VisibleSwordLength(const Boss& boss, const BossBody& body) {
    const float authored = body.bodySwordVisibleLength > 0.0f ? body.bodySwordVisibleLength : boss.swordLength;
    return std::min(authored, boss.swordLength + boss.swordHazeLength);
}

// 見える刀の、握りの中央から剣先までの長さ。
inline float VisibleBladeReach(const Boss& boss, const BossBody& body) {
    return std::max(0.01f, VisibleSwordLength(boss, body) - body.bodySwordGripAlong);
}

// 刀の模型の拡大（見える刀の長さに合わせる）と、手の模型の拡大。
inline float SwordScale(const Boss& boss, const BossBody& body) {
    return VisibleBladeReach(boss, body) / std::max(0.01f, body.bodySwordModelLength);
}
inline float HandScale(const Boss& boss, const BossBody& body) {
    return SwordScale(boss, body) * body.bodyHandScale;
}

} // namespace Project_TD2_2::BossBodyMath
