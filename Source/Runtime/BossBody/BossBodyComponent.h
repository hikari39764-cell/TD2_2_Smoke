#pragma once

#include <string_view>

#include "HIKARI/Game.h"
#include "BossAnimBody.h"
#include "BossBodySettings.h"

namespace Project_TD2_2 {

// ボスの体の見た目の部品（c-bd1a7a でボスの部品から分けた）：ボスと同じ物体に付ける。骨格の体を流し、体に付く効果
// （胴の煙・火の舌・刀光・目・手の光・フェーズ 2 の場）を出す。見た目だけの部品で、固定 tick の規則は読まない
// （規則が使う骨格の体の物体・クリップの刀はボスの部品の animBody・clipSwords）。無ければ体を出さない。
// 進めるのはボスの System（BossSystem の LateUpdate・PreRender。前と同じ順に呼ぶ）。
struct BossBody : HIKARI::Component {
    HIKARI_COMPONENT(BossBody, "ボスの体の見た目")
    // 描画のフレームで、体と体に付く効果（胴の煙・突進の尾など）を前の tick とこの tick の間で補間して置く（エンジンが
    // 体を描くのと同じ。切ると tick の位置に置く：描画が tick より速い時、速い動きでがくつく）。
    FIELD(bool, bodyInterpolate, true, "体を前の tick との間で補間して置く（描画が tick より速い時のがくつきを消す）", HIKARI::Group("体（大きさ・胴の煙と火・フェーズ 2 の場）"))
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。見える刀の長さ（swordVisibleLength）に合うように拡大する。
    // 模型を作るスクリプトが出す値（KATANA_LENGTH）に合わせる。
    FIELD(float, bodySwordModelLength, 2.3411f, "刀の模型の、握りの中央から剣先までの長さ (m。見える刀の長さに合うよう拡大する)", HIKARI::Range(0.1f, 20.0f))
    // 見える刀の、剣の付け根（Boss.swordShoulder）から剣先までの長さ。刀も手もこの長さに合わせて大きくなる。
    // 当たりの線（Boss.swordLength＋swordHazeLength）より先へは伸ばさない（当たらない刃を見せない）。0 なら Boss.swordLength。
    FIELD(float, bodySwordVisibleLength, 5.13f, "見える刀の長さ：剣の付け根から剣先まで (m。刀と手の大きさが決まる。当たりの線より先へは伸びない。0 で剣の長さ)", HIKARI::Range(0.0f, 20.0f))
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    FIELD(float, bodySwordGripAlong, 0.0945f, "握りの位置：剣の付け根から剣の向きへ (m)", HIKARI::Range(-1.0f, 2.0f))
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    FIELD(float, bodyHandScale, 1.25f, "手の大きさ（刀の拡大に掛ける倍率）", HIKARI::Range(0.1f, 5.0f))
    // 体の大きさ（倍率）：鎧（胸当て・兜・肩当て・草摺）に付く効果の大きさに掛ける（下の位置の値は 1 倍の体での値）。体を大きくする時は、Boss.bodyHalfHeight・bodyRadius・シーンのカプセル・
    // 胴の煙のエフェクト（make_body_smoke.py の scale）も一緒に変える（調整ガイド g-fe0396 の一覧）。
    // 1.6875＝身長 6.75 m（c-a30eaf で 1.875、c-e97b18 でその 0.9 倍）。
    FIELD(float, bodyScale, 1.6875f, "体の大きさ（倍率。鎧に付く効果の大きさに掛ける。当たりの型には掛けない）", HIKARI::Range(0.2f, 5.0f))
    // 籠手の筒の模型の長さ（手首から先まで、模型の実寸 m。模型を作るスクリプトの build_cuff の length）。
    FIELD(float, bodyCuffModelLength, 0.62f, "籠手の筒の模型の長さ (m。模型を作るスクリプトの値)", HIKARI::Range(0.05f, 5.0f))
    FIELD(float, bodyHeadScale, 1.16f, "鎧：兜の大きさ（倍率）", HIKARI::Range(0.2f, 4.0f))
    // 草摺（ノード KusazuriPanel）：枚数、吊る高さ、腰の輪郭の半径（横・前後）、裾を外へ開く角度。上の縁は胸当ての
    // 下の縁の内に隠す（c-584825）。いまは外してある（0 枚。揺れ・めり込みの不具合が多いため。直すまで出さない。
    // 0 の間は、登場の役者の草摺も隠す）。戻す時は 7。
    FIELD(int, bodyTassetCount, 0, "鎧：草摺の枚数", HIKARI::Range(0, kBossBodyTassetCapacity))
    // 胴の煙：ボスの足元に付いて動く流体のエフェクト（鎧の中を満たし、裾から流れ落ちる）。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodySmokeEffect, (HIKARI::VfxAsset{"7c41e9d2a05b4f6e9b3d8a1f2c6e5d04"}), "胴の煙のエフェクト（ボスの足元に付く流体。空なら出さない）")
    // 煙の量（倍率）と、フェーズ 2 で煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）。
    // （c-9143b5：視界を塞がないよう 1.0 → 0.8。）
    FIELD(float, bodySmokeAmount, 0.8f, "胴の煙の量（倍率）", HIKARI::Range(0.0f, 8.0f))
    // （c-857ae3：胴の煙の発光を強めて火の色にしたので 5 → 1.6。芯の煙は暗い橙の熾火の色に留まり（鎧を覆う大きな光に
    // ならない）、炎は襟と袖口の火の舌が受け持つ。）
    FIELD(float, bodySmokeHeatPhaseTwo, 1.6f, "フェーズ 2 で胴の煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）", HIKARI::Range(0.0f, 64.0f))
    // 気絶している間の煙の量（倍率。煙が薄れて、鎧がからっぽに見える）。
    FIELD(float, bodySmokeAmountStunned, 0.35f, "気絶の間の胴の煙の量（倍率）", HIKARI::Range(0.0f, 8.0f))
    // 骨に付いて行く煙（c-271cba）：骨格の体の、腰から胸への背骨に沿う点と、両腕（肩 → 肘 → 袖口）に沿う点から、胴の煙の
    // 域へ煙を入れる。体が傾く・屈む・腕を振る時に、煙が鎧に付いて行き、胴と手の間（腕）が煙でつながる。背骨・腕の
    // 毎秒の濃さ（0 で出さない）と半径（1 倍の体での m）、腕の煙が手の方へ流れる速さ (m/秒)。
    FIELD(float, bodySpineSmoke, 26.0f, "骨に付いて行く煙：背骨（腰から胸）の濃さ（毎秒。0 で出さない）", HIKARI::Range(0.0f, 500.0f))
    FIELD(float, bodySpineSmokeRadius, 0.30f, "骨に付いて行く煙：背骨の半径（1 倍の体での m）", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, bodyArmSmoke, 34.0f, "骨に付いて行く煙：両腕（肩から袖口）の濃さ（毎秒。0 で出さない）", HIKARI::Range(0.0f, 500.0f))
    FIELD(float, bodyArmSmokeRadius, 0.17f, "骨に付いて行く煙：腕の半径（1 倍の体での m）", HIKARI::Range(0.02f, 2.0f))
    // 胴の煙を体の傾きに付ける（フェーズの変わり目の演出の間。c-271cba）：胴の煙の域を、腰を軸にして背骨の向き
    // （腰 → 胸）へ倒す。smokeTilt は付いて行く割合（0 倒さない 〜 1 背骨の向きまで）、smokeTiltMaxDegrees は倒す角度の
    // 上限（度）、smokeTiltFollow は寄せる速さ（1/秒。大きいほど体の動きにすぐ付く）、smokeHipsHeight は胴の煙の資産の
    // 中の腰の高さ（足元から m。立ち姿の腰の骨の高さ。ここを軸に倒す）。
    FIELD(float, bodySmokeTilt, 1.0f, "胴の煙を体の傾きに付ける割合（変わり目の演出の間。0 倒さない 〜 1 背骨の向きまで）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodySmokeTiltMaxDegrees, 62.0f, "胴の煙を倒す角度の上限（度）", HIKARI::Range(0.0f, 85.0f))
    FIELD(float, bodySmokeTiltFollow, 7.0f, "胴の煙が体の傾きに寄る速さ（1/秒）", HIKARI::Range(0.5f, 60.0f))
    // 体がほとんど動いていない間（座り込み・待機）に、域が腰と背骨の細かい揺れへ寄る速さ（1/秒。小さいほど煙が
    // 落ち着く。大きく動いた時は smokeTiltFollow で追う）。
    FIELD(float, bodySmokeCalmFollow, 0.7f, "胴の煙が、体の細かい揺れへ寄る速さ（1/秒。小さいほど煙が落ち着く）", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, bodySmokeHipsHeight, 2.9f, "胴の煙の資産の中の腰の高さ（足元から m。ここを軸に倒す）", HIKARI::Range(0.0f, 8.0f))
    // 戦いの間（骨格の体が出ている間）も、胴の煙の域を背骨へ倒す割合（0 倒さない＝立ったまま 〜 1。屈む・のけぞる体に、
    // 煙の柱が付いて行く）と、その間の寄せる速さ（1/秒。技の屈みは速いので、演出の smokeTiltFollow より速く追う）。
    FIELD(float, bodySmokeTiltCombat, 1.0f, "戦いの間も、胴の煙を体の傾きに付ける割合（0 立ったまま 〜 1 背骨の向きまで）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodySmokeTiltCombatFollow, 12.0f, "戦いの間、胴の煙が体の傾きに寄る速さ（1/秒）", HIKARI::Range(0.5f, 60.0f))
    FIELD(float, bodyArmSmokeFlow, 0.8f, "骨に付いて行く煙：腕の煙が手の方へ流れる速さ (m/秒)", HIKARI::Range(0.0f, 10.0f))
    // フェーズ 2 の本体の鎧を、灼けた鎧（継ぎ目の糸と隙間の闇が熾火に光る模型のノード ○○Hot）に差し替える（c-857ae3）。
    FIELD(bool, bodyArmorHot, true, "フェーズ 2 の本体の鎧を灼けた鎧（継ぎ目と隙間が熾火に光る）に差し替える")
    // フェーズ 2 の燃える胴（c-857ae3）：フェーズ 2 に入ると fireRiseSeconds かけて火が付き、撃破で同じ速さで消える。
    // 熱さは smokeHeatPhaseTwo へ寄せ、煙の量は smokeAmountPhaseTwo 倍、火の粉と熱のゆがみ（胴の煙の資産の入力 burn）は
    // fireBurn 倍。気絶の間は火を fireStunnedShare の割合に弱める。
    FIELD(float, bodyFireRiseSeconds, 1.5f, "フェーズ 2 の燃える胴：火が付く・消える時間 (秒)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodySmokeAmountPhaseTwo, 1.0f, "フェーズ 2 の燃える胴：胴の煙の量（倍率）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, bodyFireBurn, 1.0f, "フェーズ 2 の燃える胴：火の粉と熱のゆがみの量（倍率。0 で出さない）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, bodyFireStunnedShare, 0.35f, "フェーズ 2 の燃える胴：気絶の間の火の強さ（割合）", HIKARI::Range(0.0f, 1.0f))
    // 火の舌：襟の両脇（胴の中心から、1 倍の体の局所 m。右。左は左右を反転）と、籠手の筒の口（手首 0 〜 肘の側 1 の
    // 割合 fireCuffAlong）から、燃料を上へ吹き出す（胴の煙の域だけに届く注入。昇りながら燃えて光る煤になる）。濃さは
    // 1 秒に入れる燃料（中心）、半径は 1 倍の体の m（体の大きさを掛ける）、熱さは入れる燃料の温度（0.3 で燃え始める）、
    // 速さは上へ m/秒。
    // 袖口の火は、襟の火に fireCuffShare を掛けた強さ。襟の火は片側に 2 本、前後に fireCollarSpread（1 倍の体の m）だけ
    // 離して並べる（太い 1 本より、細い本数で見せる方が大きさが揺れない）。
    FIELD(HIKARI::Vec3, bodyFireCollar, (HIKARI::Vec3{-0.36f, 0.60f, -0.06f}), "火の舌：襟の口（胴の中心から、1 倍の体の局所 m。右。左は左右を反転）")
    FIELD(float, bodyFireCollarSpread, 0.1f, "火の舌：襟の片側 2 本の前後の間隔 (1 倍の体の m。0 で 1 本に重なる)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyFireCuffAlong, 0.85f, "火の舌：袖口の位置（籠手の筒の手首 0 〜 肘の側の口 1）", HIKARI::Range(0.0f, 1.5f))
    FIELD(float, bodyFireTongueDensity, 22.0f, "火の舌：1 秒に入れる燃料の濃さ（襟。0 で出さない）", HIKARI::Range(0.0f, 500.0f))
    FIELD(float, bodyFireTongueRadius, 0.15f, "火の舌：入れる半径 (1 倍の体の m。体の大きさを掛ける)", HIKARI::Range(0.02f, 2.0f))
    FIELD(float, bodyFireTongueHeat, 2.0f, "火の舌：入れる燃料の温度（0.3 で燃え始める。燃えて熱くなった煤が 1.5 から光り、4.5 で橙黄）", HIKARI::Range(0.0f, 50.0f))
    FIELD(float, bodyFireTongueRise, 2.0f, "火の舌：上へ吹き出す速さ (m/秒)", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, bodyFireCuffShare, 0.8f, "火の舌：袖口の強さ（襟に対する割合。0 で出さない）", HIKARI::Range(0.0f, 4.0f))
    // 腰の周りの火の舌（c-280b71。3 本：前の左右と後ろ）：胴の煙の柱の中を、下から火が上がる（体そのものが燃えて
    // 見える）。襟の火に対する強さ（0 で出さない）、半径の倍率、腰からの距離（1 倍の体の m）と、腰から下げる高さ（同）。
    FIELD(float, bodyFireHemShare, 0.7f, "火の舌：腰の周りの 3 本の強さ（襟に対する割合。0 で出さない）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, bodyFireHemRadius, 1.5f, "火の舌：腰の周りの火の半径（襟の火の半径に対する倍率）", HIKARI::Range(0.2f, 4.0f))
    FIELD(float, bodyFireHemRing, 0.42f, "火の舌：腰の周りの火を置く、腰からの距離 (1 倍の体の m)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, bodyFireHemDrop, 0.35f, "火の舌：腰の周りの火を、腰から下げる高さ (1 倍の体の m)", HIKARI::Range(0.0f, 2.0f))
    // フェーズ 2 の場（c-857ae3）：場の色（環境の色調整を、暗部・中間・明部の色味で暖かく置き換える。1 で変えない）が
    // fieldWarmSeconds かけて変わり、撃破で戻る。漂う火の粉はボスの足元に付けて出し（量は fieldEmbers 倍）、焦げた輪は
    // 変わり目にボスが立っていた所に fieldScorchSeconds かけて現れる（濃さ fieldScorch・熾火の明るさ fieldScorchGlow・
    // 大きさ fieldScorchSize 倍）。エフェクトが空なら火の粉と焦げた輪を出さない。
    FIELD(HIKARI::VfxAsset, bodyFieldEffect, (HIKARI::VfxAsset{"5be8f2a1c94d4e7b8a3f61d2c7e09b54"}), "フェーズ 2 の場：漂う火の粉と焦げた輪のエフェクト（空なら出さない）")
    FIELD(float, bodyFieldWarmSeconds, 4.0f, "フェーズ 2 の場：場の色が変わる・戻る時間 (秒)", HIKARI::Range(0.0f, 30.0f))
    FIELD(HIKARI::Vec3, bodyFieldShadowsTint, (HIKARI::Vec3{1.18f, 0.92f, 0.76f}), "フェーズ 2 の場の色：暗部の色味（倍率。1 で変えない）")
    FIELD(HIKARI::Vec3, bodyFieldMidtonesTint, (HIKARI::Vec3{1.2f, 0.98f, 0.76f}), "フェーズ 2 の場の色：中間の色味（倍率。1 で変えない）")
    FIELD(HIKARI::Vec3, bodyFieldHighlightsTint, (HIKARI::Vec3{1.08f, 0.98f, 0.86f}), "フェーズ 2 の場の色：明部の色味（倍率。1 で変えない）")
    FIELD(float, bodyFieldEmbers, 1.0f, "フェーズ 2 の場：漂う火の粉の量（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyFieldScorch, 1.0f, "フェーズ 2 の場：焦げた輪の濃さ（0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    // （stageB：熾火の橙の斑は「燃える地面」と紛れるので、既定は出さない。焦げは黒い斑だけ。）
    FIELD(float, bodyFieldScorchGlow, 0.0f, "フェーズ 2 の場：焦げた輪に残る熾火の明るさ（倍率）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyFieldScorchSize, 1.0f, "フェーズ 2 の場：焦げた輪の大きさ（倍率。1 で直径 約 9 m）", HIKARI::Range(0.1f, 4.0f))
    FIELD(float, bodyFieldScorchSeconds, 2.5f, "フェーズ 2 の場：焦げた輪が現れる時間 (秒)", HIKARI::Range(0.0f, 30.0f))
    NOTE("体は骨格の体（「骨格の体」の節）が描きます。ここの値は、体に付く効果（刀光・胴の煙・火の舌・兜の光）の"
                "大きさと置き場所です。剣の長さは Boss.swordLength です。")
    // 刀に付くエフェクト（刀光の帯と、燃える刀身の炎・ゆがみ・光）。見える刀の刃に付けて出し続ける。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyBladeEffect, (HIKARI::VfxAsset{"9d2c47e1b6a3405f8e17c5d0a4b2f693"}), "刀に付くエフェクト（刀光の帯と、燃える刀身の炎・ゆがみ・光。空なら出さない）", HIKARI::Group("体：技の演出（見た目だけ）"))
    NOTE("技の点描の演出です。当たり・F 数は変えません。色・明るさ・消える時間は、演出の値の資産と各エフェクトの中に"
                "あります。")
    // エフェクトを付ける刃の始まり（握りから剣先までの割合。鍔のすぐ先）。
    FIELD(float, bodyBladeFromShare, 0.14f, "刀に付くエフェクト：刃の始まり（握りから剣先までの割合）", HIKARI::Range(0.0f, 0.9f))
    // 刀光：振っている間、刃が通った面に光る帯とゆがみを出す。
    FIELD(bool, bodyTrail, true, "刀光：振りの間、刃が通った面に光る帯とゆがみを出す")
    // 命中の窓が閉じた後も、振り抜きの間は出し続ける時間 (秒) と、その間に要る剣先の速さ (m/秒)。
    FIELD(float, bodyTrailFollowSeconds, 0.08f, "刀光：命中の窓の後も、振り抜きの間は出し続ける時間 (秒)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyTrailMinTipSpeed, 25.0f, "刀光：振り抜きの間に出すのに要る剣先の速さ (m/秒)", HIKARI::Range(0.0f, 100.0f))
    // 剣先が地面からこの高さ (m) より下へ来たら、刀光を切る。
    FIELD(float, bodyTrailGroundCut, 0.35f, "刀光：剣先が地面からこの高さより下へ来たら切る (m)", HIKARI::Range(0.0f, 3.0f))
    // 刀光の色と明るさ（1 を超えると光る）：霞の技（刀が燃えていない）と、刀が燃えている技。
    FIELD(HIKARI::Vec3, bodyTrailMistColor, (HIKARI::Vec3{0.80f, 1.20f, 2.20f}), "刀光：霞の技の色と明るさ（1 を超えると光る）")
    FIELD(HIKARI::Vec3, bodyTrailFireColor, (HIKARI::Vec3{2.40f, 1.00f, 0.30f}), "刀光：刀が燃えている技の色と明るさ")
    // 振りのゆがみ（屈折）の強さ（画面の 6% × この値ずらす。0.3 を超えると段が見える。0 で出さない）。刀光と同じ間に出し、
    // 刀光の色とは別なので、霞の技でも火の技でも同じ強さ。出たゆがみはエフェクト側の時間（0.4 秒）で消える。
    FIELD(float, bodySwingHaze, 0.3f, "振りのゆがみ（屈折）の強さ（0.1〜0.3。0.3 を超えると段が見える。0 で出さない）", HIKARI::Range(0.0f, 0.5f))
    // 振りのゆがみを、刀光より前から出し始める剣先の速さ (m/秒)。刀を振る技の打ち込み（攻撃中）の間だけ見る（構えへ上げる
    // 振りかぶりも同じくらい速いが、そこでは出さない）。0 で刀光と同じ間だけ。
    FIELD(float, bodySwingHazeMinTipSpeed, 30.0f, "振りのゆがみ：刀光より前から出し始める剣先の速さ (m/秒。打ち込みの間だけ。0 で刀光と同じ間)", HIKARI::Range(0.0f, 300.0f))
    // 燃える刀身：炎の明るさ（倍率。0 で出さない）と、火が付く・消える時間 (秒)。
    FIELD(float, bodyBladeHeat, 1.0f, "燃える刀身：炎の明るさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyBladeHeatSeconds, 0.25f, "燃える刀身：火が付く・消える時間 (秒)", HIKARI::Range(0.01f, 3.0f))
    // 燃える刀身から立つ火の粉の量（倍率。刀のエフェクトの入力 embers。0 で出さない。c-857ae3）。
    FIELD(float, bodyBladeEmbers, 1.0f, "燃える刀身：刀身から立つ火の粉の量（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    // 燃える刀身の流体の炎（c-3c908c）：刃に付いて動く局所空間の流体域を持つエフェクト（空なら出さず、シェーダーの炎の帯の
    // まま）。燃料の量（倍率）と、流体の炎を出している間に残す、シェーダーの炎の帯の割合・刀身の光の層の割合。
    FIELD(HIKARI::VfxAsset, bodyBladeFireEffect, {}, "燃える刀身：流体の炎のエフェクト（刃に付いて動く流体域。空ならシェーダーの炎の帯だけ）")
    FIELD(float, bodyBladeFireAmount, 1.0f, "…流体の炎の燃料の量（倍率。0 で出さない）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, bodyBladeFlameShare, 0.25f, "…流体の炎を出している間に残す、シェーダーの炎の帯の割合（0 で消す）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyBladeFireGlowShare, 0.8f, "…流体の炎を出している間に残す、刀身の光の層の割合", HIKARI::Range(0.0f, 1.0f))
    // …速く振る間は、流体の炎が刃から剥がれて後ろへ流れる。剣先がこの速さ (m/秒) に近づくほど、刃に貼り付くシェーダーの
    // 炎の帯を元の強さへ戻し（0 で戻さない）、止まったら bladeFlameReturnSeconds で bladeFlameShare へ下げる。
    FIELD(float, bodyBladeFlameFastSpeed, 25.0f, "…剣先がこの速さ (m/秒) に近づくほど、シェーダーの炎の帯を元の強さへ戻す（流体の炎は速い振りで剥がれる。"
        "0 で戻さない）", HIKARI::Range(0.0f, 200.0f))
    FIELD(float, bodyBladeFlameReturnSeconds, 0.35f, "…振り終わった後、シェーダーの炎の帯を下げて流体の炎へ渡す時間 (秒)", HIKARI::Range(0.01f, 3.0f))
    // 目の光：兜に付くエフェクト（頭が動くと尾を引く光・暈・技を出すと決めた瞬間に横へ伸びる閃き）。空なら出さない。
    // 光の位置はエフェクト資産の中（面頬の真ん中の隙間の前の 1 点。make_kasumi.py が出す EYE_POINT と合わせる）。
    FIELD(HIKARI::VfxAsset, bodyEyesEffect, (HIKARI::VfxAsset{"a4f1c8e27b5d4390b61e2c9d3f7a0851"}), "目の光のエフェクト（兜に付く。尾を引く光・暈・閃き。空なら出さない）")
    // 目の光の色と明るさ：ふだん（霞）と、フェーズ 2（火）。
    FIELD(HIKARI::Vec3, bodyEyesMistColor, (HIKARI::Vec3{0.70f, 1.10f, 1.90f}), "目の光：ふだんの色と明るさ")
    FIELD(HIKARI::Vec3, bodyEyesFireColor, (HIKARI::Vec3{3.60f, 1.30f, 0.30f}), "目の光：フェーズ 2 の色と明るさ")
    // 閃きの強さと、消えるまでの時間 (秒)。0 で出さない。
    FIELD(float, bodyEyeFlare, 0.6f, "目の閃き：ふつうの技を出すと決めた瞬間の強さ（0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyEyeFlareSeconds, 0.16875f, "目の閃き：消えるまでの時間 (秒)", HIKARI::Range(0.01f, 3.0f))
    // 兜の中の灯（c-e0c500）：面頬の隙間から漏れる光を、兜の中に置いた本物の灯で出す。前を向く影付きのスポットライト
    // （隙間から漏れて、面頬の縁・襟・顔の前の手を照らす）と、兜の中を満たす弱い点光（影なし）。どちらもシーンの物体で、
    // 置き場所と明るさはコードが毎フレーム入れる（色は上の目の光の色）。物体が無ければ、前の貼り付けの光（eyesEffect）を
    // 出す。明るさは灯の物体の単位（場面の光に対する倍率）。置き場所は兜の大きさ 1 の時の局所の値（+Z が前）。スポットは
    // 少し下へ向ける（度）。閃き（技を決めた瞬間・登場で目が灯る瞬間）は明るさに足す。ゆらぎは明るさに掛ける幅。
    FIELD(HIKARI::Entity, bodyHelmSpotObject, {}, "兜の中の灯：前を向くスポットライトの物体（無ければ貼り付けの光を出す）")
    FIELD(HIKARI::Entity, bodyHelmFillObject, {}, "兜の中の灯：兜の中を満たす点光の物体")
    // 面頬の隙間の中の光（模型に足した自発光の面 HelmGlow。Tools/BossBody/add_helm_glow.py）：登場の間の明るさ（画面
    // 基準：色 × この値の 1 が、露出の後の白。昼でも夜でも同じ明るさに見える）。閃きとゆらぎは、灯と同じ倍率が掛かる。
    // 戦いの間の明るさと色は、模型の材質の値（add_helm_glow.py の GLOW_*・HOT_*）。
    FIELD(float, bodyHelmGlowStrength, 3.0f, "面頬の隙間の中の光：登場の間の明るさ（画面基準。1 で白）", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, bodyHelmSpotIntensity, 3.0f, "兜の中の灯：スポットライトの明るさ（場面の光に対する倍率）", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, bodyHelmFillIntensity, 0.25f, "兜の中の灯：点光の明るさ（場面の光に対する倍率）", HIKARI::Range(0.0f, 100.0f))
    FIELD(HIKARI::Vec3, bodyHelmSpotLocal, (HIKARI::Vec3{0.0f, 0.03f, -0.06f}), "兜の中の灯：スポットライトの置き場所（兜の局所。+Z が前）")
    FIELD(HIKARI::Vec3, bodyHelmFillLocal, (HIKARI::Vec3{0.0f, 0.03f, 0.04f}), "兜の中の灯：点光の置き場所（兜の局所）")
    FIELD(float, bodyHelmSpotPitchDegrees, 4.0f, "兜の中の灯：スポットライトを下へ向ける角度（度）", HIKARI::Range(-45.0f, 45.0f))
    FIELD(float, bodyHelmFlareGain, 1.0f, "兜の中の灯：閃きを明るさに足す倍率", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodyHelmLightFlicker, 0.10f, "兜の中の灯：ゆらぎの幅（0 で揺らがない）", HIKARI::Range(0.0f, 1.0f))
    // 刃口の常時の光（刀に付くエフェクトの Edge Line・Edge Glow。入力 input.edge）：刀を見れば何をしているか分かるように、
    // 段階で明るさを変える（倍率。色は edgeMistColor／edgeFireColor に掛ける）。待機 edgeIdle は弱いが必ず見える、前隙は
    // 進みにつれて edgeWindup まで（曲がり edgeRampPower、ゆっくり脈打つ）、
    // 振る間（命中の窓）は edgeSwing、後隙は待機へ戻る。気絶は edgeStunned。燃える刀身の間は edgeHotShare を掛ける
    // （炎と重ねて白く飛ばない）。地面や画面の予告ではなく、刀そのものの見え方。
    FIELD(HIKARI::Vec3, bodyEdgeMistColor, (HIKARI::Vec3{0.55f, 0.85f, 1.60f}), "刃口の光：霞の色と明るさ（段階の倍率を掛ける）")
    FIELD(HIKARI::Vec3, bodyEdgeFireColor, (HIKARI::Vec3{1.80f, 0.75f, 0.22f}), "刃口の光：刀が燃えている時の色と明るさ")
    FIELD(float, bodyEdgeIdle, 0.35f, "刃口の光：待機の明るさ（倍率。弱いが必ず見える）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyEdgeWindup, 0.9f, "刃口の光：前隙の終わり（構え切った所）の明るさ", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyEdgeSwing, 1.6f, "刃口の光：振る間（命中の窓）の明るさ", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyEdgeStunned, 0.15f, "刃口の光：気絶の間の明るさ", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyEdgeRampPower, 1.5f, "刃口の光：前隙の立ち上がりの曲がり（1 で一定、大きいほど終わりに急に明るく）", HIKARI::Range(0.2f, 6.0f))
    FIELD(float, bodyEdgePulseDepth, 0.25f, "刃口の光：前隙の脈の深さ（0〜1。0 で脈打たない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyEdgePulseFrequency, 1.4f, "刃口の光：前隙の脈の速さ（1 秒あたりの回数）", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodyEdgeHotShare, 0.6f, "刃口の光：燃える刀身の間に掛ける割合 (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyEdgeRiseSeconds, 0.05f, "刃口の光：明るくなる時間 (秒)", HIKARI::Range(0.005f, 2.0f))
    FIELD(float, bodyEdgeFallSeconds, 0.15f, "刃口の光：暗くなる時間 (秒)", HIKARI::Range(0.005f, 2.0f))
    // 刃のまわりの暗い縁（Edge Dark。入力 input.edgeDark の A）の濃さ（倍率。0 で出さない）。明るい背景（炎・夕方の空・
    // 白い霧）の前で、明るい線だけだと刀の形が消えるので、外側に煙の黒の柔らかい帯を重ねる。
    FIELD(float, bodyEdgeDark, 1.0f, "刃のまわりの暗い縁：濃さ（倍率。0 で出さない）", HIKARI::Range(0.0f, 2.0f))
    // 霞の技と実体の刀の技の見分け（c-ecd963）：剣気を飛ばす技（表の当て方が実でなく、斬撃速度が 0 より大きい）の前隙〜
    // 振りの間は、刀身の輪郭の光（材質）と刃口の光をいつも通り灯し、刃文の流れ・霞の帯を hazeBoost 倍に強める（刀身が
    // 青白く明るい）。実体の刀だけで当てる技の間は、輪郭の光を realRimShare、刃口の光を realEdgeShare、流れ・霞を
    // realFlowShare 倍に落とす（暗い縁と刃文だけ）。後隙・待機・歩きはどちらでもない。
    FIELD(float, bodyHazeBoost, 1.8f, "霞の技（剣気を飛ばす）の間、刃文の流れ・霞の帯を強める倍率", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyRealEdgeShare, 0.3f, "実体の刀の技の間、刃口の光に掛ける割合", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodyRealFlowShare, 0.35f, "実体の刀の技の間、刃文の流れ・霞の帯に掛ける割合", HIKARI::Range(0.0f, 1.0f))
    // 刀の常時の層（BossBlade.hvfx の Hamon Flow・Blade Mist・Tip Wake）：刃文に沿って鍔元から剣先へ流れる光（input.flow）、
    // 刀身をうっすら包む霞の帯（input.mist）、剣先の 3 分の 1 が通った所に残る残光（input.wake）。色は霞（フェーズ 1）と
    // 火（燃える刀身・フェーズ 2）の 2 つ、明るさは待機の倍率（霞の技の間は hazeBoost を掛ける）。残光は剣先の速さが
    // wakeMinTipSpeed から wakeFullTipSpeed (m/秒) の間で 0→1（速いほど明るく、帯の長さは資産の履歴の秒数）。
    FIELD(HIKARI::Vec3, bodyFlowMistColor, (HIKARI::Vec3{0.55f, 0.90f, 1.50f}), "刃文の流れ：霞の色と明るさ")
    FIELD(HIKARI::Vec3, bodyFlowFireColor, (HIKARI::Vec3{1.70f, 0.70f, 0.20f}), "刃文の流れ：刀が燃えている時の色と明るさ")
    FIELD(float, bodyFlowIdle, 1.2f, "刃文の流れ：明るさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(HIKARI::Vec3, bodyMistMistColor, (HIKARI::Vec3{0.70f, 0.85f, 1.10f}), "霞の帯：霞の色と明るさ")
    FIELD(HIKARI::Vec3, bodyMistFireColor, (HIKARI::Vec3{1.30f, 0.55f, 0.20f}), "霞の帯：刀が燃えている時の色と明るさ")
    FIELD(float, bodyMistIdle, 1.0f, "霞の帯：明るさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(HIKARI::Vec3, bodyWakeMistColor, (HIKARI::Vec3{0.60f, 0.90f, 1.60f}), "剣先の残光：霞の色と明るさ")
    FIELD(HIKARI::Vec3, bodyWakeFireColor, (HIKARI::Vec3{1.80f, 0.75f, 0.22f}), "剣先の残光：刀が燃えている時の色と明るさ")
    FIELD(float, bodyWakeGain, 1.2f, "剣先の残光：明るさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyWakeMinTipSpeed, 3.0f, "剣先の残光：出始める剣先の速さ (m/秒)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, bodyWakeFullTipSpeed, 30.0f, "剣先の残光：いちばん明るくなる剣先の速さ (m/秒)", HIKARI::Range(0.1f, 200.0f))
    // 剣先が地面（刀光を切る高さ trailGroundCut）からこの高さ (m) の間は、残光を弱める（地面に白い筋を引かない）。
    FIELD(float, bodyWakeGroundFade, 1.0f, "剣先の残光：地面の近くで弱める高さ (m)", HIKARI::Range(0.0f, 10.0f))
    // 剣気を放つ瞬間（c-ecd963）：刀身を走る光（glintEffect）をもう一度、色を releaseGlint 倍にして出す。
    // releaseHoldTicks F の間、時間を releaseHoldScale 倍にする（全体の止め。0 F で止めない＝既定。ボスが振るたびに
    // プレイヤーの操作まで遅くなるので、実機で試して決める）。刀だけを止めると、止めの後に刀が跳んで追い付いた。
    FIELD(float, bodyReleaseGlint, 1.6f, "剣気を放つ瞬間：刀身を走る光の明るさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    FIELD(int, bodyReleaseHoldTicks, 0, "剣気を放つ瞬間：時間を止める長さ (F。0 で止めない)", HIKARI::Range(0, 10))
    FIELD(float, bodyReleaseHoldScale, 0.35f, "剣気を放つ瞬間：止める間の時間の倍率 (0〜1)", HIKARI::Range(0.0f, 1.0f))
    // 刀身を走る光：刀を振る技を出すと決めた瞬間に、鍔元から剣先へ光が走る（1 回きりのエフェクト）。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyGlintEffect, (HIKARI::VfxAsset{"c7e2b9041d6f4a8e95b3f0a1d2c4e687"}), "刀身を走る光のエフェクト（刀を振る技を出すと決めた瞬間。空なら出さない）")
    FIELD(HIKARI::Vec3, bodyGlintMistColor, (HIKARI::Vec3{0.80f, 1.10f, 1.60f}), "刀身を走る光：霞の技の色と明るさ")
    FIELD(HIKARI::Vec3, bodyGlintFireColor, (HIKARI::Vec3{1.80f, 0.90f, 0.35f}), "刀身を走る光：刀が燃えている技の色と明るさ")
    // 溜めの気配：溜めている間、体を包んで立ちのぼるエフェクト。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyAuraEffect, (HIKARI::VfxAsset{"5b0d3e9a7c124f6e8a2b1c0d9e3f4a75"}), "溜めの気配のエフェクト（溜めている間、体を包んで立ちのぼる。空なら出さない）")
    FIELD(HIKARI::Vec3, bodyAuraMistColor, (HIKARI::Vec3{0.45f, 0.75f, 1.40f}), "溜めの気配：霞の技の色と明るさ")
    FIELD(HIKARI::Vec3, bodyAuraFireColor, (HIKARI::Vec3{1.60f, 0.55f, 0.12f}), "溜めの気配：刀が燃えている技の色と明るさ")
    // 気配が立ち上がる・引く時間 (秒)。
    FIELD(float, bodyAuraSeconds, 0.25f, "溜めの気配：立ち上がる・引く時間 (秒)", HIKARI::Range(0.01f, 3.0f))
    // 胴の煙を技に合わせる：溜め切った時の量（倍率）、フェーズ 1 で刀が燃えている間の熱さ（熱い煙は橙に光る）、
    // 振り終わった直後に吹き出す量（倍率）とその時間 (秒)。
    // （c-66ec5e：溜め 2.4・吹き出し 2.5 倍では煙が膨らんで画面を塞いだので下げた。）
    FIELD(float, bodySmokeChargeAmount, 1.6f, "胴の煙：溜め切った時の量（倍率）", HIKARI::Range(0.0f, 8.0f))
    // プレイヤーが近い時は胴の煙を薄くする（カメラがボスの近くまで寄って、煙が画面を覆わないように。c-66ec5e）：
    // smokeNearMeters 以内で smokeNearScale 倍、smokeFarMeters 以遠で元の量、その間はなめらかに。
    FIELD(float, bodySmokeNearScale, 0.35f, "胴の煙：プレイヤーが近い時の量（倍率。画面を塞がないように）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, bodySmokeNearMeters, 5.0f, "胴の煙：この距離 (m) 以内で近い時の量", HIKARI::Range(0.0f, 40.0f))
    FIELD(float, bodySmokeFarMeters, 12.0f, "胴の煙：この距離 (m) 以遠で元の量", HIKARI::Range(0.0f, 40.0f))
    FIELD(float, bodySmokeLitHeat, 5.0f, "胴の煙：フェーズ 1 で刀が燃えている間の熱さ（熱い煙は橙に光る。1 で光らない）", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, bodySmokeSwingPuff, 1.5f, "胴の煙：振り終わった直後に吹き出す量（倍率。1 で吹き出さない）", HIKARI::Range(1.0f, 8.0f))
    FIELD(float, bodySmokeSwingPuffSeconds, 0.30f, "胴の煙：吹き出しが収まるまでの時間 (秒)", HIKARI::Range(0.01f, 3.0f))
    // 衝撃の輪：重い一撃が当たった瞬間に、地面に沿って広がるゆがみと光の輪（1 回きりのエフェクト）。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyShockEffect, (HIKARI::VfxAsset{"e1a7c3f05b2d4d96a8c4f7b16d0e3a29"}), "衝撃の輪のエフェクト（重い一撃が当たった瞬間、地面に沿って広がる。空なら出さない）")
    FIELD(HIKARI::Vec3, bodyShockMistColor, (HIKARI::Vec3{0.80f, 1.10f, 1.70f}), "衝撃の輪：霞の技の色と明るさ")
    FIELD(HIKARI::Vec3, bodyShockFireColor, (HIKARI::Vec3{2.00f, 0.85f, 0.30f}), "衝撃の輪：刀が燃えている技の色と明るさ")
    // 回し蹴りの輪の大きさ（倍率。1 で半径 約 3.4 m まで広がる。0 で出さない）。
    FIELD(float, bodyShockKick, 1.0f, "衝撃の輪の大きさ：回し蹴り", HIKARI::Range(0.0f, 6.0f))
    // 突進の尾：体が速く動く間、肩と背から流れる光の帯と、空気のゆがみの尾を引く。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyDashEffect, (HIKARI::VfxAsset{"2c9f6b1e8a7d43f0b5e3d1c0a9f84726"}), "突進の尾のエフェクト（体が速く動く間の光の帯とゆがみ。空なら出さない）")
    FIELD(HIKARI::Vec3, bodyDashMistColor, (HIKARI::Vec3{0.60f, 0.90f, 1.50f}), "突進の尾：ふだんの色と明るさ")
    FIELD(HIKARI::Vec3, bodyDashFireColor, (HIKARI::Vec3{1.80f, 0.70f, 0.20f}), "突進の尾：刀が燃えている間の色と明るさ")
    // 尾が出始める体の速さと、出切る速さ (m/秒)。
    FIELD(float, bodyDashMinSpeed, 9.0f, "突進の尾：出始める体の速さ (m/秒)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, bodyDashFullSpeed, 18.0f, "突進の尾：出切る体の速さ (m/秒)", HIKARI::Range(0.1f, 100.0f))
    // 左手の光：投げナイフの前隙の間、掌に集まる光（暈・芯・ゆがみ・速く動いた時の尾）。左手の掌に付けて出し続ける。
    // 空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyHandEffect, (HIKARI::VfxAsset{"7e3a9c5d1b8f4e62a0d4c6b2f1e8a937"}), "左手の光のエフェクト（投げナイフの前隙の間、掌に集まる光。空なら出さない）")
    // 左手の弾け：ナイフを投げる瞬間に、手元で弾ける閃きと輪（1 回きり）。空なら出さない。
    FIELD(HIKARI::VfxAsset, bodyHandBurstEffect, (HIKARI::VfxAsset{"0f6d2b8e4c1a49d7b3e5a7c9d1f20b64"}), "左手の弾けのエフェクト（ナイフを投げる瞬間。空なら出さない）")
    // 色と明るさ。
    FIELD(HIKARI::Vec3, bodyHandMistColor, (HIKARI::Vec3{0.70f, 1.05f, 1.80f}), "左手の光：色と明るさ")
    // 光の強さ（引いた手。0 で出さない）。
    FIELD(float, bodyHandKnife, 0.5f, "左手の光の強さ（投げナイフの引いた手）", HIKARI::Range(0.0f, 8.0f))
    // 光が立ち上がる・引く時間 (秒) と、弾けの大きさ（倍率。0 で出さない）。
    FIELD(float, bodyHandSeconds, 0.12f, "左手の光：立ち上がる・引く時間 (秒)", HIKARI::Range(0.01f, 2.0f))
    FIELD(float, bodyHandBurst, 1.0f, "左手の弾けの大きさ（倍率。0 で出さない）", HIKARI::Range(0.0f, 6.0f))
    // 光を置く所（左手の模型の座標・実寸 m。掌の真ん中の少し前）。
    FIELD(HIKARI::Vec3, bodyHandPoint, (HIKARI::Vec3{0.0f, -0.07f, 0.20f}), "左手の光を置く所（左手の模型の座標・実寸 m）")
    // 範囲爆発の溜め（c-3c908c）：叩きつけの前に、左手へ霧（剣が燃えている技は火）を集めて掌へ押し縮める。手に付いて動く
    // 流体域を持つエフェクト（霞／火。両方空なら出さない）と、押し縮める瞬間の一度きりのエフェクト。
    FIELD(HIKARI::VfxAsset, bodyGatherMistEffect, {}, "範囲爆発の溜め：左手へ霧を集めるエフェクト（手に付いて動く流体域。空なら出さない）")
    FIELD(HIKARI::VfxAsset, bodyGatherFireEffect, {}, "…剣が燃えている技で、火を集めるエフェクト（空なら霧と同じ）")
    FIELD(HIKARI::VfxAsset, bodyGatherPinchEffect, {}, "…掌へ押し縮める瞬間のエフェクト（内へ縮む輪・一閃。空なら出さない）")
    // 拍：叩きつけ（輪を放つ tick）の何 F 前から集め始めるか、何 F 前に押し縮めるか（集めるのを止め、内へ縮む輪と一閃を
    // 出す。その後は、縮んだ芯だけが手に付いて落ちる）。
    FIELD(int, bodyGatherLeadFrames, 62, "…叩きつけの何 F 前から集め始めるか", HIKARI::Range(1, 600))
    FIELD(int, bodyGatherPinchFrames, 22, "…叩きつけの何 F 前に押し縮めるか（集めるのを止める）", HIKARI::Range(0, 600))
    // 掌の芯の光（倍率）：集め終わりの明るさと、押し縮めた後の明るさ。押し縮めた後の細かい明滅の深さ（0 で明滅なし）。
    FIELD(float, bodyGatherCore, 0.45f, "…掌の芯の光：集め終わりの明るさ（倍率）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyGatherCorePinched, 1.6f, "…掌の芯の光：押し縮めた後の明るさ（倍率）", HIKARI::Range(0.0f, 8.0f))
    FIELD(float, bodyGatherCoreFlicker, 0.18f, "…押し縮めた後の芯の細かい明滅の深さ（0 で明滅なし）", HIKARI::Range(0.0f, 1.0f))
    // 押し縮める瞬間のエフェクトの色（霞／火）。
    FIELD(HIKARI::Vec3, bodyGatherMistColor, (HIKARI::Vec3{0.78f, 0.92f, 1.0f}), "…押し縮める瞬間の色（霞）")
    FIELD(HIKARI::Vec3, bodyGatherFireColor, (HIKARI::Vec3{1.0f, 0.58f, 0.22f}), "…押し縮める瞬間の色（火）")
    // 集めている間、足元の霧と場の火を、手の真下へ寄せる加速度 (m/s²。0 は寄せない) と、寄せる輪の半径 (m)。
    FIELD(float, bodyGatherFogPull, 30.0f, "…集めている間、足元の霧と場の火を手の真下へ寄せる加速度 (m/s²。0 は寄せない)", HIKARI::Range(0.0f, 400.0f))
    FIELD(float, bodyGatherFogPullRadius, 4.0f, "…寄せる輪の半径 (m)", HIKARI::Range(0.5f, 20.0f))
    // 掌へ渦を巻いて流れ込む腕（手の真下の地面の輪から立ちのぼる）：本数（8 まで）、1 本の濃さ（中心での毎秒の密度）と
    // 太さ (m)、地面の輪の半径 (m)、掌へ着くまでに回る回数、1 秒に流れ込む回数（集め始め → 押し縮める直前。終わりへ
    // 向けて速くなる）、温度（霞／火。光り方）。
    FIELD(int, bodyGatherArms, 6, "…掌へ流れ込む腕の本数", HIKARI::Range(1, 8))
    FIELD(float, bodyGatherArmStrength, 1600.0f, "…腕の濃さ（中心での毎秒の密度）", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, bodyGatherArmRadius, 0.28f, "…腕の太さ (m)", HIKARI::Range(0.05f, 2.0f))
    FIELD(float, bodyGatherArmReach, 2.6f, "…腕が立ちのぼる地面の輪の半径（手の真下から m）", HIKARI::Range(0.5f, 10.0f))
    FIELD(float, bodyGatherArmTurns, 0.6f, "…掌へ着くまでに回る回数", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, bodyGatherArmRate, 1.8f, "…1 秒に流れ込む回数（集め始め）", HIKARI::Range(0.1f, 10.0f))
    FIELD(float, bodyGatherArmRateEnd, 3.4f, "…1 秒に流れ込む回数（押し縮める直前。大きいほど終わりが速い）", HIKARI::Range(0.1f, 10.0f))
    FIELD(float, bodyGatherArmHeatMist, 1.6f, "…腕の温度（霞。高いほど光る）", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, bodyGatherArmHeatFire, 8.0f, "…腕の温度（火）", HIKARI::Range(0.0f, 10.0f))
    // …昇り方の曲がり（大きいほど、地面を長く這ってから一気に昇る）と、輪の半径のばらつき（0〜1。腕ごとに内へ寄せる）。
    FIELD(float, bodyGatherArmLift, 1.7f, "…腕の昇り方（大きいほど、地面を長く這ってから一気に昇る）", HIKARI::Range(0.2f, 6.0f))
    FIELD(float, bodyGatherArmSpread, 0.25f, "…地面の輪の半径のばらつき（0〜1）", HIKARI::Range(0.0f, 1.0f))
    float helmGlowShare = 0.35f;

    BossBodyState state;
    BossAnimBodyState anim;
};

// 体の見た目の設定（部品が無ければ既定の設定）。登場の効果など、体の部品が無くても出すものが読む。
const BossBody& BossBodySettingsOf(const BossBody* look);

} // namespace Project_TD2_2
