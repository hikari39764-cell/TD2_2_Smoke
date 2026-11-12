#include "BossCloneComponent.h"

namespace Project_TD2_2 {

void BossCloneComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("ボスの分身", true)) {
        fields.Float("parkDepth", "出ていない間にしまう深さ (m、地面から下へ)", parkDepth, 1.0f, 1000.0f);
        fields.Note("霞分身の 1 体。ボスと同じ見た目のモデルと「被弾と HP」を同じ物体に付けて、シーンに置いておきます。"
                    "出す数はボスの技の表（霞分身の判定サイズ）で、置いた物体が足りなければある分だけ出ます。"
                    "分身の再使用・消えるまでの時間はボスの項目です。");
    }
}

} // namespace Project_TD2_2
