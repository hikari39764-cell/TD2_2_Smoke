#pragma once

#include "HIKARI/Project/EnvironmentTypes.h"
#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

// 左手の所作（手の光の演出が読む）：ふだんと、投げナイフの前隙。
enum class BossBodyLeftGesture { Balance, Knife };

// 技の点描の演出（見た目だけ）。当たり・F 数は変えない。地面や画面への予告は出さない。
struct BossBodyAccentSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 刀に付くエフェクト（刀光の帯と、燃える刀身の炎・ゆがみ・光）。見える刀の刃に付けて出し続ける。空なら出さない。
    HIKARI::GameProjectVfxAssetReference bladeEffect{"9d2c47e1b6a3405f8e17c5d0a4b2f693", "Assets/Vfx/BossBlade.hvfx"};
    // 刀光：振っている間、刃が通った面に光る帯とゆがみを出す。
    bool trail = true;
    // エフェクトを付ける刃の始まり（握りから剣先までの割合。鍔のすぐ先）。
    float bladeFromShare = 0.14f;
    // 命中の窓が閉じた後も、振り抜きの間は出し続ける時間 (秒) と、その間に要る剣先の速さ (m/秒)。
    float trailFollowSeconds = 0.08f;
    float trailMinTipSpeed = 25.0f;
    // 剣先が地面からこの高さ (m) より下へ来たら、刀光を切る。
    float trailGroundCut = 0.35f;
    // 刀光の色と明るさ（1 を超えると光る）：霞の技（刀が燃えていない）と、刀が燃えている技。
    HIKARI::GameProjectFloat3 trailMistColor{0.80f, 1.20f, 2.20f};
    HIKARI::GameProjectFloat3 trailFireColor{2.40f, 1.00f, 0.30f};
    // 振りのゆがみ（屈折）の強さ（画面の 6% × この値ずらす。0.3 を超えると段が見える。0 で出さない）。刀光と同じ間に出し、
    // 刀光の色とは別なので、霞の技でも火の技でも同じ強さ。出たゆがみはエフェクト側の時間（0.4 秒）で消える。
    float swingHaze = 0.3f;
    // 振りのゆがみを、刀光より前から出し始める剣先の速さ (m/秒)。刀を振る技の打ち込み（攻撃中）の間だけ見る（構えへ上げる
    // 振りかぶりも同じくらい速いが、そこでは出さない）。0 で刀光と同じ間だけ。
    float swingHazeMinTipSpeed = 30.0f;
    // 燃える刀身：炎の明るさ（倍率。0 で出さない）と、火が付く・消える時間 (秒)。
    float bladeHeat = 1.0f;
    float bladeHeatSeconds = 0.25f;
    // 燃える刀身から立つ火の粉の量（倍率。刀のエフェクトの入力 embers。0 で出さない。c-857ae3）。
    float bladeEmbers = 1.0f;
    // 燃える刀身の流体の炎（c-3c908c）：刃に付いて動く局所空間の流体域を持つエフェクト（空なら出さず、シェーダーの炎の帯の
    // まま）。燃料の量（倍率）と、流体の炎を出している間に残す、シェーダーの炎の帯の割合・刀身の光の層の割合。
    HIKARI::GameProjectVfxAssetReference bladeFireEffect{};
    float bladeFireAmount = 1.0f;
    float bladeFlameShare = 0.25f;
    float bladeFireGlowShare = 0.8f;
    // …速く振る間は、流体の炎が刃から剥がれて後ろへ流れる。剣先がこの速さ (m/秒) に近づくほど、刃に貼り付くシェーダーの
    // 炎の帯を元の強さへ戻し（0 で戻さない）、止まったら bladeFlameReturnSeconds で bladeFlameShare へ下げる。
    float bladeFlameFastSpeed = 25.0f;
    float bladeFlameReturnSeconds = 0.35f;
    // 刃口の常時の光（刀に付くエフェクトの Edge Line・Edge Glow。入力 input.edge）：刀を見れば何をしているか分かるように、
    // 段階で明るさを変える（倍率。色は edgeMistColor／edgeFireColor に掛ける）。待機 edgeIdle は弱いが必ず見える、前隙は
    // 進みにつれて edgeWindup まで（曲がり edgeRampPower、ゆっくり脈打つ）、
    // 振る間（命中の窓）は edgeSwing、後隙は待機へ戻る。気絶は edgeStunned。燃える刀身の間は edgeHotShare を掛ける
    // （炎と重ねて白く飛ばない）。地面や画面の予告ではなく、刀そのものの見え方。
    HIKARI::GameProjectFloat3 edgeMistColor{0.55f, 0.85f, 1.60f};
    HIKARI::GameProjectFloat3 edgeFireColor{1.80f, 0.75f, 0.22f};
    float edgeIdle = 0.35f;
    float edgeWindup = 0.9f;
    float edgeSwing = 1.6f;
    float edgeStunned = 0.15f;
    float edgeRampPower = 1.5f;
    float edgePulseDepth = 0.25f;
    float edgePulseFrequency = 1.4f;
    float edgeHotShare = 0.6f;
    float edgeRiseSeconds = 0.05f;
    float edgeFallSeconds = 0.15f;
    // 刃のまわりの暗い縁（Edge Dark。入力 input.edgeDark の A）の濃さ（倍率。0 で出さない）。明るい背景（炎・夕方の空・
    // 白い霧）の前で、明るい線だけだと刀の形が消えるので、外側に煙の黒の柔らかい帯を重ねる。
    float edgeDark = 1.0f;

    // 霞の技と実体の刀の技の見分け（c-ecd963）：剣気を飛ばす技（表の当て方が実でなく、斬撃速度が 0 より大きい）の前隙〜
    // 振りの間は、刀身の輪郭の光（材質）と刃口の光をいつも通り灯し、刃文の流れ・霞の帯を hazeBoost 倍に強める（刀身が
    // 青白く明るい）。実体の刀だけで当てる技の間は、輪郭の光を realRimShare、刃口の光を realEdgeShare、流れ・霞を
    // realFlowShare 倍に落とす（暗い縁と刃文だけ）。後隙・待機・歩きはどちらでもない。
    float hazeBoost = 1.8f;
    float realEdgeShare = 0.3f;
    float realFlowShare = 0.35f;
    // 刀の常時の層（BossBlade.hvfx の Hamon Flow・Blade Mist・Tip Wake）：刃文に沿って鍔元から剣先へ流れる光（input.flow）、
    // 刀身をうっすら包む霞の帯（input.mist）、剣先の 3 分の 1 が通った所に残る残光（input.wake）。色は霞（フェーズ 1）と
    // 火（燃える刀身・フェーズ 2）の 2 つ、明るさは待機の倍率（霞の技の間は hazeBoost を掛ける）。残光は剣先の速さが
    // wakeMinTipSpeed から wakeFullTipSpeed (m/秒) の間で 0→1（速いほど明るく、帯の長さは資産の履歴の秒数）。
    HIKARI::GameProjectFloat3 flowMistColor{0.55f, 0.90f, 1.50f};
    HIKARI::GameProjectFloat3 flowFireColor{1.70f, 0.70f, 0.20f};
    float flowIdle = 1.2f;
    HIKARI::GameProjectFloat3 mistMistColor{0.70f, 0.85f, 1.10f};
    HIKARI::GameProjectFloat3 mistFireColor{1.30f, 0.55f, 0.20f};
    float mistIdle = 1.0f;
    HIKARI::GameProjectFloat3 wakeMistColor{0.60f, 0.90f, 1.60f};
    HIKARI::GameProjectFloat3 wakeFireColor{1.80f, 0.75f, 0.22f};
    float wakeGain = 1.2f;
    float wakeMinTipSpeed = 3.0f;
    float wakeFullTipSpeed = 30.0f;
    // 剣先が地面（刀光を切る高さ trailGroundCut）からこの高さ (m) の間は、残光を弱める（地面に白い筋を引かない）。
    float wakeGroundFade = 1.0f;
    // 剣気を放つ瞬間（c-ecd963）：刀身を走る光（glintEffect）をもう一度、色を releaseGlint 倍にして出す。
    // releaseHoldTicks F の間、時間を releaseHoldScale 倍にする（全体の止め。0 F で止めない＝既定。ボスが振るたびに
    // プレイヤーの操作まで遅くなるので、実機で試して決める）。刀だけを止めると、止めの後に刀が跳んで追い付いた。
    float releaseGlint = 1.6f;
    int releaseHoldTicks = 0;
    float releaseHoldScale = 0.35f;

    // 目の光：兜に付くエフェクト（頭が動くと尾を引く光・暈・技を出すと決めた瞬間に横へ伸びる閃き）。空なら出さない。
    // 光の位置はエフェクト資産の中（面頬の真ん中の隙間の前の 1 点。make_kasumi.py が出す EYE_POINT と合わせる）。
    HIKARI::GameProjectVfxAssetReference eyesEffect{"a4f1c8e27b5d4390b61e2c9d3f7a0851", "Assets/Vfx/BossEyes.hvfx"};
    // 目の光の色と明るさ：ふだん（霞）と、フェーズ 2（火）。
    HIKARI::GameProjectFloat3 eyesMistColor{0.70f, 1.10f, 1.90f};
    HIKARI::GameProjectFloat3 eyesFireColor{3.60f, 1.30f, 0.30f};
    // 閃きの強さと、消えるまでの時間 (秒)。0 で出さない。
    float eyeFlare = 0.6f;
    float eyeFlareSeconds = 0.16875f;
    // 兜の中の灯（c-e0c500）：面頬の隙間から漏れる光を、兜の中に置いた本物の灯で出す。前を向く影付きのスポットライト
    // （隙間から漏れて、面頬の縁・襟・顔の前の手を照らす）と、兜の中を満たす弱い点光（影なし）。どちらもシーンの物体で、
    // 置き場所と明るさはコードが毎フレーム入れる（色は上の目の光の色）。物体が無ければ、前の貼り付けの光（eyesEffect）を
    // 出す。明るさは灯の物体の単位（場面の光に対する倍率）。置き場所は兜の大きさ 1 の時の局所の値（+Z が前）。スポットは
    // 少し下へ向ける（度）。閃き（技を決めた瞬間・登場で目が灯る瞬間）は明るさに足す。ゆらぎは明るさに掛ける幅。
    uint64_t helmSpotObject = 0;
    uint64_t helmFillObject = 0;
    // 面頬の隙間の中の光（模型に足した自発光の面 HelmGlow。Tools/BossBody/add_helm_glow.py）：登場の間の明るさ（画面
    // 基準：色 × この値の 1 が、露出の後の白。昼でも夜でも同じ明るさに見える）。閃きとゆらぎは、灯と同じ倍率が掛かる。
    // 戦いの間の明るさと色は、模型の材質の値（add_helm_glow.py の GLOW_*・HOT_*）。
    float helmGlowStrength = 3.0f;
    float helmSpotIntensity = 3.0f;
    float helmFillIntensity = 0.25f;
    HIKARI::GameProjectFloat3 helmSpotLocal{0.0f, 0.03f, -0.06f};
    HIKARI::GameProjectFloat3 helmFillLocal{0.0f, 0.03f, 0.04f};
    float helmSpotPitchDegrees = 4.0f;
    float helmFlareGain = 1.0f;
    float helmGlowShare = 0.35f;
    float helmLightFlicker = 0.10f;

    // 刀身を走る光：刀を振る技を出すと決めた瞬間に、鍔元から剣先へ光が走る（1 回きりのエフェクト）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference glintEffect{"c7e2b9041d6f4a8e95b3f0a1d2c4e687",
                                                     "Assets/Vfx/BossBladeGlint.hvfx"};
    HIKARI::GameProjectFloat3 glintMistColor{0.80f, 1.10f, 1.60f};
    HIKARI::GameProjectFloat3 glintFireColor{1.80f, 0.90f, 0.35f};

    // 溜めの気配：溜めている間、体を包んで立ちのぼるエフェクト。空なら出さない。
    HIKARI::GameProjectVfxAssetReference auraEffect{"5b0d3e9a7c124f6e8a2b1c0d9e3f4a75", "Assets/Vfx/BossAura.hvfx"};
    HIKARI::GameProjectFloat3 auraMistColor{0.45f, 0.75f, 1.40f};
    HIKARI::GameProjectFloat3 auraFireColor{1.60f, 0.55f, 0.12f};
    // 気配が立ち上がる・引く時間 (秒)。
    float auraSeconds = 0.25f;

    // 胴の煙を技に合わせる：溜め切った時の量（倍率）、フェーズ 1 で刀が燃えている間の熱さ（熱い煙は橙に光る）、
    // 振り終わった直後に吹き出す量（倍率）とその時間 (秒)。
    // （c-66ec5e：溜め 2.4・吹き出し 2.5 倍では煙が膨らんで画面を塞いだので下げた。）
    float smokeChargeAmount = 1.6f;
    float smokeLitHeat = 5.0f;
    float smokeSwingPuff = 1.5f;
    float smokeSwingPuffSeconds = 0.30f;
    // プレイヤーが近い時は胴の煙を薄くする（カメラがボスの近くまで寄って、煙が画面を覆わないように。c-66ec5e）：
    // smokeNearMeters 以内で smokeNearScale 倍、smokeFarMeters 以遠で元の量、その間はなめらかに。
    float smokeNearScale = 0.35f;
    float smokeNearMeters = 5.0f;
    float smokeFarMeters = 12.0f;

    // 衝撃の輪：重い一撃が当たった瞬間に、地面に沿って広がるゆがみと光の輪（1 回きりのエフェクト）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference shockEffect{"e1a7c3f05b2d4d96a8c4f7b16d0e3a29", "Assets/Vfx/BossShock.hvfx"};
    HIKARI::GameProjectFloat3 shockMistColor{0.80f, 1.10f, 1.70f};
    HIKARI::GameProjectFloat3 shockFireColor{2.00f, 0.85f, 0.30f};
    // 回し蹴りの輪の大きさ（倍率。1 で半径 約 3.4 m まで広がる。0 で出さない）。
    float shockKick = 1.0f;

    // 突進の尾：体が速く動く間、肩と背から流れる光の帯と、空気のゆがみの尾を引く。空なら出さない。
    HIKARI::GameProjectVfxAssetReference dashEffect{"2c9f6b1e8a7d43f0b5e3d1c0a9f84726", "Assets/Vfx/BossDash.hvfx"};
    HIKARI::GameProjectFloat3 dashMistColor{0.60f, 0.90f, 1.50f};
    HIKARI::GameProjectFloat3 dashFireColor{1.80f, 0.70f, 0.20f};
    // 尾が出始める体の速さと、出切る速さ (m/秒)。
    float dashMinSpeed = 9.0f;
    float dashFullSpeed = 18.0f;

    // 左手の光：投げナイフの前隙の間、掌に集まる光（暈・芯・ゆがみ・速く動いた時の尾）。左手の掌に付けて出し続ける。
    // 空なら出さない。
    HIKARI::GameProjectVfxAssetReference handEffect{"7e3a9c5d1b8f4e62a0d4c6b2f1e8a937", "Assets/Vfx/BossHand.hvfx"};
    // 左手の弾け：ナイフを投げる瞬間に、手元で弾ける閃きと輪（1 回きり）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference handBurstEffect{"0f6d2b8e4c1a49d7b3e5a7c9d1f20b64",
                                                         "Assets/Vfx/BossHandBurst.hvfx"};
    // 色と明るさ。
    HIKARI::GameProjectFloat3 handMistColor{0.70f, 1.05f, 1.80f};
    // 光の強さ（引いた手。0 で出さない）。
    float handKnife = 0.5f;
    // 光が立ち上がる・引く時間 (秒) と、弾けの大きさ（倍率。0 で出さない）。
    float handSeconds = 0.12f;
    float handBurst = 1.0f;
    // 光を置く所（左手の模型の座標・実寸 m。掌の真ん中の少し前）。
    HIKARI::GameProjectFloat3 handPoint{0.0f, -0.07f, 0.20f};
    // 範囲爆発の溜め（c-3c908c）：叩きつけの前に、左手へ霧（剣が燃えている技は火）を集めて掌へ押し縮める。手に付いて動く
    // 流体域を持つエフェクト（霞／火。両方空なら出さない）と、押し縮める瞬間の一度きりのエフェクト。
    HIKARI::GameProjectVfxAssetReference gatherMistEffect{};
    HIKARI::GameProjectVfxAssetReference gatherFireEffect{};
    HIKARI::GameProjectVfxAssetReference gatherPinchEffect{};
    // 拍：叩きつけ（輪を放つ tick）の何 F 前から集め始めるか、何 F 前に押し縮めるか（集めるのを止め、内へ縮む輪と一閃を
    // 出す。その後は、縮んだ芯だけが手に付いて落ちる）。
    int gatherLeadFrames = 62;
    int gatherPinchFrames = 22;
    // 掌の芯の光（倍率）：集め終わりの明るさと、押し縮めた後の明るさ。押し縮めた後の細かい明滅の深さ（0 で明滅なし）。
    float gatherCore = 0.45f;
    float gatherCorePinched = 1.6f;
    float gatherCoreFlicker = 0.18f;
    // 押し縮める瞬間のエフェクトの色（霞／火）。
    HIKARI::GameProjectFloat3 gatherMistColor{0.78f, 0.92f, 1.0f};
    HIKARI::GameProjectFloat3 gatherFireColor{1.0f, 0.58f, 0.22f};
    // 集めている間、足元の霧と場の火を、手の真下へ寄せる加速度 (m/s²。0 は寄せない) と、寄せる輪の半径 (m)。
    float gatherFogPull = 30.0f;
    float gatherFogPullRadius = 4.0f;
    // 掌へ渦を巻いて流れ込む腕（手の真下の地面の輪から立ちのぼる）：本数（8 まで）、1 本の濃さ（中心での毎秒の密度）と
    // 太さ (m)、地面の輪の半径 (m)、掌へ着くまでに回る回数、1 秒に流れ込む回数（集め始め → 押し縮める直前。終わりへ
    // 向けて速くなる）、温度（霞／火。光り方）。
    int gatherArms = 6;
    float gatherArmStrength = 1600.0f;
    float gatherArmRadius = 0.28f;
    float gatherArmReach = 2.6f;
    float gatherArmTurns = 0.6f;
    float gatherArmRate = 1.8f;
    float gatherArmRateEnd = 3.4f;
    float gatherArmHeatMist = 1.6f;
    float gatherArmHeatFire = 8.0f;
    // …昇り方の曲がり（大きいほど、地面を長く這ってから一気に昇る）と、輪の半径のばらつき（0〜1。腕ごとに内へ寄せる）。
    float gatherArmLift = 1.7f;
    float gatherArmSpread = 0.25f;
};

// 技の演出の実行時の状態（保存しない）。
struct BossBodyAccentState {
    // 刀に付くエフェクトの再生。
    HIKARI::GameProjectVfxPlaybackHandle bladeVfx{};
    // 燃える刀身の流体の炎の再生（刀が燃えている間だけ）と、速く振る間にシェーダーの炎の帯を戻している割合（0〜1）。
    HIKARI::GameProjectVfxPlaybackHandle bladeFireVfx{};
    float bladeFlameFast = 0.0f;
    // 刀光を出し続ける残り時間 (秒)、刀光の明るさ（0〜1）、刀身の炎の明るさ（0〜1）。
    float trailHold = 0.0f;
    float trailGain = 0.0f;
    float heatGain = 0.0f;
    // 振りのゆがみの強さ（0〜1）。
    float hazeGain = 0.0f;
    // 刃口の常時の光のいまの明るさ（倍率）と脈の時計 (秒)、暗い縁のいまの濃さ。
    float edgeGain = 0.0f;
    float edgeClock = 0.0f;
    float edgeDarkGain = 0.0f;
    // 刃文の流れ・霞の帯の段階の倍率と、剣先の残光の明るさ（c-ecd963。0〜1）。
    float flowGain = 0.0f;
    float wakeGain = 0.0f;
    // 目の光・刀身を走る光・溜めの気配の再生。
    HIKARI::GameProjectVfxPlaybackHandle eyesVfx{};
    // 兜の中の灯：ゆらぎの時計 (秒)。
    float helmClock = 0.0f;
    HIKARI::GameProjectVfxPlaybackHandle glintVfx{};
    HIKARI::GameProjectVfxPlaybackHandle auraVfx{};
    // 目の閃きの明るさ（出した瞬間の強さから 0 へ）、気配の強さ（0〜1）、前のフレームに構えの合図が出ていたか。
    float flare = 0.0f;
    // 骨格の体の面頬の隙間の光を、目の色で上書きしているか（c-271cba）。
    bool bodyGlowOverridden = false;
    float aura = 0.0f;
    bool wasGlint = false;
    // 見た剣気を放った数（c-ecd963。BossComponent::releaseCount と比べて、放つ瞬間を見つける）。
    uint32_t releaseSeen = 0;
    // 見た怯みの数（c-808641。怯みの始まりに胴の煙を吹き出す）。
    uint32_t flinchSeen = 0;
    // 胴の煙へ掛ける量と熱さ（倍率）、振り終わりの吹き出しの残り（1→0）、前のフレームが命中の窓だったか。
    float smokeAmount = 1.0f;
    float smokeHeat = 1.0f;
    float puff = 0.0f;
    bool wasWindow = false;
    // 突進の尾の再生と明るさ（0〜1）、前のフレームの足元（速さを出す）。
    HIKARI::GameProjectVfxPlaybackHandle dashVfx{};
    float dash = 0.0f;
    HIKARI::GameProjectFloat3 previousFeet{};
    bool feetSeen = false;
    // 衝撃の輪を出す瞬間を見分ける、前のフレームの回し蹴りの輪（shockSeen が偽の間は、まだ見ていない）。
    bool shockSeen = false;
    uint64_t previousRing = 0;
    // 左手の光の再生と明るさ（0〜1）、前のフレームの所作・技の段（弾ける瞬間を見分ける。-1＝まだ見ていない）。
    HIKARI::GameProjectVfxPlaybackHandle handVfx{};
    float hand = 0.0f;
    int previousGesture = -1;
    int previousHandPhase = -1;
    // 範囲爆発の溜め：手に集めるエフェクトと押し縮める瞬間のエフェクトの再生、出しているのが火の版か、もう押し縮めたか、
    // 芯の光（追う値）、明滅の時計、叩きつけの後に消すまでの残り (秒)、足元の霧を寄せる強さ（0〜1。BossSmoke が読む）。
    HIKARI::GameProjectVfxPlaybackHandle gatherVfx{};
    HIKARI::GameProjectVfxPlaybackHandle pinchVfx{};
    bool gatherFire = false;
    bool gatherPinched = false;
    float gatherCoreGain = 0.0f;
    float gatherClock = 0.0f;
    float gatherLinger = 0.0f;
    float gatherPull = 0.0f;
    // 掌へ流れ込む腕（BossSmoke が読む）：濃さ（0〜1。0 は出さない）、回りの進み（周の数）、掌の位置（世界）。
    float gatherArm = 0.0f;
    float gatherPhase = 0.0f;
    HIKARI::GameProjectFloat3 gatherPoint{};
};

// ボスの体の見た目の、人が調整する値（体に付く効果の大きさと置き場所・胴の煙と火・フェーズ 2 の場）。体そのものは
// 骨格の体（BossAnimBody）か登場の役者が描く。ボスの部品（BossComponent）が持ち、項目は「体」の節に出る。
struct BossBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 描画のフレームで、体と体に付く効果（胴の煙・突進の尾など）を前の tick とこの tick の間で補間して置く（エンジンが
    // 体を描くのと同じ。切ると tick の位置に置く：描画が tick より速い時、速い動きでがくつく）。
    bool interpolate = true;
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。見える刀の長さ（swordVisibleLength）に合うように拡大する。
    // 模型を作るスクリプトが出す値（KATANA_LENGTH）に合わせる。
    float swordModelLength = 2.3411f;
    // 見える刀の、剣の付け根（Boss.swordShoulder）から剣先までの長さ。刀も手もこの長さに合わせて大きくなる。
    // 当たりの線（Boss.swordLength＋swordHazeLength）より先へは伸ばさない（当たらない刃を見せない）。0 なら Boss.swordLength。
    float swordVisibleLength = 5.13f;
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    float swordGripAlong = 0.0945f;
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    float handScale = 1.25f;
    // 体の大きさ（倍率）：鎧（胸当て・兜・肩当て・草摺）に付く効果の大きさに掛ける（下の位置の値は 1 倍の体での値）。体を大きくする時は、Boss.bodyHalfHeight・bodyRadius・シーンのカプセル・
    // 胴の煙のエフェクト（make_body_smoke.py の scale）も一緒に変える（調整ガイド g-fe0396 の一覧）。
    // 1.6875＝身長 6.75 m（c-a30eaf で 1.875、c-e97b18 でその 0.9 倍）。
    float bodyScale = 1.6875f;
    // 籠手の筒の模型の長さ（手首から先まで、模型の実寸 m。模型を作るスクリプトの build_cuff の length）。
    float cuffModelLength = 0.62f;

    float headScale = 1.16f;
    // 草摺（ノード KusazuriPanel）：枚数、吊る高さ、腰の輪郭の半径（横・前後）、裾を外へ開く角度。上の縁は胸当ての
    // 下の縁の内に隠す（c-584825）。いまは外してある（0 枚。揺れ・めり込みの不具合が多いため。直すまで出さない。
    // 0 の間は、登場の役者の草摺も隠す）。戻す時は 7。
    int tassetCount = 0;

    // 胴の煙：ボスの足元に付いて動く流体のエフェクト（鎧の中を満たし、裾から流れ落ちる）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference smokeEffect{"7c41e9d2a05b4f6e9b3d8a1f2c6e5d04",
                                                     "Assets/Vfx/BossBodySmoke.hvfx"};
    // 煙の量（倍率）と、フェーズ 2 で煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）。
    // （c-9143b5：視界を塞がないよう 1.0 → 0.8。）
    float smokeAmount = 0.8f;
    // （c-857ae3：胴の煙の発光を強めて火の色にしたので 5 → 1.6。芯の煙は暗い橙の熾火の色に留まり（鎧を覆う大きな光に
    // ならない）、炎は襟と袖口の火の舌が受け持つ。）
    float smokeHeatPhaseTwo = 1.6f;
    // 気絶している間の煙の量（倍率。煙が薄れて、鎧がからっぽに見える）。
    float smokeAmountStunned = 0.35f;
    // 骨に付いて行く煙（c-271cba）：骨格の体の、腰から胸への背骨に沿う点と、両腕（肩 → 肘 → 袖口）に沿う点から、胴の煙の
    // 域へ煙を入れる。体が傾く・屈む・腕を振る時に、煙が鎧に付いて行き、胴と手の間（腕）が煙でつながる。背骨・腕の
    // 毎秒の濃さ（0 で出さない）と半径（1 倍の体での m）、腕の煙が手の方へ流れる速さ (m/秒)。
    float spineSmoke = 26.0f;
    float spineSmokeRadius = 0.30f;
    float armSmoke = 34.0f;
    float armSmokeRadius = 0.17f;
    float armSmokeFlow = 0.8f;
    // 胴の煙を体の傾きに付ける（フェーズの変わり目の演出の間。c-271cba）：胴の煙の域を、腰を軸にして背骨の向き
    // （腰 → 胸）へ倒す。smokeTilt は付いて行く割合（0 倒さない 〜 1 背骨の向きまで）、smokeTiltMaxDegrees は倒す角度の
    // 上限（度）、smokeTiltFollow は寄せる速さ（1/秒。大きいほど体の動きにすぐ付く）、smokeHipsHeight は胴の煙の資産の
    // 中の腰の高さ（足元から m。立ち姿の腰の骨の高さ。ここを軸に倒す）。
    float smokeTilt = 1.0f;
    float smokeTiltMaxDegrees = 62.0f;
    float smokeTiltFollow = 7.0f;
    // 体がほとんど動いていない間（座り込み・待機）に、域が腰と背骨の細かい揺れへ寄る速さ（1/秒。小さいほど煙が
    // 落ち着く。大きく動いた時は smokeTiltFollow で追う）。
    float smokeCalmFollow = 0.7f;
    float smokeHipsHeight = 2.9f;
    // 戦いの間（骨格の体が出ている間）も、胴の煙の域を背骨へ倒す割合（0 倒さない＝立ったまま 〜 1。屈む・のけぞる体に、
    // 煙の柱が付いて行く）と、その間の寄せる速さ（1/秒。技の屈みは速いので、演出の smokeTiltFollow より速く追う）。
    float smokeTiltCombat = 1.0f;
    float smokeTiltCombatFollow = 12.0f;
    // フェーズ 2 の本体の鎧を、灼けた鎧（継ぎ目の糸と隙間の闇が熾火に光る模型のノード ○○Hot）に差し替える（c-857ae3）。
    bool armorHot = true;

    // フェーズ 2 の燃える胴（c-857ae3）：フェーズ 2 に入ると fireRiseSeconds かけて火が付き、撃破で同じ速さで消える。
    // 熱さは smokeHeatPhaseTwo へ寄せ、煙の量は smokeAmountPhaseTwo 倍、火の粉と熱のゆがみ（胴の煙の資産の入力 burn）は
    // fireBurn 倍。気絶の間は火を fireStunnedShare の割合に弱める。
    float fireRiseSeconds = 1.5f;
    float smokeAmountPhaseTwo = 1.0f;
    float fireBurn = 1.0f;
    float fireStunnedShare = 0.35f;
    // 火の舌：襟の両脇（胴の中心から、1 倍の体の局所 m。右。左は左右を反転）と、籠手の筒の口（手首 0 〜 肘の側 1 の
    // 割合 fireCuffAlong）から、燃料を上へ吹き出す（胴の煙の域だけに届く注入。昇りながら燃えて光る煤になる）。濃さは
    // 1 秒に入れる燃料（中心）、半径は 1 倍の体の m（体の大きさを掛ける）、熱さは入れる燃料の温度（0.3 で燃え始める）、
    // 速さは上へ m/秒。
    // 袖口の火は、襟の火に fireCuffShare を掛けた強さ。襟の火は片側に 2 本、前後に fireCollarSpread（1 倍の体の m）だけ
    // 離して並べる（太い 1 本より、細い本数で見せる方が大きさが揺れない）。
    HIKARI::GameProjectFloat3 fireCollar{-0.36f, 0.60f, -0.06f};
    float fireCollarSpread = 0.1f;
    float fireCuffAlong = 0.85f;
    float fireTongueDensity = 22.0f;
    float fireTongueRadius = 0.15f;
    float fireTongueHeat = 2.0f;
    float fireTongueRise = 2.0f;
    float fireCuffShare = 0.8f;
    // 腰の周りの火の舌（c-280b71。3 本：前の左右と後ろ）：胴の煙の柱の中を、下から火が上がる（体そのものが燃えて
    // 見える）。襟の火に対する強さ（0 で出さない）、半径の倍率、腰からの距離（1 倍の体の m）と、腰から下げる高さ（同）。
    float fireHemShare = 0.7f;
    float fireHemRadius = 1.5f;
    float fireHemRing = 0.42f;
    float fireHemDrop = 0.35f;

    // フェーズ 2 の場（c-857ae3）：場の色（環境の色調整を、暗部・中間・明部の色味で暖かく置き換える。1 で変えない）が
    // fieldWarmSeconds かけて変わり、撃破で戻る。漂う火の粉はボスの足元に付けて出し（量は fieldEmbers 倍）、焦げた輪は
    // 変わり目にボスが立っていた所に fieldScorchSeconds かけて現れる（濃さ fieldScorch・熾火の明るさ fieldScorchGlow・
    // 大きさ fieldScorchSize 倍）。エフェクトが空なら火の粉と焦げた輪を出さない。
    HIKARI::GameProjectVfxAssetReference fieldEffect{"5be8f2a1c94d4e7b8a3f61d2c7e09b54",
                                                     "Assets/Vfx/BossPhaseTwoField.hvfx"};
    float fieldWarmSeconds = 4.0f;
    HIKARI::GameProjectFloat3 fieldShadowsTint{1.18f, 0.92f, 0.76f};
    HIKARI::GameProjectFloat3 fieldMidtonesTint{1.2f, 0.98f, 0.76f};
    HIKARI::GameProjectFloat3 fieldHighlightsTint{1.08f, 0.98f, 0.86f};
    float fieldEmbers = 1.0f;
    float fieldScorch = 1.0f;
    // （stageB：熾火の橙の斑は「燃える地面」と紛れるので、既定は出さない。焦げは黒い斑だけ。）
    float fieldScorchGlow = 0.0f;
    float fieldScorchSize = 1.0f;
    float fieldScorchSeconds = 2.5f;


    // 技の点描の演出。
    BossBodyAccentSettings accents;

private:
    void DescribeShape(HIKARI::PropertyFields& fields);
};

// 草摺の枚数の上限（表示の番号を先に取っておく数）。
inline constexpr int kBossBodyTassetCapacity = 12;

// 体の見た目の実行時の状態（保存しない）。
struct BossBodyState {
    HIKARI::GameProjectVfxPlaybackHandle smokeVfx{};
    // フェーズ 2 の燃える胴の強さ（0〜1。c-857ae3）。
    float fire = 0.0f;
    // フェーズ 2 の場（BossPhaseTwoField）：場の色の進み（0〜1）と、最後に送った進み（負は「置き換えていない」）、
    // 置き換える前の色調整（初めて置き換える時に覚える）、火の粉と焦げた輪のエフェクト、焦げた輪の場所と進み。
    float fieldWarm = 0.0f;
    float fieldWarmSent = -1.0f;
    bool fieldBaseSeen = false;
    HIKARI::GameProjectColorGrading fieldBase{};
    HIKARI::GameProjectVfxPlaybackHandle fieldEmbersVfx{};
    HIKARI::GameProjectVfxPlaybackHandle fieldScorchVfx{};
    HIKARI::GameProjectFloat3 fieldScorchAt{};
    bool fieldScorchPlaced = false;
    float fieldScorchShow = 0.0f;
    // 左手の所作（技の型から。手の光の演出が読む）。
    BossBodyLeftGesture leftGesture = BossBodyLeftGesture::Balance;
    BossBodyAccentState accents;
};

} // namespace Project_TD2_2
