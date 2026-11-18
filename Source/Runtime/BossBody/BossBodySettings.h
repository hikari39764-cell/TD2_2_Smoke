#pragma once

#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

// 左手の形（模型のノードを差し替える。指の骨は無い）。
enum class BossBodyHandShape { Fist, Open, Seal };

// 左手の手振り（技の型ごとに決める。BossBodyLeftHand の表）。
enum class BossBodyLeftGesture { Balance, Detonate, Knife, Seal, FlameWipe, Aim, TwoHand, Kick, Limp };

// 左手の手振り（見た目だけ。当たり・F 数は変えない）。位置は体の足元から局所 m（+X が左、+Z が前）。
// 向きは局所の向き（長さは問わない）。模型の左手：+Z＝指先、+Y＝手の甲（掌は -Y）、親指は -X の側。
struct BossBodyLeftHandSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 手の位置のばね（1 秒あたりの揺れの回数・収まりの強さ）、素早い動き（投げる・拳を落とす）のばねの速さ、
    // 手の向きを追う時間 (秒)、刀に手を添える（両手持ち・刀身をなぞる）重みを上げ下げする時間 (秒)。
    float frequency = 4.0f;
    float damping = 0.75f;
    float snapFrequency = 10.0f;
    float turnSeconds = 0.08f;
    float attachSeconds = 0.08f;

    // 引爆：開いた手を顔の前へ掲げ（前隙のこの割合で上げ切る）、溜めの間に胸へ寄せ、爆ぜ始めに拳を握って下へ落とす。
    // 掌はプレイヤーへ（手の甲が自分の側）、指先は上。
    HIKARI::GameProjectFloat3 detonateRaise{0.45f, 2.9f, 0.9f};
    HIKARI::GameProjectFloat3 detonateChest{0.30f, 2.55f, 0.85f};
    HIKARI::GameProjectFloat3 detonateFist{0.30f, 2.25f, 0.90f};
    HIKARI::GameProjectFloat3 detonateFingers{0.0f, 1.0f, 0.1f};
    HIKARI::GameProjectFloat3 detonateBack{0.0f, 0.1f, -1.0f};
    float detonateRaiseShare = 0.35f;
    // 引爆：爆ぜ終わって後隙へ入った後も、拳を握ったままにする F。
    int detonateFistHoldFrames = 12;
    // 投げナイフ：前隙は刀印で左肩の後ろ上へ引き（指先は上、手の甲は外）、攻撃中に前へ素早く振り出して開く
    // （指先は前、手の甲は上）。
    HIKARI::GameProjectFloat3 knifePull{0.9f, 3.0f, -0.3f};
    HIKARI::GameProjectFloat3 knifePullFingers{0.1f, 1.0f, -0.2f};
    HIKARI::GameProjectFloat3 knifePullBack{1.0f, 0.0f, 0.1f};
    HIKARI::GameProjectFloat3 knifeThrow{0.55f, 2.8f, 1.4f};
    HIKARI::GameProjectFloat3 knifeThrowFingers{0.0f, 0.1f, 1.0f};
    HIKARI::GameProjectFloat3 knifeThrowBack{0.15f, 1.0f, 0.0f};
    // 霞分身・霞隠れの前隙：顔の前で刀印を立てる。指先は上、手の甲は左（外）、親指の側は顔へ。
    HIKARI::GameProjectFloat3 sealWrist{0.18f, 2.95f, 0.85f};
    HIKARI::GameProjectFloat3 sealFingers{0.0f, 1.0f, 0.0f};
    HIKARI::GameProjectFloat3 sealBack{1.0f, 0.0f, 0.0f};
    // 炎走り：前隙の wipeStartShare〜wipeEndShare の間に、開いた手で刀身を鍔の側から剣先へなぞる（刀身の wipeFrom〜wipeTo の
    // 割合の所。鍔は握りから bladeStart m 先）。掌は刀身の平らな面に付け、指先は剣先の向き。
    float wipeStartShare = 0.10f;
    float wipeEndShare = 0.70f;
    float wipeFrom = 0.15f;
    float wipeTo = 0.85f;
    float bladeStart = 0.405f;
    // 左手の模型の掌の真ん中（模型の座標・実寸 m）。刀身をなぞる時、ここが刀身に付く。
    HIKARI::GameProjectFloat3 palmCenter{0.0f, -0.045f, 0.148f};
    // 退き溜め（溜めている間）：手を前へ伸ばしてプレイヤーへ掌を向ける（指先は上）。
    HIKARI::GameProjectFloat3 aimWrist{0.35f, 2.6f, 1.3f};
    HIKARI::GameProjectFloat3 aimFingers{0.0f, 1.0f, 0.15f};
    HIKARI::GameProjectFloat3 aimBack{0.0f, 0.0f, -1.0f};
    // 両手持ち（霞の大剣・天降り斬り）：拳を右手の後ろ（柄頭の側。握りから刀の -Z へ twoHandBehind m）に握る。
    // 親指の側は剣先へ、手の甲は twoHandBack（刀の座標：+X が刀の左、+Y が棟）。拳の真ん中（模型の座標・実寸 m）が柄に乗る。
    float twoHandBehind = 0.81f;
    HIKARI::GameProjectFloat3 twoHandBack{0.4f, 0.92f, 0.0f};
    HIKARI::GameProjectFloat3 fistCenter{0.0f, -0.102f, 0.239f};
    // 回し蹴り：左の外へ手を上げて釣り合いを取る（指先は外、手の甲は上）。
    HIKARI::GameProjectFloat3 kickWrist{1.35f, 2.5f, 0.1f};
    HIKARI::GameProjectFloat3 kickFingers{1.0f, 0.15f, 0.0f};
    HIKARI::GameProjectFloat3 kickBack{0.0f, 1.0f, 0.0f};
    // 気絶・撃破：体の近くに垂らす（指先は下、手の甲は外）。
    HIKARI::GameProjectFloat3 limpWrist{0.75f, 1.55f, 0.15f};
    HIKARI::GameProjectFloat3 limpFingers{0.0f, -1.0f, 0.1f};
    HIKARI::GameProjectFloat3 limpBack{1.0f, 0.0f, 0.0f};
};

// 刀と手の動き（見た目だけ）。規則の剣の線（BossPose）・当たり・F 数は読むだけで変えない。
// 刀は「見せる構え」を目標にばねで追い、振っている間（命中の窓）だけ規則の剣の線へ重ねる（見える刀＝当たる刀）。
// 位置は体の足元から、局所 m（+X が左、+Z が前）。向きは局所の向き（長さは問わない）。
struct BossBodyMotionSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 刀の向きのばね（窓の外）：戻る速さ（1 秒あたりの揺れの回数）と収まりの強さ（小さいほど行き過ぎて戻る）。
    float swordFrequency = 2.4f;
    float swordDamping = 0.42f;
    // 振る直前（窓の少し前）に規則の線へ寄せるばねの速さ・収まり。
    float swordHitFrequency = 9.0f;
    float swordHitDamping = 0.9f;
    // 命中の窓の間に許す、剣先と規則の線のずれ (m)。
    float hitMaxTipGap = 0.081f;
    // 振り抜いた勢いを、窓の後へ持ち越す割合（大きいほど振り抜いた先へ行き過ぎる）。
    float overshootCarry = 0.30f;
    // 両手の型の間（規則の構えの twoHand の重み）は、窓の外も刀と握りを行き過ぎずに追う（規則の振りが速さの途切れない
    // 曲線になったので、ばねで行き過ぎ・戻りを足さない）。その追う速さ（1 秒あたりの回数）。
    float followFrequency = 7.0f;
    // 振りの何 F 前から規則の線へ寄せ始めるか（溜めの誇張もこの間に消す）。
    int lockLeadFrames = 8;
    // 窓が閉じた後、手と刀を体の傾きへ戻す時間 (秒)。
    float bodyBlendSeconds = 0.10f;

    // 待機の構え（下段を右の外へ開く。正面のプレイヤーから刀の長さが見える）：握りの位置と刀の向き。規則の剣が下ろした構えからこの角度 (度) 離れるまでは、
    // この構えを混ぜる（待機・溜めの始め・後隙の後半）。
    HIKARI::GameProjectFloat3 idleGrip{-0.62f, 1.70f, 0.50f};
    HIKARI::GameProjectFloat3 idleDirection{-0.60f, -0.36f, 0.71f};
    float restBlendDegrees = 40.0f;
    // 待機の息：握りの上下 (m)、剣先の上下 (度)、速さ (1 秒あたりの回数)。
    float breathMeters = 0.03375f;
    float breathDegrees = 1.5f;
    float breathFrequency = 0.28f;

    // 溜め：握りを剣の向きへ送り出す長さ (m) と、持ち上げる高さ (m)。構えている間の小さな震え（握り m・剣先 度・速さ）。
    float chargeReach = 0.405f;
    float chargeLift = 0.2025f;
    float chargeTremorMeters = 0.0135f;
    float chargeTremorDegrees = 0.6f;
    float chargeTremorFrequency = 11.0f;
    // 握りのばね（窓の外）：速さ（1 秒あたりの揺れの回数）と収まりの強さ。
    float gripFrequency = 3.2f;
    float gripDamping = 0.75f;
    // 振りの中ほどで握りを剣の線に沿って外へ伸ばす長さ (m) と、振りのどこ（0〜1）で伸び切るか。
    float armReach = 0.4725f;
    float armReachPeak = 0.55f;

    // 刃の向き：刃を動きへ向け始める剣先の速さ（度/秒。これより遅い時は前の向きを保つ。片手の振り）、振っている間に
    // 合わせる時間 (秒)、構えを移る間（振り上げ・両手の型の斬る向きへ）に回す時間 (秒)、待機の刃の向きへ戻る時間 (秒)。
    float edgeMinDegreesPerSecond = 40.0f;
    float edgeTurnSeconds = 0.04f;
    float edgeStanceSeconds = 0.07f;
    float edgeIdleSeconds = 0.6f;
    // 待機の刃の向き（局所。刀に直交する成分を使う）と、溜めの間に刃を向ける先（これから斬る向き。局所）。
    HIKARI::GameProjectFloat3 idleEdge{-0.5f, -1.0f, 0.35f};
    HIKARI::GameProjectFloat3 chargeEdgeToward{0.0f, -0.45f, 0.9f};

    // 胴の捻り：刀の横の向き（局所 x）1 あたりの角度 (度)、上限 (度)、追う時間 (秒)、草摺に掛ける割合。
    float twistDegrees = 26.0f;
    float twistMaxDegrees = 22.0f;
    float twistSeconds = 0.08f;
    float tassetTwistShare = 0.5f;
    // 兜がプレイヤーを向く：横の上限 (度) と追う時間 (秒)。
    float headTurnMaxDegrees = 50.0f;
    float headTurnSeconds = 0.25f;
    // 左手の釣り合い：刀の向きが待機の構えから外れた量 1 あたり、反対へ動く長さ (m)、上限 (m)、追う時間 (秒)。
    float leftBalanceMeters = 0.30f;
    float leftBalanceMax = 0.30f;
    float leftBalanceSeconds = 0.15f;

    // 剣先を地面からこの高さより下へ入れない (m)。下へ入る分は、剣の向きを起こして地面に沿わせる。負で切る。
    float tipGroundClearance = 0.05f;

    // 霞の大剣が出ている間は、見せる刀を大剣の柄と向きに重ねる（霧の大剣が刀を包む）。握りを柄から剣の向きへ
    // ずらす長さ (m) と、重ね始める・離す時間 (秒)。
    float greatswordGripAlong = 0.135f;
    float greatswordBlendSeconds = 0.15f;

    // 確かめ用：命中の窓の間、毎フレーム刃の向き・規則の線とのずれを記録に出す。
    bool traceMotion = false;
};

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
    // 刀身の輪郭の光（刀身の模型に掛ける材質の効果 HIKARI_Dissolve の輪郭の発光。シェーダーで刀身の全長が光る。色は
    // エンジンの青白）：両手の斬りの構え・溜めで灯り（溜めの強さで増す）、振る間は強く、振り終わりで消える。
    // 0 で掛けない。燃える刀身の間はこの割合へ弱める（炎と重ねて白く飛ばない）。
    float bladeRimKamae = 1.2f;
    float bladeRimSwing = 3.0f;
    float bladeRimCharge = 1.5f;
    float bladeRimHotShare = 0.35f;
    // 輪郭の光の鋭さ（大きいほど縁だけ細く光る）、明滅の速さ (rad/秒)、灯る・消える時間 (秒)。
    float bladeRimPower = 2.5f;
    float bladeRimPulse = 6.0f;
    float bladeRimRiseSeconds = 0.12f;
    float bladeRimFallSeconds = 0.3f;
    // 刃口の常時の光（刀に付くエフェクトの Edge Line・Edge Glow。入力 input.edge）：刀を見れば何をしているか分かるように、
    // 段階で明るさを変える（倍率。色は edgeMistColor／edgeFireColor に掛ける）。待機 edgeIdle は弱いが必ず見える、前隙は
    // 進みにつれて edgeWindup まで（曲がり edgeRampPower、溜めの見た目 1 あたり edgeCharge を足す、ゆっくり脈打つ）、
    // 振る間（命中の窓）は edgeSwing、後隙は待機へ戻る。気絶は edgeStunned。燃える刀身の間は edgeHotShare を掛ける
    // （炎と重ねて白く飛ばない）。地面や画面の予告ではなく、刀そのものの見え方。
    HIKARI::GameProjectFloat3 edgeMistColor{0.55f, 0.85f, 1.60f};
    HIKARI::GameProjectFloat3 edgeFireColor{1.80f, 0.75f, 0.22f};
    float edgeIdle = 0.35f;
    float edgeWindup = 0.9f;
    float edgeSwing = 1.6f;
    float edgeCharge = 0.5f;
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
    float realRimShare = 0.0f;
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
    HIKARI::GameProjectVfxAssetReference eyesEffect{"a4f1c8e27b5d4390b61e2c9d3f7a0851", "Assets/Vfx/BossEyes.hvfx"};
    // 目の光の色と明るさ：ふだん（霞）と、フェーズ 2（火）。
    HIKARI::GameProjectFloat3 eyesMistColor{0.70f, 1.10f, 1.90f};
    HIKARI::GameProjectFloat3 eyesFireColor{3.60f, 1.30f, 0.30f};
    // 閃きの強さ：ふつうの技・大技（引爆・天降り斬り・退き溜め・霞の大剣）と、消えるまでの時間 (秒)。0 で出さない。
    float eyeFlare = 0.6f;
    float eyeFlareBig = 1.6f;
    float eyeFlareSeconds = 0.45f;

    // 刀身を走る光：刀を振る技を出すと決めた瞬間に、鍔元から剣先へ光が走る（1 回きりのエフェクト）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference glintEffect{"c7e2b9041d6f4a8e95b3f0a1d2c4e687",
                                                     "Assets/Vfx/BossBladeGlint.hvfx"};
    HIKARI::GameProjectFloat3 glintMistColor{0.80f, 1.10f, 1.60f};
    HIKARI::GameProjectFloat3 glintFireColor{1.80f, 0.90f, 0.35f};

    // 溜めの気配：溜めている間、体を包んで立ちのぼるエフェクト。空なら出さない。
    HIKARI::GameProjectVfxAssetReference auraEffect{"5b0d3e9a7c124f6e8a2b1c0d9e3f4a75", "Assets/Vfx/BossAura.hvfx"};
    HIKARI::GameProjectFloat3 auraMistColor{0.45f, 0.75f, 1.40f};
    HIKARI::GameProjectFloat3 auraFireColor{1.60f, 0.55f, 0.12f};
    // 気配が立ち上がる・引く時間 (秒) と、霞の大剣を作り出す間の強さ（溜め切りを 1 として）。
    float auraSeconds = 0.25f;
    float auraConjure = 0.7f;

    // 胴の煙を技に合わせる：溜め切った時の量（倍率）、フェーズ 1 で刀が燃えている間の熱さ（熱い煙は橙に光る）、
    // 振り終わった直後に吹き出す量（倍率）とその時間 (秒)。
    // （c-66ec5e：溜め 2.4・吹き出し 2.5 倍では煙が膨らんで画面を塞いだので下げた。）
    float smokeChargeAmount = 1.6f;
    float smokeLitHeat = 5.0f;
    float smokeSwingPuff = 1.5f;
    float smokeSwingPuffSeconds = 0.30f;
    // プレイヤーが近い時は胴の煙を薄くする（カメラがボスの近くまで寄って、煙が画面を覆わないように。c-66ec5e）：
    // smokeNearMeters 以内で smokeNearScale 倍、smokeFarMeters 以遠で元の量、その間はなめらかに。
    float smokeNearScale = 0.55f;
    float smokeNearMeters = 4.0f;
    float smokeFarMeters = 10.0f;

    // 衝撃の輪：重い一撃が当たった瞬間に、地面に沿って広がるゆがみと光の輪（1 回きりのエフェクト）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference shockEffect{"e1a7c3f05b2d4d96a8c4f7b16d0e3a29", "Assets/Vfx/BossShock.hvfx"};
    HIKARI::GameProjectFloat3 shockMistColor{0.80f, 1.10f, 1.70f};
    HIKARI::GameProjectFloat3 shockFireColor{2.00f, 0.85f, 0.30f};
    // 輪の大きさ（倍率。1 で半径 約 3.4 m まで広がる。0 で出さない）：天降り斬りの着地、霞の大剣の叩きつけ、回し蹴り、
    // 引爆の爆ぜ始め、溜めた一撃を放つ瞬間（退き溜め）。
    float shockSkyfall = 1.6f;
    float shockCleave = 1.4f;
    float shockKick = 1.0f;
    float shockDetonate = 1.8f;
    float shockRelease = 0.9f;

    // 突進の尾：体が速く動く間、肩と背から流れる光の帯と、空気のゆがみの尾を引く。空なら出さない。
    HIKARI::GameProjectVfxAssetReference dashEffect{"2c9f6b1e8a7d43f0b5e3d1c0a9f84726", "Assets/Vfx/BossDash.hvfx"};
    HIKARI::GameProjectFloat3 dashMistColor{0.60f, 0.90f, 1.50f};
    HIKARI::GameProjectFloat3 dashFireColor{1.80f, 0.70f, 0.20f};
    // 尾が出始める体の速さと、出切る速さ (m/秒)。
    float dashMinSpeed = 9.0f;
    float dashFullSpeed = 18.0f;

    // 左手の光：所作の間、掌に集まる光（暈・芯・ゆがみ・速く動いた時の尾）。左手の掌に付けて出し続ける。空なら出さない。
    HIKARI::GameProjectVfxAssetReference handEffect{"7e3a9c5d1b8f4e62a0d4c6b2f1e8a937", "Assets/Vfx/BossHand.hvfx"};
    // 左手の弾け：握る・印を切る・投げる・刀身をなぞり終える瞬間に、手元で弾ける閃きと輪（1 回きり）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference handBurstEffect{"0f6d2b8e4c1a49d7b3e5a7c9d1f20b64",
                                                         "Assets/Vfx/BossHandBurst.hvfx"};
    // 色と明るさ：霞の所作（刀印・投げナイフ・狙い）と、火の所作（引爆・刀身をなぞる・刀が燃えている時の狙い）。
    HIKARI::GameProjectFloat3 handMistColor{0.70f, 1.05f, 1.80f};
    HIKARI::GameProjectFloat3 handFireColor{2.00f, 0.80f, 0.22f};
    // 所作ごとの光の強さ（0 で出さない）：引爆（掲げた掌）、刀印、投げナイフ（引いた手）、刀身をなぞる、狙い（溜め切り）。
    float handDetonate = 1.0f;
    float handSeal = 0.8f;
    float handKnife = 0.5f;
    float handWipe = 1.2f;
    float handAim = 0.9f;
    // 光が立ち上がる・引く時間 (秒) と、弾けの大きさ（倍率。0 で出さない）。
    float handSeconds = 0.12f;
    float handBurst = 1.0f;
    // 光を置く所（左手の模型の座標・実寸 m。掌の真ん中の少し前）。
    HIKARI::GameProjectFloat3 handPoint{0.0f, -0.07f, 0.20f};
};

// 技の演出の実行時の状態（保存しない）。
struct BossBodyAccentState {
    // 刀に付くエフェクトの再生。
    HIKARI::GameProjectVfxPlaybackHandle bladeVfx{};
    // 刀光を出し続ける残り時間 (秒)、刀光の明るさ（0〜1）、刀身の炎の明るさ（0〜1）。
    float trailHold = 0.0f;
    float trailGain = 0.0f;
    float heatGain = 0.0f;
    // 振りのゆがみの強さ（0〜1）。
    float hazeGain = 0.0f;
    // 刀身の輪郭の光のいまの強さ（材質の効果の rimStrength）。
    float bladeRim = 0.0f;
    // 刃口の常時の光のいまの明るさ（倍率）と脈の時計 (秒)、暗い縁のいまの濃さ。
    float edgeGain = 0.0f;
    float edgeClock = 0.0f;
    float edgeDarkGain = 0.0f;
    // 刃文の流れ・霞の帯の段階の倍率と、剣先の残光の明るさ（c-ecd963。0〜1）。
    float flowGain = 0.0f;
    float wakeGain = 0.0f;
    // 目の光・刀身を走る光・溜めの気配の再生。
    HIKARI::GameProjectVfxPlaybackHandle eyesVfx{};
    HIKARI::GameProjectVfxPlaybackHandle glintVfx{};
    HIKARI::GameProjectVfxPlaybackHandle auraVfx{};
    // 目の閃きの明るさ（出した瞬間の強さから 0 へ）、気配の強さ（0〜1）、前のフレームに構えの合図が出ていたか。
    float flare = 0.0f;
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
    uint64_t teleportSeen = 0;
    // 衝撃の輪を出す瞬間を見分ける、前のフレームの様子（-1＝まだ見ていない）。
    int previousSkyfall = -1;
    int previousGreatsword = -1;
    int previousActPhase = -1;
    uint64_t previousRing = 0;
    float previousCharge = 0.0f;
    // 左手の光の再生と明るさ（0〜1）、前のフレームの所作・手の形（弾ける瞬間を見分ける。-1＝まだ見ていない）。
    HIKARI::GameProjectVfxPlaybackHandle handVfx{};
    float hand = 0.0f;
    bool handFire = false;
    int previousGesture = -1;
    int previousShape = -1;
    int previousHandPhase = -1;
};

// ボスの外形（模型の部品）の、人が調整する値。ボスの部品（BossComponent）が持ち、項目は「外形」の節に出る。
// 模型は 1 つの資産（glTF）で、部品がノードに分かれて入っている。コードがノードごとに置き場所を決めて出す。
struct BossBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 外形の模型の資産（GUID）。空なら何も出さない。
    std::string model{"b1a52cf03fe7473cbbe74c310e9c5c29"};
    bool visible = true;
    // 描画のフレームで、外形（鎧・刀・手・胴の煙・突進の尾）を前の tick とこの tick の間で補間して置く（エンジンが体を
    // 描くのと同じ。切ると tick の位置に置く：描画が tick より速い時、速い動きでがくつく）。
    bool interpolate = true;
    // 外形の模型が影を落とすか。切ると軽くなるが、鎧の影が地面と鎧自身に落ちなくなる。
    bool castShadow = true;
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。見える刀の長さ（swordVisibleLength）に合うように拡大する。
    // 模型を作るスクリプトが出す値（KATANA_LENGTH）に合わせる。
    float swordModelLength = 2.3411f;
    // 見える刀の、剣の付け根（Boss.swordShoulder）から剣先までの長さ。刀も手もこの長さに合わせて大きくなる。
    // 当たりの線（Boss.swordLength＋swordHazeLength）より先へは伸ばさない（当たらない刃を見せない）。0 なら Boss.swordLength。
    // 刀の長さに付く見た目の値（握りの位置・両手の間隔・鍔の位置・溜めと振りの手の動き）は、刀と同じ倍率で揃えてある。
    float swordVisibleLength = 5.13f;
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    float swordGripAlong = 0.0945f;
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    float handScale = 1.25f;
    // 体の大きさ（倍率）：鎧（胴・兜・大袖・草摺）の置き場所と大きさ、肩、左手の手振りと待機の置き場所、待機の構えの握りに
    // 掛ける（下の位置の値は 1 倍の体での値）。体を大きくする時は、Boss.bodyHalfHeight・bodyRadius・シーンのカプセル・
    // 胴の煙のエフェクト（make_body_smoke.py の scale）も一緒に変える（調整ガイド g-fe0396 の一覧）。
    // 1.6875＝身長 6.75 m（c-a30eaf で 1.875、c-e97b18 でその 0.9 倍）。
    float bodyScale = 1.6875f;
    // 握りの右手の模型の形（刀の座標、模型の実寸）：手首の位置・肘の向き・手の甲の向き。籠手の筒を手首に付けるのに使う。
    // 模型を作るスクリプトが出す値（GRIP_WRIST・GRIP_ELBOW・GRIP_BACK）に合わせる。
    HIKARI::GameProjectFloat3 gripWrist{-0.0289f, 0.2395f, -0.1030f};
    HIKARI::GameProjectFloat3 gripElbow{0.3367f, 0.8333f, -0.4384f};
    HIKARI::GameProjectFloat3 gripBack{-0.9272f, 0.3746f, 0.0f};
    // 肩の位置（体の足元から、局所 m。右肩。左肩は左右を反転）。籠手の筒は、手首からこの肩の方へ伸びる
    // （腕は無いが、筒が肩を向くと腕があるように読める）。
    HIKARI::GameProjectFloat3 shoulder{-0.80f, 3.05f, 0.0f};
    // 筒の向きに、握りの手の模型の肘の向きを混ぜる割合（0 で肩をまっすぐ向く、1 で手の模型どおり）。
    float cuffGripBlend = 0.25f;
    // 籠手の筒の模型の長さ（手首から先まで、模型の実寸 m。模型を作るスクリプトの build_cuff の length）。
    float cuffModelLength = 0.62f;
    // 籠手の筒の先を、肩からこれだけ手前で止める（m。体の大きさを掛ける）。手を大きくすると筒が肩まで届いて大袖・胴に
    // 刺さるので、筒の長さだけを詰める（太さはそのまま）。0 で詰めない。
    float cuffShoulderClearance = 0.55f;
    // 詰める時の下限（模型の長さに対する割合。手が肩のそばに来ても筒が消えないように）。
    float cuffMinLength = 0.35f;
    // 左手（刀を持たない手）の、何もしていない時の置き場所：手首の位置（体の足元から、局所 m）と、
    // 指先・手の甲の向き（体の向きに対する局所の向き。長さは問わない）。
    HIKARI::GameProjectFloat3 leftRestWrist{1.0f, 1.80f, 0.35f};
    HIKARI::GameProjectFloat3 leftRestFingers{0.10f, -0.85f, 0.50f};
    HIKARI::GameProjectFloat3 leftRestBack{0.85f, 0.25f, 0.45f};

    // 鎧の部品の置き場所（体の足元から、局所 m。+X が左、+Z が前）。鎧は体の傾き・屈みに付いて動く。
    // 胴（ノード Do）の中心。模型を作るスクリプトの DO_CENTER_Y と合わせる。
    HIKARI::GameProjectFloat3 torsoCenter{0.0f, 2.50f, 0.0f};
    // 兜（ノード Kabuto・目の光）の中心と大きさ。
    HIKARI::GameProjectFloat3 headCenter{0.0f, 3.50f, 0.03f};
    float headScale = 1.16f;
    // 大袖（ノード Sode）：吊る所（右の袖。左は左右を反転）、外へ向ける角度（真横が 90 度。小さいほど前へ向く）、
    // 裾を外へ開く角度。
    HIKARI::GameProjectFloat3 sodeHang{-0.88f, 3.14f, 0.02f};
    float sodeYawDegrees = 76.0f;
    float sodeFlareDegrees = 24.0f;
    // 草摺（ノード KusazuriPanel）：枚数、吊る高さ、腰の輪郭の半径（横・前後）、裾を外へ開く角度。
    int tassetCount = 7;
    float tassetHangHeight = 2.02f;
    float tassetRadiusSide = 0.57f;
    float tassetRadiusFront = 0.50f;
    float tassetFlareDegrees = 15.0f;

    // 胴の煙：ボスの足元に付いて動く流体のエフェクト（鎧の中を満たし、裾から流れ落ちる）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference smokeEffect{"7c41e9d2a05b4f6e9b3d8a1f2c6e5d04",
                                                     "Assets/Vfx/BossBodySmoke.hvfx"};
    // 煙の量（倍率）と、フェーズ 2 で煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）。
    float smokeAmount = 1.0f;
    float smokeHeatPhaseTwo = 5.0f;
    // 気絶している間の煙の量（倍率。煙が薄れて、鎧がからっぽに見える）。
    float smokeAmountStunned = 0.35f;

    // 垂れた部品（大袖・草摺）の揺れ。体が動くと、動きと反対へ遅れて振れ、止まると行き過ぎて戻る。
    // 体の速さ 1 m/s あたりの振れ（度）、振れの上限（度）、戻る速さ（1 秒あたりの揺れの回数）、収まりの強さ（1 で揺れ戻さない）。
    float swayDegreesPerSpeed = 4.0f;
    float swayMaxDegrees = 40.0f;
    float swayFrequency = 1.6f;
    float swayDamping = 0.38f;
    // 止まっている時の小さな揺らぎ（度）と速さ（1 秒あたりの回数）。
    float flutterDegrees = 1.5f;
    float flutterFrequency = 0.45f;
    // 撃たれた時に、後ろへ弾かれる振れ（度）。
    float hitKickDegrees = 16.0f;

    // 刀と手の動き。
    BossBodyMotionSettings motion;
    // 左手の手振り。
    BossBodyLeftHandSettings leftHand;
    // 技の点描の演出。
    BossBodyAccentSettings accents;

private:
    void DescribeShape(HIKARI::PropertyFields& fields);
};

// 草摺の枚数の上限（表示の番号を先に取っておく数）。
inline constexpr int kBossBodyTassetCapacity = 12;

// 刀と手の動きの実行時の状態（保存しない）。向き・位置は体の向き（yaw）の局所（足元が原点）。
struct BossBodyMotionState {
    bool ready = false;
    // 規則の剣の向きと、その動く速さ（固定 tick ごとに測る。局所の接ベクトル rad/秒）。
    HIKARI::GameProjectFloat3 ruleDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 ruleVelocity{};
    uint64_t ruleTick = 0;
    float ruleClock = 0.0f;
    // 見せる刀の向き（剣先）・その速さ（接ベクトル rad/秒）・刃の向き（刀に直交）・刃を向けたい先。
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 velocity{};
    HIKARI::GameProjectFloat3 edge{0.0f, -1.0f, 0.0f};
    HIKARI::GameProjectFloat3 edgeGoal{0.0f, -1.0f, 0.0f};
    // 見せる握りの位置と速さ。
    HIKARI::GameProjectFloat3 grip{};
    HIKARI::GameProjectFloat3 gripVelocity{};
    // 規則の線（体の向きだけの枠）へ寄せる重み（0＝体の傾きに付く、1＝規則の線）。前のフレームが窓だったか。
    float lock = 0.0f;
    bool wasWindow = false;
    // 溜めの重み（見た目の震えに使う）。
    float charge = 0.0f;
    // 胴の捻り・兜の向き（体の向きからの rad）と、左手の釣り合いのずれ（局所 m）。
    float twist = 0.0f;
    float headYaw = 0.0f;
    HIKARI::GameProjectFloat3 leftOffset{};
    // 左手：手首の位置と速さ・指先と手の甲の向き（局所）、刀に添える重み、いまの形と手振り。
    HIKARI::GameProjectFloat3 leftWrist{};
    HIKARI::GameProjectFloat3 leftVelocity{};
    HIKARI::GameProjectFloat3 leftFingers{0.0f, -1.0f, 0.0f};
    HIKARI::GameProjectFloat3 leftBack{1.0f, 0.0f, 0.0f};
    float leftAttach = 0.0f;
    BossBodyHandShape leftShape = BossBodyHandShape::Open;
    BossBodyLeftGesture leftGesture = BossBodyLeftGesture::Balance;
    // 霞の大剣に重ねる重み（0〜1）と、確かめ用に覚える前の tick の大剣の向き（局所）・その tick。
    float great = 0.0f;
    HIKARI::GameProjectFloat3 greatPrevious{0.0f, 1.0f, 0.0f};
    uint64_t greatTick = 0;
    // 前のフレームの足元（確かめ用の記録に出す足元の速さを測る）と、前に見た規則の層の teleportCount。
    HIKARI::GameProjectFloat3 ground{};
    uint64_t teleportSeen = 0;
};

// 外形の実行時の状態（保存しない）。
struct BossBodyState {
    // 模型を出せなかった知らせは、1 回だけ記録に出す（毎フレーム出さない）。
    bool submitWarned = false;
    HIKARI::GameProjectVfxPlaybackHandle smokeVfx{};
    // 垂れた部品の振れ（度。世界座標の x・z が水平の向き、y が持ち上がり）と、その速さ。
    HIKARI::GameProjectFloat3 sway{};
    HIKARI::GameProjectFloat3 swayVelocity{};
    HIKARI::GameProjectFloat3 previousPosition{};
    bool swayReady = false;
    float clock = 0.0f;
    // 撃たれた数（増えたら、振れを弾く）。
    uint64_t seenHits = 0;
    // 兜が向く先：プレイヤーの位置（BossSystem が固定 tick ごとに書く。見た目だけが読む）。
    HIKARI::GameProjectFloat3 lookTarget{};
    bool lookTargetSeen = false;
    // 補間：前の tick とこの tick の終わりに体の物体へ置いた位置、覚えた tick の数（2 で補間できる）、前に見た
    // teleportCount（瞬間移動した tick は補間しない）。BossSystem が固定 tick の終わりに書く（BossBodyVisuals::NoteTick）。
    HIKARI::GameProjectFloat3 tickFrom{};
    HIKARI::GameProjectFloat3 tickTo{};
    int ticksSeen = 0;
    uint64_t tickTeleports = 0;
    // このフレームに外形をずらした量（補間した位置 − この tick の位置）。刀を規則の線へ寄せる枠も同じだけずらす。
    HIKARI::GameProjectFloat3 presentShift{};
    BossBodyMotionState motion;
    BossBodyAccentState accents;
};

} // namespace Project_TD2_2
