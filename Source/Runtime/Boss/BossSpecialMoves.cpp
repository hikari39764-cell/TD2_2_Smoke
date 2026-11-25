#include "BossSpecialMoves.h"

#include <algorithm>
#include <cmath>

#include "BossBurnMarks.h"
#include "BossKasumiStep.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 回転斬りの刀の当たりを、前の tick の刀からいまの刀までを何か所に分けて見るか（速く回るので体を飛び越えないように）。
constexpr int kSpinSweepSteps = 6;
// 跳躍の後隙：最初の 1/3 で降りきる（試作 3.6 × (1 − 後隙の進み × 3)）。
constexpr float kJumpLandSpeed = 3.0f;

// 回転斬り（c-427df4）：捻り込んだ所から振りの速さの曲線で一気に回り（刀は横なぎの型で払う）、回りの進みが
// spinRingShare を過ぎた所で霞の輪を出す。刀そのものの当たり（1 回だけ）も見る。
void SpinAct(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    const float coil = boss.spinCoilDegrees * kRadiansPerDegree;
    const float share = g.hit ? BossPose::SwingShare(boss, g.tIn, g.swingLength) : 1.0f;
    // クリップの技（c-7b1a09）：回転も沈みもクリップが演じるので、規則の体は回さない（骨格の体は回らず、技の終わりに
    // 向きが回転ぶん飛ぶ）。輪は当たりの窓ごとに 1 つ（叩きつけごとに広がる）。
    const bool clip = BossAnimBody::ClipMove(boss, move);
    boss.spin = clip ? 0.0f : Lerp(-coil, boss.spinTurns * 2.0f * kPi, share);
    const bool ringDue = clip ? g.hit && g.hitIndex != boss.spinRingHit && share >= boss.spinRingShare
                              : !boss.spinRingOut && share >= boss.spinRingShare;
    if (ringDue) {
        boss.spinRingOut = true;
        boss.spinRingHit = g.hitIndex;
        BossWaves::SpawnRing(frame, boss.move);
        BossWaves::AddBlast(boss, {.effectPending = true,
                                   .fire = boss.bladeLit,
                                   .scale = boss.ringBurstScale,
                                   .position = {boss.ground.x, boss.ground.y + boss.ringHeight, boss.ground.z}});
    }
    if (!g.hit || g.segment < 0 || static_cast<size_t>(g.segment) >= boss.swings.size())
        return;
    BossSwing& swing = boss.swings[static_cast<size_t>(g.segment)];
    const BossSwordPoints sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    if (swing.samples > 0 && !swing.done) {
        bool touched = false;
        for (int k = 1; k <= kSpinSweepSteps && !touched; ++k) {
            const float u = static_cast<float>(k) / static_cast<float>(kSpinSweepSteps);
            touched = BossWaves::Touches(frame, Lerp(swing.lastBase, sword.base, u),
                                         Lerp(swing.lastOuter, sword.outer, u), boss.thickness);
        }
        if (touched) {
            const BossStrikeData& strike = BossMoveTable::Strike(move, g.hitIndex);
            BossWaves::HitPlayer(frame, strike.damage, Sub(PlayerGround(frame), boss.ground), strike.blow, boss.move,
                                 g.hitIndex, "sword", boss.bladeLit);
            swing.done = true;
        }
    }
    swing.lastBase = sword.base;
    swing.lastOuter = sword.outer;
    ++swing.samples;
}

} // namespace

void BossSpecialMoves::Pre(BossFrame& frame, const BossMove& move, float progress) {
    BossComponent& boss = frame.boss;
    switch (move.type) {
    case BossMoveType::Jump: {
        const float share = std::clamp(boss.jumpCrouchShare, 0.0f, 0.95f);
        const float lift = progress < share ? 0.0f : (progress - share) / (1.0f - share);
        boss.height = boss.jumpHeight * EaseOut(lift);
        break;
    }
    case BossMoveType::Kick: {
        // 回転斬り（c-427df4）：刀を引きながら体を逆へ捻り込み、沈んで溜める（溜めの揺らぎで止まっている間もそのまま）。
        if (BossAnimBody::ClipMove(boss, move))
            break;
        boss.spin = -boss.spinCoilDegrees * kRadiansPerDegree *
                    Smooth(Share(boss.stateFrame, std::max(1, std::min(boss.spinCoilFrames, move.pre))));
        break;
    }
    default: break;
    }
}

void BossSpecialMoves::Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g, float progress) {
    switch (move.type) {
    case BossMoveType::Kick: SpinAct(frame, move, g); break;
    case BossMoveType::KasumiStep: BossKasumiStep::Act(frame, progress); break;
    default: break;
    }
}

void BossSpecialMoves::Post(BossComponent& boss, const BossMove& move, float progress) {
    boss.spin = 0.0f;
    switch (move.type) {
    case BossMoveType::Jump: boss.height = boss.jumpHeight * std::max(0.0f, 1.0f - progress * kJumpLandSpeed); break;
    case BossMoveType::KasumiStep: BossKasumiStep::Post(boss); break;
    default: break;
    }
}

} // namespace Project_TD2_2
