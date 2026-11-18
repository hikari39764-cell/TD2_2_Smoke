#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// フェーズ 2 の場の雰囲気（見た目だけ。c-857ae3）：場の色を暖かくし、ボスの足元に漂う火の粉と、変わり目にボスが
// 立っていた所の焦げた輪を出す。撃破で戻す。
// 場の色は環境の色調整の置き換え（EnvironmentView::SetColorGradingOverride）で付ける：全画面の効果の口
// （SubmitActionPostProfile）は 1 つで優先度の高いものが丸ごと勝つので、フェーズ 2 の間ずっと使うと筋斗雲の速度感と
// 取り合う。色調整は別の段（トーンマップの後の LUT）で、場面の環境の色調整（LUT・色味）を土台にして掛け合わせる。
class BossPhaseTwoField final {
public:
    // 毎フレーム呼ぶ（外形を出さないフレームも。撃破で戻すため）。seconds はゲームの時間。
    static void Update(HIKARI::WorldView& world, BossComponent& boss, float seconds);
    // 出し続けているエフェクトを止め、場の色を元へ戻す。
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
