#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

// 技の表（CSV）を読むための小さな文字の処理。表の読み方は試作「霞の剣客」の行動表と同じ。
namespace Project_TD2_2::BossCsv {

using Row = std::vector<std::string>;

bool Contains(std::string_view text, std::string_view part);
bool StartsWith(std::string_view text, std::string_view part);
// 前後の空白（半角・全角・改行）を落とす。
std::string Trim(std::string_view text);
// セルの中の改行（"..."）にも対応した CSV。空の行は捨てる。
std::vector<Row> ParseCsv(std::string_view text);
// 最初に出てくる数（-?\d+(\.\d+)?）。無ければ空。「72F」は 72。
std::optional<double> NumberOf(std::string_view text);
// 区切り（/ ／ , 、 と、spaces の時は空白も、dots の時は ・ も）で分ける。
std::vector<std::string> Split(std::string_view text, bool spaces, bool dots = false);
// 試作と同じ丸め（Math.round：0.5 は上へ）。
int RoundHalfUp(double value);

} // namespace Project_TD2_2::BossCsv
