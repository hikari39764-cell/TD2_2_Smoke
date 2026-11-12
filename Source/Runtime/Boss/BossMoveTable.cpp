#include "BossMoveTable.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>

#include "BossCsv.h"

namespace Project_TD2_2 {
namespace {

constexpr int kTypeCount = static_cast<int>(BossMoveType::Count);
// 型の名前（表の「動きの型」列に表示名の代わりに書いてもよい）。試作の TYPES のキーと同じ。
constexpr const char* kTypeKeys[kTypeCount] = {"dash",     "jump",   "slashR",   "slashL",  "overhead", "kick",
                                               "knife",    "string", "ambush",   "clone",   "slashH",   "slashV",
                                               "walkSide", "walkIn", "backstep", "detonate"};

// 表の見出し。左から順に「見出しがこれで始まる」→「これを含む」で列を決める（試作の HEADER_MAP と同じ）。
enum class Column {
    Name,
    Description,
    Role,
    Pre,
    Act,
    Post,
    Condition,
    Weight,
    Size,
    Damage,
    Combo,
    Type,
    Chance,
    Speed,
    Homing,
    Knock,
    Step,
    Side,
    BurnShape,
    BurnWidth,
    BurnSeconds,
    TrackStop,
    WindupJitter,
    BreakHits,
    Count
};
struct HeaderKey {
    const char* text;
    Column column;
};
// 「追尾を止める」は「追尾」より、「崩せる被弾数」は「被弾」より先に見る（後ろの短い見出しに取られないように）。
constexpr HeaderKey kHeaders[] = {
    {"追尾を止める", Column::TrackStop},
    {"溜めの揺らぎ", Column::WindupJitter},
    {"崩せる", Column::BreakHits},
    {"左右移動", Column::Side},
    {"被弾", Column::Knock},
    {"吹っ飛", Column::Knock},
    {"踏み込み", Column::Step},
    {"ボス行動", Column::Name},
    {"行動名", Column::Name},
    {"技", Column::Name},
    {"概要", Column::Description},
    {"役割", Column::Role},
    {"前隙", Column::Pre},
    {"攻撃中", Column::Act},
    {"持続", Column::Act},
    {"後隙", Column::Post},
    {"発生条件", Column::Condition},
    {"条件", Column::Condition},
    {"距離重み", Column::Weight},
    {"判定サイズ", Column::Size},
    {"判定", Column::Size},
    {"威力", Column::Damage},
    {"ダメージ", Column::Damage},
    {"コンボ先", Column::Combo},
    {"派生", Column::Combo},
    {"動きの型", Column::Type},
    {"コンボ確率", Column::Chance},
    {"斬撃速度", Column::Speed},
    {"追尾", Column::Homing},
    {"残す火", Column::BurnShape},
    {"火の幅", Column::BurnWidth},
    {"火の秒数", Column::BurnSeconds},
};

using namespace BossCsv;

std::optional<BossStrikeDirection> DirectionOf(std::string_view body) {
    if (body == "横")
        return BossStrikeDirection::Horizontal;
    if (body == "縦")
        return BossStrikeDirection::Vertical;
    if (body == "袈裟")
        return BossStrikeDirection::Kesa;
    if (body == "逆袈裟")
        return BossStrikeDirection::ReverseKesa;
    return std::nullopt;
}

// 連撃の一撃の書き方「(小|中|大)?(遅)?(本体)」。
struct StrikeToken {
    char size = 'S';
    bool sized = false;
    bool feint = false;
    std::string body;
};
StrikeToken ReadStrikeToken(std::string_view text) {
    StrikeToken token;
    if (StartsWith(text, "小") || StartsWith(text, "中") || StartsWith(text, "大")) {
        token.size = StartsWith(text, "小") ? 'S' : StartsWith(text, "中") ? 'M' : 'L';
        token.sized = true;
        text.remove_prefix(3);
    }
    if (StartsWith(text, "遅")) {
        token.feint = true;
        text.remove_prefix(3);
    }
    token.body = std::string(text);
    return token;
}

// 型の列が空のとき、行動名から型を推す（試作の guessType と同じ順）。
BossMoveType GuessType(std::string_view name) {
    if (Contains(name, "下が") || Contains(name, "後退") || Contains(name, "バック"))
        return BossMoveType::Backstep;
    if (Contains(name, "詰め") || Contains(name, "近づ") || Contains(name, "接近"))
        return BossMoveType::WalkIn;
    if (Contains(name, "歩"))
        return BossMoveType::WalkSide;
    if (Contains(name, "分身"))
        return BossMoveType::Clone;
    if (Contains(name, "蹴"))
        return BossMoveType::Kick;
    if (Contains(name, "ナイフ") || Contains(name, "投げ"))
        return BossMoveType::Knife;
    if (Contains(name, "隠れ"))
        return BossMoveType::Ambush;
    int strokes = 0;
    for (size_t at = 0; at + 3 <= name.size(); at += 1) {
        const std::string_view piece = name.substr(at, 3);
        if (piece == "横" || piece == "縦")
            ++strokes;
    }
    if (strokes >= 3)
        return BossMoveType::String;
    if (Contains(name, "突進") || Contains(name, "ダッシュ"))
        return BossMoveType::Dash;
    if (Contains(name, "ジャンプ") || Contains(name, "跳"))
        return BossMoveType::Jump;
    if (Contains(name, "上段") || Contains(name, "溜め") || Contains(name, "縦"))
        return BossMoveType::Overhead;
    if (Contains(name, "右"))
        return BossMoveType::SlashR;
    if (Contains(name, "左"))
        return BossMoveType::SlashL;
    return BossMoveType::SlashH;
}

std::optional<BossStrikeDirection> DefaultDirection(BossMoveType type) {
    switch (type) {
    case BossMoveType::SlashH:
    case BossMoveType::Dash:
    case BossMoveType::Ambush: return BossStrikeDirection::Horizontal;
    case BossMoveType::SlashV:
    case BossMoveType::Overhead: return BossStrikeDirection::Vertical;
    case BossMoveType::SlashR:
    case BossMoveType::Jump: return BossStrikeDirection::Kesa;
    case BossMoveType::SlashL: return BossStrikeDirection::ReverseKesa;
    default: return std::nullopt;
    }
}

struct RawMove {
    std::string cells[static_cast<int>(Column::Count)];
    const std::string& operator[](Column column) const { return cells[static_cast<int>(column)]; }
};

// 「残す火」「火の幅 m」「火の秒数」。列が無い・空欄・「なし」は火を残さない。読めない値は警告して火を残さない。
void ReadBurn(const RawMove& raw, BossMove& move) {
    const std::string& shape = raw[Column::BurnShape];
    if (shape.empty() || shape == "なし")
        return;
    BossBurnShape parsed = BossBurnShape::None;
    if (shape == "線")
        parsed = BossBurnShape::Line;
    else if (shape == "輪")
        parsed = BossBurnShape::Ring;
    else if (shape == "池")
        parsed = BossBurnShape::Pool;
    if (parsed == BossBurnShape::None) {
        move.warnings.push_back("残す火「" + shape + "」が読めないので火を残しません（なし／線／輪／池）");
        return;
    }
    const auto width = NumberOf(raw[Column::BurnWidth]);
    const auto seconds = NumberOf(raw[Column::BurnSeconds]);
    if (!width || *width <= 0.0) {
        move.warnings.push_back("火の幅「" + raw[Column::BurnWidth] +
                                "」が読めないので火を残しません（0 より大きい m）");
        return;
    }
    if (!seconds || *seconds <= 0.0) {
        move.warnings.push_back("火の秒数「" + raw[Column::BurnSeconds] +
                                "」が読めないので火を残しません（0 より大きい秒）");
        return;
    }
    move.burnShape = parsed;
    move.burnWidth = static_cast<float>(*width);
    move.burnSeconds = static_cast<float>(*seconds);
}

// 「追尾を止める F」「溜めの揺らぎ F」「崩せる被弾数」。列が無い・空欄は今までどおり（最後まで追う・揺らがない・崩れない）。
void ReadWindup(const RawMove& raw, BossMove& move) {
    const std::string& track = raw[Column::TrackStop];
    const auto stop = NumberOf(track);
    if (!track.empty() && (!stop || *stop < 0.0))
        move.warnings.push_back("追尾を止める F「" + track + "」が読めないので最後まで追います（0 以上の F）");
    move.trackStopFrames = stop && *stop > 0.0 ? RoundHalfUp(*stop) : 0;
    const std::string& jitter = raw[Column::WindupJitter];
    for (const auto& part : Split(jitter, true)) {
        const auto extra = NumberOf(part);
        if (!extra || *extra < 0.0) {
            move.warnings.push_back("溜めの揺らぎ F「" + jitter +
                                    "」が読めないので揺らしません（0/20/40 のように書く）");
            move.windupExtras.clear();
            break;
        }
        move.windupExtras.push_back(RoundHalfUp(*extra));
    }
    const std::string& hits = raw[Column::BreakHits];
    const auto breakHits = NumberOf(hits);
    if (!hits.empty() && (!breakHits || *breakHits < 1.0))
        move.warnings.push_back("崩せる被弾数「" + hits + "」が読めないので崩れません（1 以上の回数）");
    move.breakHits = breakHits && *breakHits >= 1.0 ? RoundHalfUp(*breakHits) : 0;
}

// 1 行を読む中間結果。連撃の参照（別の行の名前）を解くため、全部の行を 2 回読む。
struct Parsed {
    BossMove move;
    // 連撃の一撃ごとの読み。ref は参照した行の番号。
    struct Strike {
        char size = 'S';
        bool sized = false;
        bool feint = false;
        BossStrikeDirection direction = BossStrikeDirection::Horizontal;
        int ref = -1;
        bool named = false;
    };
    std::vector<Strike> strikes;
    std::vector<int> strikeDamage;
    bool hasReference = false;
};

// コンボ先の名前を行につなぐ：完全一致 → 無ければ名前を含む行（自分は除く。合う行は全部）。
// 「/」「、」「・」で複数書ける（試作と同じ）。
void LinkCombos(std::vector<BossMove>& moves, std::vector<std::string>& warnings) {
    for (size_t self = 0; self < moves.size(); ++self) {
        BossMove& move = moves[self];
        for (const auto& part : Split(move.comboText, false, true)) {
            const std::string name = Trim(part);
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
                if (std::find(move.comboTargets.begin(), move.comboTargets.end(), hit) == move.comboTargets.end())
                    move.comboTargets.push_back(hit);
            }
        }
    }
}

} // namespace

const char* BossMoveTable::TypeKey(BossMoveType type) {
    const int index = static_cast<int>(type);
    return index >= 0 && index < kTypeCount ? kTypeKeys[index] : "";
}

const char* BossMoveTable::BurnKey(BossBurnShape shape) {
    switch (shape) {
    case BossBurnShape::Line: return "line";
    case BossBurnShape::Ring: return "ring";
    case BossBurnShape::Pool: return "pool";
    default: return "none";
    }
}

bool BossMoveTable::IsMoveType(BossMoveType type) {
    return type == BossMoveType::WalkSide || type == BossMoveType::WalkIn || type == BossMoveType::Backstep;
}

bool BossMoveTable::IsSwingType(BossMoveType type) {
    switch (type) {
    case BossMoveType::Dash:
    case BossMoveType::Jump:
    case BossMoveType::SlashR:
    case BossMoveType::SlashL:
    case BossMoveType::SlashH:
    case BossMoveType::SlashV:
    case BossMoveType::Overhead:
    case BossMoveType::String:
    case BossMoveType::Ambush: return true;
    default: return false;
    }
}

bool BossMoveTable::IsExecutable(BossMoveType type) { return type != BossMoveType::Count; }

const BossStrikeData& BossMoveTable::Strike(const BossMove& move, int strike) {
    static const BossStrikeData kNone{};
    if (move.strikes.empty())
        return kNone;
    return move.strikes[static_cast<size_t>(std::clamp(strike, 0, static_cast<int>(move.strikes.size()) - 1))];
}

int BossMoveTable::Find(std::string_view name) const {
    for (size_t i = 0; i < moves_.size(); ++i) {
        if (moves_[i].name == name)
            return static_cast<int>(i);
    }
    return -1;
}

bool BossMoveTable::LoadTypes(std::string_view csv, std::vector<std::string>& warnings) {
    const auto rows = ParseCsv(csv);
    if (rows.size() < 2) {
        warnings.push_back("動きの型の表に行がありません");
        return false;
    }
    int loaded = 0;
    for (size_t r = 1; r < rows.size(); ++r) {
        const Row& row = rows[r];
        auto cell = [&](size_t index) { return index < row.size() ? Trim(row[index]) : std::string(); };
        const std::string key = cell(0);
        int index = -1;
        for (int t = 0; t < kTypeCount; ++t) {
            if (key == kTypeKeys[t])
                index = t;
        }
        if (index < 0) {
            warnings.push_back("動きの型「" + key + "」は実行器が知らない型なので飛ばしました");
            continue;
        }
        BossTypeDefaults& type = types_[index];
        type.label = cell(1);
        type.pre = static_cast<int>(NumberOf(cell(2)).value_or(0.0));
        type.act = cell(3);
        type.post = static_cast<int>(NumberOf(cell(4)).value_or(0.0));
        type.size = static_cast<float>(NumberOf(cell(5)).value_or(0.0));
        type.damage = static_cast<float>(NumberOf(cell(6)).value_or(0.0));
        type.speed = static_cast<float>(NumberOf(cell(7)).value_or(0.0));
        type.homing = static_cast<float>(NumberOf(cell(8)).value_or(0.0));
        type.step = static_cast<float>(NumberOf(cell(9)).value_or(0.0));
        type.loaded = true;
        ++loaded;
    }
    for (int t = 0; t < kTypeCount; ++t) {
        if (!types_[t].loaded)
            warnings.push_back(std::string("動きの型「") + kTypeKeys[t] + "」の既定値が表にありません（0 で動きます）");
    }
    return loaded > 0;
}

bool BossMoveTable::LoadMoves(std::string_view csv, const BossTiming& timing, std::vector<std::string>& warnings) {
    moves_.clear();
    const auto rows = ParseCsv(csv);
    if (rows.size() < 2) {
        warnings.push_back("技の表には見出し行と、少なくとも 1 行のデータが要ります");
        return false;
    }
    int columns[static_cast<int>(Column::Count)];
    std::fill(std::begin(columns), std::end(columns), -1);
    for (size_t c = 0; c < rows[0].size(); ++c) {
        const std::string header = Trim(rows[0][c]);
        const HeaderKey* found = nullptr;
        for (const auto& key : kHeaders) {
            if (StartsWith(header, key.text)) {
                found = &key;
                break;
            }
        }
        if (found == nullptr) {
            for (const auto& key : kHeaders) {
                if (Contains(header, key.text)) {
                    found = &key;
                    break;
                }
            }
        }
        if (found != nullptr && columns[static_cast<int>(found->column)] < 0)
            columns[static_cast<int>(found->column)] = static_cast<int>(c);
    }
    if (columns[static_cast<int>(Column::Name)] < 0) {
        warnings.push_back("技の表に「ボス行動」の見出しがありません");
        return false;
    }
    std::vector<RawMove> raws;
    for (size_t r = 1; r < rows.size(); ++r) {
        RawMove raw;
        for (int c = 0; c < static_cast<int>(Column::Count); ++c) {
            if (columns[c] >= 0 && static_cast<size_t>(columns[c]) < rows[r].size())
                raw.cells[c] = Trim(rows[r][static_cast<size_t>(columns[c])]);
        }
        if (!raw[Column::Name].empty())
            raws.push_back(raw);
    }

    const double strikeLength = timing.swingInFrames + timing.swingFrames + timing.followFrames;
    // 振り上げ＋構えが必ず見えるだけの間。
    const int minimumGap = timing.kamaeFrames + timing.raiseFrames;
    auto gapFrames = [&](char size) {
        return size == 'L' ? timing.gapLargeFrames : size == 'M' ? timing.gapMediumFrames : timing.gapSmallFrames;
    };
    auto autoDamage = [&](double gap) {
        return std::max(1, RoundHalfUp(timing.damageBase + gap * static_cast<double>(timing.damagePerFrame)));
    };

    auto parseOne = [&](const RawMove& raw, int self, const std::vector<Parsed>* references) {
        Parsed parsed;
        BossMove& move = parsed.move;
        move.name = raw[Column::Name];
        move.description = raw[Column::Description];
        move.role = raw[Column::Role];
        move.condition = raw[Column::Condition];
        move.comboText = raw[Column::Combo];
        const std::string& typeText = raw[Column::Type];
        int typeIndex = -1;
        for (int t = 0; t < kTypeCount && !typeText.empty(); ++t) {
            if (typeText == kTypeKeys[t] || (!types_[t].label.empty() && typeText == types_[t].label))
                typeIndex = t;
        }
        if (typeIndex < 0) {
            if (!typeText.empty())
                move.warnings.push_back("動きの型「" + typeText + "」が読めないので行動名から決めました");
            typeIndex = static_cast<int>(GuessType(move.name));
        }
        move.type = static_cast<BossMoveType>(typeIndex);
        const BossTypeDefaults& T = types_[typeIndex];
        double pre = NumberOf(raw[Column::Pre]).value_or(T.pre);
        const double post = NumberOf(raw[Column::Post]).value_or(T.post);
        const std::string actText = raw[Column::Act].empty() ? T.act : raw[Column::Act];

        const auto rawTokens = Split(actText, true);
        bool allDirections = rawTokens.size() > 1;
        for (const auto& token : rawTokens)
            allDirections = allDirections && DirectionOf(ReadStrikeToken(token).body).has_value();
        if (move.type == BossMoveType::String || allDirections) {
            for (const auto& text : rawTokens) {
                const StrikeToken token = ReadStrikeToken(text);
                Parsed::Strike strike;
                strike.size = token.size;
                strike.sized = token.sized;
                strike.feint = token.feint;
                if (const auto direction = DirectionOf(token.body)) {
                    strike.direction = *direction;
                    parsed.strikes.push_back(strike);
                    continue;
                }
                if (!token.body.empty() && token.body[0] >= '0' && token.body[0] <= '9')
                    continue;
                strike.named = true;
                parsed.hasReference = true;
                if (references != nullptr) {
                    // 完全一致 → 名前を含む行（自分と注記は除く）。
                    int found = -1;
                    for (size_t i = 0; i < references->size() && found < 0; ++i) {
                        if (static_cast<int>(i) != self && (*references)[i].move.name == token.body)
                            found = static_cast<int>(i);
                    }
                    for (size_t i = 0; i < references->size() && found < 0; ++i) {
                        const std::string& other = (*references)[i].move.name;
                        if (static_cast<int>(i) != self && (Contains(other, token.body) || Contains(token.body, other)))
                            found = static_cast<int>(i);
                    }
                    if (found < 0) {
                        move.warnings.push_back(
                            "連撃の「" + token.body +
                            "」という行動が表にありません（横/縦/袈裟/逆袈裟か、行動名を書きます）");
                    } else {
                        strike.ref = found;
                        const Parsed& ref = (*references)[static_cast<size_t>(found)];
                        if (ref.move.type == BossMoveType::String && !ref.strikes.empty())
                            strike.direction = ref.strikes[0].direction;
                        else
                            strike.direction =
                                DefaultDirection(ref.move.type).value_or(BossStrikeDirection::Horizontal);
                    }
                }
                parsed.strikes.push_back(strike);
            }
            if (parsed.strikes.empty()) {
                move.warnings.push_back("連撃は「小横/小横/大縦」のように書きます（小中大＋横/縦/袈裟/逆袈裟、"
                                        "または行動名。「遅」を付けるとフェイント：中遅横）");
                parsed.strikes.push_back({});
            }
            auto gapOf = [&](const Parsed::Strike& strike) {
                const int base = strike.ref >= 0 && !strike.sized
                                     ? (*references)[static_cast<size_t>(strike.ref)].move.pre
                                     : gapFrames(strike.size);
                return std::max(base, minimumGap) + (strike.feint ? timing.feintFrames : 0);
            };
            if (!NumberOf(raw[Column::Pre]))
                pre = gapOf(parsed.strikes[0]);
            for (size_t k = 0; k < parsed.strikes.size(); ++k) {
                const auto& strike = parsed.strikes[k];
                if (k > 0)
                    move.segments.push_back({gapOf(strike), false, static_cast<int>(k), strike.feint, 0, 0});
                const int length = strike.ref >= 0 ? std::max(static_cast<int>(strikeLength),
                                                              (*references)[static_cast<size_t>(strike.ref)].move.act)
                                                   : static_cast<int>(strikeLength);
                move.segments.push_back({length, true, static_cast<int>(k), false, 0, 0});
            }
        } else if (IsMoveType(move.type)) {
            // 移動：「40~100F」なら出すたびに 40〜100F のどれか。
            int low = -1, high = -1;
            const std::string& text = actText;
            for (size_t i = 0; i < text.size() && high < 0; ++i) {
                if (text[i] < '0' || text[i] > '9')
                    continue;
                size_t end = i;
                while (end < text.size() && text[end] >= '0' && text[end] <= '9')
                    ++end;
                const int value = std::atoi(text.substr(i, end - i).c_str());
                if (low < 0) {
                    size_t after = end;
                    while (after < text.size() && (text[after] == ' ' || text[after] == 'F'))
                        ++after;
                    const std::string_view rest = std::string_view(text).substr(after);
                    if (StartsWith(rest, "~") || StartsWith(rest, "-") || StartsWith(rest, "〜") ||
                        StartsWith(rest, "～") || StartsWith(rest, "－")) {
                        low = value;
                        i = after;
                        continue;
                    }
                    low = high = value;
                } else {
                    high = value;
                }
                i = end;
            }
            if (low < 0) {
                low = high = 60;
                move.warnings.push_back("移動の長さ「" + text + "」が読めないので 60F にしました");
            }
            if (high < 0)
                high = low;
            const int a = std::min(low, high), b = std::max(low, high);
            move.segments.push_back({std::max(1, RoundHalfUp((a + b) / 2.0)), false, 0, false, a, b});
        } else if (move.type == BossMoveType::Ambush) {
            // 霞隠れ：試作どおり「隠れF/現れF/斬りF×回数」。数が 1 つだけなら 1 回の長さとして 4:3:3 で割る。
            int count = 3;
            const size_t times = actText.find("×");
            if (times != std::string::npos)
                count = std::max(1, static_cast<int>(NumberOf(std::string_view(actText).substr(times)).value_or(3)));
            std::vector<double> numbers;
            for (const auto& part : Split(actText.substr(0, times), false)) {
                const auto value = NumberOf(part);
                if (value && *value > 0)
                    numbers.push_back(*value);
            }
            int hide = 0, appear = 0, hit = 0;
            if (numbers.size() >= 3) {
                hide = static_cast<int>(numbers[0]);
                appear = static_cast<int>(numbers[1]);
                hit = static_cast<int>(numbers[2]);
            } else {
                const int cycle = static_cast<int>(std::max(24.0, numbers.empty() ? 64.0 : numbers[0]));
                hide = RoundHalfUp(cycle * 0.4);
                appear = RoundHalfUp(cycle * 0.3);
                hit = cycle - hide - appear;
            }
            hit = std::max(hit, timing.swingInFrames + timing.swingFrames + 2);
            for (int k = 0; k < count; ++k) {
                move.segments.push_back({hide, false, k, false, 0, 0, BossSegmentKind::Hide});
                move.segments.push_back({appear, false, k, false, 0, 0, BossSegmentKind::Appear});
                move.segments.push_back({hit, true, k, false, 0, 0});
            }
            if (Contains(actText, "遅") && move.segments.size() >= 2) {
                move.segments[move.segments.size() - 2].length += timing.feintFrames;
                move.segments[move.segments.size() - 2].feint = true;
            }
        } else {
            std::vector<double> parts;
            for (const auto& part : Split(actText, false)) {
                const auto value = NumberOf(part);
                if (value && *value > 0)
                    parts.push_back(*value);
            }
            if (parts.empty()) {
                parts.push_back(NumberOf(T.act).value_or(1.0));
                if (!raw[Column::Act].empty())
                    move.warnings.push_back("攻撃中「" + raw[Column::Act] + "」が読めないので型の既定値にしました");
            }
            for (size_t k = 0; k < parts.size(); ++k)
                move.segments.push_back({static_cast<int>(parts[k]), k % 2 == 0, static_cast<int>(k / 2), false, 0, 0});
        }
        switch (move.type) {
        case BossMoveType::Dash:
        case BossMoveType::Jump:
        case BossMoveType::SlashR:
        case BossMoveType::SlashL:
        case BossMoveType::SlashH:
        case BossMoveType::SlashV:
        case BossMoveType::Overhead:
            // 前隙は最低「振り上げ＋構え」。
            pre = std::max(pre, static_cast<double>(timing.kamaeFrames + timing.raiseFrames));
            break;
        default: break;
        }
        const std::string& knock = raw[Column::Knock];
        move.knock = Contains(knock, "吹") ? (Contains(knock, "最後") ? BossKnock::LastBlow : BossKnock::Blow)
                                           : BossKnock::Flinch;
        move.step = static_cast<float>(NumberOf(raw[Column::Step]).value_or(T.step));
        move.distanceWeight = static_cast<float>(NumberOf(raw[Column::Weight]).value_or(-1.0));
        move.comboChance = static_cast<float>(NumberOf(raw[Column::Chance]).value_or(-1.0));
        move.comboOnly = Contains(move.role, "コンボ用");
        move.when = ParseBossMoveCondition(move.condition);
        if (move.when.unreadable)
            move.warnings.push_back("発生条件「" + move.condition +
                                    "」が読めないので「常に」にしました（読める例：フェーズ2以上 / HP60%以下 / "
                                    "距離8m以内 / 連続攻撃）");
        // 出し方：連続攻撃が条件で距離重みが空欄＝割り込み専用、距離重みも書いてあれば始動＋割り込み。
        move.use = move.type == BossMoveType::Clone || move.type == BossMoveType::Detonate ? BossMoveUse::Special
                   : move.when.pressure && move.distanceWeight < 0.0f                      ? BossMoveUse::Interrupt
                   : move.comboOnly                                                        ? BossMoveUse::Combo
                                                                                           : BossMoveUse::Start;
        move.pre = static_cast<int>(pre);
        move.post = static_cast<int>(post);
        // 威力が空欄なら「隙が小さいほど低い」：基本＋（前隙＋後隙）×係数。連撃は一撃ごとに前の構え、ナイフは 1 本あたり。
        const auto damage = NumberOf(raw[Column::Damage]);
        move.damageAuto = !damage.has_value();
        const size_t strikeCount = std::max<size_t>(1, parsed.strikes.size());
        if (damage) {
            move.damage = RoundHalfUp(*damage);
        } else if (move.type == BossMoveType::String) {
            move.damage = autoDamage(pre + post / static_cast<double>(strikeCount));
        } else if (move.type == BossMoveType::Knife) {
            move.damage = std::max(1, RoundHalfUp(autoDamage(pre + post) / 2.0));
        } else if (move.type == BossMoveType::Ambush) {
            move.damage = autoDamage((pre + post) / 2.0);
        } else if (IsMoveType(move.type) || move.type == BossMoveType::Clone) {
            move.damage = 0;
        } else {
            move.damage = autoDamage(pre + post);
        }
        for (size_t k = 0; k < parsed.strikes.size(); ++k) {
            const auto& strike = parsed.strikes[k];
            if (strike.ref >= 0) {
                const Parsed& ref = (*references)[static_cast<size_t>(strike.ref)];
                parsed.strikeDamage.push_back(ref.strikeDamage.empty() ? ref.move.damage : ref.strikeDamage[0]);
            } else if (move.damageAuto) {
                const double gap = (k > 0 ? gapFrames(strike.size) + (strike.feint ? timing.feintFrames : 0) : pre) +
                                   (k + 1 == parsed.strikes.size() ? post : 0.0);
                parsed.strikeDamage.push_back(autoDamage(gap));
            } else {
                parsed.strikeDamage.push_back(move.damage);
            }
        }
        move.size = static_cast<float>(NumberOf(raw[Column::Size]).value_or(T.size));
        move.speed = static_cast<float>(NumberOf(raw[Column::Speed]).value_or(T.speed));
        move.homing = static_cast<float>(NumberOf(raw[Column::Homing]).value_or(T.homing));
        move.side = static_cast<float>(NumberOf(raw[Column::Side]).value_or(0.0));
        ReadBurn(raw, move);
        ReadWindup(raw, move);
        move.act = 0;
        move.hitCount = 0;
        for (const auto& segment : move.segments) {
            move.act += segment.length;
            move.hitCount += segment.hit ? 1 : 0;
        }
        move.executable = IsExecutable(move.type);
        return parsed;
    };

    std::vector<Parsed> first;
    for (size_t i = 0; i < raws.size(); ++i)
        first.push_back(parseOne(raws[i], static_cast<int>(i), nullptr));
    std::vector<Parsed> second;
    for (size_t i = 0; i < raws.size(); ++i)
        second.push_back(first[i].hasReference ? parseOne(raws[i], static_cast<int>(i), &first) : first[i]);

    for (size_t i = 0; i < second.size(); ++i) {
        Parsed& parsed = second[i];
        BossMove& move = parsed.move;
        int hitIndex = 0;
        for (const auto& segment : move.segments) {
            if (!segment.hit)
                continue;
            BossStrikeData data;
            data.size = move.size;
            data.speed = move.speed;
            data.homing = move.homing;
            data.step = move.step;
            data.blow =
                move.knock == BossKnock::Blow || (move.knock == BossKnock::LastBlow && hitIndex >= move.hitCount - 1);
            data.damage = move.damage;
            data.direction = DefaultDirection(move.type).value_or(BossStrikeDirection::Horizontal);
            if (!parsed.strikes.empty()) {
                const auto& strike = parsed.strikes[static_cast<size_t>(
                    std::min<int>(segment.strike, static_cast<int>(parsed.strikes.size()) - 1))];
                data.direction = strike.direction;
                data.feint = strike.feint;
                data.damage = parsed.strikeDamage[static_cast<size_t>(
                    std::min<int>(segment.strike, static_cast<int>(parsed.strikeDamage.size()) - 1))];
                if (strike.ref >= 0) {
                    // 参照した行の値（判定サイズ・斬撃速度・追尾・踏み込み・被弾）。
                    const BossMove& ref = first[static_cast<size_t>(strike.ref)].move;
                    data.reference = strike.ref;
                    data.size = ref.size;
                    data.speed = ref.speed;
                    data.homing = ref.homing;
                    data.step = ref.step;
                    data.blow = ref.knock != BossKnock::Flinch;
                }
            }
            move.strikes.push_back(data);
            ++hitIndex;
        }
        for (const auto& warning : move.warnings)
            warnings.push_back("「" + move.name + "」: " + warning);
        moves_.push_back(std::move(move));
    }
    LinkCombos(moves_, warnings);
    return !moves_.empty();
}

} // namespace Project_TD2_2
