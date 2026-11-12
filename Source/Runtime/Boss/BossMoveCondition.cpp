#include "BossMoveCondition.h"

#include <cstdlib>
#include <optional>
#include <string>

namespace Project_TD2_2 {
namespace {

bool StartsWith(std::string_view text, std::string_view part) { return text.substr(0, part.size()) == part; }
bool IsDigit(char ch) { return ch >= '0' && ch <= '9'; }

// 半角の空白・タブと全角の空白を飛ばす。
void SkipSpaces(std::string_view& text) {
    for (;;) {
        if (!text.empty() && (text[0] == ' ' || text[0] == '\t'))
            text.remove_prefix(1);
        else if (StartsWith(text, "\xE3\x80\x80"))
            text.remove_prefix(3);
        else
            return;
    }
}

// 「keyword 数 [unit] suffix」の数（試作の正規表現 keyword\s*(\d+(\.\d+)?)\s*unit?\s*suffix と同じ）。
// keyword が何度か出てくる時は、形が合う最初のもの。
std::optional<double> NumberBetween(std::string_view text, std::string_view keyword, std::string_view unit,
                                    std::string_view suffix) {
    for (size_t at = text.find(keyword); at != std::string_view::npos; at = text.find(keyword, at + 1)) {
        std::string_view rest = text.substr(at + keyword.size());
        SkipSpaces(rest);
        size_t end = 0;
        while (end < rest.size() && IsDigit(rest[end]))
            ++end;
        if (end == 0)
            continue;
        if (end + 1 < rest.size() && rest[end] == '.' && IsDigit(rest[end + 1])) {
            ++end;
            while (end < rest.size() && IsDigit(rest[end]))
                ++end;
        }
        const double value = std::strtod(std::string(rest.substr(0, end)).c_str(), nullptr);
        rest.remove_prefix(end);
        SkipSpaces(rest);
        if (!unit.empty() && StartsWith(rest, unit)) {
            rest.remove_prefix(unit.size());
            SkipSpaces(rest);
        }
        if (StartsWith(rest, suffix))
            return value;
    }
    return std::nullopt;
}

bool Contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

} // namespace

BossMoveCondition ParseBossMoveCondition(std::string_view text) {
    BossMoveCondition condition;
    // 「HP」は大文字・小文字どちらでもよい。半角の英字だけを小文字にする（日本語のバイトは変わらない）。
    std::string lowered(text);
    for (char& ch : lowered) {
        if (ch >= 'A' && ch <= 'Z')
            ch = static_cast<char>(ch - 'A' + 'a');
    }
    // 空欄は「常に」。
    std::string_view written = lowered;
    SkipSpaces(written);
    if (written.empty())
        return condition;
    bool known = false;
    auto read = [&](std::string_view keyword, std::string_view unit, std::string_view suffix, auto&& store) {
        if (const auto value = NumberBetween(lowered, keyword, unit, suffix)) {
            store(*value);
            known = true;
        }
    };
    read("フェーズ", "", "以上", [&](double value) { condition.phaseAtLeast = static_cast<int>(value); });
    read("フェーズ", "", "以下", [&](double value) { condition.phaseAtMost = static_cast<int>(value); });
    if (condition.phaseAtMost < 0) {
        read("フェーズ", "", "のみ", [&](double value) { condition.phaseOnly = static_cast<int>(value); });
        read("フェーズ", "", "だけ", [&](double value) { condition.phaseOnly = static_cast<int>(value); });
    }
    read("hp", "%", "以下", [&](double value) { condition.healthAtMost = value; });
    read("hp", "%", "以上", [&](double value) { condition.healthAtLeast = value; });
    read("距離", "m", "以内", [&](double value) { condition.distanceWithin = value; });
    read("距離", "m", "以上", [&](double value) { condition.distanceBeyond = value; });
    if (Contains(lowered, "連続攻撃") || Contains(lowered, "連撃") || Contains(lowered, "攻撃し続") ||
        Contains(lowered, "撃ち続")) {
        condition.pressure = true;
        known = true;
    }
    condition.unreadable = !known;
    return condition;
}

bool BossMoveConditionHolds(const BossMoveCondition& condition, const BossSituation& situation) {
    if (condition.phaseAtLeast >= 0 && situation.phase < condition.phaseAtLeast)
        return false;
    if (condition.phaseAtMost >= 0 && situation.phase > condition.phaseAtMost)
        return false;
    if (condition.phaseOnly >= 0 && situation.phase != condition.phaseOnly)
        return false;
    if (condition.healthAtMost >= 0.0 && situation.healthPercent > condition.healthAtMost)
        return false;
    if (condition.healthAtLeast >= 0.0 && situation.healthPercent < condition.healthAtLeast)
        return false;
    const double distance = static_cast<double>(situation.distance);
    if (condition.distanceWithin >= 0.0 && distance > condition.distanceWithin)
        return false;
    if (condition.distanceBeyond >= 0.0 && distance < condition.distanceBeyond)
        return false;
    return true;
}

} // namespace Project_TD2_2
