#include "BossBurnMarks.h"

#include <cstdio>
#include <optional>
#include <string>
#include <vector>

#include "BossCsv.h"
#include "BossMath.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using namespace BossCsv;

// 交差の 2 本は、ボス→狙った所の向きから左右へこの角度ずつ回す（直交する X）。
constexpr float kCrossDegrees = 45.0f;
// 全角の「＠」「：」。表は日本語の入力のまま書かれることが多いので、半角と同じに読む。
constexpr std::string_view kWideAt = "\xEF\xBC\xA0";
constexpr std::string_view kWideColon = "\xEF\xBC\x9A";
constexpr std::string_view kPhaseWord = "フェーズ";
constexpr const char* kSyntaxHint =
    "（線／跡／交差／池／輪。一撃だけは「交差:2」、フェーズ 2 以上だけは「輪@フェーズ2」）";

struct Shape {
    std::string_view text;
    BossBurnShape shape;
};
constexpr Shape kShapes[] = {{"線", BossBurnShape::Line},
                             {"跡", BossBurnShape::Trail},
                             {"交差", BossBurnShape::Cross},
                             {"池", BossBurnShape::Pool},
                             {"輪", BossBurnShape::Ring}};

// text の中で最初に出てくる narrow か wide の位置と、その長さ。
size_t FindMark(std::string_view text, char narrow, std::string_view wide, size_t& length) {
    const size_t a = text.find(narrow);
    const size_t b = text.find(wide);
    if (b != std::string_view::npos && (a == std::string_view::npos || b < a)) {
        length = wide.size();
        return b;
    }
    length = 1;
    return a;
}

// 1 以上の整数として読めるか（「2」「2.0」は 2。「0」「-1」「1.5」は読めない）。
std::optional<int> CountOf(std::string_view text) {
    const auto value = NumberOf(text);
    if (!value || *value < 1.0 || *value != static_cast<double>(RoundHalfUp(*value)))
        return std::nullopt;
    return RoundHalfUp(*value);
}

// 「交差:2@フェーズ2」の 1 項目。読めなければ空。
std::optional<BossBurnMark> ReadMark(std::string_view token) {
    std::string text = Trim(token);
    BossBurnMark mark;
    size_t length = 0;
    const size_t at = FindMark(text, '@', kWideAt, length);
    if (at != std::string::npos) {
        const std::string phase = Trim(std::string_view(text).substr(at + length));
        if (!StartsWith(phase, kPhaseWord))
            return std::nullopt;
        const auto number = CountOf(std::string_view(phase).substr(kPhaseWord.size()));
        if (!number)
            return std::nullopt;
        mark.minPhase = *number;
        text = Trim(std::string_view(text).substr(0, at));
    }
    const size_t colon = FindMark(text, ':', kWideColon, length);
    if (colon != std::string::npos) {
        const auto number = CountOf(std::string_view(text).substr(colon + length));
        if (!number)
            return std::nullopt;
        mark.strike = *number;
        text = Trim(std::string_view(text).substr(0, colon));
    }
    for (const auto& shape : kShapes) {
        if (text == shape.text)
            mark.shape = shape.shape;
    }
    if (mark.shape == BossBurnShape::None)
        return std::nullopt;
    return mark;
}

// その形を塗る動きがある型か。無い型に書いた火は塗られないまま剣だけ燃えるので、読む時に断る。
const char* ShapeProblem(BossBurnShape shape, BossMoveType type) {
    switch (shape) {
    case BossBurnShape::Line: return BossMoveTable::IsSwingType(type) ? nullptr : "線は斬撃を飛ばす型だけが残します";
    case BossBurnShape::Trail: return type == BossMoveType::Dash ? nullptr : "跡は瞬間突進の型だけが残します";
    case BossBurnShape::Ring: return type == BossMoveType::Kick ? nullptr : "輪は回し蹴り（霞の輪）の型だけが残します";
    default: return BossMoveTable::IsMoveType(type) ? "移動の型には一撃が無いので火を残せません" : nullptr;
    }
}

// 「火の幅 m」「火の秒数」：1 つの数（全部の火に同じ）か、残す火と同じ数を / で並べたもの。
// 数が合わなければ警告して false（どの火がどの値か分からないので、その行は火を残さない）。
bool ReadValues(std::string_view text, const char* label, const char* unit, size_t count, BossMove& move,
                std::vector<std::optional<double>>& values) {
    const auto parts = Split(text, false);
    if (parts.empty()) {
        move.warnings.push_back(std::string(label) + "が空なので火を残しません（0 より大きい" + unit + "）");
        return false;
    }
    if (parts.size() != 1 && parts.size() != count) {
        move.warnings.push_back(std::string(label) + "「" + std::string(text) + "」の数が残す火の数（" +
                                std::to_string(count) +
                                "）と合わないので火を残しません（1 つの数か、残す火と同じ数を / で並べる）");
        return false;
    }
    values.clear();
    for (size_t k = 0; k < count; ++k) {
        const auto value = NumberOf(parts[parts.size() == 1 ? 0 : k]);
        values.push_back(value && *value > 0.0 ? value : std::nullopt);
    }
    return true;
}

bool Applies(const BossBurnMark& mark, int strike, int phase) {
    return mark.shape != BossBurnShape::None && phase >= mark.minPhase &&
           (mark.strike == 0 || mark.strike == strike + 1);
}

Vec3 Floor(const BossComponent& boss, Vec3 point) { return {point.x, boss.ground.y, point.z}; }

void LogPaint(const BossComponent& boss, const BossMove& move, int strike, const BossBurnMark& mark,
              const char* geometry) {
    char message[384]{};
    std::snprintf(message, sizeof(message), "Paint %s tick=%llu move=%s strike=%d width=%.3f seconds=%.3f%s%s",
                  BossMoveTable::BurnKey(mark.shape), static_cast<unsigned long long>(boss.tick), move.name.c_str(),
                  strike + 1, static_cast<double>(mark.width), static_cast<double>(mark.seconds), *geometry ? " " : "",
                  geometry);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Burn", message);
}

void PaintPool(BossFrame& frame, const BossMove& move, int strike, const BossBurnMark& mark, Vec3 center) {
    const float radius = mark.width * 0.5f;
    BurnFieldRules::PaintDisc(frame.world, center, radius, mark.seconds);
    char geometry[128]{};
    std::snprintf(geometry, sizeof(geometry), "center=(%.3f,%.3f) radius=%.3f", static_cast<double>(center.x),
                  static_cast<double>(center.z), static_cast<double>(radius));
    LogPaint(frame.boss, move, strike, mark, geometry);
}

// 交差：center で、2 本の線分が中点で交わる X。toward はボスから狙った所への向き。
void PaintCross(BossFrame& frame, const BossMove& move, int strike, const BossBurnMark& mark, Vec3 center,
                Vec3 toward) {
    const BossComponent& boss = frame.boss;
    const float half = boss.burnCrossLength * 0.5f;
    for (const float sign : {1.0f, -1.0f}) {
        const Vec3 along = Mul(RotateY(toward, sign * kCrossDegrees * kRadiansPerDegree), half);
        BurnFieldRules::PaintSegment(frame.world, Sub(center, along), Add(center, along), mark.width, mark.seconds);
    }
    char geometry[160]{};
    std::snprintf(geometry, sizeof(geometry), "center=(%.3f,%.3f) length=%.3f yaw=%.4f", static_cast<double>(center.x),
                  static_cast<double>(center.z), static_cast<double>(boss.burnCrossLength),
                  static_cast<double>(YawOf(toward)));
    LogPaint(boss, move, strike, mark, geometry);
}

} // namespace

void BossBurnMarks::Read(std::string_view shapes, std::string_view widths, std::string_view seconds, BossMove& move) {
    move.burnMarkCount = 0;
    const std::string text = Trim(shapes);
    if (text.empty() || text == "なし")
        return;
    const auto tokens = Split(text, false);
    std::vector<std::optional<double>> widthValues, secondValues;
    if (!ReadValues(widths, "火の幅", "m", tokens.size(), move, widthValues) ||
        !ReadValues(seconds, "火の秒数", "秒", tokens.size(), move, secondValues))
        return;
    for (size_t k = 0; k < tokens.size(); ++k) {
        const std::string token = Trim(tokens[k]);
        auto mark = ReadMark(token);
        if (!mark) {
            move.warnings.push_back("残す火「" + token + "」が読めないので、その火を残しません" + kSyntaxHint);
            continue;
        }
        if (const char* problem = ShapeProblem(mark->shape, move.type)) {
            move.warnings.push_back("残す火「" + token + "」：" + problem + "（この行では残しません）");
            continue;
        }
        if (mark->strike > move.hitCount) {
            move.warnings.push_back("残す火「" + token + "」：この技の一撃は " + std::to_string(move.hitCount) +
                                    " つなので、その火を残しません");
            continue;
        }
        if (!widthValues[k]) {
            move.warnings.push_back("火の幅「" + std::string(widths) + "」の「" + token +
                                    "」の分が読めないので、その火を残しません（0 より大きい m）");
            continue;
        }
        if (!secondValues[k]) {
            move.warnings.push_back("火の秒数「" + std::string(seconds) + "」の「" + token +
                                    "」の分が読めないので、その火を残しません（0 より大きい秒）");
            continue;
        }
        if (move.burnMarkCount >= static_cast<int>(kBossBurnMarkCapacity)) {
            move.warnings.push_back("残す火は " + std::to_string(kBossBurnMarkCapacity) + " つまでなので「" + token +
                                    "」は残しません");
            continue;
        }
        mark->width = static_cast<float>(*widthValues[k]);
        mark->seconds = static_cast<float>(*secondValues[k]);
        move.burnMarks[static_cast<size_t>(move.burnMarkCount++)] = *mark;
    }
}

const BossBurnMark* BossBurnMarks::Find(const BossMove& move, BossBurnShape shape, int strike, int phase) {
    for (int k = 0; k < move.burnMarkCount; ++k) {
        const BossBurnMark& mark = move.burnMarks[static_cast<size_t>(k)];
        if (mark.shape == shape && Applies(mark, strike, phase))
            return &mark;
    }
    return nullptr;
}

bool BossBurnMarks::StrikeLeaves(const BossMove& move, int strike, int phase) {
    for (int k = 0; k < move.burnMarkCount; ++k) {
        if (Applies(move.burnMarks[static_cast<size_t>(k)], strike, phase))
            return true;
    }
    return false;
}

void BossBurnMarks::LightBlade(BossComponent& boss, const BossMove& move) {
    // 火を残すのが 1 撃だけの技も、技の間ずっと燃やす（どの技が火の技かを、プレイヤーが構えから読めるように）。
    boss.bladeLit = false;
    for (int k = 0; k < move.burnMarkCount; ++k)
        boss.bladeLit = boss.bladeLit || boss.lastPhaseNumber >= move.burnMarks[static_cast<size_t>(k)].minPhase;
    char message[192]{};
    std::snprintf(message, sizeof(message), "BladeLit tick=%llu move=%s lit=%d phase=%d",
                  static_cast<unsigned long long>(boss.tick), move.name.c_str(), static_cast<int>(boss.bladeLit),
                  boss.lastPhaseNumber);
    BossLog::Write(message);
}

void BossBurnMarks::ArmWave(const BossComponent& boss, const BossMove& move, BossWave& wave) {
    // ナイフの strike は扇の何本目か、輪は一撃しかないので、どちらも技の 1 撃目として見る。
    const int strike = wave.knife || wave.ring ? 0 : wave.strike;
    wave.fire = StrikeLeaves(move, strike, boss.lastPhaseNumber);
    if (wave.knife)
        return;
    const BossBurnMark* mark =
        Find(move, wave.ring ? BossBurnShape::Ring : BossBurnShape::Line, strike, boss.lastPhaseNumber);
    if (mark == nullptr)
        return;
    wave.burnWidth = mark->width;
    wave.burnSeconds = mark->seconds;
    wave.burnFrom = wave.position;
    // 線は飛んでいる間に少しずつ塗るので、放った時に 1 回だけ知らせる。輪は広がりきった時（RingEnded）に知らせる。
    if (!wave.ring)
        LogPaint(boss, move, strike, *mark, "");
}

void BossBurnMarks::Released(BossFrame& frame, const BossMove& move, int strike) {
    BossComponent& boss = frame.boss;
    if (const BossBurnMark* cross = Find(move, BossBurnShape::Cross, strike, boss.lastPhaseNumber)) {
        // 狙った所（プレイヤーの足元。いなければボスの前、線の長さの半分）。
        const Vec3 forward = Forward(boss.yaw);
        const Vec3 center = frame.player != nullptr ? Floor(boss, frame.playerCenter)
                                                    : Add(boss.ground, Mul(forward, boss.burnCrossLength * 0.5f));
        const Vec3 toward = FlatNormalize(Sub(center, boss.ground), forward);
        // この一撃で放った斬撃があれば、それが狙った所へ届いて爆ぜるまで待つ（地面が先に燃えない）。
        BossWave* carrier = nullptr;
        for (auto& wave : boss.waves) {
            if (wave.active && !wave.knife && !wave.ring && wave.move == boss.move && wave.strike == strike &&
                (carrier == nullptr || wave.id > carrier->id))
                carrier = &wave;
        }
        if (carrier != nullptr) {
            carrier->crossPending = true;
            carrier->crossCenter = center;
            carrier->crossToward = toward;
            carrier->crossWidth = cross->width;
            carrier->crossSeconds = cross->seconds;
            carrier->crossReach = FlatLength(Sub(center, carrier->position));
        } else {
            PaintCross(frame, move, strike, *cross, center, toward);
        }
    }
    const BossBurnMark* pool = Find(move, BossBurnShape::Pool, strike, boss.lastPhaseNumber);
    if (pool == nullptr || move.type == BossMoveType::Ambush)
        return;
    // 上段溜めは、振り下ろした剣が地面を打つ所（ボスの前）。ほかの型はボスの足元。
    const Vec3 center = move.type == BossMoveType::Overhead
                            ? Add(boss.ground, Mul(Forward(boss.yaw), boss.burnPoolForward))
                            : boss.ground;
    PaintPool(frame, move, strike, *pool, center);
}

void BossBurnMarks::Appeared(BossFrame& frame, const BossMove& move, int strike) {
    if (const BossBurnMark* pool = Find(move, BossBurnShape::Pool, strike, frame.boss.lastPhaseNumber))
        PaintPool(frame, move, strike, *pool, frame.boss.ground);
}

void BossBurnMarks::DashLanded(BossFrame& frame, const BossMove& move) {
    const BossComponent& boss = frame.boss;
    // 突進は 1 撃目の前の動きなので、1 撃目の火として見る。
    const BossBurnMark* mark = Find(move, BossBurnShape::Trail, 0, boss.lastPhaseNumber);
    if (mark == nullptr)
        return;
    const Vec3 from = Floor(boss, boss.dashFrom), to = Floor(boss, boss.dashTo);
    BurnFieldRules::PaintSegment(frame.world, from, to, mark->width, mark->seconds);
    char geometry[192]{};
    std::snprintf(geometry, sizeof(geometry), "from=(%.3f,%.3f) to=(%.3f,%.3f) length=%.3f",
                  static_cast<double>(from.x), static_cast<double>(from.z), static_cast<double>(to.x),
                  static_cast<double>(to.z), static_cast<double>(FlatLength(Sub(to, from))));
    LogPaint(boss, move, 0, *mark, geometry);
}

void BossBurnMarks::WaveLanded(BossFrame& frame, const BossMove& move, BossWave& wave) {
    if (!wave.crossPending)
        return;
    wave.crossPending = false;
    BossComponent& boss = frame.boss;
    // 狙った所まで届かずに消えた斬撃（遠すぎた）は、消えた所で爆ぜる。
    const Vec3 center = wave.travelled >= wave.crossReach ? wave.crossCenter : Floor(boss, wave.position);
    // 外れても地面で爆ぜる（当たった時は HitPlayer が同じ爆発を出しているので足さない）。
    if (!wave.struck) {
        BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
        blast = {};
        blast.pushPending = blast.effectPending = true;
        blast.fire = true;
        blast.scale = 1.0f;
        blast.position = {center.x, boss.ground.y + boss.bodyHalfHeight, center.z};
    }
    BossBurnMark mark;
    mark.shape = BossBurnShape::Cross;
    mark.width = wave.crossWidth;
    mark.seconds = wave.crossSeconds;
    PaintCross(frame, move, wave.strike, mark, center, wave.crossToward);
}

void BossBurnMarks::RingEnded(BossFrame& frame, const BossMove& move, const BossWave& wave) {
    if (!(wave.burnWidth > 0.0f))
        return;
    BurnFieldRules::PaintRing(frame.world, wave.position, wave.maxRadius, wave.burnWidth, wave.burnSeconds);
    BossBurnMark mark;
    mark.shape = BossBurnShape::Ring;
    mark.width = wave.burnWidth;
    mark.seconds = wave.burnSeconds;
    char geometry[128]{};
    std::snprintf(geometry, sizeof(geometry), "center=(%.3f,%.3f) radius=%.3f", static_cast<double>(wave.position.x),
                  static_cast<double>(wave.position.z), static_cast<double>(wave.maxRadius));
    LogPaint(frame.boss, move, 0, mark, geometry);
}

} // namespace Project_TD2_2
