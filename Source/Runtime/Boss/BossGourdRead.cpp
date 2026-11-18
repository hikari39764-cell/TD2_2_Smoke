#include "BossGourdRead.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossFrame.h"
#include "BossMath.h"
#include "BossMoveTable.h"
#include "BossObserver.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 煙弾の費用が読めない時（砲撃が無い）の「空に近い」の境 (%)。SmokeArena の 1 発は容量の 20 %。
constexpr float kFallbackShotPercent = 20.0f;

bool IsLow(const BossComponent& boss) {
    const BossGourdReadSettings& s = boss.gourdRead;
    const BossPlayerWatch& watch = boss.watch;
    if (!watch.present || watch.gourdPercent < 0.0f || watch.gourdFull)
        return false;
    const float shot = watch.shotCostPercent > 0.0f ? watch.shotCostPercent : kFallbackShotPercent;
    return watch.gourdPercent < shot * std::max(0.0f, s.lowShots);
}

// 剣気を吸われた時刻のうち、新しい方から window 秒のうちの本数。
int RecentCatches(const BossGourdReadMemory& m, float now, float window) {
    int count = 0;
    for (size_t i = 0; i < m.catchCount; ++i) {
        const size_t at = (m.catchNext + m.catches.size() - 1 - i) % m.catches.size();
        if (now - m.catches[at] > window)
            break;
        ++count;
    }
    return count;
}

} // namespace

void BossGourdReadSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("ヒョウタンの読み（満杯・空に近い・吸われ続けた・構え続け・転がる癖）", false))
        return;
    fields.Note("攻勢の技選び（行動ツリーの節「姿勢で技を選ぶ」）の重みに掛ける。吸いやすい剣気の印は "
                "Data/Boss/kasumi_bushi_stance.csv の「吸いやすい」の列。ログ [Boss] Stance Pick … read=・GourdHold・"
                "GourdChain・GourdLean。");
    fields.Bool("gourdReadEnabled", "ヒョウタンの読みを使う（全体の入・切）", enabled);
    fields.Bool("gourdFullEnabled", "満杯（もう吸えない）を読む", fullEnabled);
    fields.Float("gourdFullFarScale", "満杯：遠距離の重みに掛ける倍率", fullFarScale, 0.0f, 10.0f);
    fields.Float("gourdFullNearScale", "満杯：接近の重みに掛ける倍率", fullNearScale, 0.0f, 10.0f);
    fields.Bool("gourdLowEnabled", "空に近い（弾 1 発ぶんに足りない）を読む", lowEnabled);
    fields.Float("gourdLowShots", "空に近い：煙が弾この発ぶんより少ない", lowShots, 0.0f, 10.0f);
    fields.Float("gourdLowEasyScale", "空に近い：吸いやすい剣気（表の印）の重みに掛ける倍率", lowEasyScale, 0.0f,
                 20.0f);
    fields.Float("gourdLowFarScale", "空に近い：ほかの遠距離の重みに掛ける倍率", lowFarScale, 0.0f, 10.0f);
    fields.Float("gourdLowNearScale", "空に近い：接近の重みに掛ける倍率（追い詰めないので 1 以下）", lowNearScale, 0.0f,
                 1.0f);
    fields.Float(
        "gourdLowGuaranteeSeconds",
        "空に近い：吸いやすい剣気をこの秒数出していなければ、次の攻勢の技は吸いやすい剣気だけから (秒。0 で使わない)",
        lowGuaranteeSeconds, 0.0f, 120.0f);
    fields.Bool("gourdChainEnabled", "続けて剣気を吸われたら接近へ寄せる", chainEnabled);
    fields.Int("gourdChainCount", "続けて吸われた：本数", chainCount, 1, 8);
    fields.Float("gourdChainWindowSeconds", "続けて吸われた：この秒数のうちに", chainWindowSeconds, 0.0f, 60.0f);
    fields.Float("gourdChainSeconds", "続けて吸われた：接近へ寄せる長さ (秒)", chainSeconds, 0.0f, 60.0f);
    fields.Float("gourdChainFarScale", "続けて吸われた：遠距離の重みに掛ける倍率", chainFarScale, 0.0f, 10.0f);
    fields.Float("gourdChainNearScale", "続けて吸われた：接近の重みに掛ける倍率", chainNearScale, 0.0f, 10.0f);
    fields.Bool(
        "gourdChainNoHaze",
        "続けて吸われた：霞の剣気（吸える刃）を出す技を選ばない（技の表の当て方が「実」でなく、斬撃速度がある技。"
        "表の種類が接近でも外す）",
        chainNoHaze);
    fields.Bool("gourdHoldEnabled", "吸い込みを構え続けていたら、剣気の構えを一拍止める変化を混ぜる", holdEnabled);
    fields.Float("gourdHoldAbsorbSeconds", "一拍止め：吸い込みを構え続けている時間 (秒)", holdAbsorbSeconds, 0.0f,
                 5.0f);
    fields.Float("gourdHoldChance", "一拍止め：混ぜる確率 (%)", holdChance, 0.0f, 100.0f);
    fields.Float("gourdHoldCooldownSeconds", "一拍止め：混ぜた後、次に混ぜられるまで (秒)", holdCooldownSeconds, 0.0f,
                 60.0f);
    fields.Int("gourdHoldMinFrames", "一拍止め：止める長さ（最小、F。ローリングの無敵 15 F より長く）", holdMinFrames,
               0, 120);
    fields.Int("gourdHoldMaxFrames", "一拍止め：止める長さ（最大、F）", holdMaxFrames, 0, 120);
    fields.Bool("gourdLeanEnabled", "同じ側へ転がる癖があれば、横の剣気をその側へ寄せる", leanEnabled);
    fields.Int("gourdLeanRolls", "転がる癖：最近の何回を見るか", leanRolls, 1, 16);
    fields.Int("gourdLeanMinRolls", "転がる癖：少なくとも何回数えてから", leanMinRolls, 1, 16);
    fields.Float("gourdLeanAtLeastPercent", "転がる癖：左か右がこの割合以上 (%)", leanAtLeastPercent, 0.0f, 100.0f);
    fields.Float("gourdLeanMeters", "寄せる幅（プレイヤーの所で、m）", leanMeters, 0.0f, 10.0f);
    fields.Float("gourdLeanMaxDegrees", "寄せる角度の上限 (度)", leanMaxDegrees, 0.0f, 45.0f);
}

void BossGourdRead::Observe(BossComponent& boss) {
    BossGourdReadMemory& m = boss.gourdReadMemory;
    if (!IsLow(boss)) {
        m.lowSince = -1.0f;
        return;
    }
    if (m.lowSince < 0.0f)
        m.lowSince = boss.clockSeconds;
}

BossGourdReading BossGourdRead::Read(const BossComponent& boss) {
    const BossGourdReadSettings& s = boss.gourdRead;
    const BossGourdReadMemory& m = boss.gourdReadMemory;
    BossGourdReading reading;
    if (!s.enabled || !boss.watch.present || boss.watch.gourdPercent < 0.0f)
        return reading;
    const float now = boss.clockSeconds;
    reading.state = BossGourdState::Mid;
    if (s.fullEnabled && boss.watch.gourdFull)
        reading.state = BossGourdState::Full;
    else if (s.lowEnabled && IsLow(boss))
        reading.state = BossGourdState::Low;
    // 空に近い間は接近へ寄せない（追い詰めない）。
    reading.chain = s.chainEnabled && now < m.chainUntil && reading.state != BossGourdState::Low;
    if (reading.state == BossGourdState::Low && s.lowGuaranteeSeconds > 0.0f && m.lowSince >= 0.0f)
        reading.guarantee = now - std::max(m.lowSince, m.lastEasySeconds) >= s.lowGuaranteeSeconds;
    return reading;
}

float BossGourdRead::Scale(const BossComponent& boss, const BossGourdReading& reading, const BossStanceRow& row) {
    const BossGourdReadSettings& s = boss.gourdRead;
    const bool isFar = row.kind == BossMoveClass::Far;
    const bool isNear = row.kind == BossMoveClass::Near;
    float scale = 1.0f;
    switch (reading.state) {
    case BossGourdState::Full: scale *= isFar ? s.fullFarScale : isNear ? s.fullNearScale : 1.0f; break;
    case BossGourdState::Low:
        scale *= row.easy ? s.lowEasyScale : isFar ? s.lowFarScale : isNear ? std::min(1.0f, s.lowNearScale) : 1.0f;
        break;
    default: break;
    }
    if (reading.chain)
        scale *= isFar ? s.chainFarScale : isNear ? s.chainNearScale : 1.0f;
    return std::max(0.0f, scale);
}

bool BossGourdRead::ThrowsHaze(const BossMove& move) {
    // 斬撃を飛ばすのは刀を振る型だけ（BossMoveExecutor の Release）。当て方が「実」の技・斬撃速度が 0 の一撃は飛ばさない。
    // 一撃の値が無い技は、放つ時の斬撃速度が 0（BossMoveTable::Strike）。
    if (!BossMoveTable::IsSwingType(move.type) || move.delivery == BossDelivery::Blade)
        return false;
    return std::any_of(move.strikes.begin(), move.strikes.end(),
                       [](const BossStrikeData& strike) { return strike.speed > 0.0f; });
}

bool BossGourdRead::Excluded(const BossComponent& boss, const BossGourdReading& reading, const BossStanceRow& row) {
    if (!reading.chain || !boss.gourdRead.chainNoHaze || row.row < 0 ||
        static_cast<size_t>(row.row) >= boss.table.GetMoves().size())
        return false;
    return ThrowsHaze(boss.table.GetMoves()[static_cast<size_t>(row.row)]);
}

void BossGourdRead::NotePicked(BossComponent& boss, const BossStanceRow& row, bool attack) {
    const BossGourdReadSettings& s = boss.gourdRead;
    BossGourdReadMemory& m = boss.gourdReadMemory;
    const float now = boss.clockSeconds;
    m.pendingHold = 0;
    m.pendingRow = -1;
    if (!s.enabled || !attack)
        return;
    if (row.easy)
        m.lastEasySeconds = now;
    const BossPlayerWatch& watch = boss.watch;
    if (!s.holdEnabled || row.kind != BossMoveClass::Far || now < m.holdReadyAt || !watch.present || !watch.absorbing ||
        now - watch.absorbSince < s.holdAbsorbSeconds)
        return;
    const float roll = BossRandom(boss) * 100.0f;
    const bool pass = roll < s.holdChance;
    int frames = 0;
    if (pass) {
        const int low = std::max(0, s.holdMinFrames);
        const int high = std::max(low, s.holdMaxFrames);
        frames = std::min(high, low + static_cast<int>(BossRandom(boss) * static_cast<float>(high - low + 1)));
        m.pendingHold = frames;
        m.pendingRow = row.row;
        m.holdReadyAt = now + std::max(0.0f, s.holdCooldownSeconds);
    }
    const BossMove* move = static_cast<size_t>(row.row) < boss.table.GetMoves().size()
                               ? &boss.table.GetMoves()[static_cast<size_t>(row.row)]
                               : nullptr;
    char message[224]{};
    std::snprintf(message, sizeof(message),
                  "GourdHold tick=%llu move=%s held=%.2f roll=%.1f chance=%.1f pass=%d hold=%d",
                  static_cast<unsigned long long>(boss.tick), move != nullptr ? move->name.c_str() : "?",
                  static_cast<double>(now - watch.absorbSince), static_cast<double>(roll),
                  static_cast<double>(s.holdChance), static_cast<int>(pass), frames);
    BossLog::Write(message);
}

void BossGourdRead::NoteCatch(BossComponent& boss) {
    const BossGourdReadSettings& s = boss.gourdRead;
    BossGourdReadMemory& m = boss.gourdReadMemory;
    const float now = boss.clockSeconds;
    m.catches[m.catchNext] = now;
    m.catchNext = (m.catchNext + 1) % m.catches.size();
    m.catchCount = std::min(m.catches.size(), m.catchCount + 1);
    const int recent = RecentCatches(m, now, s.chainWindowSeconds);
    const bool start = s.enabled && s.chainEnabled && recent >= std::max(1, s.chainCount);
    if (start)
        m.chainUntil = now + std::max(0.0f, s.chainSeconds);
    char message[160]{};
    std::snprintf(message, sizeof(message), "GourdChain tick=%llu recent=%d start=%d until=%.2f",
                  static_cast<unsigned long long>(boss.tick), recent, static_cast<int>(start),
                  static_cast<double>(std::max(0.0f, m.chainUntil - now)));
    BossLog::Write(message);
}

int BossGourdRead::TakeHold(BossComponent& boss, int row) {
    BossGourdReadMemory& m = boss.gourdReadMemory;
    const int frames = m.pendingRow == row ? m.pendingHold : 0;
    m.pendingHold = 0;
    m.pendingRow = -1;
    return frames;
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

const char* BossGourdRead::StateName(BossGourdState state) {
    switch (state) {
    case BossGourdState::Mid: return "mid";
    case BossGourdState::Full: return "full";
    case BossGourdState::Low: return "low";
    default: return "off";
    }
}

} // namespace Project_TD2_2
