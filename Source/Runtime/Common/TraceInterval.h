#pragma once

#include <cstdint>

// 確かめ用の記録を出す間隔の判定（各所で「every > 0 && count % every == 0」と書いていたものをまとめた）。
namespace Project_TD2_2::TraceInterval {

// every（F または tick。0 以下は出さない）ごとに、count がその節目か。
inline bool Due(int every, uint64_t count) { return every > 0 && count % static_cast<uint64_t>(every) == 0; }

} // namespace Project_TD2_2::TraceInterval
