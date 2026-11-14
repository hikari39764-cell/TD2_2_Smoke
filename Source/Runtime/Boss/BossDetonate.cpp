#include "BossDetonate.h"

#include <algorithm>
#include <cmath>
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
                  "DetonateEnd tick=%llu exploded=%u hit=%d dodged=%d zone=%s cleared=%.3f interrupted=%d frames=%d "
                  "blasts=%u blastsDropped=%u",
                  static_cast<unsigned long long>(boss.tick), detonation.exploded, static_cast<int>(detonation.hit),
                  static_cast<int>(detonation.dodged), BurnFieldRules::ZoneKey(detonation.zone),
                  static_cast<double>(cleared), static_cast<int>(interrupted), boss.stateFrame, detonation.blasts,
                  detonation.blastsDropped);
    BossLog::Write(message);
}

// 突き立てた所の火球を、地面からこの高さに出す。
constexpr float kPlantBlastLift = 0.3f;
// 脈の立ち上がりの長さ（その脈までの間隔に対する割合）と、脈の後の緩み方の速さ。
constexpr float kPulseAttack = 0.18f;
constexpr float kPulseDecay = 5.0f;

// 溜めの進み h（0〜1）での、場の火の締まり方（0＝緩んでいる、1＝締まり切っている）。脈は count 回で、間隔は quicken 倍ずつ
// 縮む（最後の脈から爆ぜるまでも、その続きの間隔）。脈ごとに、手前で素早く締まり、その後ゆっくり緩む。最後は締まり切って終わる。
float Pulse(float h, int count, float quicken) {
    h = std::clamp(h, 0.0f, 1.0f);
    if (count <= 0)
        return h;
    // 間隔は first, first·q, first·q², …（count + 1 個で 1 になる）。
    float sum = 0.0f;
    float power = 1.0f;
    for (int k = 0; k <= count; ++k) {
        sum += power;
        power *= quicken;
    }
    float value = 0.0f;
    float at = 0.0f;
    float gap = 1.0f / sum;
    for (int k = 0; k <= count; ++k) {
        // この間隔の終わりが、脈（最後の間隔の終わりは、爆ぜる時）。
        const float peak = at + gap;
        const float attack = gap * kPulseAttack;
        if (h <= peak)
            value = std::max(value, std::clamp((h - (peak - attack)) / attack, 0.0f, 1.0f));
        else
            value = std::max(value, std::exp(-kPulseDecay * (h - peak) / (gap * quicken)));
        at = peak;
        gap *= quicken;
    }
    return value;
}

} // namespace

void BossDetonate::Charge(BossFrame& frame, float progress) {
    BossComponent& boss = frame.boss;
    if (!boss.detonation.charging) {
        boss.detonation.charging = true;
        boss.detonation.planted = false;
        char message[192]{};
        std::snprintf(message, sizeof(message), "DetonateCharge tick=%llu cells=%zu area=%.3f percent=%.3f",
                      static_cast<unsigned long long>(boss.tick), BurnFieldRules::GetBurningCells(frame.world),
                      static_cast<double>(BurnFieldRules::GetBurningArea(frame.world)),
                      static_cast<double>(BurnFieldRules::GetBurningFraction(frame.world) * 100.0f));
        BossLog::Write(message);
    }
    // 前隙の動き：掲げる（体が浮く）→ 突き立てる（落ちて屈む）→ 溜める。地面や画面の予兆は出さず、ボスの体と剣先の光、
    // 場の火の様子で見せる。
    const float raise = std::clamp(boss.detonateRaiseShare, 0.05f, 0.9f);
    const float plant = std::clamp(boss.detonatePlantShare, raise + 0.01f, 0.99f);
    if (progress < raise) {
        const float up = progress / raise;
        boss.height = boss.detonateLift * (1.0f - (1.0f - up) * (1.0f - up));
        boss.crouch = 0.0f;
        boss.chargeVisual = 0.6f * up;
        BurnFieldRules::SetCharge(frame.world, 0.0f);
        return;
    }
    if (progress < plant) {
        const float down = (progress - raise) / (plant - raise);
        boss.height = boss.detonateLift * (1.0f - down * down);
        boss.crouch = down * down;
        boss.chargeVisual = 0.6f + 0.4f * down;
        BurnFieldRules::SetCharge(frame.world, 0.0f);
        return;
    }
    boss.height = 0.0f;
    boss.crouch = 1.0f;
    if (!boss.detonation.planted) {
        // 突き立てた瞬間：足元から火が噴き、足元の霧を押しのける。
        boss.detonation.planted = true;
        const Vec3 feet{boss.ground.x, boss.ground.y + kPlantBlastLift, boss.ground.z};
        if (boss.detonatePlantBlast > 0.0f)
            BurnFieldRules::AddBlast(frame.world, feet, boss.detonatePlantBlast, BurnBlastKind::Hit);
        BossWaves::AddBlast(boss, {.pushPending = true, .position = feet});
    }
    // 溜め：場の火が脈を打つ（火が白く締まっては緩む）。間隔はだんだん縮み、最後の脈の後、締まり切って爆ぜる。
    const float pulse = Pulse((progress - plant) / (1.0f - plant), boss.detonatePulses,
                              std::clamp(boss.detonatePulseQuicken, 0.3f, 1.0f));
    const float depth = std::clamp(boss.detonatePulseDepth, 0.0f, 1.0f);
    BurnFieldRules::SetCharge(frame.world, 1.0f - depth * (1.0f - pulse));
    boss.chargeVisual = 0.5f + 0.5f * pulse;
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
    if (explosion.zone != BurnBlastZone::None && !detonation.caught) {
        // 1 回の引爆で当たるのは 1 回だけ。無敵でよけた時も、その引爆はもう当たらない（斬撃と同じ）。
        // 前線が届いた tick に当てる（爆心を待って遅らせない）：爆心は表の威力と被弾、外の輪は減らした威力でのけぞる。
        // 前線は少しずつ進むので、火の上に立つ相手にも外の輪が先に届く。まだ爆ぜていない火が爆心の近さにあれば、
        // この引爆がすぐそこも爆ぜさせるので、爆心として当てる。
        detonation.caught = true;
        const bool core = explosion.zone == BurnBlastZone::Core ||
                          BurnFieldRules::IsCoreOfBurning(frame.world, frame.playerCenter, strike.size);
        detonation.zone = core ? BurnBlastZone::Core : explosion.zone;
        const Vec3 player{frame.playerCenter.x, detonation.origin.y, frame.playerCenter.z};
        const int damage =
            static_cast<int>(std::lround(static_cast<float>(strike.damage) * (core ? 1.0f : explosion.damageShare)));
        detonation.hit =
            BossWaves::HitPlayer(frame, damage, Sub(player, detonation.origin), core && strike.blow, boss.move, 0,
                                 "detonate", PlayerDamageType::Explosion, true, &detonation.dodged);
    }
    if (boss.traceEveryFrames > 0 && explosion.cells > 0) {
        char message[320]{};
        std::snprintf(message, sizeof(message),
                      "DetonateFront tick=%llu frame=%d inner=%.3f outer=%.3f cells=%u near=%.3f far=%.3f blasts=%u "
                      "caught=%d zone=%s playerDistance=%.3f",
                      static_cast<unsigned long long>(boss.tick), boss.stateFrame, static_cast<double>(inner),
                      static_cast<double>(outer), explosion.cells, static_cast<double>(explosion.nearest),
                      static_cast<double>(explosion.farthest), explosion.blasts,
                      static_cast<int>(explosion.zone != BurnBlastZone::None), BurnFieldRules::ZoneKey(explosion.zone),
                      static_cast<double>(explosion.playerDistance));
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
            // 前隙の途中で崩された・気絶した：浮き上がっていた体を地面へ戻す。
            detonation.charging = false;
            boss.height = 0.0f;
            BurnFieldRules::SetCharge(frame.world, 0.0f);
        }
    }
    if (detonation.active && !InPhase(boss, BossActPhase::Act))
        Finish(frame, true);
}

} // namespace Project_TD2_2
