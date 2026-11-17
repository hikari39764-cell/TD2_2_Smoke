#include "BossMoveLinks.h"

#include <algorithm>
#include <string_view>

#include "BossCsv.h"

namespace Project_TD2_2 {
namespace {

using namespace BossCsv;

// 「名前:重み」を名前と重みに分ける（全角の「：」も可）。重みが読めない・0 以下なら、名前全体を名前とし重み 1。
std::string SplitWeight(const std::string& part, float& weight, std::string& bad) {
    weight = 1.0f;
    size_t at = part.rfind(':');
    size_t width = 1;
    const size_t wide = part.rfind("\xEF\xBC\x9A");
    if (wide != std::string::npos && (at == std::string::npos || wide > at)) {
        at = wide;
        width = 3;
    }
    if (at == std::string::npos)
        return Trim(part);
    const std::string text = Trim(std::string_view(part).substr(at + width));
    const auto value = NumberOf(text);
    if (!value || *value <= 0.0) {
        bad = text;
        return Trim(std::string_view(part).substr(0, at));
    }
    weight = static_cast<float>(*value);
    return Trim(std::string_view(part).substr(0, at));
}

} // namespace

void BossMoveLinks::LinkCombos(std::vector<BossMove>& moves, std::vector<std::string>& warnings) {
    for (size_t self = 0; self < moves.size(); ++self) {
        BossMove& move = moves[self];
        for (const auto& part : Split(move.comboText, false, true)) {
            float weight = 1.0f;
            std::string bad;
            const std::string name = SplitWeight(part, weight, bad);
            if (!bad.empty())
                warnings.push_back("「" + move.name + "」: コンボ先「" + part + "」の重み「" + bad +
                                   "」が読めないので 1 にしました（0 より大きい数）");
            if (name.empty())
                continue;
            std::vector<int> hits;
            for (size_t i = 0; i < moves.size(); ++i) {
                if (moves[i].name == name)
                    hits.push_back(static_cast<int>(i));
            }
            if (hits.empty()) {
                for (size_t i = 0; i < moves.size(); ++i) {
                    if (i != self && (Contains(moves[i].name, name) || Contains(name, moves[i].name)))
                        hits.push_back(static_cast<int>(i));
                }
            }
            if (hits.empty())
                warnings.push_back("「" + move.name + "」: コンボ先「" + name + "」という行動が表にありません");
            for (const int hit : hits) {
                if (std::find(move.comboTargets.begin(), move.comboTargets.end(), hit) == move.comboTargets.end()) {
                    move.comboTargets.push_back(hit);
                    move.comboWeights.push_back(weight);
                }
            }
        }
    }
}

void BossMoveLinks::AddSwitches(std::vector<BossMove>& moves, std::vector<std::string>& warnings) {
    const size_t original = moves.size();
    for (size_t self = 0; self < original; ++self) {
        const std::string name = moves[self].switchText;
        if (name.empty() || moves[self].switchChance <= 0.0f)
            continue;
        int target = -1;
        for (size_t i = 0; i < original; ++i) {
            if (moves[i].name == name)
                target = static_cast<int>(i);
        }
        if (target < 0 || target == static_cast<int>(self)) {
            warnings.push_back("「" + moves[self].name + "」: 変化先「" + name + "」という行動が表にありません");
            continue;
        }
        BossMove copy = moves[static_cast<size_t>(target)];
        copy.name += "（変化）";
        copy.pre = std::max(2, moves[self].switchPre);
        copy.windupExtras.clear();
        copy.breakHits = 0;
        copy.switchText.clear();
        copy.switchChance = 0.0f;
        copy.switchMove = -1;
        copy.comboOnly = true;
        copy.use = BossMoveUse::Combo;
        copy.distanceWeight = -1.0f;
        moves[self].switchMove = static_cast<int>(moves.size());
        moves.push_back(std::move(copy));
    }
}

} // namespace Project_TD2_2
