#pragma once

#include <string_view>

#include "HIKARI/Game.h"
#include "BossIntroSettings.h"

namespace Project_TD2_2 {

// ボスの登場の部品（c-eb3b08 でボスの部品から分けた）：ボスと同じ物体に付ける。無ければ登場をしない（登場を切った時と
// 同じ。初めから組み上がって立っている）。進めるのはボスの System（BossSystem。ボスの tick の中で、前と同じ順に呼ぶ）、
// 規則は BossIntro、見た目は BossIntroVisuals・BossIntroActor・BossIntroSmoke・BossIntroEffects・BossIntroCamera。
// 登場とフェーズの変わり目の演出が共通で使う効果の口は、ボスの部品の BossCinematicFx に残す。
struct BossIntro : HIKARI::Component {
    HIKARI_COMPONENT(BossIntro, "ボスの登場")
    // 登場をするか。切ると、初めから組み上がって立っている。
    FIELD(bool, introEnabled, true, "登場をする（切ると、初めから組み上がって立っている）", HIKARI::Group("登場（崩れた鎧が煙に起こされ、手甲と兜が付き、刀を抜く。動きを捕まえた演技を骨格の動画で流す）"))
    NOTE("戦いの前、鎧は地面に崩れ、兜と手甲は離れて落ちています。戦いが始まると、役者（骨格と動画を持つ模型）が演技を"
        "演じ、済むと戦いの体に替わります。時計・カメラ・片段を流す時刻（事件段 Anim.<片段>。段の長さ＝流す長さ）・部品が"
        "飛ぶ時刻（Part.<部品>.Fly）・効果の拍は、演出の資産（Sequence）が受け持ちます。兜と手甲が落ちている所は、その"
        "物体の位置（シーンか、演出の資産の物体の轨）で決めます。登場の間、ボスは当たらず、動きません。")
    // Play の間に 1 度だけにする（負けてやり直した後は、初めから組み上がって立っている）。
    FIELD(bool, introOncePerPlay, true, "登場は Play の間に 1 度だけ（やり直しの後は流さない）")
    // 役者（骨格の動画で演じる体。模型と動画の部品を持つシーンの物体）と、付く前の部品の物体（兜・右の手甲・左の
    // 手甲。役者の子）。役者が無い・動画の機能が切ってある時は、登場をしない（初めから構えて立つ）。部品の物体の位置と
    // 向きは、シーンか演出の資産の物体の轨で決める（付く時は、コードが骨の所まで寄せる）。
    FIELD(HIKARI::Entity, introActorObject, {}, "役者の物体（骨格の動画で演じる体。無ければ登場をしない）")
    FIELD(HIKARI::Entity, introHelmObject, {}, "付く前の兜の物体")
    FIELD(HIKARI::Entity, introGauntletRightObject, {}, "付く前の右の手甲の物体")
    FIELD(HIKARI::Entity, introGauntletLeftObject, {}, "付く前の左の手甲の物体")
    // 演出の間だけ点ける灯（役者の子。置き場所・向き・明るさは、演出の資産の物体の轨がカットごとに決める）。0 なら使わない。
    FIELD(HIKARI::Entity, introKeyLightObject, {}, "演出の間だけ点ける灯の物体（置き場所と明るさは演出の資産の物体の轨）")
    // 演出の資産（Sequence の GUID）。演技の時計・カメラ・片段・部品・効果の拍。空・流せない時は登場をしない。
    FIELD(HIKARI::Sequence, introSequence, (HIKARI::Sequence{"d66fc20a364f475699efb34fe4632336"}), "演出の資産（時計・カメラ・片段・部品・効果の拍）")
    // 演技を流す速さの倍率（1 で資産のまま）。
    FIELD(float, introPlaybackRate, 1.0f, "演技を流す速さの倍率（1 で資産のまま）", HIKARI::Range(0.1f, 4.0f))
    // 演技が済んで戦いの体に替わった後、プレイヤーの入力を止めたまま待つ長さ (秒。ゲームのカメラへ戻り、肩に担いだ
    // 刀を待機の構えへ下ろす間)。
    FIELD(float, introSettleSeconds, 1.4f, "演技が済んだ後、入力を止めたまま、担いだ刀を構えへ下ろす長さ (秒)", HIKARI::Range(0.0f, 10.0f))
    // 一息の段に入ってから、役者を戦いの体に替えるまでの秒（settleSeconds より短く）。演技の後はカメラがゲームのカメラへ
    // 滑らかに引いて行くので、すぐ替えると、近くで体が入れ替わるのが見える。カメラが引いてから替える。0 ですぐ替える。
    FIELD(float, introSettleSwapSeconds, 0.8f, "演技が済んでから、役者を戦いの体に替えるまでの秒（カメラが引いてから替える。0 ですぐ替える）", HIKARI::Range(0.0f, 10.0f))
    // 一息の段に入ってから、演出の灯をこの秒で 0 まで落とす（settleSwapSeconds より長ければ、替える時に 0 にそろえる）。
    // 灯を替える瞬間に急に消すと、ボスに当たる光が一気に変わって画が光ったように見え、動的な GI も作り直しで重くなる。
    FIELD(float, introSettleLightFadeSeconds, 0.7f, "演技が済んでから、演出の灯を消し切るまでの秒（役者を替える時には必ず 0。0 で替える時にすぐ消す）", HIKARI::Range(0.0f, 10.0f))
    // 演出の動きのぼかし（c-bc5908）：演技の間、画面のカメラに掛ける動きのぼかし。シャッター角（度。180 で映画の
    // 標準、大きいほど長く流れる。0 で掛けない）と、ぼかしの長さの上限（1080p の画素）。一息の段では、ゲームのカメラへ
    // 渡すのに合わせて弱めて 0 にする。画質の設定の motionBlurQuality が Off なら出ない。カットの切り替わりでは
    // エンジンが前の画を捨てるので、切り替わりの画は流れない。既定は掛けない（SmokeArena の場面で 180°）。
    FIELD(float, introMotionBlurShutter, 0.0f, "演出の動きのぼかし：シャッター角（度。180 で映画の標準、大きいほど長く流れる。0 で掛けない）", HIKARI::Range(0.0f, 360.0f))
    FIELD(float, introMotionBlurMaxPixels, 24.0f, "演出の動きのぼかし：ぼかしの長さの上限（1080p の画素）", HIKARI::Range(0.0f, 64.0f))
    // …事件段「Blur:<シャッター角>:<px>」の間だけ、その値へ寄せる（速いカットだけ強く流す）。寄せる長さ (秒)。
    FIELD(float, introMotionBlurBlendSeconds, 0.2f, "演出の動きのぼかし：強い所（事件段 Blur:<角>:<px>）へ寄せる・戻す長さ (秒)", HIKARI::Range(0.0f, 3.0f))
    // 被写界深度（c-280b71）：演出の資産の事件段「Focus:<m>[:<強さ>]」（そのカットの始めのピントの距離。視線に沿った m）と
    // 「Focus><m>[:<強さ>]」（前の段から続く）に従って、ピントの合う距離を決める。段の間は、次に続く段の値へ線形に寄る
    // （ピント送り）。段が無い間は掛けない。ぼけ方は、ピントの距離に対する割合で決める（寄りのカットほど浅い）：ピントが
    // 合って見える奥行き、手前・奥がぼけ切るまでの奥行き、ぼけの大きさの上限（1080p の画素。段の「強さ」を掛ける）。
    // 段の無い所との出入りの長さ (秒)。
    FIELD(float, introFocusRangeShare, 0.10f, "被写界深度：ピントが合って見える奥行き（ピントの距離に対する割合。事件段 Focus:<m> が無いカットは掛けない）", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, introFocusNearShare, 0.5f, "被写界深度：手前がぼけ切るまでの奥行き（ピントの距離に対する割合）", HIKARI::Range(0.02f, 4.0f))
    FIELD(float, introFocusFarShare, 1.4f, "被写界深度：奥がぼけ切るまでの奥行き（ピントの距離に対する割合）", HIKARI::Range(0.02f, 20.0f))
    FIELD(float, introFocusNearBlurPixels, 12.0f, "被写界深度：手前のぼけの大きさの上限（1080p の画素）", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, introFocusFarBlurPixels, 8.0f, "被写界深度：奥のぼけの大きさの上限（1080p の画素）", HIKARI::Range(0.0f, 64.0f))
    FIELD(float, introFocusFadeSeconds, 0.3f, "被写界深度：掛かる・外れる長さ (秒)", HIKARI::Range(0.0f, 5.0f))
    // 上下の黒帯：演技の間、画をこの横縦比に見せる（2.39 でシネスコ。画面の比より小さければ出さない。0 で出さない）。
    // 出入りの長さ (秒)。演技が済んだら、ゲームのカメラへ渡す間に引っ込む。
    FIELD(float, introBarsAspect, 0.0f, "上下の黒帯：演技の間、画をこの横縦比に見せる（2.39 でシネスコ。画面の比より小さければ・0 なら出さない）", HIKARI::Range(0.0f, 4.0f))
    FIELD(float, introBarsFadeSeconds, 0.9f, "上下の黒帯：出る・引っ込む長さ (秒)", HIKARI::Range(0.0f, 5.0f))
    // 一度きりの特効（空なら出さない）：煙が鎧に届いて引き起こす瞬間の足元の砂ぼこり、刀が抜けた瞬間の土くれと砂の輪、
    // 目が灯る瞬間の面頬の前の熱のゆがみ、演技の間じゅう舞台に漂う塵と灰（カメラの手前でぼけて、奥行きを出す）。
    // それぞれの大きさ（倍率）。
    FIELD(HIKARI::VfxAsset, introDustEffect, {}, "特効：煙が鎧を引き起こす瞬間の足元の砂ぼこり（空なら出さない）")
    FIELD(float, introDustScale, 1.0f, "…砂ぼこりの大きさ（倍率）", HIKARI::Range(0.1f, 10.0f))
    FIELD(HIKARI::VfxAsset, introDebrisEffect, {}, "特効：刀が抜けた瞬間の土くれと砂の輪（空なら出さない）")
    FIELD(float, introDebrisScale, 1.0f, "…土くれと砂の輪の大きさ（倍率）", HIKARI::Range(0.1f, 10.0f))
    FIELD(HIKARI::VfxAsset, introHazeEffect, {}, "特効：目が灯る瞬間の、面頬の前の熱のゆがみ（空なら出さない）")
    FIELD(float, introHazeScale, 1.0f, "…熱のゆがみの大きさ（倍率）", HIKARI::Range(0.1f, 10.0f))
    FIELD(HIKARI::VfxAsset, introMotesEffect, {}, "特効：演技の間、舞台に漂う塵と灰（空なら出さない）")
    FIELD(float, introMotesAmount, 1.0f, "…漂う塵と灰の量（倍率）", HIKARI::Range(0.0f, 10.0f))
    // 剣先の寄りのカット（事件段 Sword.Tip）の間だけ、刀の根元に置く細かい流体の域（BossIntroTipSmoke。空なら置かず、
    // 舞台の域へ滲ませる：寄りの画では、煙が丸い影に見える）と、地の割れ目から滲み出す煙の濃さ（倍率）。
    FIELD(HIKARI::VfxAsset, introTipSmokeEffect, {}, "剣先の寄り：刀の根元に置く細かい流体の域（BossIntroTipSmoke。空なら置かない）")
    FIELD(float, introTipSmokeAmount, 1.0f, "…地の割れ目から滲み出す煙の濃さ（倍率）", HIKARI::Range(0.0f, 10.0f))
    // 手を握る拍（事件段 Hand.Fist）の間だけ、右手の所に置く細かい流体の域（BossIntroFistSmoke。空なら出さない）と、
    // 指の間から押し出される煙の濃さ（倍率）。
    FIELD(HIKARI::VfxAsset, introFistSmokeEffect, {}, "手を握る：右手の所に置く細かい流体の域（BossIntroFistSmoke。空なら出さない）")
    FIELD(float, introFistSmokeAmount, 1.0f, "…指の間から押し出される煙の濃さ（倍率）", HIKARI::Range(0.0f, 10.0f))
    // 部品が骨へ飛ぶ道：手甲は吸われる（初めゆっくり、最後に速い。この指数が大きいほど最後が速い）、途中で浮く高さ (m)。
    // 兜は煙に持ち上げられる（ゆっくり動き出してゆっくり止まる）、途中で外へふくらむ幅 (m)。
    FIELD(float, introGauntletFlyPower, 2.6f, "手甲が吸われる道：最後の速さ（大きいほど、初めゆっくりで最後に速い）", HIKARI::Range(1.0f, 6.0f))
    FIELD(float, introGauntletFlyLift, 0.9f, "手甲が吸われる道：途中で浮く高さ (m)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, introHelmFlyBulge, 0.25f, "兜が持ち上がる道：途中で外へふくらむ幅 (m)", HIKARI::Range(0.0f, 3.0f))
    // 演出の後、ゲームのカメラへ渡す間にゲームのカメラが向くボスの高さ (m。戻り切った時、ボスを見ている)。
    FIELD(float, introCameraFocusHeight, 4.0f, "演出の後、ゲームのカメラへ渡す間にカメラが向くボスの高さ (m)", HIKARI::Range(0.0f, 20.0f))
    // 飛ばす：登場の間にこの入力（行動 id）を押すと、組み上がった待機の構えへ進む（空なら飛ばせない）。
    FIELD(std::string, introSkipAction, "Gameplay.Menu", "飛ばす：登場の間にこの入力（行動 id）を押すと、構えへ進む（空なら飛ばせない）")
    // 確かめ用（正式シーンでは使わない）：0 以上なら、戦いの流れに関わらず、この tick に登場が始まる。
    FIELD(int, introTestStartFrames, -1, "確かめ用：この tick に登場を始める（-1 で使わない。戦いの流れに従う）", HIKARI::Range(-1, 100000))
    // 登場の黒い煙（BossIntroSmoke）：コードからだけ煙を入れる流体の域（空なら煙を出さない）。量は「1 秒に入れる煙の総量」
    // （点の大きさや数を変えても、全体の濃さは変わらない）。
    FIELD(HIKARI::VfxAsset, introSmokeEffect, (HIKARI::VfxAsset{"5c0e7a1b93d24f6e8a41b7d2c6f09e35"}), "煙：黒い煙の流体の域（BossIntroSmoke。空なら煙を出さない）")
    // 地を走って鎧を起こす煙（拍 Smoke.Streak）：頭に付いて行く域のエフェクト（空なら出さない）、量の倍率、刀の向こう
    // 何 m から来るか、途中で左右へうねる幅 (m。鎧に近づくほど小さくなる)、鎧に着いた瞬間に舞台の域で鎧を包む煙の量。
    FIELD(HIKARI::VfxAsset, introStreakEffect, (HIKARI::VfxAsset{"8f3b2c6d17a94e05b6d8c2a4e91f7350"}), "煙：地を走って鎧を起こす煙のエフェクト（BossIntroStreak。空なら出さない）")
    FIELD(float, introStreakAmount, 1.0f, "煙：地を走る煙の量の倍率", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, introStreakLead, 30.0f, "煙：地を走る煙が、刀の向こう何 m から来るか", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, introStreakSwerve, -1.2f, "煙：地を走る煙が左右へうねる幅 (m)", HIKARI::Range(-5.0f, 5.0f))
    FIELD(float, introWrapAmount, 6.0f, "煙：鎧に着いた後、鎧を包む煙の量", HIKARI::Range(0.0f, 5000.0f))
    // 鎧を引き起こす煙（Smoke.Rise）：鎧の下の地面から、腰と胸を通って上へ抜ける柱の量。
    FIELD(float, introRiseAmount, 14.0f, "煙：鎧を引き起こす煙の柱の量", HIKARI::Range(0.0f, 5000.0f))
    // 腕の煙：手甲が付く前に肩から伸びる煙の腕（右 Smoke.Tendril・左 Smoke.TendrilLeft）の量、付いた後に鎧と手甲を
    // つなぐ細い煙の量。手甲が付いた瞬間の弾け（Hand.Right・Hand.Left）の量と、その時のカメラの揺れ。
    FIELD(float, introTendrilAmount, 1.2f, "煙：手甲が付く前、肩から伸びる煙の腕の量", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introArmAmount, 0.25f, "煙：手甲が付いた後、鎧と手甲をつなぐ細い煙の量", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introPuffAmount, 4.0f, "煙：手甲が付いた瞬間に弾ける煙の量", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introAttachShake, 0.3f, "手甲が付いた瞬間のカメラの揺れの強さ（0 で揺らさない）", HIKARI::Range(0.0f, 3.0f))
    // 兜を持ち上げる煙の柱（Smoke.HelmLift）の量。
    FIELD(float, introLiftAmount, 0.6f, "煙：兜を持ち上げる煙の柱の量", HIKARI::Range(0.0f, 5000.0f))
    // 兜が頭に収まった瞬間（Helm.Seat。白黒の閃きと一緒に）：襟から噴く煙の量、衝撃の輪の大きさ（2 つ目は 1.4 倍）、
    // カメラの揺れの強さ、時間を止める長さ (F)。
    FIELD(float, introSeatBurstAmount, 10.0f, "兜が収まった瞬間：襟から噴く煙の量", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, introSeatShock, 2.6f, "兜が収まった瞬間：衝撃の輪の大きさ（0 で出さない）", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, introSeatShake, 1.2f, "兜が収まった瞬間：カメラの揺れの強さ（1 以上で画角も広げる）", HIKARI::Range(0.0f, 3.0f))
    FIELD(int, introSeatHitStopFrames, 7, "兜が収まった瞬間：時間を止める長さ (F)", HIKARI::Range(0, 60))
    // 刀を握っている間、画面の揺れが高まって行く強さ（握り終わりの強さ。0 で揺らさない）。
    FIELD(float, introGrabRumble, 0.55f, "刀を握っている間：高まって行く画面の揺れの強さ（握り終わり。0 で揺らさない）", HIKARI::Range(0.0f, 3.0f))
    // 全画面の効果（BossImpactMono の衝撃と張りつめ。白黒の閃きと同じ効果の資産）：握っている間に高まる張りつめの強さ
    // （縁の減光と色ずれ。0 で出さない）、抜けた瞬間に根元から画の上を走る衝撃の輪の強さ（0 で出さない）と、輪が画面を
    // 走り切る長さ (秒。実時間)。輪の幅・歪み・ぼかし・色ずれは、資産 BossImpactMono.hposteffect の項目。
    FIELD(float, introGrabTension, 0.7f, "刀を握っている間：画の縁が締まって色がずれる強さ（握り終わり。0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, introDrawShockPost, 1.0f, "刀が抜けた瞬間：根元から画の上を走る衝撃の輪の強さ（0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, introDrawShockPostSeconds, 1.1f, "刀が抜けた瞬間：衝撃の輪が画面を走り切る長さ (秒。実時間)", HIKARI::Range(0.05f, 3.0f))
    // 見得の終わり（拍 Smoke.Disperse。音も衝撃も付けず、静かに晴れる）：黒い霧を主役から外へ流す加速 (m/秒²) と、
    // 場の霧を外へ押す速さ (m/秒)。演技の後も、ゲームのカメラへ渡す間、流しながら消し切る。
    FIELD(float, introDisperseBlow, 24.0f, "見得の終わり：黒い霧を主役から外へ流す加速 (m/秒²)", HIKARI::Range(0.0f, 200.0f))
    FIELD(float, introDisperseFogPush, 2.5f, "見得の終わり：場の霧を外へ押す速さ (m/秒)", HIKARI::Range(0.0f, 60.0f))
    // 序幕の黒い煙（演技の前にプレイヤーを待つ間から、地を走る煙が来るまで。拍 Smoke.Ambient）：部品（鎧・兜・両の手甲・
    // 刀の根元）の周りを巡る黒い煙の筋の量（全部を合わせて 1 秒に入れる総量）、筋が走る速さ (m/秒。遅いと尾がつながって、
    // その場に溜まった塊に見える)、巡る輪の大きさ（部品ごとの半径に掛ける倍率）、煙の温度（上向きの加速 m/秒²。高いほど
    // 立ちのぼる。0 なら地面を這う）、輪を同じ向きへ回し続ける加速 (m/秒²)、部品の周りの場の霧を押しのける速さ
    // (m/秒。0 で押さない)。
    FIELD(float, introAmbientAmount, 18.0f, "序幕：部品の周りを巡る黒い煙の筋の量（1 秒に入れる総量）", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introAmbientTurn, 2.2f, "序幕：煙の筋が走る速さ (m/秒。遅いと、その場に溜まった塊に見える)", HIKARI::Range(0.0f, 12.0f))
    FIELD(float, introAmbientRing, 1.0f, "序幕：煙の筋が巡る輪の大きさ（部品ごとの半径に掛ける倍率）", HIKARI::Range(0.1f, 3.0f))
    FIELD(float, introAmbientHeat, 0.1f, "序幕：煙の温度（上向きの加速 m/秒²。高いほど立ちのぼる。0 で地面を這う）", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, introAmbientStir, 0.8f, "序幕：煙の輪を同じ向きへ回し続ける加速 (m/秒²)", HIKARI::Range(0.0f, 50.0f))
    FIELD(float, introAmbientFogPush, 1.2f, "序幕：部品の周りの場の霧を押しのける速さ (m/秒。0 で押さない)", HIKARI::Range(0.0f, 20.0f))
    // 筋の尾を薄める速さ (1/秒。大きいほど尾が短い。0 だと尾がつながって、丸い塊に溜まる)。
    FIELD(float, introAmbientFade, 1.2f, "序幕：煙の筋の尾を薄める速さ (1/秒。大きいほど尾が短い)", HIKARI::Range(0.0f, 20.0f))
    // 前触れ（拍 Omen）：煙の筋が自分の部品を離れて鎧の周りへ寄り、渦になる。その渦を回す加速 (m/秒²。拍の終わり)、
    // 画が暗くなる強さと縁が締まる強さ（全画面の効果。0 で出さない）、低く続く画面の揺れの強さ（拍の終わり。揺れの
    // 速さはボスの部品の introRumbleFrequency）。
    FIELD(float, introOmenPull, 4.0f, "前触れ：鎧の周りへ寄った煙の渦を回す加速 (m/秒²。拍の終わり)", HIKARI::Range(0.0f, 50.0f))
    FIELD(float, introOmenDim, 0.5f, "前触れ：画が暗くなる強さ（0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, introOmenTension, 0.45f, "前触れ：画の縁が締まる強さ（0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, introOmenRumble, 0.3f, "前触れ：低く続く画面の揺れの強さ（拍の終わり。0 で揺らさない）", HIKARI::Range(0.0f, 3.0f))
    // 落ちている兜の、面頬の隙間の瞬き（拍 Helm.Glint）：明るさ（画面基準。0 で出さない）。
    FIELD(float, introHelmGlint, 2.4f, "序幕：落ちている兜の隙間の瞬きの明るさ（画面基準。0 で出さない）", HIKARI::Range(0.0f, 64.0f))
    // 刀を握っている間、外から根元へ地面を這って集まって来る黒い霧の量。
    FIELD(float, introGatherAmount, 26.0f, "刀を握っている間：根元へ集まって来る黒い霧の量", HIKARI::Range(0.0f, 5000.0f))
    // 見得の黒い霧（抜刀の後から演技の終わりまで）：主役の後ろにゆっくり巻きながら立ち上がる壁と、両脇の地面を前へ
    // 這い出す霧の量。見得の間の、画の縁の締まり（全画面の効果。0 で出さない）。
    FIELD(float, introBackdropAmount, 90.0f, "見得：主役の後ろに立つ黒い霧と、前へ這い出す霧の量", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introFinaleTension, 0.35f, "見得：画の縁が締まる強さ（0 で出さない）", HIKARI::Range(0.0f, 1.0f))
    // 刀身の光（登場の間だけ）：振り上げの頂点で剣先に出る星の明るさ（全画面の効果。0 で出さない）。刃口の細い線の
    // 明るさは、ボスの部品の introBladeEdgeGlow。
    FIELD(float, introBladeStar, 1.0f, "刀身の光（登場だけ）：振り上げの頂点で剣先に出る星の明るさ（0 で出さない）", HIKARI::Range(0.0f, 8.0f))
    // 目の炎（既定では出さない。目は兜の中の自発光の模型 BossHelmEyes で見せる：BossBody の helmEyesObject）：
    // 目が灯ってから演技の終わりまで、面頬の左右の隙間から立つ青白い炎。兜の前に付いて動く細かい流体の域の
    // エフェクト（空なら出さない）と、量（片目の、1 秒に入れる煙の総量。目が灯る拍の初めは 4 倍。0 で出さない）。
    FIELD(HIKARI::VfxAsset, introEyeFireEffect, (HIKARI::VfxAsset{"b7e94c21d05a4f38a6c1e3f9027d5b84"}), "目の炎：兜の前に付く細かい流体の域（BossEyeFire。空なら出さない）")
    FIELD(float, introEyeFireAmount, 0.0f, "目の炎：面頬の隙間から立つ青白い炎の量（0 で出さない）", HIKARI::Range(0.0f, 100.0f))
    // 刀を握っている間（Sword.Grab）、根元から滲む煙の量。
    FIELD(float, introSeepAmount, 42.0f, "煙：刀を握っている間、根元から滲む煙の量", HIKARI::Range(0.0f, 5000.0f))
    // 刀が地から抜けた瞬間（Sword.Out。白黒の閃きと一緒に）：噴き上げる煙の量と噴く長さ (秒)、柱が上がる速さ
    // (m/秒)、場の霧を外へ押す速さ (m/秒) と届く半径 (m)、刀身が引く煙の量、衝撃の輪の大きさ
    // （2 つ目は 1.4 倍）、カメラの揺れの強さ、時間を止める長さ (F)。
    FIELD(float, introDrawBurstAmount, 115.0f, "刀が抜けた瞬間：噴き上げる黒い煙の量", HIKARI::Range(0.0f, 10000.0f))
    FIELD(float, introDrawBurstSeconds, 1.4f, "刀が抜けた瞬間：煙を噴く長さ (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, introDrawJetSpeed, 4.0f, "刀が抜けた瞬間：煙の柱が上がる速さ (m/秒)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, introDrawPush, 3.0f, "刀が抜けた瞬間：場の霧を外へ押す速さ (m/秒)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, introDrawPushRadius, 5.0f, "刀が抜けた瞬間：外へ押す力が届く半径 (m)", HIKARI::Range(0.5f, 40.0f))
    FIELD(float, introDrawTrailAmount, 18.0f, "刀が抜けた瞬間：刀身が引く煙の量", HIKARI::Range(0.0f, 5000.0f))
    FIELD(float, introDrawShock, 3.8f, "刀が抜けた瞬間：衝撃の輪の大きさ（0 で出さない）", HIKARI::Range(0.0f, 20.0f))
    FIELD(float, introDrawShake, 1.6f, "刀が抜けた瞬間：カメラの揺れの強さ（1 以上で画角も広げる）", HIKARI::Range(0.0f, 3.0f))
    FIELD(int, introDrawHitStopFrames, 9, "刀が抜けた瞬間：時間を止める長さ (F)", HIKARI::Range(0, 60))
    // 刀へ歩く一歩（拍 Step。動きの記録の足が着く時刻に置いてある）：カメラの揺れの強さと、足元に立つ煙の量。
    FIELD(float, introStepShake, 0.32f, "刀へ歩く一歩：カメラの揺れの強さ（0 で揺らさない）", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, introStepAmount, 2.5f, "刀へ歩く一歩：足元に立つ煙の量", HIKARI::Range(0.0f, 5000.0f))
    // 刀が抜けた瞬間の停止の後、噴き上がる間のスローの長さ (F。間の時間の倍率は drawSlowScale。0 でスローにしない)、
    // 根元で弾ける閃光の大きさ（0 で出さない）。
    FIELD(int, introDrawSlowFrames, 0, "刀が抜けた瞬間：停止の後のスローの長さ (F。0 でスローにしない)", HIKARI::Range(0, 240))
    FIELD(float, introDrawFlashScale, 3.2f, "刀が抜けた瞬間：根元で弾ける閃光の大きさ（0 で出さない）", HIKARI::Range(0.0f, 20.0f))
    // 煙を消す（Smoke.Clear と演技の後）：消す速さ (1/秒) と、演技の後に消し続けてから域を止めるまでの長さ (秒)。
    FIELD(float, introClearAbsorption, 6.0f, "煙：消す速さ (1/秒。拍 Smoke.Clear と演技の後)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, introSmokeLingerSeconds, 1.5f, "煙：演技の後、消し続けてから域を止めるまでの長さ (秒)", HIKARI::Range(0.0f, 10.0f))

    BossIntroState state;
};

} // namespace Project_TD2_2
