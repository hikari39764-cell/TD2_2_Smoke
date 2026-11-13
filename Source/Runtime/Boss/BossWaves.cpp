#include "BossWaves.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "BossBurnMarks.h"
#include "BossMath.h"
#include "../Burn/BurnFieldRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 当たりを確かめる回数（1 tick を 2 つに分ける）。速い斬撃（67.5 m/s ≒ 1.1 m/F）がプレイヤーを抜けないように。
constexpr int kHitSubsteps = 2;
// 地面からこれより下へ潜った斬撃は消す (m)。
constexpr float kBelowGroundLimit = 1.0f;
constexpr float kResidueStep = 2.0f;
// 放った直後に貯めた残り煙を、1 tick にこの割合ずつ足して返す。
constexpr float kOwedSmokeShare = 0.25f;
// F は 1/60 秒の固定 tick。1 tick の量を 1 秒あたりの量へ直す。
constexpr float kFramesPerSecond = 60.0f;
// 回し蹴りの輪が消えた所に残り煙を置く点の数（試作と同じ 6 か所）。
constexpr int kRingResiduePoints = 6;
constexpr float kTau = 6.28318530718f;

const BossMove* MoveOf(const BossComponent& boss, int index) {
    const auto& moves = boss.table.GetMoves();
    return index >= 0 && static_cast<size_t>(index) < moves.size() ? &moves[static_cast<size_t>(index)] : nullptr;
}

BossWave* FreeWave(BossComponent& boss) {
    for (auto& wave : boss.waves) {
        if (!wave.active)
            return &wave;
    }
    // 枠が無ければ一番遠くまで飛んだものを捨てる（もうすぐ消える）。
    auto oldest = std::max_element(boss.waves.begin(), boss.waves.end(),
                                   [](const BossWave& a, const BossWave& b) { return a.travelled < b.travelled; });
    return &*oldest;
}

void LogWave(const BossComponent& boss, const BossWave& wave, const char* event) {
    const BossMove* move = MoveOf(boss, wave.move);
    char message[512]{};
    std::snprintf(
        message, sizeof(message),
        "%s tick=%llu move=%s strike=%d id=%llu damage=%d blow=%d fire=%d width=%.3f speed=%.3f travelled=%.3f "
        "position=(%.3f,%.3f,%.3f) direction=(%.3f,%.3f,%.3f) across=(%.3f,%.3f,%.3f)",
        event, static_cast<unsigned long long>(boss.tick), move ? move->name.c_str() : "?", wave.strike,
        static_cast<unsigned long long>(wave.id), wave.damage, static_cast<int>(wave.blow), static_cast<int>(wave.fire),
        static_cast<double>(wave.width), static_cast<double>(wave.speed), static_cast<double>(wave.travelled),
        static_cast<double>(wave.position.x), static_cast<double>(wave.position.y),
        static_cast<double>(wave.position.z), static_cast<double>(wave.direction.x),
        static_cast<double>(wave.direction.y), static_cast<double>(wave.direction.z),
        static_cast<double>(wave.across.x), static_cast<double>(wave.across.y), static_cast<double>(wave.across.z));
    BossLog::Write(message);
}

// 吸い込みの扇（プレイヤーの向きの前、片側 absorbCatchAngle 度、absorbCatchRange m 以内）に斬撃・ナイフの刃が
// 入ったか（試作の inhaleCatch：刃の線分のうちプレイヤーに一番近い点で測る。近ければ向きを問わない）。
bool InCatch(const BossFrame& frame, Vec3 a, Vec3 b) {
    if (!frame.absorbing || frame.player == nullptr)
        return false;
    const BossComponent& boss = frame.boss;
    const Vec3 ab = Sub(b, a);
    const float t = std::clamp(Dot(Sub(frame.playerCenter, a), ab) / std::max(1e-6f, Dot(ab, ab)), 0.0f, 1.0f);
    const Vec3 offset = Sub(Add(a, Mul(ab, t)), frame.playerCenter);
    const float distance = FlatLength(offset);
    if (distance > boss.absorbCatchRange)
        return false;
    if (distance < boss.absorbCatchNear)
        return true;
    const Vec3 facing = FlatNormalize(PlayerRules::GetFacing(*frame.player));
    return Dot(FlatNormalize(offset), facing) >= std::cos(boss.absorbCatchAngle * kRadiansPerDegree);
}

// 霞の輪：広がりながら、輪の線がプレイヤーの体を通った時に当てる（高さは見ない。試作と同じ）。
void StepRing(BossFrame& frame, BossWave& wave) {
    BossComponent& boss = frame.boss;
    for (int s = 0; s < kHitSubsteps; ++s) {
        wave.radius = std::min(wave.maxRadius, wave.radius + wave.speed * frame.dt / static_cast<float>(kHitSubsteps));
        wave.travelled = wave.radius;
        if (wave.done || frame.player == nullptr)
            continue;
        const Vec3 offset = Sub(frame.playerCenter, wave.position);
        if (std::abs(FlatLength(offset) - wave.radius) < wave.thickness * 0.5f + boss.playerHitRadius) {
            bool dodged = false;
            wave.struck =
                BossWaves::HitPlayer(frame, wave.damage, offset, wave.blow, wave.move, wave.strike, "ring", dodged);
            wave.done = true;
            wave.dodged = dodged;
        }
    }
    if (wave.radius < wave.maxRadius)
        return;
    // 消えた所に残り煙（最大半径の kickResidueRadiusShare 倍の円に 6 か所、斬撃 2 m ぶんの kickResidueScale 倍ずつ）。
    for (int k = 0; k < kRingResiduePoints; ++k) {
        const float angle = kTau * static_cast<float>(k) / static_cast<float>(kRingResiduePoints);
        const Vec3 at = Add(wave.position, Mul(Vec3{std::sin(angle), 0.0f, std::cos(angle)},
                                               wave.maxRadius * boss.kickResidueRadiusShare));
        BossWaves::AddDeposit(boss, {at.x, boss.ground.y + boss.depositHeight, at.z},
                              boss.residuePerTwoMeters * boss.kickResidueScale, boss.depositRadius);
    }
    if (const BossMove* move = MoveOf(boss, wave.move))
        BossBurnMarks::RingEnded(frame, *move, wave);
    if (boss.traceEveryFrames > 0)
        LogWave(boss, wave, "RingEnd");
    wave.active = false;
}

} // namespace

bool BossWaves::Touches(const BossFrame& frame, Vec3 a, Vec3 b, float thickness) {
    if (frame.player == nullptr)
        return false;
    const BossComponent& boss = frame.boss;
    const Vec3 low = Add(frame.playerCenter, {0.0f, boss.playerHitLow, 0.0f});
    const Vec3 high = Add(frame.playerCenter, {0.0f, boss.playerHitHigh, 0.0f});
    return SegmentDistance(a, b, low, high) < thickness * 0.5f + boss.playerHitRadius;
}

bool BossWaves::HitPlayer(BossFrame& frame, int damage, Vec3 direction, bool blow, int move, int strike,
                          const char* source, bool& dodged) {
    if (frame.player == nullptr || !PlayerRules::IsAlive(*frame.player))
        return false;
    BossComponent& boss = frame.boss;
    const BossMove* row = MoveOf(boss, move);
    char message[320]{};
    if (PlayerRules::IsInvulnerable(*frame.player)) {
        // 無敵で抜けた斬撃はもう当たらない（吹っ飛び・ダウン中の無敵も同じ）。
        dodged = true;
        ++boss.playerDodges;
        std::snprintf(message, sizeof(message), "PlayerDodge tick=%llu move=%s strike=%d source=%s playerState=%d",
                      static_cast<unsigned long long>(boss.tick), row ? row->name.c_str() : "?", strike, source,
                      static_cast<int>(frame.player->state));
        BossLog::Write(message);
        return false;
    }
    const Vec3 push = FlatNormalize(direction);
    const bool accepted = PlayerRules::ApplyDamage(*frame.player, static_cast<float>(damage), push,
                                                   blow ? PlayerDamageKind::Knockback : PlayerDamageKind::Stagger);
    if (accepted) {
        ++boss.playerHits;
        // 当たった所に爆発を出す（ナイフは小さく）。枠は輪で使い回す。
        BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
        blast = {};
        blast.pushPending = blast.effectPending = true;
        blast.fire = boss.bladeLit;
        blast.scale = std::string_view(source) == "knife" ? boss.knifeBlastScale : 1.0f;
        blast.position = frame.playerCenter;
    }
    std::snprintf(message, sizeof(message),
                  "PlayerHit tick=%llu move=%s strike=%d source=%s damage=%d kind=%s accepted=%d playerHealth=%.3f "
                  "playerState=%d",
                  static_cast<unsigned long long>(boss.tick), row ? row->name.c_str() : "?", strike, source, damage,
                  blow ? "Knockback" : "Stagger", static_cast<int>(accepted),
                  static_cast<double>(PlayerRules::GetHealth(*frame.player)), static_cast<int>(frame.player->state));
    BossLog::Write(message);
    return accepted;
}

void BossWaves::SpawnSlash(BossFrame& frame, const BossSlashLaunch& launch) {
    BossComponent& boss = frame.boss;
    const BossMove* move = MoveOf(boss, launch.move);
    if (move == nullptr)
        return;
    const BossStrikeData& strike = BossMoveTable::Strike(*move, launch.strike);
    BossWave& wave = *FreeWave(boss);
    wave = {};
    wave.active = true;
    wave.id = boss.nextWaveId++;
    wave.move = launch.move;
    wave.strike = launch.strike;
    wave.position = launch.from;
    wave.direction = launch.direction;
    wave.across = launch.across;
    wave.fullWidth = strike.size;
    wave.growStart =
        launch.startWidth > 0.0f ? std::clamp(launch.startWidth / std::max(0.1f, strike.size), 0.15f, 1.0f) : 1.0f;
    wave.grow = wave.growStart;
    wave.width = wave.fullWidth * wave.grow;
    wave.thickness = boss.thickness;
    wave.speed = strike.speed;
    wave.damage = strike.damage;
    wave.blow = strike.blow;
    wave.done = launch.noHit;
    wave.absorbSmoke = boss.waveAbsorbSmoke;
    // 火は放った行そのものの値（連撃が参照した「縦振り」の行の火は持ってこない）。
    BossBurnMarks::ArmWave(boss, *move, wave);
    LogWave(boss, wave, "WaveRelease");
}

void BossWaves::SpawnKnives(BossFrame& frame, int moveIndex, Vec3 from, Vec3 aim) {
    BossComponent& boss = frame.boss;
    const BossMove* move = MoveOf(boss, moveIndex);
    if (move == nullptr)
        return;
    // 判定サイズ＝本数。扇状に同時に投げる。
    const int count = std::max(1, static_cast<int>(std::floor(move->size + 0.5f)));
    const BossStrikeData& strike = BossMoveTable::Strike(*move, 0);
    for (int k = 0; k < count; ++k) {
        const float angle = (static_cast<float>(k) - static_cast<float>(count - 1) * 0.5f) * boss.knifeSpreadDegrees *
                            kRadiansPerDegree;
        BossWave& wave = *FreeWave(boss);
        wave = {};
        wave.active = true;
        wave.knife = true;
        wave.id = boss.nextWaveId++;
        wave.move = moveIndex;
        wave.strike = k;
        wave.position = from;
        wave.direction = Normalize(RotateY(aim, angle));
        wave.across = Normalize(Cross(wave.direction, {0.0f, 1.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
        wave.fullWidth = wave.width = boss.knifeWidth;
        wave.thickness = boss.knifeThickness;
        wave.speed = move->speed;
        wave.damage = strike.damage;
        wave.blow = strike.blow;
        wave.absorbSmoke = boss.knifeAbsorbSmoke;
        BossBurnMarks::ArmWave(boss, *move, wave);
        LogWave(boss, wave, "KnifeRelease");
    }
}

void BossWaves::SpawnRing(BossFrame& frame, int moveIndex) {
    BossComponent& boss = frame.boss;
    const BossMove* move = MoveOf(boss, moveIndex);
    if (move == nullptr)
        return;
    const BossStrikeData& strike = BossMoveTable::Strike(*move, 0);
    BossWave& wave = *FreeWave(boss);
    wave = {};
    wave.active = true;
    wave.ring = true;
    wave.id = boss.nextWaveId++;
    wave.move = moveIndex;
    wave.position = boss.ground;
    // 判定サイズ＝輪の最大半径。
    wave.maxRadius = std::max(strike.size, boss.kickRingStartRadius);
    wave.radius = wave.travelled = std::min(boss.kickRingStartRadius, wave.maxRadius);
    wave.width = wave.fullWidth = wave.maxRadius;
    wave.thickness = boss.thickness;
    wave.speed = strike.speed;
    wave.damage = strike.damage;
    wave.blow = strike.blow;
    BossBurnMarks::ArmWave(boss, *move, wave);
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "RingRelease tick=%llu move=%s id=%llu damage=%d blow=%d fire=%d startRadius=%.3f maxRadius=%.3f "
                  "speed=%.3f",
                  static_cast<unsigned long long>(boss.tick), move->name.c_str(),
                  static_cast<unsigned long long>(wave.id), wave.damage, static_cast<int>(wave.blow),
                  static_cast<int>(wave.fire), static_cast<double>(wave.radius), static_cast<double>(wave.maxRadius),
                  static_cast<double>(wave.speed));
    BossLog::Write(message);
}

void BossWaves::Step(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    const float growPerFrame = 1.0f / static_cast<float>(std::max(1, boss.waveGrowFrames));
    for (auto& wave : boss.waves) {
        wave.smokeRate = 0.0f;
        if (!wave.active)
            continue;
        if (wave.ring) {
            StepRing(frame, wave);
            continue;
        }
        // 吸い込み中なら、扇に入った斬撃・ナイフは当たらずに吸われて消える（振っている剣そのものは吸えない）。
        const Vec3 edge = Mul(wave.across, wave.width * 0.5f);
        if (!wave.done && InCatch(frame, Sub(wave.position, edge), Add(wave.position, edge))) {
            boss.absorbedSmoke += wave.absorbSmoke;
            ++boss.wavesAbsorbed;
            ++boss.absorbedWavesPending;
            LogWave(boss, wave, "WaveAbsorbed");
            wave.active = false;
            continue;
        }
        // 放った直後の数 F で判定サイズまで伸びる。
        if (wave.grow < 1.0f) {
            wave.grow = std::min(1.0f, wave.grow + (1.0f - wave.growStart) * growPerFrame);
            wave.width = wave.fullWidth * wave.grow;
        }
        const float before = wave.travelled;
        for (int s = 0; s < kHitSubsteps; ++s) {
            const float stepLength = wave.speed * frame.dt / static_cast<float>(kHitSubsteps);
            wave.position = Add(wave.position, Mul(wave.direction, stepLength));
            wave.travelled += stepLength;
            if (wave.done)
                continue;
            const Vec3 half = Mul(wave.across, wave.width * 0.5f);
            if (Touches(frame, Sub(wave.position, half), Add(wave.position, half), wave.thickness)) {
                bool dodged = false;
                wave.struck = HitPlayer(frame, wave.damage, wave.direction, wave.blow, wave.move, wave.strike,
                                        wave.knife ? "knife" : "wave", dodged);
                wave.done = true;
                wave.dodged = dodged;
            }
        }
        const bool ending =
            wave.travelled > boss.range || wave.position.y < boss.ground.y - kBelowGroundLimit || wave.struck;
        // 飛んだ地面を燃やす。火は刃の burnLineLag m 後ろまで（刃より先に地面が燃えて見えないように）。当たって・
        // 飛びきって消える tick には、残りを刃の所まで塗ってから消す。
        if (wave.burnWidth > 0.0f) {
            const float lag = ending ? 0.0f : std::max(0.0f, boss.burnLineLag);
            if (wave.travelled > lag) {
                const Vec3 to = Sub(wave.position, Mul(wave.direction, lag));
                BurnFieldRules::PaintSegment(frame.world, wave.burnFrom, to, wave.burnWidth, wave.burnSeconds);
                wave.burnFrom = to;
            }
        }
        // 「交差」を待つ斬撃：狙った所へ届いた・当たった・飛びきった時に、そこで爆ぜて X の火を残す。
        if (wave.crossPending && (ending || wave.travelled >= wave.crossReach)) {
            if (const BossMove* move = MoveOf(boss, wave.move))
                BossBurnMarks::WaveLanded(frame, *move, wave);
        }
        // 斬撃は霞：通った跡に煙が残る（2 m 進むごとに residuePerTwoMeters）。ナイフは消えた所だけ。
        // 煙は刃の waveResidueLag 後ろへ入れる。そこまで進む前の分は貯めておき、後から少しずつ足す。
        if (!wave.knife) {
            const float smoke = (wave.travelled - before) / kResidueStep * boss.residuePerTwoMeters;
            if (wave.travelled < boss.waveResidueLag) {
                wave.smokeOwed += smoke;
            } else {
                const float payback = wave.smokeOwed * kOwedSmokeShare;
                wave.smokeOwed -= payback;
                wave.smokeRate = (smoke + payback) / frame.dt;
            }
        }
        // 当たった斬撃・ナイフは、当たった所で爆発して終わる（見えない斬撃が後ろへ残り煙を引き続けないように）。
        if (ending) {
            const float smoke =
                boss.residuePerTwoMeters * (wave.knife ? boss.knifeEndResidueScale : 1.0f) + wave.smokeOwed;
            AddDeposit(boss, {wave.position.x, boss.ground.y + boss.depositHeight, wave.position.z}, smoke,
                       boss.depositRadius);
            if (boss.traceEveryFrames > 0)
                LogWave(boss, wave, "WaveEnd");
            wave.active = false;
        }
    }
}

void BossWaves::AddDeposit(BossComponent& boss, Vec3 position, float smoke, float radius) {
    if (smoke <= 0.0f)
        return;
    BossSmokeDeposit* target = nullptr;
    for (auto& deposit : boss.deposits) {
        if (!deposit.active) {
            target = &deposit;
            break;
        }
    }
    if (target == nullptr) {
        // 空きが無ければ一番近い所へ足す（煙の総量を保つ）。
        float best = 0.0f;
        for (auto& deposit : boss.deposits) {
            const float distance = FlatLength(Sub(deposit.position, position));
            if (target == nullptr || distance < best) {
                target = &deposit;
                best = distance;
            }
        }
        target->smokeLeft += smoke;
        target->smokePerFrame = target->smokeLeft / static_cast<float>(std::max(1, boss.depositFrames));
        return;
    }
    *target = {};
    target->active = true;
    target->position = position;
    target->radius = radius;
    target->smokeLeft = smoke;
    target->smokePerFrame = smoke / static_cast<float>(std::max(1, boss.depositFrames));
}

void BossWaves::StepDeposits(BossComponent& boss) {
    for (auto& deposit : boss.deposits) {
        deposit.rate = 0.0f;
        if (!deposit.active)
            continue;
        if (deposit.smokeLeft <= 0.0001f) {
            deposit.active = false;
            continue;
        }
        const float amount = std::min(deposit.smokeLeft, deposit.smokePerFrame);
        deposit.smokeLeft -= amount;
        deposit.rate = amount * kFramesPerSecond;
    }
}

void BossWaves::Clear(BossComponent& boss) {
    for (auto& wave : boss.waves)
        wave = {};
    for (auto& deposit : boss.deposits)
        deposit = {};
    for (auto& blast : boss.blasts)
        blast = {};
}

} // namespace Project_TD2_2
