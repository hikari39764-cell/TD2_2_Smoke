#pragma once

#include <string_view>

#include "HIKARI/Project/GameComponent.h"
#include "BossPhaseShiftSettings.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossPhaseShiftType = "Project_TD2_2.BossPhaseShift";

// ボスのフェーズの変わり目の演出の部品（c-3a0f87 でボスの部品から分けた）：ボスと同じ物体に付ける。無ければ演出を
// しない（演出を切った時と同じ。変わり目は Boss.phaseShiftFrames の間、待機のまま）。進めるのはボスの System
// （BossSystem。ボスの tick の中で、前と同じ順に呼ぶ）、規則は BossPhaseShift、見た目は BossPhaseShiftVisuals。
// フェーズ 2 の見た目へ切り替える時（閃光）は、ボスの部品の phaseTwoLookPending に書く（体の効果・場の色が読む）。
class BossPhaseShiftComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    BossPhaseShiftSettings settings;
    BossPhaseShiftState state;
};

} // namespace Project_TD2_2
