#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 技の表の読み込み（c-855f05 で BossSystem から分けた）：動きの型・技の表・クリップの長さ・区間の表を Data/ から読み、
// 中身が変わった時だけ表を作り直して、読んだ技を記録に書く。
class BossTableLoader final {
public:
    // 表を読む。中身が前と同じなら何もしない（技を出すたびに呼ぶので、Play 中に保存した CSV が次の技から効く）。
    static void Load(HIKARI::WorldView& world, Boss& boss);
    // 検査器の「技の表」の項目（表の場所）。
};

} // namespace Project_TD2_2
