#include "BossGourdRead.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossFrame.h"
#include "BossMath.h"
#include "BossMoveTable.h"
#include "BossObserver.h"

namespace Project_TD2_2 {

using namespace BossMath;

void BossGourdReadSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("ヒョウタンの読み（転がる癖）", false))
        return;
    fields.Note("同じ側へ転がる癖があれば、横の剣気をその側へ寄せる。ログ [Boss] GourdLean。");
    fields.Bool("gourdReadEnabled", "ヒョウタンの読みを使う（全体の入・切）", enabled);
    fields.Bool("gourdLeanEnabled", "同じ側へ転がる癖があれば、横の剣気をその側へ寄せる", leanEnabled);
    fields.Int("gourdLeanRolls", "転がる癖：最近の何回を見るか", leanRolls, 1, 16);
    fields.Int("gourdLeanMinRolls", "転がる癖：少なくとも何回数えてから", leanMinRolls, 1, 16);
    fields.Float("gourdLeanAtLeastPercent", "転がる癖：左か右がこの割合以上 (%)", leanAtLeastPercent, 0.0f, 100.0f);
    fields.Float("gourdLeanMeters", "寄せる幅（プレイヤーの所で、m）", leanMeters, 0.0f, 10.0f);
    fields.Float("gourdLeanMaxDegrees", "寄せる角度の上限 (度)", leanMaxDegrees, 0.0f, 45.0f);
}

void BossGourdRead::LeanSlash(const BossFrame& frame, BossWave& wave) {
    const BossComponent& boss = frame.boss;
    const BossGourdReadSettings& s = boss.gourdRead;
    if (!s.enabled || !s.leanEnabled || frame.player == nullptr || !boss.watch.present)
        return;
    // 横の剣気だけ（三日月の幅の向きが水平に近い）。縦・袈裟は寄せない。
    if (std::abs(wave.across.y) > 0.5f)
        return;
    int counts[4]{};
    const int rolls = BossObserver::CountRecentRolls(boss.watch, std::max(1, s.leanRolls), counts);
    if (rolls < std::max(1, s.leanMinRolls))
        return;
    const float left =
        static_cast<float>(counts[static_cast<int>(BossRollDirection::Left)]) * 100.0f / static_cast<float>(rolls);
    const float right =
        static_cast<float>(counts[static_cast<int>(BossRollDirection::Right)]) * 100.0f / static_cast<float>(rolls);
    // ボスから見た右（向き d の右は (−d.z, 0, d.x)。BossObserver のローリングの左右と同じ決め方）。
    const float side = right >= s.leanAtLeastPercent && right >= left ? 1.0f
                       : left >= s.leanAtLeastPercent                 ? -1.0f
                                                                      : 0.0f;
    if (side == 0.0f)
        return;
    const float distance = std::max(1.0f, FlatLength(Sub(frame.playerCenter, wave.position)));
    const float angle =
        std::min(std::atan2(std::max(0.0f, s.leanMeters), distance), s.leanMaxDegrees * kRadiansPerDegree) * side;
    const float c = std::cos(angle);
    const float n = std::sin(angle);
    // 水平に angle だけ回す（向きと幅の向きを同じだけ回す。上下の成分はそのまま）。
    auto turn = [&](Vec3 v) { return Vec3{v.x * c - v.z * n, v.y, v.z * c + v.x * n}; };
    wave.direction = turn(wave.direction);
    wave.across = turn(wave.across);
    char message[192]{};
    std::snprintf(
        message, sizeof(message), "GourdLean tick=%llu move=%d side=%s degrees=%.1f rolls=%d left=%d right=%d",
        static_cast<unsigned long long>(boss.tick), wave.move, side > 0.0f ? "right" : "left",
        static_cast<double>(angle / kRadiansPerDegree), rolls, counts[static_cast<int>(BossRollDirection::Left)],
        counts[static_cast<int>(BossRollDirection::Right)]);
    BossLog::Write(message);
}

} // namespace Project_TD2_2
