#pragma once

#include <cstdint>

#include "BossFrame.h"
#include "../Combat/Damageable.h"

namespace Project_TD2_2 {

// ボスの被弾の規則（c-855f05 で BossSystem から分けた）：HP の持ち主の Damageable から受けた量を読み、気絶値をため、
// 気絶・崩し・体勢（怯み・ひざまずき）・フェーズの変わり目・撃破を決める。受ける威力の倍率と無敵も入れる。
class BossHealth final {
public:
    // HP の持ち主は Damageable。受けた量から気絶値をため、フェーズと死亡を決める。この tick に新しく当てられた数を返す。
    // HP はフェーズごとの条（BossHealthBars。1 本目が空になった tick に変わり目へ入る）。
    static uint64_t Sync(BossFrame& frame, HIKARI::ObjectView object, Damageable& health);
    // 連段が終わった時（待機へ戻る時・新しく技を始める時）に、たまった体勢を決める（c-808641）：満ちていればひざまずき、
    // 半分ほど（poiseFlinchShare）なら怯んで後ろへ跳ぶ技を始める。どちらかを始めたら true。
    static bool ResolvePoise(BossFrame& frame);
    // 検査器の「フェーズと気絶」の項目（HP の条・気絶・崩し）。
};

} // namespace Project_TD2_2
