#pragma once

#include <string_view>

#include "HIKARI/Project/GameComponent.h"
#include "BossAnimBody.h"
#include "BossBodySettings.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossBodyType = "Project_TD2_2.BossBody";

// ボスの体の見た目の部品（c-bd1a7a でボスの部品から分けた）：ボスと同じ物体に付ける。骨格の体を流し、体に付く効果
// （胴の煙・火の舌・刀光・目・手の光・フェーズ 2 の場）を出す。見た目だけの部品で、固定 tick の規則は読まない
// （規則が使う骨格の体の物体・クリップの刀はボスの部品の animBody・clipSwords）。無ければ体を出さない。
// 進めるのはボスの System（BossSystem の LateUpdate・PreRender。前と同じ順に呼ぶ）。
class BossBodyComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    BossBodySettings settings;
    BossBodyState state;
    BossAnimBodyState anim;
};

// 体の見た目の設定（部品が無ければ既定の設定）。登場の効果など、体の部品が無くても出すものが読む。
const BossBodySettings& BossBodySettingsOf(const BossBodyComponent* look);

} // namespace Project_TD2_2
