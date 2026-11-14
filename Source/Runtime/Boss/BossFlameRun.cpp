#include "BossFlameRun.h"

#include <algorithm>
#include <cstdio>

#include "BossBurnMarks.h"
#include "BossMath.h"
#include "BossRules.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

Vec3 Flat(Vec3 point) { return {point.x, 0.0f, point.z}; }

// 前線の、走り始めから distance m の所（地面の高さ）。
Vec3 Along(const BossFlameRunFront& front, float distance) { return Add(front.origin, Mul(front.direction, distance)); }

// 次の前線（run.released 本目）を放つ。扇の真ん中の向き run.yaw から見て、左（yaw が増える側）の端から右へ
// flameRunStepDegrees ずつ。走り始める所は、ボスの足元からその前線の向きへ flameRunStartDistance m。
void Release(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossFlameRunState& run = boss.flameRun;
    const int index = run.released;
    BossFlameRunFront& front = run.fronts[static_cast<size_t>(index)];
    const float offset =
        (static_cast<float>(run.count - 1) * 0.5f - static_cast<float>(index)) * boss.flameRunStepDegrees;
    front = {};
    front.running = true;
    front.yaw = WrapPi(run.yaw + offset * kRadiansPerDegree);
    front.direction = Forward(front.yaw);
    const Vec3 start = Add(boss.ground, Mul(front.direction, boss.flameRunStartDistance));
    front.origin = {start.x, boss.ground.y, start.z};
    front.previous = front.front = front.origin;
    front.length = boss.flameRunLength;
    front.releaseTick = boss.tick;
    front.move = boss.move;
    front.speed = std::max(0.0f, BossMoveTable::Strike(move, 0).speed);
    if (const BossBurnMark* mark = BossBurnMarks::Find(move, BossBurnShape::Line, 0, boss.lastPhaseNumber)) {
        front.burnWidth = mark->width;
        front.burnSeconds = mark->seconds;
    }
    ++run.released;
    char message[192]{};
    std::snprintf(message, sizeof(message), "FlameRunRelease tick=%llu index=%d yaw=%.4f from=(%.3f,%.3f)",
                  static_cast<unsigned long long>(boss.tick), index, static_cast<double>(front.yaw),
                  static_cast<double>(front.origin.x), static_cast<double>(front.origin.z));
    BossLog::Write(message);
}

void LogEnd(const BossComponent& boss, size_t index, const BossFlameRunFront& front) {
    char message[224]{};
    std::snprintf(message, sizeof(message),
                  "FlameRunEnd tick=%llu index=%zu travelled=%.3f hit=%d dodged=%d painted=%.3f",
                  static_cast<unsigned long long>(boss.tick), index, static_cast<double>(front.travelled),
                  static_cast<int>(front.hit), static_cast<int>(front.dodged), static_cast<double>(front.painted));
    BossLog::Write(message);
}

} // namespace

void BossFlameRun::Begin(BossComponent& boss, const BossMove& move) {
    if (move.type != BossMoveType::FlameRun)
        return;
    BossFlameRunState& run = boss.flameRun;
    // 前線の枠の数を超えて放たない（シーンに書かれた項目は、属性の範囲を通らずに読まれる）。
    run.count = std::clamp(boss.lastPhaseNumber >= 2 ? boss.flameRunCountPhase2 : boss.flameRunCount, 1,
                           static_cast<int>(kBossFlameRunCapacity));
    run.released = 0;
    boss.actFrames = move.act + (run.count - 1) * std::max(1, boss.flameRunIntervalFrames);
}

void BossFlameRun::Act(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    BossFlameRunState& run = boss.flameRun;
    // 攻撃中の最初の tick（stateFrame 1）に 1 本目、そこから flameRunIntervalFrames ごとに次の 1 本。
    if (run.released >= run.count || boss.stateFrame - 1 != run.released * std::max(1, boss.flameRunIntervalFrames))
        return;
    if (run.released == 0) {
        // 扇の向きは前隙の向き直りが終わった向き。攻撃中はボスが向き直らないので、放つ間ずっと同じ扇になる。
        run.yaw = boss.yaw;
        char message[160]{};
        std::snprintf(message, sizeof(message), "FlameRunStart tick=%llu count=%d yaw=%.4f step=%.3f",
                      static_cast<unsigned long long>(boss.tick), run.count, static_cast<double>(run.yaw),
                      static_cast<double>(boss.flameRunStepDegrees));
        BossLog::Write(message);
    }
    Release(frame, move);
}

void BossFlameRun::Cut(BossComponent& boss) {
    BossFlameRunState& run = boss.flameRun;
    if (run.released >= run.count)
        return;
    char message[128]{};
    std::snprintf(message, sizeof(message), "FlameRunCut tick=%llu released=%d count=%d",
                  static_cast<unsigned long long>(boss.tick), run.released, run.count);
    BossLog::Write(message);
    run.count = run.released;
}

// 前線が通った線分（1 tick 前〜いまの前線）から、プレイヤーの足元が水平に「判定サイズ＋体の半径」以内なら当てる。
// 1 本につき 1 回だけ（無敵でよけた時も、その前線はもう当たらない）。ほかの前線はそれぞれ当たり得る。
void BossFlameRun::Step(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    for (size_t k = 0; k < boss.flameRun.fronts.size(); ++k) {
        BossFlameRunFront& front = boss.flameRun.fronts[k];
        if (!front.running)
            continue;
        const BossMove* row = BossRules::GetMove(boss, front.move);
        front.previous = front.front;
        front.travelled = std::min(front.length, front.travelled + front.speed * frame.dt);
        front.front = Along(front, front.travelled);
        if (!front.caught && frame.player != nullptr && row != nullptr) {
            const BossStrikeData& strike = BossMoveTable::Strike(*row, 0);
            const Vec3 feet = Flat(frame.playerCenter);
            if (SegmentDistance(Flat(front.previous), Flat(front.front), feet, feet) <=
                strike.size + boss.playerHitRadius) {
                front.caught = true;
                front.hit = BossWaves::HitPlayer(frame, strike.damage, front.direction, strike.blow, front.move, 0,
                                                 "flameRun", PlayerDamageType::Slash, true, &front.dodged);
            }
        }
        const bool done = front.travelled >= front.length;
        if (front.burnWidth > 0.0f && row != nullptr) {
            // 火は前線の burnLineLag m 後ろまで塗り（前線より先に地面が燃えて見えないように）、走り切った tick に残りを塗る。
            const float to =
                done ? front.travelled : std::max(front.painted, front.travelled - std::max(0.0f, boss.burnLineLag));
            if (to > front.painted || done) {
                BossBurnMark mark;
                mark.shape = BossBurnShape::Line;
                mark.width = front.burnWidth;
                mark.seconds = front.burnSeconds;
                const Vec3 origin = front.origin;
                BossBurnMarks::PaintCleaveLine(frame, *row, Along(front, front.painted), Along(front, to), mark,
                                               done ? &origin : nullptr);
                front.painted = to;
            }
        }
        if (done) {
            front.running = false;
            front.ended = true;
            LogEnd(boss, k, front);
        }
    }
}

} // namespace Project_TD2_2
