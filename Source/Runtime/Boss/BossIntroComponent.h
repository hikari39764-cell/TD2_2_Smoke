#pragma once

#include <string_view>

#include "HIKARI/Project/GameComponent.h"
#include "BossIntroSettings.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossIntroType = "Project_TD2_2.BossIntro";

// ボスの登場の部品（c-eb3b08 でボスの部品から分けた）：ボスと同じ物体に付ける。無ければ登場をしない（登場を切った時と
// 同じ。初めから組み上がって立っている）。進めるのはボスの System（BossSystem。ボスの tick の中で、前と同じ順に呼ぶ）、
// 規則は BossIntro、見た目は BossIntroVisuals・BossIntroActor・BossIntroSmoke・BossIntroEffects・BossIntroCamera。
// 登場とフェーズの変わり目の演出が共通で使う効果の口は、ボスの部品の BossCinematicFx に残す。
class BossIntroComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    BossIntroSettings settings;
    BossIntroState state;
};

} // namespace Project_TD2_2
