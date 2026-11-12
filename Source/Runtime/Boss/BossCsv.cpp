#include "BossCsv.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Project_TD2_2::BossCsv {

bool Contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }
bool StartsWith(std::string_view text, std::string_view part) { return text.substr(0, part.size()) == part; }

std::string Trim(std::string_view text) {
    size_t begin = 0, end = text.size();
    while (begin < end && (text[begin] == ' ' || text[begin] == '\t' || text[begin] == '\r' || text[begin] == '\n'))
        ++begin;
    while (end > begin &&
           (text[end - 1] == ' ' || text[end - 1] == '\t' || text[end - 1] == '\r' || text[end - 1] == '\n'))
        --end;
    // 全角の空白も落とす。
    std::string result(text.substr(begin, end - begin));
    while (StartsWith(result, "\xE3\x80\x80"))
        result.erase(0, 3);
    while (result.size() >= 3 && result.compare(result.size() - 3, 3, "\xE3\x80\x80") == 0)
        result.erase(result.size() - 3);
    return result;
}

std::vector<Row> ParseCsv(std::string_view text) {
    std::vector<Row> rows;
    Row row;
    std::string cell;
    bool quoted = false;
    if (StartsWith(text, "\xEF\xBB\xBF"))
        text.remove_prefix(3);
    auto endRow = [&]() {
        row.push_back(cell);
        cell.clear();
        if (std::any_of(row.begin(), row.end(), [](const std::string& c) { return !Trim(c).empty(); }))
            rows.push_back(row);
        row.clear();
    };
    for (size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (quoted) {
            if (ch == '"') {
                if (i + 1 < text.size() && text[i + 1] == '"') {
                    cell += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cell += ch;
            }
            continue;
        }
        if (ch == '"' && cell.empty()) {
            quoted = true;
        } else if (ch == ',') {
            row.push_back(cell);
            cell.clear();
        } else if (ch == '\n' || ch == '\r') {
            if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                ++i;
            endRow();
        } else {
            cell += ch;
        }
    }
    if (!cell.empty() || !row.empty())
        endRow();
    return rows;
}

std::optional<double> NumberOf(std::string_view text) {
    for (size_t i = 0; i < text.size(); ++i) {
        const bool digit = text[i] >= '0' && text[i] <= '9';
        const bool sign = text[i] == '-' && i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9';
        if (!digit && !sign)
            continue;
        size_t end = i + (sign ? 1 : 0);
        while (end < text.size() && text[end] >= '0' && text[end] <= '9')
            ++end;
        if (end + 1 < text.size() && text[end] == '.' && text[end + 1] >= '0' && text[end + 1] <= '9') {
            ++end;
            while (end < text.size() && text[end] >= '0' && text[end] <= '9')
                ++end;
        }
        return std::strtod(std::string(text.substr(i, end - i)).c_str(), nullptr);
    }
    return std::nullopt;
}

std::vector<std::string> Split(std::string_view text, bool spaces, bool dots) {
    std::vector<std::string> parts;
    std::string current;
    auto flush = [&]() {
        if (!current.empty())
            parts.push_back(current);
        current.clear();
    };
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '/' || text[i] == ',' || (spaces && (text[i] == ' ' || text[i] == '\t'))) {
            flush();
            ++i;
        } else if (StartsWith(text.substr(i), "\xEF\xBC\x8F") || StartsWith(text.substr(i), "\xE3\x80\x81") ||
                   (spaces && StartsWith(text.substr(i), "\xE3\x80\x80")) ||
                   (dots && StartsWith(text.substr(i), "\xE3\x83\xBB"))) {
            flush();
            i += 3;
        } else {
            current += text[i++];
        }
    }
    flush();
    return parts;
}

int RoundHalfUp(double value) { return static_cast<int>(std::floor(value + 0.5)); }

} // namespace Project_TD2_2::BossCsv
