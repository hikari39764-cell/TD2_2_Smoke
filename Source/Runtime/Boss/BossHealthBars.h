#pragma once

#include "BossFrame.h"
#include "BossPhaseShiftComponent.h"
#include "../Combat/Damageable.h"

namespace Project_TD2_2 {

// ボスの HP の条（c-3e0ffa）。条はフェーズごとに 1 本：フェーズ 1 は 1 本目（phaseOneHealth）、1 本目が空になった tick
// に変わり目（phaseShiftFrames F。無敵・技を出さない）へ入り、同じ条を 2 本目（phaseTwoHealth）として満たす。
// HP の持ち主は Damageable のまま、いまの条の値をその最大 HP と HP に入れる（2 本目が空になれば、Damageable の死亡が
// そのまま撃破になる）。累計のダメージ（気絶値の基準）と当たりの数は条を満たしても戻さないので、差分を読む規則はずれない。
class BossHealthBars final {
public:
    // 最初の同期：Damageable の最大 HP と HP を 1 本目の値にする（シーンを読み直すと部品から作り直すので、
    // やり直しの後も 1 本目から）。
    static void Prepare(BossComponent& boss, const BossPhaseShiftComponent* shift, Damageable& health);
    // 固定 tick の始め：変わり目の残りを 1 F 進める（0 になった tick から待機の行動を許す）。
    static void Step(BossComponent& boss);
    // 1 本目が空なら変わり目に入って true：いまの技を打ち切って待機へ（剣気の扱いは .cpp）、気絶値を 0 に戻し、
    // フェーズを 2 にして 2 本目を満たす（余ったダメージは持ち越さない）。rawDamage はこの tick に受けた量（ログ用）。
    static bool CheckEmptied(BossComponent& boss, const BossPhaseShiftComponent* shift, Damageable& health,
                             double rawDamage);
    // 2 本ぶんの残りの割合（%。2 本の合計に対する）。技の表の「HP60%以下」は、前と同じくボス全体の HP で見る。
    static double TotalPercent(const BossComponent& boss, const Damageable& health);
    // 物体変数へ出す値（いまの条の HP と最大・フェーズ・フェーズに入ってからの秒）を写す。
    static void Publish(BossComponent& boss, const Damageable& health);
};

} // namespace Project_TD2_2
