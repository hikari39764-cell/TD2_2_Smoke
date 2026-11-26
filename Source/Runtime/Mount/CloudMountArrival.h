#pragma once

#include "CloudMountComponent.h"

namespace Project_TD2_2 {

// 召喚と解除の演出の規則（雲が飛んで来る道・着いた後の沈みと膨らみ・向きの戻し方）。世界を知らない。雲を動かして
// エフェクトへ送るのは CloudMountVisuals。値は CloudMount の項目（節：召喚の演出）。
// 道は足元を終点にした 2 次のベジェ曲線で、足元から見た位置で持つ（体が動いても、落ちていても、足元へ届く）。
class CloudMountArrival final {
public:
    // 召喚の始め。facing は体の向き（水平の単位ベクトル）、catching は落下の受け止め。飛ぶ道と長さを決める。
    // 飛ぶ速さの見積もりが summonFlySpeedLimit を超えるなら、道を足元の方へ縮める。
    static void Begin(const CloudMount& mount, CloudMountArrivalState& state, HIKARI::GameProjectFloat3 facing,
                      bool catching);
    // 飛び始めからの秒 age での道の進み（0〜1。出だしが速く、足元の手前でゆるむ）。
    static float Progress(const CloudMount& mount, const CloudMountArrivalState& state);
    // 進み u の時の、足元から見た位置と、進む向き（u で微分した道の向き。長さは意味を持たない）。
    static HIKARI::GameProjectFloat3 PathOffset(const CloudMountArrivalState& state, float u);
    static HIKARI::GameProjectFloat3 PathDirection(const CloudMountArrivalState& state, float u);
    // 着いてからの、雲を下げる量 (m)。一度沈んで戻る。
    static float Sink(const CloudMount& mount, const CloudMountArrivalState& state);
    // 着いてからの余韻（1→0）。着いた瞬間が 1 で、summonPuffSeconds で 0 へ戻る。体の雲に足す量（膨らみ）は
    // summonPuffAmount × これ、渦の強さは飛んで来る間の summonFlySwirl からこれに合わせて 1 へ戻す。
    static float Afterglow(const CloudMount& mount, const CloudMountArrivalState& state);
    // 着いてから、向きを飛ぶ向きから体の向きへ戻す割合（0〜1）。着いていない・戻し終えた時は 1。
    static float Settle(const CloudMount& mount, const CloudMountArrivalState& state);
    // 向き（度）：水平の向きと、上り下りの角度（下りが正）。limitDegrees で上り下りを抑える。
    static float YawOf(HIKARI::GameProjectFloat3 direction);
    static float PitchOf(HIKARI::GameProjectFloat3 direction, float limitDegrees);
    // a から b へ割合 t だけ回した角度（度。近い方へ回る）。
    static float BlendDegrees(float a, float b, float t);
};

} // namespace Project_TD2_2
