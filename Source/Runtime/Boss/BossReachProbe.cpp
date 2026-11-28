#include "BossReachProbe.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "BossMath.h"
#include "BossMoveTable.h"
#include "BossPose.h"
#include "BossRules.h"
#include "../BossBody/BossAnimBody.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 1 tick の刀の動きを分けて確かめる数（実行器の振りの当たりと同じ考え：速い振りが体を飛び越えないように）。
constexpr int kSweepSteps = 8;
// 高さを無視した筒の上下の長さ (m)。刀がどの高さを通っても届けば当たる、と見るための十分に長い筒。
constexpr float kTallReach = 1000.0f;

// 測っている間だけ書き換えるボスの状態（測り終えたら元に戻す）。
struct SavedState {
    explicit SavedState(Boss& boss)
        : boss_(boss), move_(boss.move), stateFrame_(boss.stateFrame), state_(boss.state), phase_(boss.phase),
          ground_(boss.ground), yaw_(boss.yaw), height_(boss.height), floatHeight_(boss.floatHeight),
          spin_(boss.spin), aimTwist_(boss.aimTwist) {}
    ~SavedState() {
        boss_.aimTwist = aimTwist_;
        boss_.move = move_;
        boss_.stateFrame = stateFrame_;
        boss_.state = state_;
        boss_.phase = phase_;
        boss_.ground = ground_;
        boss_.yaw = yaw_;
        boss_.height = height_;
        boss_.floatHeight = floatHeight_;
        boss_.spin = spin_;
    }
    SavedState(const SavedState&) = delete;
    SavedState& operator=(const SavedState&) = delete;

private:
    Boss& boss_;
    int move_;
    int stateFrame_;
    BossState state_;
    BossActPhase phase_;
    Vec3 ground_;
    float yaw_;
    float height_;
    float floatHeight_;
    float spin_;
    float aimTwist_;
};

struct SwordSample {
    Vec3 base;
    Vec3 outer;
};

// 前の tick の刀からいまの刀までを分けて、筒（low〜high、半径 radius）に触れるか。
bool SweepTouches(const SwordSample& from, const SwordSample& to, Vec3 low, Vec3 high, float radius) {
    for (int k = 1; k <= kSweepSteps; ++k) {
        const float u = static_cast<float>(k) / static_cast<float>(kSweepSteps);
        if (SegmentDistance(Lerp(from.base, to.base, u), Lerp(from.outer, to.outer, u), low, high) < radius)
            return true;
    }
    return false;
}

// 当たった角度の並びを「-40..30,90..120」の形にする。
std::string Intervals(const std::vector<int>& angles, int step) {
    std::string text;
    for (size_t i = 0; i < angles.size();) {
        size_t j = i;
        while (j + 1 < angles.size() && angles[j + 1] - angles[j] == step)
            ++j;
        if (!text.empty())
            text += ",";
        text += std::to_string(angles[i]) + ".." + std::to_string(angles[j]);
        i = j + 1;
    }
    return text.empty() ? "-" : text;
}

// 1 つの斬り（当たりの窓）の刀を、ボスを原点・正面固定に置いて読む。
std::vector<SwordSample> SampleStrike(Boss& boss, const BossMove& move, int moveIndex, int start, int length,
                                      int hitIndex) {
    boss.move = moveIndex;
    boss.state = BossState::Act;
    boss.phase = BossActPhase::Act;
    boss.ground = {};
    boss.yaw = 0.0f;
    boss.height = 0.0f;
    // 攻撃中は一番低く降りる（BossMoveExecutor::StepFloat）。
    boss.floatHeight = boss.floatMin;
    // 狙いのずれは入れない（クリップの刀そのものが落ちる向きを測る。狙いのずれの表はこの値から作る）。
    boss.aimTwist = 0.0f;
    std::vector<SwordSample> samples;
    for (int t = 1; t <= length; ++t) {
        boss.stateFrame = start + t;
        boss.spin = move.actSpin != 0.0f && hitIndex == 0 ? move.actSpin * BossPose::SwingShare(boss, t, length) : 0.0f;
        const BossSwordPoints sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
        samples.push_back({sword.base, sword.outer});
    }
    return samples;
}

void ProbeStrike(const Boss& boss, const BossMove& move, int hitIndex, int start, int length,
                 const std::vector<SwordSample>& samples, float centerHeight) {
    const float radius = boss.thickness * 0.5f + boss.playerHitRadius;
    const float step = std::max(0.05f, boss.reachProbeDistanceStep);
    const int angleStep = std::clamp(boss.reachProbeAngleStep, 1, 90);
    // 刀の一番低い所・高い所（足元から m）。近くで頭の上を通っているかの目安。
    float lowest = 1e9f, highest = -1e9f;
    for (const SwordSample& s : samples) {
        lowest = std::min({lowest, s.base.y, s.outer.y});
        highest = std::max({highest, s.base.y, s.outer.y});
    }
    char message[640]{};
    // hitStart・hitEnd は区間の表の当たりの始め・終わり F、clip〜fingerprint は狙いのずれの表（verify.py aim）に写す。
    std::snprintf(message, sizeof(message),
                  "ReachStrike move=%s strike=%d window=%d-%d frames=%zu bladeLow=%.2f bladeHigh=%.2f radius=%.3f "
                  "playerCenter=%.2f clip=%s clipFrames=%d fingerprint=%08x hitStart=%d hitEnd=%d",
                  move.name.c_str(), hitIndex, move.pre + start + 1, move.pre + start + length, samples.size(),
                  static_cast<double>(lowest), static_cast<double>(highest), static_cast<double>(radius),
                  static_cast<double>(centerHeight), move.clip.c_str(), move.clipFrames,
                  BossAnimBody::SwordFingerprint(boss, move.clip), move.pre + start, move.pre + start + length);
    BossLog::Write(message);
    for (float d = boss.reachProbeDistanceMin; d <= boss.reachProbeDistanceMax + 1e-4f; d += step) {
        std::vector<int> hit, tall;
        for (int angle = -180 + angleStep; angle <= 180; angle += angleStep) {
            // 角度は正がボスの右（向きの 0 は +Z、右は −X）。
            const Vec3 at = Mul(Forward(-static_cast<float>(angle) * kRadiansPerDegree), d);
            const Vec3 center{at.x, centerHeight, at.z};
            const Vec3 low = Add(center, {0.0f, boss.playerHitLow, 0.0f});
            const Vec3 high = Add(center, {0.0f, boss.playerHitHigh, 0.0f});
            const Vec3 tallLow{at.x, -kTallReach, at.z}, tallHigh{at.x, kTallReach, at.z};
            bool real = false, reach = false;
            for (size_t k = 1; k < samples.size() && !(real && reach); ++k) {
                real = real || SweepTouches(samples[k - 1], samples[k], low, high, radius);
                reach = reach || SweepTouches(samples[k - 1], samples[k], tallLow, tallHigh, radius);
            }
            if (real)
                hit.push_back(angle);
            if (reach)
                tall.push_back(angle);
        }
        std::snprintf(message, sizeof(message), "ReachProbe move=%s strike=%d d=%.2f hit=%s tall=%s", move.name.c_str(),
                      hitIndex, static_cast<double>(d), Intervals(hit, angleStep).c_str(),
                      Intervals(tall, angleStep).c_str());
        BossLog::Write(message);
    }
}

} // namespace

void BossReachProbe::Step(BossFrame& frame) {
    Boss& boss = frame.boss;
    if (!boss.reachProbe || boss.reachProbeDone || frame.player == nullptr || boss.state != BossState::Idle)
        return;
    const std::vector<BossMove>& moves = boss.table.GetMoves();
    bool ready = false;
    for (const BossMove& move : moves)
        ready = ready || BossAnimBody::ClipMove(boss, move);
    if (!ready)
        return;
    boss.reachProbeDone = true;
    // プレイヤーの体の中心の、足元からの高さ（いま立っている所で測る）。
    const float centerHeight = frame.playerCenter.y - boss.ground.y;
    SavedState saved(boss);
    for (size_t m = 0; m < moves.size(); ++m) {
        const BossMove& move = moves[m];
        // 刀で当てる技だけ（左手の技の弾・輪、飛ぶ斬撃だけで当てる技は刀の当たりが無い）。
        if (!BossAnimBody::ClipMove(boss, move) || !BossMoveTable::IsSwingType(move.type) ||
            move.delivery == BossDelivery::Haze)
            continue;
        int accumulated = 0, hitIndex = 0;
        for (const BossSegment& segment : move.segments) {
            if (segment.hit) {
                const std::vector<SwordSample> samples =
                    SampleStrike(boss, move, static_cast<int>(m), accumulated, segment.length, hitIndex);
                ProbeStrike(boss, move, hitIndex, accumulated, segment.length, samples, centerHeight);
                ++hitIndex;
            }
            accumulated += segment.length;
        }
    }
    BossLog::Write("ReachDone");
}

} // namespace Project_TD2_2
