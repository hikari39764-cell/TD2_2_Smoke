#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyRig.h"
#include "BossBodyComponent.h"
#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

// 登場の見た目（c-e0c500）：演技の時計を進め、役者（BossIntroActor。骨格の動画で演じる体）をその時刻の姿にする。演技の
// 前と間は、骨格の体は出さず、体に付く効果の置き場所だけを役者の骨から入れる。段と tick は規則
// （BossIntro）が進める。効果の拍は BossIntroEffects が鳴らす。
class BossIntroVisuals final {
public:
    // 描画のフレームごとに、演技の時計を進めて役者を動かす（効果の置き場所を決める前に呼ぶ）。演出の資産を流している間は、その
    // 資産の時刻に合わせる（エンジンが資産を進めるのは systems の後なので 1 フレーム遅れる）。演技が済んだら役者を隠す。
    // body は体の見た目の設定。
    static void Advance(HIKARI::WorldView& world, const BossBody& body, BossIntro& intro,
                        float seconds);
    // 役者が出ている間（演技の前・演技の間）。この間、骨格の体は出さない。
    static bool Showing(const BossIntro* intro);
    // 見た目の体の変換：役者が出ている間は、役者の腰の真下の足元と向き（胴の煙・気配・衝撃の輪など、体に付く効果が
    // 演技に付いて行く）。ほかは body のまま。body は体の物体の変換（原点は体の中心）で、返す変換も同じ約束。
    static HIKARI::GameProjectTransform BodyTransform(const Boss& boss, const BossIntro* intro,
                                                      const HIKARI::GameProjectTransform& body);
    // 体に付く効果の置き場所（pose）のうち、体に付く効果が使う置き場所（兜・刀・胴）を、役者の骨のものに直す。効果の拍も
    // ここで進める（seconds＝このフレームの秒）。
    static void ApplyPose(Boss& boss, BossIntro& intro, BossBodyPose& pose, float seconds);
    // 胴の煙の量に掛ける倍率（散らばって待つ間は 0＝出さない。演技の間から普段の量）。
    static float SmokeScale(const BossIntro* intro);
    // 編集器のプレビュー（Play の外）：演出ワークスペースがこのボスの登場の資産を流していれば、役者をその時刻の姿に
    // する（流していない・別の資産なら、触ったものを元へ戻す）。
    static void Preview(HIKARI::WorldView& world, const BossBody& body, BossIntro& intro);
};

} // namespace Project_TD2_2
