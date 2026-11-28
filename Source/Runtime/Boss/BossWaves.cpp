#include "BossWaves.h"

#include "BossReactionNodes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>

#include "BossBurnMarks.h"
#include "BossGourdRead.h"
#include "BossMath.h"
#include "BossRules.h"
#include "../Burn/BurnField.h"
#include "../Gourd/GourdRules.h"
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
// 回し蹴りの輪が消えた所に残り煙を置く点の数（試作と同じ 6 か所）。
constexpr int kRingResiduePoints = 6;
// …その 1 点の半径（斬撃が消えた所の煙の半径 depositRadius に対する割合。円に沿って滑らせるので細くする）。
constexpr float kRingResidueRadiusShare = 0.6f;
// 引き込みの前半のこの割合の間に、三日月の端（ナイフは刃先）を口へ向け終える。
constexpr float kPullTurnShare = 0.5f;
// 爆発の跡の煙の半径の下限 (m)。注入の量の換算（FluidEmit::EmitVolume）は 0 より大きい半径が要る。
constexpr float kMinimumFogRadius = 0.05f;

BossWave* FreeWave(Boss& boss) {
    for (auto& wave : boss.waves) {
        if (!wave.active)
            return &wave;
    }
    // 枠が無ければ一番遠くまで飛んだものを捨てる（もうすぐ消える）。
    auto oldest = std::max_element(boss.waves.begin(), boss.waves.end(),
                                   [](const BossWave& a, const BossWave& b) { return a.travelled < b.travelled; });
    return &*oldest;
}

void LogWave(const Boss& boss, const BossWave& wave, const char* event) {
    const BossMove* move = BossRules::GetMove(boss, wave.move);
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

// 斬撃・霧の弾を放った瞬間の手応え（カメラの打ち込み。c-e0a9c3 の続き）の強さを覚える：放った所がプレイヤーから
// releaseCameraNear (m) より遠いと弱まり、releaseCameraRange (m) で 0。描画フレームの BossFeedback が使う。
void NoteRelease(const BossFrame& frame, const BossWave& wave, float scale) {
    Boss& boss = frame.boss;
    if (frame.player == nullptr)
        return;
    const float distance = Length(Sub(frame.playerCenter, wave.position));
    const float span = std::max(0.1f, boss.releaseCameraRange - boss.releaseCameraNear);
    const float falloff = std::clamp((boss.releaseCameraRange - distance) / span, 0.0f, 1.0f);
    boss.releaseFeedbackPending = std::max(boss.releaseFeedbackPending, scale * falloff);
}

// 吸い込みの扇（プレイヤーの向きの前、片側 absorbCatchAngle 度、absorbCatchRange m 以内）に斬撃・ナイフの刃が
// 入ったか（試作の inhaleCatch：刃の線分のうちプレイヤーに一番近い点で測る。近ければ向きを問わない）。
// 満杯かどうかは見ない（HasCatchRoom）。
bool InCatch(const BossFrame& frame, Vec3 a, Vec3 b) {
    if (!frame.absorbing || frame.player == nullptr)
        return false;
    const Boss& boss = frame.boss;
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

// ヒョウタンにまだ空きがある（空きが少しでもあれば捕まえる。入りきらない煙は、入りきった時に捨てる）。
bool HasCatchRoom(const BossFrame& frame) {
    return frame.catchRoom != nullptr && GourdRules::IsRoomLeft(*frame.catchRoom);
}

// 吸い込みの扇に入ったが、満杯で吸えなかった（飛び続けて当たる）。1 本につき 1 回だけ記録する。
void Refuse(const BossFrame& frame, BossWave& wave) {
    if (wave.refused)
        return;
    wave.refused = true;
    char message[192]{};
    std::snprintf(message, sizeof(message), "WaveRefused tick=%llu id=%llu knife=%d room=%.3f",
                  static_cast<unsigned long long>(frame.boss.tick), static_cast<unsigned long long>(wave.id),
                  static_cast<int>(wave.knife),
                  frame.catchRoom != nullptr ? static_cast<double>(*frame.catchRoom) : 0.0);
    BossLog::Write(message);
}

// 吸い込みの扇に入った斬撃・ナイフを捕まえる。この tick から当たらず、火も残り煙も置かない。
// 入りきった時に足す煙を、捕まえられる空きから先に引く（同じ tick に続けて入った分は、残りの空きで決まる）。
void Capture(BossFrame& frame, BossWave& wave) {
    Boss& boss = frame.boss;
    if (frame.catchRoom != nullptr)
        *frame.catchRoom -= wave.absorbSmoke;
    // 同時に引き込んでいる分だけ遅らせて、順に入って見えるようにする。
    int pulling = 0;
    for (const auto& other : boss.waves)
        pulling += other.active && other.captured ? 1 : 0;
    wave.captured = true;
    wave.done = true;
    wave.pullFrame = 0;
    wave.pullFrames = std::max(1, wave.knife ? boss.knifePullFrames : boss.absorbPullFrames) +
                      std::max(0, boss.absorbPullStaggerFrames) * pulling;
    wave.pullFrom = wave.previousPosition = wave.position;
    wave.pullDirection = wave.direction;
    wave.pullAcross = wave.across;
    wave.pullWidth = wave.width;
    // 巻き込む弧は、プレイヤーの体から見てヒョウタンのある側へふくらませる（体の反対側を回ると、体とカメラの間を
    // 通って口の反対から入るように見える）。
    const Vec3 line = Sub(frame.mouth, wave.position);
    const Vec3 side = Normalize(Vec3{line.z, 0.0f, -line.x}, Vec3{1.0f, 0.0f, 0.0f});
    wave.pullSide = Dot(side, Sub(frame.mouth, frame.playerCenter)) >= 0.0f ? 1.0f : -1.0f;
    // 口へ向けるのは、三日月の端のうち口に近い側。
    wave.pullTip = Dot(wave.across, Sub(frame.mouth, wave.position)) >= 0.0f ? 1.0f : -1.0f;
    wave.burnWidth = 0.0f;
    wave.crossPending = false;
    wave.smokeOwed = 0.0f;
    // ヒョウタンの「噛む」手応えへ渡す（BossGourdLink::HandOff。c-e0a9c3）。
    ++boss.caught.bites;
    boss.caught.fireBites += wave.fire ? 1 : 0;
    boss.caught.bitePosition = wave.position;
    LogWave(boss, wave, "WaveCaptured");
}

// 捕まえた斬撃・ナイフを、口へ 1 tick ぶん引き込む。入りきったら真。
bool StepPull(const BossFrame& frame, BossWave& wave) {
    const Boss& boss = frame.boss;
    wave.previousPosition = wave.position;
    ++wave.pullFrame;
    const float t =
        std::clamp(static_cast<float>(wave.pullFrame) / static_cast<float>(std::max(1, wave.pullFrames)), 0.0f, 1.0f);
    // 初めゆっくり、終わり速く。
    const float eased = std::pow(t, std::max(0.1f, boss.absorbPullEasePower));
    // 口は動くので、毎 tick いまの口へ向け直す。道は、口への線から横へふくらむ弧（出だしの向きが
    // absorbPullSwirlDegrees だけ横を向く）。
    const Vec3 line = Sub(frame.mouth, wave.pullFrom);
    const Vec3 side = Normalize(Vec3{line.z, 0.0f, -line.x}, Vec3{1.0f, 0.0f, 0.0f});
    const float bow = FlatLength(line) * std::tan(boss.absorbPullSwirlDegrees * kRadiansPerDegree) / kPi *
                      std::sin(kPi * eased) * wave.pullSide;
    const Vec3 point = Add(Add(wave.pullFrom, Mul(line, eased)), Mul(side, bow));
    wave.width = wave.pullWidth * Lerp(1.0f, std::clamp(boss.absorbPullEndScale, 0.01f, 1.0f), eased);
    const Vec3 toMouth = Normalize(Sub(frame.mouth, point), Normalize(line, wave.pullDirection));
    const float turn = std::clamp(eased / kPullTurnShare, 0.0f, 1.0f);
    // ナイフは刃先を口へ向けきる。斬撃は absorbPullTurn の割合だけ回す（域ごと大きく回すと、域の中に残った霧が
    // 大きな扇になって振り回される。霧を口へ寄せるのは、口の力に任せる）。
    const float aligned =
        turn * turn * (3.0f - 2.0f * turn) * (wave.knife ? 1.0f : std::clamp(boss.absorbPullTurn, 0.0f, 1.0f));
    if (wave.knife) {
        // ナイフ：刃先を口へ向けて飛び込む。
        wave.direction = Normalize(Lerp(wave.pullDirection, toMouth, aligned), toMouth);
        wave.across = Normalize(Sub(wave.pullAcross, Mul(wave.direction, Dot(wave.pullAcross, wave.direction))),
                                Cross(wave.direction, Vec3{0.0f, 1.0f, 0.0f}));
        wave.position = point;
    } else {
        // 斬撃：三日月の片方の端が口を向くように回り、その端から細い筋になって入る（中心は、幅の半分だけ口の手前）。
        const Vec3 tipward = Mul(toMouth, wave.pullTip);
        wave.across = Normalize(Lerp(wave.pullAcross, tipward, aligned), tipward);
        wave.direction = Normalize(Sub(wave.pullDirection, Mul(wave.across, Dot(wave.pullDirection, wave.across))),
                                   Cross(wave.across, Vec3{0.0f, 1.0f, 0.0f}));
        wave.position = Sub(point, Mul(wave.across, wave.pullTip * wave.width * 0.5f * aligned));
    }
    return wave.pullFrame >= wave.pullFrames;
}

// 霞の輪：広がりながら、輪の線がプレイヤーの体を通った時に当てる（高さは見ない。試作と同じ）。
void StepRing(BossFrame& frame, BossWave& wave) {
    Boss& boss = frame.boss;
    for (int s = 0; s < kHitSubsteps; ++s) {
        wave.radius = std::min(wave.maxRadius, wave.radius + wave.speed * frame.dt / static_cast<float>(kHitSubsteps));
        wave.travelled = wave.radius;
        if (wave.done || frame.player == nullptr)
            continue;
        const Vec3 offset = Sub(frame.playerCenter, wave.position);
        if (std::abs(FlatLength(offset) - wave.radius) < wave.thickness * 0.5f + boss.playerHitRadius) {
            wave.struck =
                BossWaves::HitPlayer(frame, wave.damage, offset, wave.blow, wave.move, wave.strike, "ring", wave.fire);
            wave.done = true;
        }
    }
    if (wave.radius < wave.maxRadius)
        return;
    // 消えた所に残り煙（最大半径の kickResidueRadiusShare 倍の円に 6 か所、斬撃 2 m ぶんの kickResidueScale 倍ずつ）。
    // 6 つの点は、出している間に円に沿って隣の点の所まで滑る（球が 6 つ並ばず、霧の輪になる。c-3c908c）。
    const Vec3 center{wave.position.x, boss.ground.y + boss.depositHeight, wave.position.z};
    const float orbitStep =
        kTau / static_cast<float>(kRingResiduePoints) / static_cast<float>(std::max(1, boss.depositFrames));
    for (int k = 0; k < kRingResiduePoints; ++k) {
        const float angle = kTau * static_cast<float>(k) / static_cast<float>(kRingResiduePoints);
        const Vec3 at = Add(
            center, Mul(Vec3{std::sin(angle), 0.0f, std::cos(angle)}, wave.maxRadius * boss.kickResidueRadiusShare));
        BossWaves::AddDeposit(boss, at, boss.residuePerTwoMeters * boss.kickResidueScale,
                              boss.depositRadius * kRingResidueRadiusShare, boss.depositFrames, 0, center, orbitStep);
    }
    if (const BossMove* move = BossRules::GetMove(boss, wave.move))
        BossBurnMarks::RingEnded(frame, *move, wave);
    if (boss.traceEveryFrames > 0)
        LogWave(boss, wave, "RingEnd");
    wave.active = false;
}

} // namespace

bool BossWaves::Touches(const BossFrame& frame, Vec3 a, Vec3 b, float thickness) {
    if (frame.player == nullptr)
        return false;
    const Boss& boss = frame.boss;
    const Vec3 low = Add(frame.playerCenter, {0.0f, boss.playerHitLow, 0.0f});
    const Vec3 high = Add(frame.playerCenter, {0.0f, boss.playerHitHigh, 0.0f});
    return SegmentDistance(a, b, low, high) < thickness * 0.5f + boss.playerHitRadius;
}

bool BossWaves::HitPlayer(BossFrame& frame, int damage, Vec3 direction, bool blow, int move, int strike,
                          const char* source, bool fire, bool* dodged) {
    if (frame.player == nullptr || !PlayerRules::IsAlive(*frame.player))
        return false;
    Boss& boss = frame.boss;
    const BossMove* row = BossRules::GetMove(boss, move);
    char message[320]{};
    if (PlayerRules::IsInvulnerable(*frame.player)) {
        // 無敵で抜けた斬撃はもう当たらない（吹っ飛び・ダウン中の無敵も同じ）。
        if (dodged != nullptr)
            *dodged = true;
        std::snprintf(message, sizeof(message), "PlayerDodge tick=%llu move=%s strike=%d source=%s playerState=%d",
                      static_cast<unsigned long long>(boss.tick), row ? row->name.c_str() : "?", strike, source,
                      static_cast<int>(PlayerRules::GetState(*frame.player)));
        BossLog::Write(message);
        return false;
    }
    const Vec3 push = FlatNormalize(direction);
    // 吹っ飛ばす距離は斬撃の表の値（空欄はプレイヤーの既定）。
    const float knockDistance = row != nullptr ? BossMoveTable::Strike(*row, strike).knockDistance : 0.0f;
    const bool accepted = PlayerRules::ApplyDamage(*frame.player, static_cast<float>(damage), push,
                                                   blow ? PlayerDamageKind::Knockback : PlayerDamageKind::Stagger,
                                                   PlayerDamageType::Slash, knockDistance);
    // 火の刃は体に燃え移る（ログの順を「当たった → 燃えた」にするため、PlayerHit の後で点ける）。
    const bool ignite = accepted && fire;
    if (accepted) {
        ++boss.playerHitCount;
        BossReactionNodes::NoteHit(boss);
    }
    if (accepted) {
        // 当たった所の手応えを出す（ナイフは小さく）。火の当たりは炎が体を突き抜け、霞の当たりは霞が弾ける。
        AddBlast(boss, {.pushPending = true,
                        .effectPending = true,
                        .fire = fire,
                        .playerHit = true,
                        .scale = std::string_view(source) == "knife" ? boss.knifeBlastScale : 1.0f,
                        .position = frame.playerCenter,
                        .direction = push});
    }
    std::snprintf(message, sizeof(message),
                  "PlayerHit tick=%llu move=%s strike=%d source=%s damage=%d kind=%s type=%s fire=%d accepted=%d "
                  "playerHealth=%.3f playerState=%d",
                  static_cast<unsigned long long>(boss.tick), row ? row->name.c_str() : "?", strike, source, damage,
                  blow ? "Knockback" : "Stagger", "slash", static_cast<int>(fire), static_cast<int>(accepted),
                  static_cast<double>(PlayerRules::GetHealth(*frame.player)),
                  static_cast<int>(PlayerRules::GetState(*frame.player)));
    BossLog::Write(message);
    if (ignite)
        PlayerRules::Ignite(*frame.player, PlayerBurnCause::Slash, boss.tick);
    return accepted;
}

void BossWaves::SpendWave(Boss& boss, uint64_t waveId) {
    if (waveId == 0)
        return;
    for (auto& wave : boss.waves) {
        if (wave.active && wave.id == waveId)
            wave.done = true;
    }
}

uint64_t BossWaves::SpawnSlash(BossFrame& frame, const BossSlashLaunch& launch) {
    Boss& boss = frame.boss;
    const BossMove* move = BossRules::GetMove(boss, launch.move);
    if (move == nullptr)
        return 0;
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
    // ヒョウタンの読み（c-14392d）：同じ側へ転がる癖があれば、横の剣気をその側へ寄せる。
    BossGourdRead::LeanSlash(frame, wave);
    // 火は放った行そのものの値（連撃が参照した「縦振り」の行の火は持ってこない）。
    BossBurnMarks::ArmWave(boss, *move, wave);
    NoteRelease(frame, wave, 1.0f);
    LogWave(boss, wave, "WaveRelease");
    return wave.id;
}

void BossWaves::SpawnKnives(BossFrame& frame, int moveIndex, Vec3 from, Vec3 aim) {
    Boss& boss = frame.boss;
    const BossMove* move = BossRules::GetMove(boss, moveIndex);
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
        NoteRelease(frame, wave, boss.releaseKnifeScale);
        LogWave(boss, wave, "KnifeRelease");
    }
}

void BossWaves::SpawnRing(BossFrame& frame, int moveIndex) {
    Boss& boss = frame.boss;
    const BossMove* move = BossRules::GetMove(boss, moveIndex);
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
    Boss& boss = frame.boss;
    const float growPerFrame = 1.0f / static_cast<float>(std::max(1, boss.waveGrowFrames));
    for (auto& wave : boss.waves) {
        wave.smokeRate = 0.0f;
        if (!wave.active)
            continue;
        if (wave.ring) {
            StepRing(frame, wave);
            continue;
        }
        // 捕まえた斬撃・ナイフ：口へ引き込み、入りきった tick に煙と吸い取りの知らせを渡す。プレイヤーやヒョウタンが
        // いなくなったら（倒れた・シーンが変わった）、渡さずに捨てる。
        if (wave.captured) {
            if (!frame.hasMouth || frame.player == nullptr) {
                wave.active = false;
            } else if (StepPull(frame, wave)) {
                DeliverCatch(boss, wave);
                LogWave(boss, wave, "WaveSwallowed");
                wave.active = false;
            }
            continue;
        }
        // 吸い込み中なら、扇に入った斬撃・ナイフは当たらずに捕まる（振っている剣そのものは吸えない）。満杯で空きが
        // 無ければ捕まらず、そのまま飛んで当たる。
        const Vec3 edge = Mul(wave.across, wave.width * 0.5f);
        if (!wave.done && frame.hasMouth && InCatch(frame, Sub(wave.position, edge), Add(wave.position, edge))) {
            if (HasCatchRoom(frame)) {
                Capture(frame, wave);
                continue;
            }
            Refuse(frame, wave);
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
                wave.struck = HitPlayer(frame, wave.damage, wave.direction, wave.blow, wave.move, wave.strike,
                                        wave.knife ? "knife" : "wave", wave.fire);
                wave.done = true;
                // 振り抜く前に放った斬撃が当たった（か、無敵で抜けられた）：同じ一撃の剣は、もう当たらない。
                for (auto& swing : boss.swings) {
                    if (swing.active && swing.waveId == wave.id)
                        swing.done = true;
                }
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
                BurnFieldAccess::PaintSegment(frame.world, wave.burnFrom, to, wave.burnWidth, wave.burnSeconds);
                wave.burnFrom = to;
            }
        }
        // 「交差」を待つ斬撃：狙った所へ届いた・当たった・飛びきった時に、そこで爆ぜて X の火を残す。
        if (wave.crossPending && (ending || wave.travelled >= wave.crossReach)) {
            if (const BossMove* move = BossRules::GetMove(boss, wave.move))
                BossBurnMarks::WaveLanded(frame, *move, wave);
        }
        // 斬撃は霞：通った跡に煙が残る（2 m 進むごとに residuePerTwoMeters）。
        // 煙は刃の waveResidueLag 後ろへ入れる。そこまで進む前の分は貯めておき、後から少しずつ足す。
        // ナイフ・霧の弾は、飛んだ跡の真下の地面近くへ薄く残し（2 m 進むごとに knifeTrailResidue）、消えた所にも残す。
        if (wave.knife) {
            wave.smokeRate = (wave.travelled - before) / kResidueStep * boss.knifeTrailResidue / frame.dt;
        } else {
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
                       boss.depositRadius, boss.depositFrames);
            if (boss.traceEveryFrames > 0)
                LogWave(boss, wave, "WaveEnd");
            wave.active = false;
        }
    }
}

void BossWaves::AddDeposit(Boss& boss, Vec3 position, float smoke, float radius, int frames, int waitFrames,
                           Vec3 orbitCenter, float orbitStep) {
    if (smoke <= 0.0f)
        return;
    const float perFrameShare = 1.0f / static_cast<float>(std::max(1, frames));
    BossSmokeDeposit* target = nullptr;
    for (auto& deposit : boss.deposits) {
        if (!deposit.active) {
            target = &deposit;
            break;
        }
    }
    if (target == nullptr) {
        // 空きが無ければ一番近い所へ足す（煙の総量を保つ）。待つ煙を足した時は、長い方の待ちに揃える（炎の中へ出さない）。
        float best = 0.0f;
        for (auto& deposit : boss.deposits) {
            const float distance = FlatLength(Sub(deposit.position, position));
            if (target == nullptr || distance < best) {
                target = &deposit;
                best = distance;
            }
        }
        target->smokeLeft += smoke;
        target->smokePerFrame = target->smokeLeft * perFrameShare;
        target->waitFrames = std::max(target->waitFrames, waitFrames);
        return;
    }
    *target = {};
    target->active = true;
    target->position = position;
    target->radius = radius;
    target->smokeLeft = smoke;
    target->smokePerFrame = smoke * perFrameShare;
    target->waitFrames = std::max(0, waitFrames);
    target->orbitCenter = orbitCenter;
    target->orbitStep = orbitStep;
}

void BossWaves::DeliverCatch(Boss& boss, const BossWave& wave) {
    boss.caught.smoke += wave.absorbSmoke;
    ++boss.caught.count;
    if (wave.fire)
        ++boss.caught.fireCount;
    if (wave.knife)
        ++boss.caught.knifeCount;
}

float BossWaves::GetPullingSmoke(const Boss& boss) {
    float smoke = 0.0f;
    for (const auto& wave : boss.waves)
        smoke += wave.active && wave.captured ? wave.absorbSmoke : 0.0f;
    return smoke;
}

void BossWaves::AddBlast(Boss& boss, const BossBlast& blast) {
    boss.blasts[boss.blastCursor++ % boss.blasts.size()] = blast;
    // 爆発の跡：真下の地面の近くへ、吸える煙を残す。
    if (!blast.leavesFog)
        return;
    const float scale = std::max(0.0f, blast.scale);
    const float smoke = boss.blastFogSmoke * scale;
    // 量は大きさに比例、半径は大きさの平方根に比例：場の霧は床に近い薄い層なので、広がる面積あたりの濃さが同じになる
    // （半径も大きさに比例させると、大きい爆発ほど薄くなり、層より上へ出た分はすぐ消えて吸える量も減る）。
    const float radius = std::max(kMinimumFogRadius, boss.blastFogRadius * std::sqrt(scale));
    if (!(smoke > 0.0f))
        return;
    const Vec3 at{blast.position.x, boss.ground.y + boss.depositHeight, blast.position.z};
    AddDeposit(boss, at, smoke, radius, boss.blastFogFrames, boss.blastFogDelayFrames);
    boss.blastFogTotal += static_cast<double>(smoke);
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "BlastFog tick=%llu smoke=%.3f radius=%.3f scale=%.3f wait=%d frames=%d fire=%d playerHit=%d "
                  "position=(%.3f,%.3f,%.3f) total=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(smoke), static_cast<double>(radius),
                  static_cast<double>(scale), boss.blastFogDelayFrames, boss.blastFogFrames,
                  static_cast<int>(blast.fire), static_cast<int>(blast.playerHit), static_cast<double>(at.x),
                  static_cast<double>(at.y), static_cast<double>(at.z), boss.blastFogTotal);
    BossLog::Write(message);
}

void BossWaves::StepDeposits(Boss& boss) {
    for (auto& deposit : boss.deposits) {
        deposit.rate = 0.0f;
        if (!deposit.active)
            continue;
        if (deposit.waitFrames > 0) {
            --deposit.waitFrames;
            continue;
        }
        if (deposit.smokeLeft <= 0.0001f) {
            deposit.active = false;
            continue;
        }
        const float amount = std::min(deposit.smokeLeft, deposit.smokePerFrame);
        deposit.smokeLeft -= amount;
        deposit.rate = amount * kFramesPerSecond;
        if (deposit.orbitStep != 0.0f) {
            const Vec3 arm = RotateY(Sub(deposit.position, deposit.orbitCenter), deposit.orbitStep);
            deposit.position = Add(deposit.orbitCenter, arm);
        }
    }
}

void BossWaves::Clear(Boss& boss) {
    for (auto& wave : boss.waves)
        wave = {};
    for (auto& deposit : boss.deposits)
        deposit = {};
    for (auto& blast : boss.blasts)
        blast = {};
}

} // namespace Project_TD2_2
