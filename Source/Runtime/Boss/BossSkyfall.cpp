#include "BossSkyfall.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 着地の爆発を出す高さ（着地点の上 m）。エフェクトを作るまでの仮の見せ方なので項目にはしない。
constexpr float kSkyfallBlastLift = 1.5f;

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
    bool dodged = false;
    state.hit = BossWaves::HitPlayer(frame, strike.damage, push, strike.blow, boss.move, 0, "skyfall", dodged);
    state.dodged = dodged;
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
        const float u = static_cast<float>(t - state.crouchFrames) / static_cast<float>(state.riseFrames);
        const float e = 1.0f - (1.0f - u) * (1.0f - u);
        boss.crouch = 0.0f;
        boss.height = boss.skyfallHeight * e;
        boss.ground = Add(state.takeoff, Mul(state.away, boss.skyfallBackDrift * e));
    } else {
        SetPhase(boss, BossSkyfallPhase::Hang);
        ++state.hangElapsed;
        boss.height = boss.skyfallHeight;
        state.charge = std::clamp(
            static_cast<float>(state.hangElapsed) / static_cast<float>(std::max(1, state.hangFrames)), 0.0f, 1.0f);
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
    state.landedTick = boss.tick;
    boss.ground = state.target;
    boss.height = 0.0f;
    boss.crouch = 1.0f;
    SetPhase(boss, BossSkyfallPhase::Landed);
    if (!(boss.skyfallBlastScale > 0.0f))
        return;
    // 着地の爆発（当たった所の爆発と同じ枠。見た目は BossVisuals、足元の霧を押すのは BossSmoke）。
    // フェーズ 2 は火球、フェーズ 1 は霞の塊。当てたかどうかに関わらず出す。
    BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
    blast = {};
    blast.pushPending = blast.effectPending = true;
    blast.fire = boss.lastPhaseNumber >= 2;
    blast.scale = boss.skyfallBlastScale;
    blast.position = {state.target.x, state.target.y + kSkyfallBlastLift, state.target.z};
}

void BossSkyfall::Post(BossComponent& boss) {
    boss.height = 0.0f;
    boss.crouch = 1.0f - std::clamp(static_cast<float>(boss.stateFrame) /
                                        static_cast<float>(std::max(1, boss.skyfallLandCrouchFrames)),
                                    0.0f, 1.0f);
}

void BossSkyfall::Reset(BossComponent& boss, const char* reason) {
    SetPhase(boss, BossSkyfallPhase::None, reason);
    boss.skyfall = {};
}

} // namespace Project_TD2_2
