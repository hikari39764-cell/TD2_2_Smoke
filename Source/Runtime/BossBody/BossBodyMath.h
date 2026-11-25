#pragma once

#include <algorithm>
#include <cmath>

#include "BossBodyRig.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossPose.h"

// 外形（刀・手）の計算で共通に使う小さな道具。見た目だけが使い、規則は使わない。
namespace Project_TD2_2::BossBodyMath {

using namespace BossMath;

// ばねを 1 刻みで進める角の上限（角速度 × 刻み）。粗いと、陽に進めるばねが暴れる。刻みの数の上限。
inline constexpr float kMaxSpringStep = 0.5f;
inline constexpr int kMaxSubsteps = 64;
// 割る数・時間の下限（0 で割らない）。
inline constexpr float kTiny = 1e-4f;

inline Vec3 ClampLength(Vec3 v, float maximum) {
    const float length = Length(v);
    return length > maximum && length > kTiny ? Mul(v, maximum / length) : v;
}

// 時間 seconds で追う時の、1 フレームで縮める割合。
inline float Follow(float dt, float seconds) { return 1.0f - std::exp(-dt / std::max(kTiny, seconds)); }

// axis に直交する向き。直交する成分が無ければ、axis に直交する何かの向きを返す。
inline Vec3 Perpendicular(Vec3 v, Vec3 axis) {
    const Vec3 fallback = std::abs(axis.y) < 0.9f ? Vec3{0.0f, -1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f};
    return Normalize(ProjectOut(v, axis), Normalize(ProjectOut(fallback, axis)));
}

// 局所 +Z を direction、+Y を spine（direction に直交する成分）へ向ける回転。
inline Quat AimZ(Vec3 direction, Vec3 spine) {
    const Vec3 side = Normalize(Cross(spine, direction), {1.0f, 0.0f, 0.0f});
    return BasisRotation(side, Cross(direction, side));
}

inline const BossMove* CurrentMove(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return nullptr;
    return &moves[static_cast<size_t>(boss.move)];
}

// フェーズ 2 の見た目（燃える胴・灼けた鎧・場の色・待機の構え・火の目）を出すか。見た目の切り替えの入口はここ 1 か所
// （c-857ae3）。変わり目の演出（c-8dd296）をする時は、その閃光（爆の、刀が地面を離れた tick。飛ばした時はその tick）で
// 切り替わる。演出を切った時は、規則がフェーズ 2 に入った tick から。刀の火は規則の IsBladeLit が決めるので別（演出の
// 間の刀身は BossPhaseShiftVisuals が決める）。
inline bool PhaseTwoLook(const BossComponent& boss) {
    return boss.lastPhaseNumber >= 2 && !boss.shiftState.lookPending;
}

// 待機の構えの形：握り（局所。1 倍の体の値）・剣先の向き・刃の向き。フェーズ 2 は phaseTwoKamae で案を選ぶ。
struct KamaeShape {
    Vec3 grip;
    Vec3 direction;
    Vec3 edge;
};

// 見える刀の、剣の付け根から剣先までの長さ。当たりの線（剣の長さ＋剣先の霞）より先へは伸ばさない。
inline float VisibleSwordLength(const BossComponent& boss) {
    const float authored = boss.body.swordVisibleLength > 0.0f ? boss.body.swordVisibleLength : boss.swordLength;
    return std::min(authored, boss.swordLength + boss.swordHazeLength);
}

// 見える刀の、握りの中央から剣先までの長さ。
inline float VisibleBladeReach(const BossComponent& boss) {
    return std::max(0.01f, VisibleSwordLength(boss) - boss.body.swordGripAlong);
}

// 刀の模型の拡大（見える刀の長さに合わせる）と、手の模型の拡大。
inline float SwordScale(const BossComponent& boss) {
    return VisibleBladeReach(boss) / std::max(0.01f, boss.body.swordModelLength);
}
inline float HandScale(const BossComponent& boss) { return SwordScale(boss) * boss.body.handScale; }

// 局所（体の向きの枠。足元が原点）の点・向きを世界へ移す。lock で「体の物体の変換（傾き・屈み込み）」と
// 「規則の線の枠（足元と体の向きだけ）」を混ぜる。規則の線の枠も、外形を補間でずらした分だけずらす（体と刀を離さない）。
struct MotionFrame {
    const BossComponent& boss;
    const HIKARI::GameProjectTransform& body;
    float lock;

    Vec3 Point(Vec3 local) const {
        const Vec3 shift = boss.bodyState.presentShift;
        const Vec3 root{boss.ground.x + shift.x, boss.ground.y + BossPose::RootHeight(boss) + shift.y,
                        boss.ground.z + shift.z};
        // 規則の枠は体ごとの回転（回し蹴り・体の回転の技）も含める（刀の当たりと同じ所。c-b1824c）。
        const Vec3 rule = Add(root, RotateY(local, boss.yaw + boss.spin));
        const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y,
                            local.z * body.scale.z};
        return Lerp(Add(body.position, Rotate(body.rotation, centered)), rule, lock);
    }
    Vec3 Direction(Vec3 local) const {
        return Normalize(Lerp(Rotate(body.rotation, local), RotateY(local, boss.yaw + boss.spin), lock), local);
    }
};

} // namespace Project_TD2_2::BossBodyMath
