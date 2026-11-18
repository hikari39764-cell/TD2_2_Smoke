#include "BossStance.h"

#include <algorithm>
#include <cstdio>

#include "BossComponent.h"
#include "BossCsv.h"
#include "BossFrame.h"
#include "BossGourdRead.h"
#include "BossMoveTable.h"
#include "BossSelection.h"

namespace Project_TD2_2 {
namespace {

using namespace BossCsv;

constexpr const char* kStanceNames[] = {"攻勢", "散歩", "離脱"};
constexpr const char* kBandNames[] = {"密着", "中", "遠", "圏外"};
constexpr const char* kClassNames[] = {"遠距離", "接近", "受け返し"};

// 歩いて詰める間、1 回の歩きでこれだけ詰まらなければ止められたとみなす (m)。
constexpr float kApproachProgressMeters = 0.3f;

template <typename E, size_t N>
bool ReadName(const std::string& text, const char* const (&names)[N], E& out) {
    for (size_t i = 0; i < N; ++i) {
        if (text == names[i]) {
            out = static_cast<E>(i);
            return true;
        }
    }
    return false;
}

float Between(BossComponent& boss, float low, float high) {
    return low + (std::max(low, high) - low) * BossRandom(boss);
}

int BetweenInt(BossComponent& boss, int low, int high) {
    const int top = std::max(low, high);
    return std::min(top, low + static_cast<int>(BossRandom(boss) * static_cast<float>(top - low + 1)));
}

// 散歩・見る・長い間の長さ（秒）。フェーズ 2 ではフェーズ 2 の項目（c-dc91cc）。
bool PhaseTwo(const BossComponent& boss) { return boss.lastPhaseNumber >= 2; }

float LongPauseSeconds(BossComponent& boss) {
    const BossStanceSettings& s = boss.stance;
    return PhaseTwo(boss) ? Between(boss, s.longPauseMinSecondsPhase2, s.longPauseMaxSecondsPhase2)
                          : Between(boss, s.longPauseMinSeconds, s.longPauseMaxSeconds);
}

float StrollSeconds(BossComponent& boss) {
    const BossStanceSettings& s = boss.stance;
    return PhaseTwo(boss) ? Between(boss, s.strollMinSecondsPhase2, s.strollMaxSecondsPhase2)
                          : Between(boss, s.strollMinSeconds, s.strollMaxSeconds);
}

float ObserveSeconds(BossComponent& boss) {
    const BossStanceSettings& s = boss.stance;
    return PhaseTwo(boss) ? Between(boss, s.observeMinSecondsPhase2, s.observeMaxSecondsPhase2)
                          : Between(boss, s.observeMinSeconds, s.observeMaxSeconds);
}

// 技の型で、最初の行（散歩・見るの横歩き、離脱の後ろ跳び、圏外から歩いて入る）。
int FirstOfType(const BossComponent& boss, BossMoveType type) {
    const auto& moves = boss.table.GetMoves();
    for (size_t i = 0; i < moves.size(); ++i) {
        if (moves[i].type == type && moves[i].executable)
            return static_cast<int>(i);
    }
    return -1;
}

void Log(const BossComponent& boss, const char* what, const char* extra) {
    char message[480]{};
    std::snprintf(message, sizeof(message), "Stance %s tick=%llu stance=%s %s", what,
                  static_cast<unsigned long long>(boss.tick), kStanceNames[static_cast<int>(boss.stanceMemory.stance)],
                  extra);
    BossLog::Write(message);
}

void Enter(BossComponent& boss, BossStanceKind stance, const char* reason) {
    const BossStanceSettings& s = boss.stance;
    BossStanceMemory& m = boss.stanceMemory;
    const BossStanceKind from = m.stance;
    m.stance = stance;
    m.since = boss.clockSeconds;
    m.observing = false;
    m.withdrawSteps = 0;
    m.approaching = false;
    m.playerIn = 0.0f;
    m.playerInMin = 0.0f;
    m.playerInMax = 0.0f;
    m.closeSince = -1.0f;
    m.shotsAtStart = boss.watch.shotsSeen;
    if (stance == BossStanceKind::Aggro) {
        m.combos = 0;
        m.combosTarget = BetweenInt(boss, std::max(1, s.combosMin), std::max(1, s.combosMax));
        ++m.aggroCount;
        m.playerHitsAtStart = boss.playerHitCount;
    } else {
        // 攻勢を何回か終えるごとに 1 度、散歩・見るを長くする（戦いの速さをボスが決める）。
        m.longPause = m.aggroCount >= m.nextLongPause;
        if (m.longPause)
            m.nextLongPause = m.aggroCount + BetweenInt(boss, std::max(1, s.longPauseEveryMin), s.longPauseEveryMax);
        m.slowStart = true;
        if (stance == BossStanceKind::Stroll)
            m.until = boss.clockSeconds + (m.longPause ? LongPauseSeconds(boss) : StrollSeconds(boss));
    }
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "from=%s reason=%s combos=%d long=%d until=%.2f",
                  kStanceNames[static_cast<int>(from)], reason, m.combosTarget, static_cast<int>(m.longPause),
                  static_cast<double>(m.until - boss.clockSeconds));
    Log(boss, "Enter", extra);
}

BossStanceChoice Walk(const BossComponent& boss, BossMoveType type) { return {FirstOfType(boss, type), false}; }

// 攻勢の技選び：姿勢・帯・フェーズの行から、冷却中・出せない技を除き、種類の交互・最近の技で重みを動かして 1 つ。
BossStanceChoice PickAttack(BossComponent& boss, int phase, float distance, BossBand band, float random,
                            bool noWalk = false) {
    const BossStanceSettings& s = boss.stance;
    BossStanceMemory& m = boss.stanceMemory;
    const auto& rows = boss.stanceTable.GetRows();
    BossSituation situation;
    situation.phase = phase;
    situation.distance = distance;
    // フェーズの行：いまのフェーズの行が表にあればその行だけ（この帯に無ければ歩く。フェーズ 1 の行へは落ちない）、
    // 表に無いフェーズはフェーズ 1 の行を使う（c-dc91cc）。
    const int usePhase = boss.stanceTable.RowsPhase(phase);
    // ヒョウタンの読み（c-14392d）：満杯・空に近い・続けて吸われた で、遠距離・接近・吸いやすい剣気の重みを動かす。
    const BossGourdReading reading = BossGourdRead::Read(boss);
    struct Candidate {
        const BossStanceRow* row;
        float weight;
    };
    std::vector<Candidate> candidates;
    float total = 0.0f;
    for (int pass = 0; pass < 2 && candidates.empty(); ++pass) {
        // 1 回目は遅い始まり・突進の続けを守る。候補が無ければ 2 回目はそれを外す。
        const bool strict = pass == 0;
        for (const auto& row : rows) {
            if (row.stance != BossStanceKind::Aggro || row.band != band || row.phase != usePhase || row.row < 0 ||
                !(row.weight > 0.0f) || !BossSelection::CanPerform(boss, row.row, situation))
                continue;
            if (static_cast<size_t>(row.row) < m.readyAt.size() && boss.clockSeconds < m.readyAt[row.row])
                continue;
            const BossMove& move = boss.table.GetMoves()[static_cast<size_t>(row.row)];
            const bool walking = BossMoveTable::IsMoveType(move.type);
            if (noWalk && walking)
                continue;
            if (strict && m.combos == 0 && m.slowStart && !walking && move.pre < s.slowStartFrames)
                continue;
            if (strict && m.combos == 0 && s.noDashTwice && m.lastOpenerDash && move.type == BossMoveType::Dash)
                continue;
            float weight = row.weight;
            if (!walking && m.lastClass != BossMoveClass::Count)
                weight *= row.kind == m.lastClass ? s.sameClassScale : s.classSwitchScale;
            const float recent[3] = {s.recentScale0, s.recentScale1, s.recentScale2};
            for (size_t k = 0; k < m.recent.size(); ++k) {
                if (m.recent[k] == row.row) {
                    weight *= recent[k];
                    break;
                }
            }
            if (!walking)
                weight *= BossGourdRead::Scale(boss, reading, row);
            candidates.push_back({&row, weight});
            total += weight;
        }
    }
    // 空に近いまま長く吸いやすい剣気を出していなければ、出せる吸いやすい剣気だけから選ぶ（補給できずに詰ませない）。
    bool guaranteed = false;
    if (reading.guarantee &&
        std::any_of(candidates.begin(), candidates.end(), [](const Candidate& c) { return c.row->easy; })) {
        candidates.erase(
            std::remove_if(candidates.begin(), candidates.end(), [](const Candidate& c) { return !c.row->easy; }),
            candidates.end());
        total = 0.0f;
        for (const auto& c : candidates)
            total += c.weight;
        guaranteed = true;
    }
    if (candidates.empty() || !(total > 0.0f))
        return band == BossBand::Out ? Walk(boss, BossMoveType::WalkIn) : Walk(boss, BossMoveType::WalkSide);
    float left = std::clamp(random, 0.0f, 0.9999f) * total;
    const Candidate* chosen = &candidates.back();
    for (const auto& c : candidates) {
        left -= c.weight;
        if (left < 0.0f) {
            chosen = &c;
            break;
        }
    }
    const BossMove& move = boss.table.GetMoves()[static_cast<size_t>(chosen->row->row)];
    BossStanceChoice choice{chosen->row->row, !BossMoveTable::IsMoveType(move.type), chosen->row->kind,
                            chosen->weight / total};
    BossGourdRead::NotePicked(boss, *chosen->row, choice.attack);
    char extra[320]{};
    std::snprintf(extra, sizeof(extra),
                  "band=%s dist=%.2f move=%s kind=%s share=%.2f candidates=%zu combo=%d/%d rows=%d read=%s "
                  "chain=%d easy=%d guarantee=%d gourd=%.1f",
                  kBandNames[static_cast<int>(band)], static_cast<double>(distance), move.name.c_str(),
                  kClassNames[static_cast<int>(chosen->row->kind)], static_cast<double>(choice.weight),
                  candidates.size(), m.combos + 1, m.combosTarget, usePhase, BossGourdRead::StateName(reading.state),
                  static_cast<int>(reading.chain), static_cast<int>(chosen->row->easy), static_cast<int>(guaranteed),
                  static_cast<double>(boss.watch.gourdPercent));
    Log(boss, "Pick", extra);
    return choice;
}

// 冷却（秒）：いまのフェーズで使う行のうち、その技の行の一番長い冷却。
float CooldownOf(const BossComponent& boss, int row) {
    const int usePhase = boss.stanceTable.RowsPhase(boss.lastPhaseNumber);
    float cooldown = 0.0f;
    for (const auto& r : boss.stanceTable.GetRows()) {
        if (r.row == row && r.phase == usePhase)
            cooldown = std::max(cooldown, r.cooldown);
    }
    return cooldown;
}

} // namespace

bool BossStanceTable::Load(std::string_view csv, std::vector<std::string>& warnings) {
    rows_.clear();
    const auto table = ParseCsv(csv);
    // 「吸いやすい」の列（c-14392d）は見出しで探す（無い表は印なし）。
    size_t easyColumn = static_cast<size_t>(-1);
    if (!table.empty()) {
        for (size_t c = 0; c < table[0].size(); ++c) {
            if (Trim(table[0][c]) == "吸いやすい")
                easyColumn = c;
        }
    }
    for (size_t r = 1; r < table.size(); ++r) {
        const Row& cells = table[r];
        auto cell = [&](size_t index) { return index < cells.size() ? Trim(cells[index]) : std::string(); };
        if (cell(3).empty())
            continue;
        BossStanceRow row;
        row.phase = static_cast<int>(NumberOf(cell(0)).value_or(1.0));
        const std::string where = "姿勢の表 " + std::to_string(r + 1) + " 行目";
        if (!ReadName(cell(1), kStanceNames, row.stance)) {
            warnings.push_back(where + "：姿勢「" + cell(1) + "」が読めません（攻勢・散歩・離脱）");
            continue;
        }
        if (!ReadName(cell(2), kBandNames, row.band)) {
            warnings.push_back(where + "：帯「" + cell(2) + "」が読めません（密着・中・遠・圏外）");
            continue;
        }
        row.move = cell(3);
        if (!ReadName(cell(4), kClassNames, row.kind))
            warnings.push_back(where + "：種類「" + cell(4) +
                               "」が読めないので接近にしました（遠距離・接近・受け返し）");
        row.weight = static_cast<float>(NumberOf(cell(5)).value_or(1.0));
        row.cooldown = static_cast<float>(NumberOf(cell(6)).value_or(0.0));
        // 空欄・0・「-」以外を印とみなす（○ や 1）。
        const std::string easy = easyColumn < cells.size() ? cell(easyColumn) : std::string();
        row.easy = !easy.empty() && easy != "0" && easy != "-";
        rows_.push_back(row);
    }
    return !rows_.empty();
}

int BossStanceTable::RowsPhase(int phase) const {
    for (const auto& row : rows_) {
        if (row.phase == phase)
            return phase;
    }
    return 1;
}

void BossStanceTable::Resolve(const BossMoveTable& moves, std::vector<std::string>& warnings) {
    for (auto& row : rows_) {
        row.row = moves.Find(row.move);
        if (row.row < 0)
            warnings.push_back("姿勢の表：技「" + row.move + "」が技の表にありません");
    }
}

void BossStanceSettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("姿勢と間合い（攻勢・散歩・離脱）", false))
        return;
    fields.Note(
        "行動ツリーの節「姿勢で技を選ぶ」が使う値。技ごとの重み・種類・冷却は Data/Boss/kasumi_bushi_stance.csv。");
    fields.Float("stanceCloseMeters", "帯の境：密着／中 (m)", closeMeters, 0.0f, 40.0f);
    fields.Float("stanceMidMeters", "帯の境：中／遠 (m)", midMeters, 0.0f, 40.0f);
    fields.Float("stanceFarMeters", "帯の境：遠／圏外 (m。プレイヤーの弾が消える距離)", farMeters, 0.0f, 60.0f);
    fields.Int("stanceCombosMin", "攻勢で出す連段の組の数（最小）", combosMin, 1, 6);
    fields.Int("stanceCombosMax", "攻勢で出す連段の組の数（最大）", combosMax, 1, 6);
    fields.Float("stanceWithdrawChance", "攻勢の後に離脱へ移る確率 (%。残りは散歩)", withdrawChance, 0.0f, 100.0f);
    fields.Float("stanceWithdrawAfterHitChance", "プレイヤーに当てた後に離脱へ移る確率 (%)", withdrawAfterHitChance,
                 0.0f, 100.0f);
    fields.Float("stanceStrollMinSeconds", "散歩の長さ（最小、秒）", strollMinSeconds, 0.0f, 30.0f);
    fields.Float("stanceStrollMaxSeconds", "散歩の長さ（最大、秒）", strollMaxSeconds, 0.0f, 30.0f);
    fields.Float("stanceObserveMinSeconds", "離脱の後に圏外で見る長さ（最小、秒）", observeMinSeconds, 0.0f, 30.0f);
    fields.Float("stanceObserveMaxSeconds", "離脱の後に圏外で見る長さ（最大、秒）", observeMaxSeconds, 0.0f, 30.0f);
    fields.Int("stanceLongPauseEveryMin", "長い間を置く間隔（攻勢の回数、最小）", longPauseEveryMin, 1, 20);
    fields.Int("stanceLongPauseEveryMax", "長い間を置く間隔（攻勢の回数、最大）", longPauseEveryMax, 1, 20);
    fields.Float("stanceLongPauseMinSeconds", "長い間の長さ（最小、秒）", longPauseMinSeconds, 0.0f, 30.0f);
    fields.Float("stanceLongPauseMaxSeconds", "長い間の長さ（最大、秒）", longPauseMaxSeconds, 0.0f, 30.0f);
    fields.Int("stanceWithdrawStepsMax", "離脱で後ろへ跳ぶ回数の上限", withdrawStepsMax, 0, 6);
    fields.Float("stanceClassSwitchScale", "直前と違う種類（遠距離／接近）の重みに掛ける倍率", classSwitchScale, 0.0f,
                 10.0f);
    fields.Float("stanceSameClassScale", "直前と同じ種類の重みに掛ける倍率", sameClassScale, 0.0f, 10.0f);
    fields.Float("stanceRecentScale0", "最近出した技の重みに掛ける倍率（直前）", recentScale0, 0.0f, 1.0f);
    fields.Float("stanceRecentScale1", "最近出した技の重みに掛ける倍率（2 つ前）", recentScale1, 0.0f, 1.0f);
    fields.Float("stanceRecentScale2", "最近出した技の重みに掛ける倍率（3 つ前）", recentScale2, 0.0f, 1.0f);
    fields.Int("stanceSlowStartFrames", "散歩・見るの後、攻勢の始めの技の前隙の下限 (F)", slowStartFrames, 0, 240);
    fields.Bool("stanceNoDashTwice", "圏外から入る時、突進を続けて出さない", noDashTwice);
    fields.Note(
        "散歩・見るの切り上げ：プレイヤーが撃つ・当てる・寄る・張り付く・離れる・見ている所へ入ると、歩きの途中でも"
        "すぐ攻勢へ。");
    fields.Float("stanceEngageAfterSeconds", "散歩に入ってから切り上げられるまで (秒)", engageAfterSeconds, 0.0f,
                 10.0f);
    fields.Float("stanceApproachMeters", "寄って来た：散歩の間の一番遠い距離からこれだけ近づく (m)", approachMeters,
                 0.0f, 20.0f);
    fields.Float("stanceCloseSeconds", "張り付いている：密着の帯にこれだけいる (秒)", closeSeconds, 0.0f, 10.0f);
    fields.Float("stanceChaseMeters", "離れた：散歩の間の一番近い距離からこれだけ離れる (m。遠の帯より遠い時)",
                 chaseMeters, 0.0f, 30.0f);
    fields.Bool("stanceObserveEngage", "見る（離脱の後）の間、プレイヤーが中の帯へ入ったら攻勢へ", observeEngage);
    fields.Float("stanceKeepMeters", "散歩で保つ間合い (m)", keepMeters, 0.0f, 30.0f);
    fields.Float("stanceKeepGain", "散歩で間合いのずれ 1 m あたりに前後へ寄る速さ（歩く速さの割合）", keepGain, 0.0f,
                 5.0f);
    fields.Float("stanceKeepSpeedScale", "散歩で前後へ寄る速さの上限（歩く速さの割合）", keepSpeedScale, 0.0f, 3.0f);
    fields.Float("stanceMirrorSpeed",
                 "散歩で回り合う：プレイヤーの横の速さがこれを超えたら逆の向きへ回る (m/秒。0 で回らない)", mirrorSpeed,
                 0.0f, 20.0f);
    fields.Note(
        "フェーズ 2 の間の長さ：フェーズ 2 では散歩・見る・長い間をこちらの長さにする（技の重みは表のフェーズ 2 の行）。");
    fields.Float("stanceStrollMinSecondsPhase2", "フェーズ 2：散歩の長さ（最小、秒）", strollMinSecondsPhase2, 0.0f,
                 30.0f);
    fields.Float("stanceStrollMaxSecondsPhase2", "フェーズ 2：散歩の長さ（最大、秒）", strollMaxSecondsPhase2, 0.0f,
                 30.0f);
    fields.Float("stanceObserveMinSecondsPhase2", "フェーズ 2：離脱の後に圏外で見る長さ（最小、秒）",
                 observeMinSecondsPhase2, 0.0f, 30.0f);
    fields.Float("stanceObserveMaxSecondsPhase2", "フェーズ 2：離脱の後に圏外で見る長さ（最大、秒）",
                 observeMaxSecondsPhase2, 0.0f, 30.0f);
    fields.Float("stanceLongPauseMinSecondsPhase2", "フェーズ 2：長い間の長さ（最小、秒）", longPauseMinSecondsPhase2,
                 0.0f, 30.0f);
    fields.Float("stanceLongPauseMaxSecondsPhase2", "フェーズ 2：長い間の長さ（最大、秒）", longPauseMaxSecondsPhase2,
                 0.0f, 30.0f);
}

BossBand BossStance::BandOf(const BossStanceSettings& s, float distance) {
    if (distance < s.closeMeters)
        return BossBand::Close;
    if (distance < s.midMeters)
        return BossBand::Mid;
    if (distance < s.farMeters)
        return BossBand::Far;
    return BossBand::Out;
}

const char* BossStance::BandName(BossBand band) { return kBandNames[std::min(3, static_cast<int>(band))]; }
const char* BossStance::StanceName(BossStanceKind stance) {
    return kStanceNames[std::min(2, static_cast<int>(stance))];
}
const char* BossStance::ClassName(BossMoveClass kind) { return kClassNames[std::min(2, static_cast<int>(kind))]; }

BossStanceChoice BossStance::Decide(BossComponent& boss, int phase, float distance, float random0, float) {
    const BossStanceSettings& s = boss.stance;
    BossStanceMemory& m = boss.stanceMemory;
    m.readyAt.resize(boss.table.GetMoves().size(), 0.0f);
    const BossBand band = BandOf(s, distance);
    if (!m.started) {
        // 決まった開幕は作らない：始めの間合いで、圏外なら見る、射程の中なら短く散歩してから攻勢へ。
        m.started = true;
        m.nextLongPause = BetweenInt(boss, std::max(1, s.longPauseEveryMin), s.longPauseEveryMax);
        Enter(boss, band == BossBand::Out ? BossStanceKind::Withdraw : BossStanceKind::Stroll, "start");
    }
    if (m.stance == BossStanceKind::Withdraw) {
        if (!m.observing) {
            if (band != BossBand::Out && m.withdrawSteps < s.withdrawStepsMax) {
                ++m.withdrawSteps;
                return Walk(boss, BossMoveType::Backstep);
            }
            // 圏外へ出られた、または跳び切って遠の帯にいる：そこで見る（射程の端。プレイヤーの弾は弱まる）。
            if (band == BossBand::Out || band == BossBand::Far) {
                m.observing = true;
                m.until = boss.clockSeconds + (m.longPause ? LongPauseSeconds(boss) : ObserveSeconds(boss));
                char extra[96]{};
                std::snprintf(extra, sizeof(extra), "dist=%.2f for=%.2f", static_cast<double>(distance),
                              static_cast<double>(m.until - boss.clockSeconds));
                Log(boss, "Observe", extra);
            } else {
                // 跳んでも圏外へ出られない（闘技場の縁・プレイヤーが付いて来る）：射程の中で散歩へ。
                Enter(boss, BossStanceKind::Stroll, "cornered");
            }
        }
        if (m.stance == BossStanceKind::Withdraw) {
            if (boss.clockSeconds < m.until)
                return Walk(boss, BossMoveType::WalkSide);
            Enter(boss, BossStanceKind::Aggro, "observed");
        }
    }
    if (m.stance == BossStanceKind::Stroll) {
        if (boss.clockSeconds < m.until)
            return Walk(boss, BossMoveType::WalkSide);
        Enter(boss, BossStanceKind::Aggro, "strolled");
    }
    // 歩いて詰めている間は、中の帯へ入るまで選び直さずに歩き続ける（毎回選び直すと、遠くで技を撃ってばかりになる）。
    bool stalled = false;
    if (m.approaching) {
        // 前に頼んだ時より 0.3 m 以上詰まっていなければ（闘技場の縁で止められている）、歩くのをやめて今の帯の技を選ぶ。
        stalled = distance > m.approachLast - kApproachProgressMeters;
        m.approachLast = distance;
        if ((band == BossBand::Out || band == BossBand::Far) && !stalled)
            return Walk(boss, BossMoveType::WalkIn);
        m.approaching = false;
        if (stalled)
            Log(boss, "ApproachStalled", "");
    }
    BossStanceChoice choice = PickAttack(boss, phase, distance, band, random0, stalled || m.noWalkOnce);
    m.noWalkOnce = false;
    const auto& moves = boss.table.GetMoves();
    if (choice.row >= 0 && static_cast<size_t>(choice.row) < moves.size() &&
        moves[static_cast<size_t>(choice.row)].type == BossMoveType::WalkIn) {
        m.approaching = true;
        m.approachLast = distance;
        m.approachSince = boss.clockSeconds;
        m.approachShots = boss.watch.shotsSeen;
    }
    return choice;
}

bool BossStance::Engage(BossComponent& boss, float distance) {
    const BossStanceSettings& s = boss.stance;
    BossStanceMemory& m = boss.stanceMemory;
    if (!m.started)
        return false;
    const BossBand band = BandOf(s, distance);
    const char* reason = nullptr;
    // 散歩・見るに入ってから、プレイヤーが撃ったか・ボスが当てられたか。
    const bool shot = boss.watch.shotsSeen > m.shotsAtStart;
    bool hurt = false;
    for (size_t i = 0; i < boss.brain.hitCount; ++i)
        hurt = hurt || boss.brain.hitSeconds[i] > m.since;
    if (m.stance == BossStanceKind::Aggro && m.approaching) {
        bool struck = false;
        for (size_t i = 0; i < boss.brain.hitCount; ++i)
            struck = struck || boss.brain.hitSeconds[i] > m.approachSince;
        if (!struck && boss.watch.shotsSeen <= m.approachShots)
            return false;
        m.approaching = false;
        m.noWalkOnce = true;
        char extra[64]{};
        std::snprintf(extra, sizeof(extra), "why=%s dist=%.2f", struck ? "hurt" : "shot",
                      static_cast<double>(distance));
        Log(boss, "ApproachPunish", extra);
        return true;
    }
    if (m.stance == BossStanceKind::Stroll) {
        if (band == BossBand::Close) {
            if (m.closeSince < 0.0f)
                m.closeSince = boss.clockSeconds;
        } else {
            m.closeSince = -1.0f;
        }
        if (hurt || shot)
            reason = hurt ? "hurt" : "shot";
        else if (boss.clockSeconds - m.since < s.engageAfterSeconds)
            return false;
        else if (distance < s.midMeters && m.playerIn - m.playerInMin >= s.approachMeters)
            reason = "approach";
        else if (m.closeSince >= 0.0f && boss.clockSeconds - m.closeSince >= s.closeSeconds)
            reason = "close";
        else if (distance >= s.midMeters && m.playerInMax - m.playerIn >= s.chaseMeters)
            reason = "chase";
    } else if (m.stance == BossStanceKind::Withdraw && m.observing && s.observeEngage) {
        if (distance < s.midMeters)
            reason = "intrude";
        else if (hurt || shot)
            reason = hurt ? "hurt" : "shot";
    }
    if (reason == nullptr)
        return false;
    char extra[128]{};
    std::snprintf(extra, sizeof(extra), "why=%s dist=%.2f in=%.2f min=%.2f max=%.2f", reason,
                  static_cast<double>(distance), static_cast<double>(m.playerIn), static_cast<double>(m.playerInMin),
                  static_cast<double>(m.playerInMax));
    Log(boss, "Engage", extra);
    Enter(boss, BossStanceKind::Aggro, reason);
    // プレイヤーの動きへの返しなので、遅い始まりに限らない。
    m.slowStart = false;
    return true;
}

void BossStance::Force(BossComponent& boss, BossStanceKind stance, const char* reason) {
    if (!boss.stanceMemory.started || boss.stanceMemory.stance == stance)
        return;
    Enter(boss, stance, reason);
}

bool BossStance::Read(std::string_view text, BossStanceKind& out) {
    constexpr const char* kKeys[] = {"aggro", "stroll", "withdraw"};
    for (int i = 0; i < 3; ++i) {
        if (text == kKeys[i] || text == kStanceNames[i]) {
            out = static_cast<BossStanceKind>(i);
            return true;
        }
    }
    return false;
}

void BossStance::Finished(BossComponent& boss, const BossStanceChoice& choice, float random) {
    const BossStanceSettings& s = boss.stance;
    BossStanceMemory& m = boss.stanceMemory;
    if (!choice.attack || choice.row < 0 || m.stance != BossStanceKind::Aggro)
        return;
    const BossMove* move = boss.table.GetMoves().size() > static_cast<size_t>(choice.row)
                               ? &boss.table.GetMoves()[static_cast<size_t>(choice.row)]
                               : nullptr;
    if (m.combos == 0)
        m.lastOpenerDash = move != nullptr && move->type == BossMoveType::Dash;
    ++m.combos;
    m.slowStart = false;
    m.lastClass = choice.kind;
    m.recent = {choice.row, m.recent[0], m.recent[1]};
    if (static_cast<size_t>(choice.row) < m.readyAt.size())
        m.readyAt[static_cast<size_t>(choice.row)] = boss.clockSeconds + CooldownOf(boss, choice.row);
    if (m.combos < m.combosTarget)
        return;
    const bool hit = boss.playerHitCount > m.playerHitsAtStart;
    const float chance = hit ? s.withdrawAfterHitChance : s.withdrawChance;
    Enter(boss, random * 100.0f < chance ? BossStanceKind::Withdraw : BossStanceKind::Stroll, hit ? "hit" : "done");
}

} // namespace Project_TD2_2
