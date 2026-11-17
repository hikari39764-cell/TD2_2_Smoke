#include "BossKasumiStep.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

#include "BossCsv.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossWaves.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 弾の線にほぼ乗っていて、どちらの側にいるか決められないとみなす外れ (m)。乗っていれば左右は乱数で決める。
constexpr float kShotCenterBand = 0.25f;
// 燃えている地面を確かめる、行き先の周りの点の数（行き先そのものと道の真ん中も見る）。
constexpr int kFireProbePoints = 4;
// 跳ぶ間に、前隙の屈みを戻す割合（1 tick あたり）。
constexpr float kTakeoffRelease = 0.35f;
// 確かめる行き先の数の上限（表の向き 8 ＋ 基本の 6 向き、弾の側は 4 通り）。
constexpr size_t kCandidateCapacity = 24;

constexpr std::array<const char*, 7> kDirectionKeys = {"left",     "right",     "leftForward", "rightForward",
                                                       "leftBack", "rightBack", "awayFromShot"};
constexpr std::array<BossStepDirection, 6> kBasicDirections = {
    BossStepDirection::Left,         BossStepDirection::Right,    BossStepDirection::LeftForward,
    BossStepDirection::RightForward, BossStepDirection::LeftBack, BossStepDirection::RightBack};

void Log(const BossComponent& boss, const char* event, const char* extra) {
    char message[448]{};
    std::snprintf(message, sizeof(message), "%s tick=%llu %s", event, static_cast<unsigned long long>(boss.tick),
                  extra);
    BossLog::Write(message);
}

// プレイヤーへの向きと、その時のボスの左（右手系で前 f の左は (f.z, 0, −f.x)。体の局所 +X）。真横へ跳ぶ向きを
// プレイヤーの方へ曲げる角の cos・sin（真横へまっすぐ跳ぶと、跳ぶたびにプレイヤーから遠ざかるので、プレイヤーを中心に
// 回る弦の向きへ寄せる。Boss.kasumiStepOrbit が 1 なら、跳ぶ前と同じ距離に降りる）。
struct StepFrame {
    Vec3 forward;
    Vec3 left;
    float bendCos = 1.0f;
    float bendSin = 0.0f;
};

StepFrame FrameOf(const BossFrame& frame, float distance) {
    const BossComponent& boss = frame.boss;
    StepFrame f;
    f.forward = Forward(boss.yaw);
    if (frame.player != nullptr) {
        const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
        f.forward = FlatNormalize(offset, f.forward);
        const float radius = FlatLength(offset);
        const float half = radius > 1e-3f ? std::clamp(distance / (2.0f * radius), 0.0f, 1.0f) : 0.0f;
        const float bend = std::clamp(boss.kasumiStep.orbit, 0.0f, 1.0f) * std::asin(half);
        f.bendCos = std::cos(bend);
        f.bendSin = std::sin(bend);
    }
    f.left = {f.forward.z, 0.0f, -f.forward.x};
    return f;
}

// 真横（side が +1 で左、−1 で右）を、プレイヤーを中心に回る弦の向きへ曲げる。
Vec3 Sideways(const StepFrame& f, Vec3 side, Vec3 toward) { return Add(Mul(side, f.bendCos), Mul(toward, f.bendSin)); }

struct Candidate {
    BossStepDirection key = BossStepDirection::Left;
    Vec3 direction{};
};

struct Candidates {
    std::array<Candidate, kCandidateCapacity> items{};
    size_t count = 0;
    void Push(BossStepDirection key, Vec3 direction) {
        if (count < items.size())
            items[count++] = {key, direction};
    }
};

// 表の向き（弾の側以外）の水平の単位ベクトル。
Vec3 DirectionOf(const BossComponent& boss, const StepFrame& f, BossStepDirection key) {
    const float angle = std::clamp(boss.kasumiStep.diagonalDegrees, 0.0f, 90.0f) * kRadiansPerDegree;
    const float across = std::cos(angle), along = std::sin(angle);
    switch (key) {
    case BossStepDirection::Right: return Sideways(f, Mul(f.left, -1.0f), f.forward);
    case BossStepDirection::LeftForward: return Add(Mul(f.left, across), Mul(f.forward, along));
    case BossStepDirection::RightForward: return Add(Mul(f.left, -across), Mul(f.forward, along));
    case BossStepDirection::LeftBack: return Add(Mul(f.left, across), Mul(f.forward, -along));
    case BossStepDirection::RightBack: return Add(Mul(f.left, -across), Mul(f.forward, -along));
    default: return Sideways(f, f.left, f.forward);
    }
}

// 弾の線から離れる側：線に直交する向き（離れる側が先）と、線から離れつつ撃つ人から遠ざかる斜め後ろ。線の上にほぼ
// 乗っている時はどちらの側か乱数で決める。弾の線をまだ見ていなければ左右。
void PushAwayFromShot(BossComponent& boss, const StepFrame& f, Candidates& out) {
    const BossPlayerWatch& watch = boss.watch;
    const Vec3 aim = watch.shotLineSeen ? FlatNormalize(watch.shotDirection, f.forward) : Mul(f.forward, -1.0f);
    const Vec3 normal{aim.z, 0.0f, -aim.x};
    const float offset = watch.shotLineSeen ? Dot(Sub(boss.ground, watch.shotOrigin), normal) : 0.0f;
    const float side = std::abs(offset) > kShotCenterBand ? (offset > 0.0f ? 1.0f : -1.0f)
                       : BossRandom(boss) < 0.5f          ? 1.0f
                                                          : -1.0f;
    const float angle = std::clamp(boss.kasumiStep.diagonalDegrees, 0.0f, 90.0f) * kRadiansPerDegree;
    for (const float s : {side, -side}) {
        out.Push(BossStepDirection::AwayFromShot, Sideways(f, Mul(normal, s), Mul(aim, -1.0f)));
        out.Push(BossStepDirection::AwayFromShot, Add(Mul(normal, s * std::cos(angle)), Mul(aim, std::sin(angle))));
    }
}

// 行き先へ降りられないわけ（降りられれば nullptr）：戦場の縁・プレイヤーの近く・燃えている地面。
const char* Blocked(BossFrame& frame, Vec3 from, Vec3 to) {
    const BossComponent& boss = frame.boss;
    const BossKasumiStepSettings& s = boss.kasumiStep;
    if (FlatLength(Sub(to, boss.arenaCenter)) > std::max(0.0f, boss.arenaRadius - s.edgeMargin))
        return "edge";
    if (frame.player != nullptr && FlatLength(Sub(to, PlayerGround(frame))) < s.playerGap)
        return "player";
    if (BurnFieldRules::IsBurning(frame.world, to) || BurnFieldRules::IsBurning(frame.world, Lerp(from, to, 0.5f)))
        return "fire";
    for (int k = 0; k < kFireProbePoints && s.fireClearance > 0.0f; ++k) {
        const float angle = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(kFireProbePoints);
        if (BurnFieldRules::IsBurning(frame.world, Add(to, Mul(Forward(angle), s.fireClearance))))
            return "fire";
    }
    return nullptr;
}

struct Choice {
    Candidate candidate;
    Vec3 to{};
    bool blocked = false;
    // 降りられなかった向きとわけ（ログ用。入りきらない分は切る）。
    char rejected[192] = {};
    size_t used = 0;
    void Reject(BossStepDirection key, const char* reason) {
        if (used + 1 >= sizeof(rejected))
            return;
        const int written = std::snprintf(rejected + used, sizeof(rejected) - used, "%s%s:%s", used > 0 ? "," : "",
                                          kDirectionKeys[static_cast<size_t>(key)], reason);
        used = std::min(sizeof(rejected) - 1, used + static_cast<size_t>(std::max(0, written)));
    }
};

// 表の向きを乱数で決めた所から順に確かめ、だめなら基本の 6 向き、それもだめなら最初の候補を戦場の内へ縮めて使う。
Choice Choose(BossFrame& frame, const BossMove& move, float distance) {
    BossComponent& boss = frame.boss;
    const StepFrame f = FrameOf(frame, distance);
    Candidates candidates;
    const int listed = move.kasumiStep.directionCount;
    const int start =
        listed > 0 ? std::min(listed - 1, static_cast<int>(BossRandom(boss) * static_cast<float>(listed))) : 0;
    for (int k = 0; k < listed; ++k) {
        const BossStepDirection key = move.kasumiStep.directions[static_cast<size_t>((start + k) % listed)];
        if (key == BossStepDirection::AwayFromShot)
            PushAwayFromShot(boss, f, candidates);
        else
            candidates.Push(key, DirectionOf(boss, f, key));
    }
    const size_t basic = kBasicDirections.size();
    const size_t offset = std::min(basic - 1, static_cast<size_t>(BossRandom(boss) * static_cast<float>(basic)));
    for (size_t k = 0; k < basic; ++k)
        candidates.Push(kBasicDirections[(offset + k) % basic],
                        DirectionOf(boss, f, kBasicDirections[(offset + k) % basic]));
    Choice choice;
    for (size_t i = 0; i < candidates.count; ++i) {
        const Candidate& candidate = candidates.items[i];
        const Vec3 to = Add(boss.ground, Mul(candidate.direction, distance));
        const char* reason = Blocked(frame, boss.ground, to);
        if (reason == nullptr) {
            choice.candidate = candidate;
            choice.to = to;
            return choice;
        }
        choice.Reject(candidate.key, reason);
    }
    // どこへも降りられない：最初の候補へ、戦場の縁の内側まで縮めて跳ぶ（燃えている地面は避けきれない）。
    choice.blocked = true;
    choice.candidate = candidates.items[0];
    choice.to = Add(boss.ground, Mul(choice.candidate.direction, distance));
    const Vec3 fromCenter = Sub(choice.to, boss.arenaCenter);
    const float limit = std::max(0.0f, boss.arenaRadius - boss.kasumiStep.edgeMargin);
    if (FlatLength(fromCenter) > limit) {
        const Vec3 inside = Mul(FlatNormalize(fromCenter), limit);
        choice.to = {boss.arenaCenter.x + inside.x, boss.ground.y, boss.arenaCenter.z + inside.z};
    }
    return choice;
}

// 続けてかわした回数を数え、使い切ったら、しばらくかわさない。
void NoteDodge(BossComponent& boss) {
    const BossKasumiStepSettings& s = boss.kasumiStep;
    BossKasumiStepState& state = boss.kasumiStepState;
    if (boss.clockSeconds - state.lastDodgeSeconds > s.dodgeChainSeconds)
        state.dodges = 0;
    ++state.dodges;
    state.lastDodgeSeconds = boss.clockSeconds;
    if (state.dodges < std::max(1, s.dodgeMax))
        return;
    state.dodges = 0;
    state.lockedUntilSeconds = boss.clockSeconds + std::max(0.0f, s.dodgeLockSeconds);
    char extra[96]{};
    std::snprintf(extra, sizeof(extra), "seconds=%.2f until=%.2f", static_cast<double>(s.dodgeLockSeconds),
                  static_cast<double>(state.lockedUntilSeconds));
    Log(boss, "DodgeSpent", extra);
}

// 着地した tick：足元の霧を押す（爆発の押しを小さくしたもの。見た目のエフェクトと吸える煙は出さない）。
void Land(BossComponent& boss) {
    BossKasumiStepState& state = boss.kasumiStepState;
    state.landed = true;
    if (boss.kasumiStep.landPush > 0.0f)
        BossWaves::AddBlast(
            boss, {.pushPending = true, .leavesFog = false, .scale = boss.kasumiStep.landPush, .position = state.to});
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "at=(%.3f,%.3f) moved=%.3f", static_cast<double>(boss.ground.x),
                  static_cast<double>(boss.ground.z), static_cast<double>(FlatLength(Sub(boss.ground, state.from))));
    Log(boss, "KasumiLand", extra);
}

bool Stepping(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    return boss.state == BossState::Act && boss.move >= 0 && static_cast<size_t>(boss.move) < moves.size() &&
           moves[static_cast<size_t>(boss.move)].type == BossMoveType::KasumiStep;
}

} // namespace

void BossKasumiStep::Read(std::string_view directions, std::string_view distance, std::string_view height,
                          BossMove& move) {
    using namespace BossCsv;
    for (const auto& part : Split(directions, true, true)) {
        const std::string text = Trim(part);
        int key = -1;
        if (Contains(text, "弾"))
            key = static_cast<int>(BossStepDirection::AwayFromShot);
        else if (StartsWith(text, "左") || StartsWith(text, "右"))
            key = (StartsWith(text, "左") ? 0 : 1) + (Contains(text, "前") ? 2 : Contains(text, "後") ? 4 : 0);
        if (key < 0) {
            move.warnings.push_back("跳ぶ向き「" + text +
                                    "」が読めないので飛ばしました（左・右・左前・右前・左後・右後・弾）");
            continue;
        }
        if (static_cast<size_t>(move.kasumiStep.directionCount) < move.kasumiStep.directions.size())
            move.kasumiStep.directions[static_cast<size_t>(move.kasumiStep.directionCount++)] =
                static_cast<BossStepDirection>(key);
    }
    if (move.kasumiStep.directionCount == 0) {
        move.kasumiStep.directions[0] = BossStepDirection::Left;
        move.kasumiStep.directions[1] = BossStepDirection::Right;
        move.kasumiStep.directionCount = 2;
    }
    const auto meters = NumberOf(distance);
    if (!distance.empty() && (!meters || *meters <= 0.0))
        move.warnings.push_back("跳ぶ距離「" + std::string(distance) +
                                "」が読めないのでボスの項目にしました（0 より大きい m）");
    move.kasumiStep.distance = meters && *meters > 0.0 ? static_cast<float>(*meters) : 0.0f;
    const auto rise = NumberOf(height);
    if (!height.empty() && (!rise || *rise < 0.0))
        move.warnings.push_back("跳ぶ高さ「" + std::string(height) +
                                "」が読めないのでボスの項目にしました（0 以上の m）");
    move.kasumiStep.height = rise && *rise >= 0.0 ? static_cast<float>(*rise) : -1.0f;
}

void BossKasumiStep::Begin(BossFrame& frame, const BossMove& move) {
    if (move.type != BossMoveType::KasumiStep)
        return;
    BossComponent& boss = frame.boss;
    const BossKasumiStepSettings& s = boss.kasumiStep;
    BossKasumiStepState& state = boss.kasumiStepState;
    const float distance = move.kasumiStep.distance > 0.0f ? move.kasumiStep.distance : s.distance;
    const Choice choice = Choose(frame, move, distance);
    state.from = boss.ground;
    state.to = choice.to;
    state.height = move.kasumiStep.height >= 0.0f ? move.kasumiStep.height : s.height;
    state.direction = static_cast<int>(choice.candidate.key);
    state.dodge = choice.candidate.key == BossStepDirection::AwayFromShot;
    state.landed = false;
    if (state.dodge)
        NoteDodge(boss);
    char extra[384]{};
    std::snprintf(
        extra, sizeof(extra),
        "move=%s dir=%s dodge=%d blocked=%d distance=%.3f frames=%d from=(%.3f,%.3f) to=(%.3f,%.3f) "
        "playerDistance=%.3f rejected=%s",
        move.name.c_str(), DirectionKey(choice.candidate.key), static_cast<int>(state.dodge),
        static_cast<int>(choice.blocked), static_cast<double>(FlatLength(Sub(state.to, state.from))), boss.actFrames,
        static_cast<double>(state.from.x), static_cast<double>(state.from.z), static_cast<double>(state.to.x),
        static_cast<double>(state.to.z),
        static_cast<double>(frame.player != nullptr ? FlatLength(Sub(PlayerGround(frame), boss.ground)) : -1.0f),
        choice.used == 0 ? "-" : choice.rejected);
    Log(boss, "KasumiStep", extra);
}

void BossKasumiStep::Pre(BossComponent& boss, float progress) {
    boss.crouch = boss.kasumiStep.takeoffCrouch * std::clamp(progress, 0.0f, 1.0f);
}

void BossKasumiStep::Act(BossFrame& frame, float progress) {
    BossComponent& boss = frame.boss;
    const BossKasumiStepState& state = boss.kasumiStepState;
    const float u = std::clamp(progress, 0.0f, 1.0f);
    const float eased = 1.0f - std::pow(1.0f - u, std::max(1.0f, boss.kasumiStep.easePower));
    boss.ground = Lerp(state.from, state.to, eased);
    boss.height = state.height * std::sin(u * kPi);
    boss.crouch = Lerp(boss.crouch, 0.0f, kTakeoffRelease);
    BossMoveExecutor::FaceRate(frame, boss.kasumiStep.turnRate);
    if (u >= 1.0f && !state.landed)
        Land(boss);
}

void BossKasumiStep::Post(BossComponent& boss, float progress) {
    // 着地で沈む：後隙の始めの 1/4 で沈みきり、残りで起きる。
    boss.height = 0.0f;
    boss.crouch = boss.kasumiStep.landCrouch * std::sin(kPi * std::sqrt(std::clamp(progress, 0.0f, 1.0f)));
}

bool BossKasumiStep::Guarding(const BossComponent& boss) {
    return boss.kasumiStep.guardFrames > 0 && Stepping(boss) && boss.kasumiStepState.dodge &&
           boss.phase == BossActPhase::Act && boss.stateFrame <= boss.kasumiStep.guardFrames;
}

void BossKasumiStep::AddLean(const BossComponent& boss, float& lean, float& sway) {
    if (!Stepping(boss) || boss.phase != BossActPhase::Act)
        return;
    const BossKasumiStepState& state = boss.kasumiStepState;
    const float u = Share(boss.stateFrame, boss.actFrames);
    const float amount = boss.kasumiStep.leanDegrees * kRadiansPerDegree * std::sin(u * kPi);
    const Vec3 direction = FlatNormalize(Sub(state.to, state.from), Forward(boss.yaw));
    const Vec3 left{std::cos(boss.yaw), 0.0f, -std::sin(boss.yaw)};
    lean += amount * Dot(direction, Forward(boss.yaw));
    // 体の傾きの規則：横の傾きが負なら左（局所 +X）へ傾く。
    sway -= amount * Dot(direction, left);
}

bool BossKasumiStep::CanDodge(const BossComponent& boss) {
    return boss.clockSeconds >= boss.kasumiStepState.lockedUntilSeconds;
}

bool BossKasumiStep::Spent(const BossComponent& boss) { return !CanDodge(boss); }

bool BossKasumiStep::Allows(const BossComponent& boss, const BossMove& move) {
    return move.type != BossMoveType::KasumiStep || !boss.kasumiStep.restWhileSpent || !Spent(boss);
}

const char* BossKasumiStep::DirectionKey(BossStepDirection direction) {
    const size_t index = static_cast<size_t>(direction);
    return index < kDirectionKeys.size() ? kDirectionKeys[index] : "?";
}

} // namespace Project_TD2_2
