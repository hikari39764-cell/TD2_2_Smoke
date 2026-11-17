#include "BossSkyfall.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossBurnMarks.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossRules.h"
#include "BossWaves.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 着地の爆発を出す高さ（着地点の上 m）。爆発の大きさ（skyfallBlastScale）で見え方は調整できるので、高さは項目にしない。
constexpr float kSkyfallBlastLift = 1.5f;
// 着地の火を爆ぜさせる時・溜めの見た目にする時に、火の筋の先（＋幅の半分）へ足す余白 (m)。縁のセルと、縁に置かれた
// 火の点を取りこぼさない。
constexpr float kSkyfallBurstMargin = 0.4f;

const char* PhaseName(BossSkyfallPhase phase) {
    switch (phase) {
    case BossSkyfallPhase::Crouch: return "crouch";
    case BossSkyfallPhase::Rise: return "rise";
    case BossSkyfallPhase::Hang: return "hang";
    case BossSkyfallPhase::Dive: return "dive";
    case BossSkyfallPhase::Landed: return "landed";
    default: return "none";
    }
}

void SetPhase(BossComponent& boss, BossSkyfallPhase phase, const char* reason = nullptr) {
    if (boss.skyfall.phase == phase)
        return;
    boss.skyfall.phase = phase;
    char message[224]{};
    std::snprintf(message, sizeof(message), "SkyfallPhase tick=%llu phase=%s height=%.3f pos=(%.3f,%.3f)%s%s",
                  static_cast<unsigned long long>(boss.tick), PhaseName(phase), static_cast<double>(boss.height),
                  static_cast<double>(boss.ground.x), static_cast<double>(boss.ground.z), reason ? " reason=" : "",
                  reason ? reason : "");
    BossLog::Write(message);
}

// 着地の火の記録：state は lit（着地して火が付いた）・burst（爆ぜた）・cut（打ち切られて爆ぜない）。
void LogBurst(const BossComponent& boss, const char* state, const char* extra) {
    const BossSkyfallBurst& burst = boss.skyfallBurst;
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "SkyfallBurst tick=%llu state=%s center=(%.3f,%.3f) elapsed=%d delay=%d radius=%.3f%s",
                  static_cast<unsigned long long>(boss.tick), state, static_cast<double>(burst.center.x),
                  static_cast<double>(burst.center.z), burst.elapsed, boss.skyfallBurstDelayFrames,
                  static_cast<double>(boss.skyfallBurstRadius), extra);
    BossLog::Write(message);
}

// 狙う所（地面）：プレイヤーの足元。プレイヤーがいなければボスの前 1 m（その場の近くに降りる）。
Vec3 AimGround(const BossFrame& frame, Vec3 playerCenter) {
    const BossComponent& boss = frame.boss;
    if (frame.player == nullptr)
        return Add(boss.ground, Forward(boss.yaw));
    return {playerCenter.x, boss.ground.y, playerCenter.z};
}

// 落ちる所：いまのボス（空中の真下）→ プレイヤーの線の上で、プレイヤーの skyfallStopDistance 手前（重ならない）。
// 進める距離は skyfallMaxTravel まで。戦場の外へは出ない。
Vec3 Predict(const BossComponent& boss, Vec3 aim) {
    const Vec3 offset = Sub(aim, boss.ground);
    const float travel = std::clamp(FlatLength(offset) - boss.skyfallStopDistance, 0.0f, boss.skyfallMaxTravel);
    Vec3 target = Add(boss.ground, Mul(FlatNormalize(offset, Forward(boss.yaw)), travel));
    target.y = boss.ground.y;
    BossMoveExecutor::ClampToArena(boss, target);
    return target;
}

void Lock(BossFrame& frame, Vec3 playerCenter) {
    BossComponent& boss = frame.boss;
    BossSkyfallState& state = boss.skyfall;
    const Vec3 aim = AimGround(frame, playerCenter);
    state.target = Predict(boss, aim);
    state.locked = true;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "SkyfallLock tick=%llu target=(%.3f,%.3f) player=(%.3f,%.3f) travel=%.3f from=(%.3f,%.3f)",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(state.target.x),
                  static_cast<double>(state.target.z), static_cast<double>(aim.x), static_cast<double>(aim.z),
                  static_cast<double>(FlatLength(Sub(state.target, boss.ground))), static_cast<double>(boss.ground.x),
                  static_cast<double>(boss.ground.z));
    BossLog::Write(message);
}

// 着地の当たり：着地点から判定サイズ（＋プレイヤーの体の半径）以内にいれば、1 回の着地で 1 回だけ当てる。
// 高さは見ない（プレイヤーは跳ばないので、地面にいれば当たる）。無敵でよけた時も、その着地はもう当たらない（斬撃と同じ）。
void StepHit(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossSkyfallState& state = boss.skyfall;
    if (state.caught || frame.player == nullptr)
        return;
    const BossStrikeData& strike = BossMoveTable::Strike(move, 0);
    const Vec3 offset = Sub(AimGround(frame, frame.playerCenter), state.target);
    if (FlatLength(offset) > strike.size + boss.playerHitRadius)
        return;
    state.caught = true;
    // 真上に降られた時は、落ちてきた向きへ吹き飛ばす。
    const Vec3 push = FlatNormalize(offset, FlatNormalize(Sub(state.target, state.diveFrom), Forward(boss.yaw)));
    state.hit = BossWaves::HitPlayer(frame, strike.damage, push, strike.blow, boss.move, 0, "skyfall",
                                     PlayerDamageType::Slash, boss.bladeLit, &state.dodged);
}

} // namespace

void BossSkyfall::Begin(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    if (move.type != BossMoveType::Skyfall) {
        // 着地からコンボでつないだ時：屈んだ体を戻してから次の技へ（ほかの型は屈みを自分で戻さない）。
        if (boss.skyfall.phase != BossSkyfallPhase::None)
            boss.crouch = 0.0f;
        Reset(boss, "next");
        return;
    }
    Reset(boss, "next");
    BossSkyfallState& state = boss.skyfall;
    state.takeoff = boss.ground;
    state.away = FlatNormalize(Sub(boss.ground, AimGround(frame, frame.playerCenter)), Mul(Forward(boss.yaw), -1.0f));
    // 前隙を屈む・跳ぶ・空中の溜めに分ける。前隙が短すぎる時は、溜め → 屈みの順に削る（跳ぶ F は最低 1）。
    state.crouchFrames = std::clamp(boss.skyfallCrouchFrames, 0, std::max(0, move.pre - 1));
    state.riseFrames = std::clamp(boss.skyfallRiseFrames, 1, std::max(1, move.pre - state.crouchFrames));
    state.hangFrames = std::max(0, move.pre - state.crouchFrames - state.riseFrames) + boss.windupHold;
    // 溜めの揺らぎで止まるのは、空中の溜めの最初の F（構えの合図の F ではなく）。揺らいだ分だけ空中に長く留まる。
    boss.windupHoldAt = std::min(state.crouchFrames + state.riseFrames + 1, std::max(1, move.pre - 1));
    state.target = Predict(boss, AimGround(frame, frame.playerCenter));
    char message[224]{};
    std::snprintf(message, sizeof(message),
                  "SkyfallBegin tick=%llu takeoff=(%.3f,%.3f) crouch=%d rise=%d hang=%d height=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(state.takeoff.x),
                  static_cast<double>(state.takeoff.z), state.crouchFrames, state.riseFrames, state.hangFrames,
                  static_cast<double>(boss.skyfallHeight));
    BossLog::Write(message);
}

void BossSkyfall::Pre(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossSkyfallState& state = boss.skyfall;
    const int t = boss.stateFrame;
    if (t <= state.crouchFrames) {
        SetPhase(boss, BossSkyfallPhase::Crouch);
        boss.crouch = static_cast<float>(t) / static_cast<float>(std::max(1, state.crouchFrames));
        boss.height = 0.0f;
    } else if (t <= state.crouchFrames + state.riseFrames) {
        SetPhase(boss, BossSkyfallPhase::Rise);
        // 跳び上がり：地面を蹴って速く上がり、上で止まる（流れる距離も同じ曲線）。
        const float e = EaseOut(static_cast<float>(t - state.crouchFrames) / static_cast<float>(state.riseFrames));
        boss.crouch = 0.0f;
        boss.height = boss.skyfallHeight * e;
        boss.ground = Add(state.takeoff, Mul(state.away, boss.skyfallBackDrift * e));
    } else {
        SetPhase(boss, BossSkyfallPhase::Hang);
        ++state.hangElapsed;
        boss.height = boss.skyfallHeight;
        state.charge = Share(state.hangElapsed, state.hangFrames);
        boss.chargeVisual = state.charge;
    }
    if (state.locked)
        return;
    // 追尾を止めた tick（実行器が止めた時のプレイヤーの位置を boss.aimPoint に持つ）に落ちる所を決める。それまでは見込み。
    if (boss.aimFrozen)
        Lock(frame, boss.aimPoint);
    else
        state.target = Predict(boss, AimGround(frame, frame.playerCenter));
}

void BossSkyfall::Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    BossSkyfallState& state = boss.skyfall;
    if (g.hit) {
        StepHit(frame, move);
        if (g.tIn == 1) {
            char message[256]{};
            std::snprintf(message, sizeof(message),
                          "SkyfallLand tick=%llu pos=(%.3f,%.3f) hit=%d dodged=%d playerDistance=%.3f radius=%.3f",
                          static_cast<unsigned long long>(boss.tick), static_cast<double>(state.target.x),
                          static_cast<double>(state.target.z), static_cast<int>(state.hit),
                          static_cast<int>(state.dodged),
                          frame.player != nullptr
                              ? static_cast<double>(FlatLength(Sub(AimGround(frame, frame.playerCenter), state.target)))
                              : -1.0,
                          static_cast<double>(BossMoveTable::Strike(move, 0).size));
            BossLog::Write(message);
        }
        if (g.tIn == move.segments[static_cast<size_t>(g.segment)].length) {
            char message[160]{};
            std::snprintf(message, sizeof(message), "SkyfallHitEnd tick=%llu hit=%d dodged=%d",
                          static_cast<unsigned long long>(boss.tick), static_cast<int>(state.hit),
                          static_cast<int>(state.dodged));
            BossLog::Write(message);
        }
        return;
    }
    if (state.phase != BossSkyfallPhase::Dive) {
        // 追尾を止める F が空欄（0）なら、落ち始めの tick のプレイヤーへ落ちる。
        if (!state.locked)
            Lock(frame, frame.playerCenter);
        state.diveFrom = boss.ground;
        state.diveHeight = boss.height;
        state.charge = 1.0f;
        SetPhase(boss, BossSkyfallPhase::Dive);
    }
    // 落ちる区間の F ＋ 1 で 1 になる：区間の最後の tick はまだ空中で、次の tick（着地の当たりの 1 F 目）に地面へ着く。
    const int frames = move.segments[static_cast<size_t>(g.segment)].length;
    state.dive = std::clamp(static_cast<float>(g.tIn) / static_cast<float>(frames + 1), 0.0f, 1.0f);
    const float e = std::pow(state.dive, std::max(1.0f, boss.skyfallDiveEasePower));
    boss.ground = Lerp(state.diveFrom, state.target, e);
    boss.height = state.diveHeight * (1.0f - e);
}

void BossSkyfall::Land(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossSkyfallState& state = boss.skyfall;
    state.dive = 1.0f;
    boss.ground = state.target;
    boss.height = 0.0f;
    boss.crouch = 1.0f;
    SetPhase(boss, BossSkyfallPhase::Landed);
    if (Bursts(boss)) {
        // フェーズ 2：着地の瞬間は足元の霧を押すだけ。火はここから広がり、少し後に爆ぜる（StepBurst）。
        boss.skyfallBurst = {};
        boss.skyfallBurst.active = true;
        boss.skyfallBurst.center = state.target;
        boss.skyfallBurst.yaw = YawOf(FlatNormalize(Sub(state.target, state.diveFrom), Forward(boss.yaw)));
        boss.skyfallBurst.move = boss.move;
        if (boss.skyfallBlastScale > 0.0f)
            BossWaves::AddBlast(boss,
                                {.pushPending = true,
                                 .scale = boss.skyfallBlastScale,
                                 .position = {state.target.x, state.target.y + kSkyfallBlastLift, state.target.z}});
        LogBurst(boss, "lit", "");
        return;
    }
    if (!(boss.skyfallBlastScale > 0.0f))
        return;
    // 着地の爆発（当たった所の爆発と同じ枠。見た目は BossVisuals、足元の霧を押すのは BossSmoke）。
    // フェーズ 2 は火球、フェーズ 1 は霞の塊。当てたかどうかに関わらず出す。
    BossWaves::AddBlast(boss, {.pushPending = true,
                               .effectPending = true,
                               .fire = boss.lastPhaseNumber >= 2,
                               .scale = boss.skyfallBlastScale,
                               .position = {state.target.x, state.target.y + kSkyfallBlastLift, state.target.z}});
}

void BossSkyfall::Post(BossComponent& boss) {
    boss.height = 0.0f;
    boss.crouch = 1.0f - Share(boss.stateFrame, boss.skyfallLandCrouchFrames);
}

void BossSkyfall::Reset(BossComponent& boss, const char* reason) {
    SetPhase(boss, BossSkyfallPhase::None, reason);
    boss.skyfall = {};
}

bool BossSkyfall::Bursts(const BossComponent& boss) {
    return boss.lastPhaseNumber >= 2 && boss.skyfallBurstDelayFrames > 0 && boss.skyfallBurstRadius > 0.0f;
}

void BossSkyfall::StepBurst(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    BossSkyfallBurst& burst = boss.skyfallBurst;
    if (!burst.active)
        return;
    // 爆発の威力と当たりの半径は、技の表の「爆発の威力」「爆発の判定 m」（空欄は 0＝当たらない）。
    const BossMove* row = BossRules::GetMove(boss, burst.move);
    const float hitRadius = row != nullptr ? row->blastSize : 0.0f;
    // 火の筋は、範囲の半径から「爆ぜた火が届く距離」を引いた所まで走る（爆発が届く端が、炎の輪の半径に揃う）。
    const float spokeLength = std::max(0.5f, boss.skyfallBurstRadius - hitRadius);
    const float reach = spokeLength + boss.skyfallBurstFireWidth * 0.5f + kSkyfallBurstMargin;
    if (burst.cut) {
        BurnFieldRules::SetHotspot(frame.world, burst.center, 0.0f, 0.0f);
        LogBurst(boss, "cut", "");
        burst = {};
        return;
    }
    ++burst.elapsed;
    // 火は着地点から端へ、地割れのような筋で走る（走り切った後は塗り足さない：吸って消した所は消えたまま）。
    // 筋は落ちてきた向きを挟んで等間隔（落ちてきた向きの真上には筋を置かない）。塗りつぶした円にはしない：
    // 広い面が一度に燃えると、炎が舌にならず、白っぽい塊になる。
    if (burst.elapsed <= boss.skyfallBurstIgniteFrames) {
        const int spokes = std::max(1, boss.skyfallBurstSpokes);
        const float length = spokeLength * Share(burst.elapsed, boss.skyfallBurstIgniteFrames);
        for (int i = 0; i < spokes; ++i) {
            const float angle = burst.yaw + 2.0f * kPi * (static_cast<float>(i) + 0.5f) / static_cast<float>(spokes);
            BurnFieldRules::PaintSegment(frame.world, burst.center, Add(burst.center, Mul(Forward(angle), length)),
                                         boss.skyfallBurstFireWidth, boss.skyfallBurstFireSeconds);
        }
    }
    if (burst.elapsed < boss.skyfallBurstDelayFrames) {
        // 溜めの見た目：始めはふつうの火で、爆ぜる前に急に燃え盛る。
        const float fuse = Share(burst.elapsed, boss.skyfallBurstDelayFrames);
        BurnFieldRules::SetHotspot(frame.world, burst.center, reach, fuse * fuse);
        return;
    }
    BurnFieldRules::SetHotspot(frame.world, burst.center, 0.0f, 0.0f);
    BurnExplosion explosion;
    BurnFieldRules::ExplodeDisc(frame.world, burst.center, reach, hitRadius,
                                frame.player != nullptr ? &frame.playerCenter : nullptr, boss.skyfallBurstPower,
                                explosion);
    // 爆ぜた塊の跡に吸える煙を残す（全部が同じ tick に爆ぜるので、この 1 回で detonationFogSmokeMax まで）。
    const float fog = BossWaves::AddDetonationFog(boss, explosion, boss.detonationFogSmokeMax, "skyfallBurst");
    // 1 回の爆発で当たるのは 1 回だけ。無敵でよけた時は当たらない（斬撃と同じ）。全部が同じ tick に爆ぜるので、
    // 一番近い火で輪を決める：爆心は威力のまま skyfallBurstBlow、外の輪は減らした威力でのけぞる。
    bool hit = false, dodged = false;
    if (explosion.zone != BurnBlastZone::None && row != nullptr) {
        const Vec3 player{frame.playerCenter.x, burst.center.y, frame.playerCenter.z};
        const bool core = explosion.zone == BurnBlastZone::Core;
        const int damage = static_cast<int>(std::lround(static_cast<float>(row->blastDamage) * explosion.damageShare));
        hit = BossWaves::HitPlayer(frame, damage, Sub(player, burst.center), core && boss.skyfallBurstBlow, burst.move,
                                   0, "skyfallBurst", PlayerDamageType::Explosion, true, &dodged);
    }
    if (explosion.cells > 0) {
        // 真ん中に大きな火球を足し、足元の霧を押す。爆ぜた後には、表の「池」が残る。
        if (boss.skyfallBurstBlastScale > 0.0f)
            BossWaves::AddBlast(boss,
                                {.pushPending = true,
                                 .effectPending = true,
                                 .fire = true,
                                 .scale = boss.skyfallBurstBlastScale,
                                 .position = {burst.center.x, burst.center.y + kSkyfallBlastLift, burst.center.z}});
        if (const BossMove* move = BossRules::GetMove(boss, burst.move))
            BossBurnMarks::SkyfallBurst(frame, *move, burst.center);
    }
    char extra[224]{};
    std::snprintf(
        extra, sizeof(extra),
        " cells=%u blasts=%u caught=%d zone=%s fireDistance=%.3f hit=%d dodged=%d playerDistance=%.3f fog=%.3f",
        explosion.cells, explosion.blasts, static_cast<int>(explosion.zone != BurnBlastZone::None),
        BurnFieldRules::ZoneKey(explosion.zone), static_cast<double>(explosion.playerDistance), static_cast<int>(hit),
        static_cast<int>(dodged),
        frame.player != nullptr
            ? static_cast<double>(FlatLength(Sub(AimGround(frame, frame.playerCenter), burst.center)))
            : -1.0,
        static_cast<double>(fog));
    LogBurst(boss, "burst", extra);
    burst = {};
}

void BossSkyfall::CutBurst(BossComponent& boss) {
    if (boss.skyfallBurst.active)
        boss.skyfallBurst.cut = true;
}

} // namespace Project_TD2_2
