#include "BossDetonate.h"

#include <algorithm>
#include <cstdio>

#include "BossMath.h"
#include "BossRules.h"
#include "BossWaves.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// いまの技が引爆で、その phase にいる。
bool InPhase(const BossComponent& boss, BossActPhase phase) {
    const BossMove* move = BossRules::GetMove(boss, BossRules::GetCurrentMove(boss));
    return move != nullptr && move->type == BossMoveType::Detonate && boss.phase == phase;
}

// 引爆を終える。前線が通り過ぎた後にまだ燃えている所（前線の後ろで付いた火など）も消して、場を一度空にする。
// 爆ぜている途中で打ち切られた（気絶・撃破）時は、まだ爆ぜていない火を残す。
void Finish(BossFrame& frame, bool interrupted) {
    BossComponent& boss = frame.boss;
    BossDetonation& detonation = boss.detonation;
    detonation.active = false;
    const float cleared = interrupted ? 0.0f : BurnFieldRules::ClearAll(frame.world);
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "DetonateEnd tick=%llu exploded=%u hit=%d dodged=%d cleared=%.3f interrupted=%d frames=%d blasts=%u "
                  "blastsDropped=%u",
                  static_cast<unsigned long long>(boss.tick), detonation.exploded, static_cast<int>(detonation.hit),
                  static_cast<int>(detonation.dodged), static_cast<double>(cleared), static_cast<int>(interrupted),
                  boss.stateFrame, detonation.blasts, detonation.blastsDropped);
    BossLog::Write(message);
}

} // namespace

void BossDetonate::Charge(BossFrame& frame, float progress) {
    BossComponent& boss = frame.boss;
    if (!boss.detonation.charging) {
        boss.detonation.charging = true;
        char message[192]{};
        std::snprintf(message, sizeof(message), "DetonateCharge tick=%llu cells=%zu area=%.3f percent=%.3f",
                      static_cast<unsigned long long>(boss.tick), BurnFieldRules::GetBurningCells(frame.world),
                      static_cast<double>(BurnFieldRules::GetBurningArea(frame.world)),
                      static_cast<double>(BurnFieldRules::GetBurningFraction(frame.world) * 100.0f));
        BossLog::Write(message);
    }
    BurnFieldRules::SetCharge(frame.world, progress);
}

void BossDetonate::Begin(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossDetonation& detonation = boss.detonation;
    // 溜めの印は残す（Sync が、爆発の間は溜めきったまま保ち、終わった tick に 0 へ戻す）。
    const bool charging = detonation.charging;
    detonation = {};
    detonation.charging = charging;
    detonation.active = true;
    detonation.origin = boss.ground;
    detonation.farthest = BurnFieldRules::GetFarthestBurning(frame.world, detonation.origin);
    detonation.startCells = static_cast<uint32_t>(BurnFieldRules::GetBurningCells(frame.world));
    char message[224]{};
    std::snprintf(message, sizeof(message), "DetonateStart tick=%llu origin=(%.3f,%.3f,%.3f) cells=%u far=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(detonation.origin.x),
                  static_cast<double>(detonation.origin.y), static_cast<double>(detonation.origin.z),
                  detonation.startCells, static_cast<double>(detonation.farthest));
    BossLog::Write(message);
}

void BossDetonate::Step(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossDetonation& detonation = boss.detonation;
    if (!detonation.active)
        return;
    // 前線は攻撃中の 1 F 目に足元から出て、1 tick に 斬撃速度 × dt 広がる。通った輪は tick ごとに隙間なく並ぶ。
    const BossStrikeData& strike = BossMoveTable::Strike(move, 0);
    const float advance = std::max(0.0f, strike.speed) * frame.dt;
    const float outer = advance * static_cast<float>(boss.stateFrame);
    const float inner = std::max(0.0f, outer - advance);
    BurnExplosion explosion;
    BurnFieldRules::Explode(frame.world, detonation.origin, inner, outer, strike.size,
                            frame.player != nullptr ? &frame.playerCenter : nullptr, explosion);
    detonation.exploded += explosion.cells;
    detonation.blasts += explosion.blasts;
    detonation.blastsDropped += explosion.blastsDropped;
    if (explosion.playerCaught && !detonation.caught) {
        // 1 回の引爆で当たるのは 1 回だけ。無敵でよけた時も、その引爆はもう当たらない（斬撃と同じ）。
        detonation.caught = true;
        const Vec3 player{frame.playerCenter.x, detonation.origin.y, frame.playerCenter.z};
        detonation.hit = BossWaves::HitPlayer(frame, strike.damage, Sub(player, detonation.origin), strike.blow,
                                              boss.move, 0, "detonate", &detonation.dodged);
    }
    if (boss.traceEveryFrames > 0 && explosion.cells > 0) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "DetonateFront tick=%llu frame=%d inner=%.3f outer=%.3f cells=%u near=%.3f far=%.3f blasts=%u "
                      "caught=%d",
                      static_cast<unsigned long long>(boss.tick), boss.stateFrame, static_cast<double>(inner),
                      static_cast<double>(outer), explosion.cells, static_cast<double>(explosion.nearest),
                      static_cast<double>(explosion.farthest), explosion.blasts,
                      static_cast<int>(explosion.playerCaught));
        BossLog::Write(message);
    }
    // 一番遠かったセルを前線が越えたら（燃えていなかったら最初の tick に）、表の攻撃中を待たずに後隙へ入る。
    if (outer > detonation.farthest || boss.stateFrame >= boss.actFrames) {
        boss.actFrames = boss.stateFrame;
        Finish(frame, false);
    }
}

void BossDetonate::Sync(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossDetonation& detonation = boss.detonation;
    // 溜めの見た目：前隙の間は Charge が進める。爆発の間は溜めきったまま保つ（前線がまだ届いていない火が、
    // 白く締まったまま待つ）。崩された・気絶した・爆発が終わった tick に 0 へ戻す。
    if (detonation.charging && !InPhase(boss, BossActPhase::Pre)) {
        // 攻撃中へ入った tick は、爆発（Begin）がまだ始まっていない。攻撃中かどうかだけで見る。
        if (InPhase(boss, BossActPhase::Act)) {
            BurnFieldRules::SetCharge(frame.world, 1.0f);
        } else {
            detonation.charging = false;
            BurnFieldRules::SetCharge(frame.world, 0.0f);
        }
    }
    if (detonation.active && !InPhase(boss, BossActPhase::Act))
        Finish(frame, true);
}

} // namespace Project_TD2_2
