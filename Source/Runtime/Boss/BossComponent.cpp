#include "BossComponent.h"

namespace Project_TD2_2 {

void BossComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("技の表", true)) {
        fields.Text("movesPath", "技の表（Data/ からの場所）", movesPath);
        fields.Text("typesPath", "動きの型の既定値の表（Data/ からの場所）", typesPath);
        fields.Note("表は技を出すたびに読み直すので、Play 中に CSV を保存すると次に出す技から変わります。"
                    "HP は同じ物体の「被弾と HP」の最大 HP（試作 1500）です。");
    }
    if (fields.Section("斬撃の共通の値", true)) {
        fields.Float("range", "斬撃の飛距離 (m)", range, 1.0f, 200.0f);
        fields.Int("swingFrames", "剣を振り抜く長さ (F)", swingFrames, 1, 120);
        fields.Int("gapSmallFrames", "連撃の構え 小 (F)", gapSmallFrames, 0, 300);
        fields.Int("gapMediumFrames", "連撃の構え 中 (F)", gapMediumFrames, 0, 300);
        fields.Int("gapLargeFrames", "連撃の構え 大 (F)", gapLargeFrames, 0, 300);
        fields.Int("followFrames", "連撃の振り抜き後 (F)", followFrames, 0, 120);
        fields.Int("feintFrames", "フェイント（遅）の溜め (F)", feintFrames, 0, 300);
        fields.Int("kamaeFrames", "振る前の構え（剣を止めて溜める）(F)", kamaeFrames, 1, 300);
        fields.Int("raiseFrames", "振り上げ（構えの前に剣を引く）(F)", raiseFrames, 0, 300);
        fields.Int("swingInFrames", "振り出し（ゆっくり動き始める）(F)", swingInFrames, 0, 60);
        fields.Float("damageBase", "威力（空欄時）＝基本", damageBase, 0.0f, 1000.0f);
        fields.Float("damagePerFrame", "…＋隙 F × この係数", damagePerFrame, 0.0f, 10.0f, 0.01f);
        fields.Float("thickness", "斬撃の厚み (m)", thickness, 0.01f, 5.0f, 0.01f);
    }
    if (fields.Section("フェーズと気絶", true)) {
        fields.Float("phaseTwoHealthPercent", "フェーズ 2 になる HP (%)", phaseTwoHealthPercent, 0.0f, 100.0f);
        fields.Float("stunMax", "気絶値（これだけたまると気絶）", stunMax, 1.0f, 100000.0f);
        fields.Int("stunLimit", "気絶する回数の上限（0＝上限なし）", stunLimit, 0, 99);
        fields.Int("stunFrames", "気絶している長さ (F)", stunFrames, 1, 3600);
        fields.Int("breakHitStopFrames", "崩し・気絶の止め (F、0 は無効)", breakHitStopFrames, 0, 30);
        fields.Float("breakHitStopScale", "崩し・気絶の止め中の時間倍率", breakHitStopScale, 0.0f, 1.0f);
        fields.Float("breakCameraAmplitude", "崩し・気絶のカメラ揺れ幅 (m、0 は無効)", breakCameraAmplitude, 0.0f,
                     1.0f);
        fields.Int("breakCameraFrames", "崩し・気絶のカメラ揺れ時間 (F)", breakCameraFrames, 0, 120);
        fields.Float("breakCameraFrequency", "崩し・気絶のカメラ揺れ周波数 (Hz)", breakCameraFrequency, 0.0f, 60.0f);
        fields.Float("breakRumbleLow", "崩し・気絶の振動：重いモーター (0〜1)", breakRumbleLow, 0.0f, 1.0f);
        fields.Float("breakRumbleHigh", "崩し・気絶の振動：軽いモーター (0〜1)", breakRumbleHigh, 0.0f, 1.0f);
        fields.Int("breakRumbleFrames", "崩し・気絶の振動時間 (F、0 は無効)", breakRumbleFrames, 0, 120);
        fields.Int("breakFlashFrames", "崩し・気絶の本体の光り (F、0 は無効)", breakFlashFrames, 0, 120);
        fields.Float("breakBlastScale", "崩し・気絶の瞬間に体から弾ける塊の大きさ（当たった所の爆発に対する倍。0 で無効）",
                     breakBlastScale, 0.0f, 6.0f);
        fields.Note("光り方（色・強さ・材質）は Damageable の被弾の発光と同じです。");
        fields.Float("stunDamageScale", "気絶中に受けるダメージ (倍)", stunDamageScale, 0.0f, 10.0f);
        fields.Float("stunDecayDelaySeconds", "気絶値が減り始めるまで (秒)", stunDecayDelaySeconds, 0.0f, 120.0f);
        fields.Float("stunDecayPerSecond", "気絶値が 1 秒に減る量", stunDecayPerSecond, 0.0f, 10000.0f);
        fields.Float("stunPerShot", "気絶値：煙弾 1 発", stunPerShot, 0.0f, 10000.0f);
        fields.Float("stunReferenceDamage", "…この威力の命中を 1 発とみなす（砲撃の基礎威力）", stunReferenceDamage,
                     0.01f, 100000.0f);
        fields.Note("気絶値は受けた威力に比例してたまります（遠くで弱まった弾は少なく）。気絶中はたまりません。");
    }
    if (fields.Section("技の選び方（行動ツリーの節が使う値）", true)) {
        fields.Float("selectFarDistance", "距離重み 1.0 になる距離 (m)", selectFarDistance, 0.1f, 200.0f);
        fields.Float("selectMinWeight", "重みの下限（得意な距離から一番遠い技にも残す分）", selectMinWeight, 0.0f, 1.0f,
                     0.01f);
        fields.Float("selectPower", "得意な距離へのかたより（重みの指数。大きいほど距離に合う技ばかり出る）",
                     selectPower, 0.0f, 8.0f, 0.1f);
        fields.Float("selectRepeatScale", "直前に始動した技の重み (倍)", selectRepeatScale, 0.0f, 1.0f, 0.05f);
        fields.Float("moveHitBoost", "待機・移動中の被弾 1 回ごとの突進系の重み (+倍)", moveHitBoost, 0.0f, 10.0f,
                     0.1f);
        fields.Int("moveHitMax", "…数える被弾の上限 (回)", moveHitMax, 0, 40);
        fields.Float("interruptPower", "割り込みの技の、得意な距離へのかたより（重みの指数）", interruptPower, 0.0f,
                     8.0f, 0.1f);
        fields.Int("comboMaxChain", "コンボの最大連続 (技の数)", comboMaxChain, 1, 10);
        fields.Int("recoveryGuardFrames", "後隙のうち、割り込み・反応で切り上げない最初の F", recoveryGuardFrames, 0,
                   600);
        fields.Float("comboChancePercent", "コンボ確率（表の「コンボ確率%」が空欄のとき）(%)", comboChancePercent, 0.0f,
                     100.0f, 1.0f);
        fields.Float("rollAxisDegrees",
                     "ローリングの向き：ボスへ向かう・離れる向きからこの角度以内なら前後、外れたら横 (度)",
                     rollAxisDegrees, 0.0f, 90.0f, 1.0f);
        fields.Note("どの技を出すかは、同じ物体の Behavior Tree（木は KasumiBushi、Update は GameCode）が決めます。"
                    "技の間の待ち・連続攻撃とみなす命中数と時間は、木の節（待ち／連続攻撃された）で調整します。"
                    "プレイヤーへの反応（回復・吸い込み・ローリング・密着を咎める）は、木の枝「プレイヤーへの反応」の"
                    "節（条件と反応の間隔）で調整します。"
                    "距離重み・発生条件・コンボ先・コンボ確率は技の表の列です。");
    }
    if (fields.Section("体と浮遊")) {
        fields.Float("bodyHalfHeight", "体の高さの半分 (m)", bodyHalfHeight, 0.1f, 20.0f);
        fields.Float("bodyRadius", "体の半径 (m)", bodyRadius, 0.05f, 10.0f);
        fields.Float3("arenaCenter", "戦場の中心（地面）", arenaCenter);
        fields.Float("arenaRadius", "動ける半径 (m)", arenaRadius, 1.0f, 500.0f);
        fields.Float("walkSpeed", "移動の速さ (m/秒)", walkSpeed, 0.0f, 50.0f);
        fields.Float("floatMin", "浮く高さ 最低 (m)", floatMin, 0.0f, 10.0f);
        fields.Float("floatMax", "浮く高さ 最高 (m)", floatMax, 0.0f, 10.0f);
        fields.Float("floatBob", "上下にゆれる幅 (m)", floatBob, 0.0f, 2.0f);
        fields.Float("idleTurnRate", "待機中に振り向く速さ (度/秒)", idleTurnRate, 0.0f, 3600.0f);
        fields.Float("preTurnRate", "前隙に振り向く速さ (度/秒)", preTurnRate, 0.0f, 3600.0f);
        fields.Float("overheadTurnRate", "上段の前隙に振り向く速さ (度/秒)", overheadTurnRate, 0.0f, 3600.0f);
        fields.Float("stringGapTurnRate", "連撃の構えの間に振り向く速さ (度/秒)", stringGapTurnRate, 0.0f, 3600.0f);
    }
    if (fields.Section("体の構え（前隙の見せ方）")) {
        fields.Float("leanFromSpeed", "動いた向きへ傾く量 (rad / m/秒)", leanFromSpeed, 0.0f, 1.0f, 0.005f);
        fields.Float("leanMaxRadians", "動きで傾く最大 (rad)", leanMaxRadians, 0.0f, 1.5f);
        fields.Float("poseLeanDegrees", "剣の上下で前後に傾く量 (度、剣 100 度あたり)", poseLeanDegrees, 0.0f, 45.0f);
        fields.Float("poseSwayDegrees", "剣の左右で横に傾く量 (度、剣 100 度あたり)", poseSwayDegrees, 0.0f, 45.0f);
        fields.Float("crouchSquash", "屈むと縮む割合", crouchSquash, 0.0f, 0.9f);
        fields.Float("stunLeanRadians", "気絶で前へ崩れる角度 (rad)", stunLeanRadians, 0.0f, 1.5f);
        fields.Note(
            "地面や画面に予告は出しません（ソウルライク）。前隙は体の傾き・屈み・剣先に集まる霞だけで見せます。");
    }
    if (fields.Section("動きの型ごとの体の動き")) {
        fields.Float("dashStopDistance", "瞬間突進：プレイヤーの手前で止まる距離 (m)", dashStopDistance, 0.0f, 30.0f);
        fields.Int("dashTeleportFrames", "瞬間突進：瞬間移動にかける長さ (F)", dashTeleportFrames, 1, 120);
        fields.Float("slashApproachSpeed", "横斬り・縦斬り：前隙に歩く速さ (m/秒)", slashApproachSpeed, 0.0f, 20.0f);
        fields.Float("stringApproachSpeed", "連撃：前隙に近づく速さ (m/秒)", stringApproachSpeed, 0.0f, 20.0f);
        fields.Float("stringApproachStop", "連撃：これより近いと近づかない (m)", stringApproachStop, 0.0f, 30.0f);
        fields.Float("overheadLungeDistance", "上段：前隙の終わりの踏み込み (m)", overheadLungeDistance, 0.0f, 20.0f);
        fields.Float("stepInClearance", "踏み込み：プレイヤーとの間に残す距離 (m)", stepInClearance, 0.0f, 10.0f);
        fields.Float("walkInStopDistance", "間合いを詰める：横歩きに切り替える距離 (m)", walkInStopDistance, 0.0f,
                     50.0f);
        fields.Float("walkInSpeedScale", "間合いを詰める：歩く速さの倍率", walkInSpeedScale, 0.0f, 10.0f);
        fields.Float("backstepDistance", "後ろへ下がる：距離 (m)", backstepDistance, 0.0f, 30.0f);
        fields.Float("backstepHeight", "後ろへ下がる：跳ねる高さ (m)", backstepHeight, 0.0f, 10.0f);
        fields.Float("jumpHeight", "跳躍：跳ぶ高さ (m)", jumpHeight, 0.0f, 20.0f);
        fields.Float("jumpCrouchShare", "跳躍：前隙のうち屈む割合（残りで跳ぶ）", jumpCrouchShare, 0.0f, 0.95f, 0.01f);
        fields.Float("kickRingStartRadius", "回し蹴り：霞の輪の始まりの半径 (m。最大は表の判定サイズ)",
                     kickRingStartRadius, 0.0f, 20.0f);
        fields.Float("kickResidueScale", "回し蹴り：輪が消えた所の残り煙（斬撃の 2 m ぶんの何倍、6 か所）",
                     kickResidueScale, 0.0f, 10.0f);
        fields.Float("kickResidueRadiusShare", "回し蹴り：残り煙を置く円（最大半径の何倍）", kickResidueRadiusShare,
                     0.0f, 2.0f);
        fields.Float("burnCrossLength", "残す火「交差」：X の 1 本の長さ (m。狙った所で 2 本が交わる)", burnCrossLength,
                     0.1f, 40.0f);
        fields.Float("burnLineLag", "残す火「線」：火が飛ぶ刃の後ろへ遅れる距離 (m。0 だと刃の真下から燃える)",
                     burnLineLag, 0.0f, 10.0f);
        fields.Float("burnPoolForward", "残す火「池」：上段溜めの池を置く所（ボスの前 m。振り下ろした所）",
                     burnPoolForward, 0.0f, 20.0f);
        fields.Float("ambushAppearDistance", "霞隠れ：プレイヤーから現れる距離 (m)", ambushAppearDistance, 0.0f, 30.0f);
        fields.Float("ambushAppearSpreadDegrees", "霞隠れ：真後ろから左右へ散らす角度 (度)", ambushAppearSpreadDegrees,
                     0.0f, 180.0f);
        fields.Float("ambushLungeDistance", "霞隠れ：振りの間に突っ込む距離 (m)", ambushLungeDistance, 0.0f, 20.0f);
        fields.Float("ambushAppearTurnRate", "霞隠れ：現れてから振り向く速さ (度/秒)", ambushAppearTurnRate, 0.0f,
                     3600.0f);
        fields.Float("hidePreSinkDepth", "霞隠れ・分身：前隙の間に霧へ沈む深さ (m)", hidePreSinkDepth, 0.0f, 10.0f);
        fields.Float("hideSinkDepth", "霞隠れ・分身：隠れている間に沈める深さ (m。体が地面の下に隠れる深さ)",
                     hideSinkDepth, 0.0f, 100.0f);
        fields.Int("hideSinkFrames", "霞隠れ：隠れる時に沈みきる長さ (F)", hideSinkFrames, 1, 120);
        fields.Int("appearRiseFrames", "霞隠れ・分身：現れる時に浮かび上がる長さ (F)", appearRiseFrames, 1, 120);
        fields.Note("霞隠れの「隠れ F／現れ F／斬り F × 回数、最後遅」と分身の数（判定サイズ）は技の表で変えます。"
                    "隠れている間はボスの体を地面の下へ沈めるので、砲撃は当たらずに抜けます。");
    }
    if (fields.Section("霞分身と吸い込み")) {
        fields.Float("cloneCooldownSeconds", "分身の再使用 (秒)", cloneCooldownSeconds, 0.0f, 600.0f);
        fields.Float("cloneLifeSeconds", "分身が消えるまで (秒)", cloneLifeSeconds, 0.1f, 600.0f);
        fields.Float("cloneMinDistance", "分身を並べる距離 最小 (m、プレイヤーから)", cloneMinDistance, 0.0f, 50.0f);
        fields.Float("cloneMaxDistance", "分身を並べる距離 最大 (m)", cloneMaxDistance, 0.0f, 50.0f);
        fields.Float("cloneSpreadRadians", "分身どうしの間隔 (rad)", cloneSpreadRadians, 0.0f, 3.2f, 0.01f);
        fields.Float("cloneSpreadJitter", "…間隔に足すばらつき (rad)", cloneSpreadJitter, 0.0f, 3.2f, 0.01f);
        fields.Float("puffResidue", "霞隠れ・分身の霞が残す煙（吸える）", puffResidue, 0.0f, 1000.0f);
        fields.Float("puffRadius", "…その半径 (m)", puffRadius, 0.05f, 10.0f);
        fields.Note("分身はシーンに置いた「ボスの分身」の物体を使います（数は表の判定サイズ。物体が足りなければ"
                    "ある分だけ）。分身を撃つと、本体が行動ツリーの「分身を撃たれた」の枝で突進横なぎを出します。");
        fields.Float("absorbCatchRange", "吸い込み：斬撃・ナイフを吸える距離 (m)", absorbCatchRange, 0.0f, 50.0f);
        fields.Float("absorbCatchAngle", "吸い込み：吸える角度（片側、度）", absorbCatchAngle, 0.0f, 180.0f);
        fields.Float("absorbCatchNear", "吸い込み：向きを問わず吸える近さ (m)", absorbCatchNear, 0.0f, 10.0f);
    }
    if (fields.Section("剣と斬撃の出方")) {
        fields.Float3("swordShoulder", "剣を持つ肩の位置（体の足元から、局所 m）", swordShoulder);
        fields.Float("swordLength", "剣の長さ (m)", swordLength, 0.1f, 20.0f);
        fields.Float("swordHazeLength", "剣先の霞の長さ (m)", swordHazeLength, 0.0f, 10.0f);
        fields.Float("waveForward", "斬撃を放つ位置：体の前 (m)", waveForward, 0.0f, 10.0f);
        fields.Float("waveMinHeight", "斬撃を放つ高さ 最低 (m)", waveMinHeight, 0.0f, 10.0f);
        fields.Float("waveMaxHeight", "斬撃を放つ高さ 最高 (m、跳んだ高さを足す)", waveMaxHeight, 0.0f, 10.0f);
        fields.Int("waveGrowFrames", "放った斬撃が判定サイズまで伸びる長さ (F)", waveGrowFrames, 1, 120);
        fields.Float("knifeHeight", "ナイフを投げる高さ (m)", knifeHeight, 0.0f, 10.0f);
        fields.Float("knifeForward", "ナイフを投げる位置：体の前 (m)", knifeForward, 0.0f, 10.0f);
        fields.Float("knifeSpreadDegrees", "ナイフの扇の間隔 (度)", knifeSpreadDegrees, 0.0f, 90.0f);
        fields.Float("knifeWidth", "ナイフの当たりの幅 (m)", knifeWidth, 0.01f, 5.0f);
        fields.Float("knifeThickness", "ナイフの当たりの厚み (m)", knifeThickness, 0.01f, 5.0f);
        fields.Float("playerHitLow", "プレイヤーの当たり：下端（原点から m）", playerHitLow, -5.0f, 5.0f);
        fields.Float("playerHitHigh", "プレイヤーの当たり：上端（原点から m）", playerHitHigh, -5.0f, 5.0f);
        fields.Float("playerHitRadius", "プレイヤーの当たり：半径 (m)", playerHitRadius, 0.0f, 5.0f);
        fields.Float("playerAimHeight", "狙う高さ（プレイヤーの原点から m）", playerAimHeight, -5.0f, 5.0f);
    }
    if (fields.Section("残る煙（場の霧へ入れる）")) {
        fields.Float("residuePerTwoMeters", "斬撃が 2 m 進むごとに残す煙", residuePerTwoMeters, 0.0f, 1000.0f);
        fields.Float("knifeEndResidueScale", "ナイフが消えた所に残す煙（斬撃の何倍）", knifeEndResidueScale, 0.0f,
                     10.0f);
        fields.Float("bossResidue", "ボスが動いた跡に残す煙（1 回ぶん）", bossResidue, 0.0f, 1000.0f);
        fields.Float("bossResidueStep", "…この距離動くごと (m)", bossResidueStep, 0.1f, 100.0f);
        fields.Float("swingSmoke", "剣の一振りが描く煙（仮）", swingSmoke, 0.0f, 1000.0f);
        fields.Float("kamaeSmoke", "構えで剣先に集まる煙（仮）", kamaeSmoke, 0.0f, 1000.0f);
        fields.Int("kamaeSmokeFrames", "構えの煙を出す長さ (F)", kamaeSmokeFrames, 1, 120);
        fields.Note(
            "斬撃の残り煙は、刃が通り過ぎた後ろの地面近くへ入れます（吸える霧）。斬撃そのものの見た目はエフェクトです。");
        fields.Float("waveSmokeRadius", "斬撃の残り煙の 1 点の半径 (m)", waveSmokeRadius, 0.05f, 10.0f);
        fields.Float("waveResidueLag", "残り煙を入れる位置（刃の何 m 後ろか。斬撃のエフェクトの域より後ろにする）",
                     waveResidueLag, 0.0f, 20.0f);
        fields.Float("waveResidueHeight", "残り煙を入れる高さの上限（地面から m）", waveResidueHeight, 0.0f, 10.0f);
        fields.Float("waveSmokeDrag", "残り煙を押す速さ（斬撃の速さに対する割合）", waveSmokeDrag, 0.0f, 2.0f);
        fields.Float("waveSmokeSink", "残り煙が下へ落ちる速さ (m/秒)", waveSmokeSink, 0.0f, 20.0f);
        fields.Float("swordSmokeRadius", "剣が煙を入れる半径 (m)", swordSmokeRadius, 0.05f, 10.0f);
        fields.Float("depositRadius", "斬撃が消えた所の煙の半径 (m)", depositRadius, 0.05f, 10.0f);
        fields.Float("depositHeight", "残す煙の高さ（地面から m）", depositHeight, 0.0f, 5.0f);
        fields.Int("depositFrames", "消えた所の煙を出し切る長さ (F)", depositFrames, 1, 600);
        fields.Float("bodySmokeRadius", "ボスの通った跡の煙の半径 (m)", bodySmokeRadius, 0.05f, 10.0f);
        fields.Float("waveAbsorbSmoke", "吸い込み：斬撃 1 つを吸った時にヒョウタンへ入る煙", waveAbsorbSmoke, 0.0f,
                     1000.0f);
        fields.Float("knifeAbsorbSmoke", "吸い込み：ナイフ 1 本を吸った時にヒョウタンへ入る煙", knifeAbsorbSmoke, 0.0f,
                     1000.0f);
        fields.Note("煙の単位はヒョウタンの煙と同じ。場の霧の量への換算はヒョウタンの「霧の量の換算」を使います。"
                    "残った煙は場の霧に混ざるので、ヒョウタンの吸い込みでそのまま吸えます。");
    }
    if (fields.Section("見た目（霞の色とエフェクト）")) {
        fields.Vfx("slashEffect", "飛ぶ斬撃のエフェクト（三日月の霧。自前の流体域を持つ）", slashEffect);
        fields.Float("slashEffectWidth", "斬撃のエフェクトが作られている幅 (m。判定サイズ ÷ これ が大きさの倍率)",
                     slashEffectWidth, 0.5f, 50.0f);
        fields.Float("slashFadeSeconds", "斬撃が消えた後、霧が消えるまで飛ばし続ける時間 (秒)", slashFadeSeconds, 0.0f,
                     3.0f);
        fields.Float("slashImpactSeconds", "当たった瞬間に刃が砕けて散る時間 (秒)", slashImpactSeconds, 0.0f, 2.0f);
        fields.Float("slashHitSpeedScale", "当たった後の霧が進む速さ（元の速さに対する割合。小さいほどその場で散る）",
                     slashHitSpeedScale, 0.0f, 1.0f);
        fields.Float("slashGlowFadeSeconds", "刃が消える・当たる時に、光る刃口や炎の層が縮んで消えるまでの時間 (秒)",
                     slashGlowFadeSeconds, 0.0f, 1.0f);
        fields.Note(
            "斬撃の霧の濃さ・色・光り方・尾の長さは、エフェクト BossSlashWave の Blade Domain と各層で調整します。"
            "同時に見せる斬撃は 2 つまで（流体域の枠）で、その間は砲撃の炎の枠が同じ数だけ減ります。");
        fields.Vfx("mistBlastEffect", "霞の刃が当たった所のエフェクト（青白い光と火花。空なら炎の刃と同じ）",
                   mistBlastEffect);
        fields.Float("wavePartPush", "飛ぶ斬撃が、真下の場の霧を左右へ割る速さ (m/s。0 で割らない)", wavePartPush, 0.0f,
                     40.0f);
        fields.Float("wavePartRadius", "…割る押しの半径 (m)", wavePartRadius, 0.1f, 6.0f);
        fields.Vfx("slashSmokeEffect", "火を残さない斬撃のエフェクト（煙の刃。空なら炎の刃で出す）", slashSmokeEffect);
        fields.Float("bladeLitEmber", "火を残す技の間の、剣先の熾火の強さ (倍)", bladeLitEmber, 0.0f, 20.0f);
        fields.Color3("bladeLitColor", "…剣先の霞の色", bladeLitColor);
        fields.Float("bladeFuel", "…剣に沿って場の火の域へ入れる燃料の強さ（0 は炎を出さない）", bladeFuel, 0.0f,
                     10000.0f);
        fields.Float("bladeFuelRadius", "…燃料の球の半径 (m)", bladeFuelRadius, 0.1f, 4.0f);
        fields.Float("bladeFuelTemperature", "…燃料の温度", bladeFuelTemperature, 0.0f, 64.0f);
        fields.Float("igniteFlashRadius", "剣に火が付く瞬間の火球の半径 (m。0 は出さない)", igniteFlashRadius, 0.0f,
                     5.0f);
        fields.Float("hitBlastRadius", "当たった所の火球・煙の塊の半径 (m)", hitBlastRadius, 0.1f, 10.0f);
        fields.Note("火を残す技（技の表の「残す火」）は、出す前から剣が燃え、斬撃は炎の刃になります。"
                    "残さない技は、剣は霞のまま、斬撃は暗い煙の刃です。"
                    "剣の炎・当たった所の火球・煙の塊は、場の火の域（ArenaFireField）の流体です。");
        fields.Vfx("blastEffect", "当たった所の爆発のエフェクト（光・火花・爆炎）", blastEffect);
        fields.Float("slashHitFadeSeconds", "当たった斬撃の炎を消すまでの時間 (秒。爆発の光に隠してすぐ消す)",
                     slashHitFadeSeconds, 0.0f, 1.0f);
        fields.Float("blastPush", "爆発が足元の霧を外へ押す速さ (m/秒、0 は無効)", blastPush, 0.0f, 100.0f);
        fields.Float("blastPushRadius", "爆発が霧を押す半径 (m)", blastPushRadius, 0.1f, 50.0f);
        fields.Float("knifeBlastScale", "ナイフが当たった時の爆発の大きさ（斬撃に対する倍率）", knifeBlastScale, 0.0f,
                     2.0f);
        fields.Note(
            "爆発は、斬撃・ナイフ・剣がプレイヤーに当たった所に出ます。霧は地面に沿って外へ押すだけで、煙は足しません"
            "（場の霧はほとんど消えないので、足すとプレイヤーの周りに溜まって画面を塞ぎます）。"
            "光・火花・爆炎の形は BossSlashBurst で調整します。");
        fields.Vfx("swordEffect", "剣先の霞・ナイフの尾のエフェクト（流体注入）", swordEffect);
        fields.Color3("hazeColor", "剣先の霞・ナイフの尾の色（霞の刃と同じ青白。火を残す技の間は bladeLitColor）",
                      hazeColor);
        fields.Float("swordEffectAmount", "振っている剣の霞の濃さ（倍）", swordEffectAmount, 0.0f, 20.0f);
        fields.Float("kamaeEffectAmount", "構えで剣先に集まる霞の濃さ（倍）", kamaeEffectAmount, 0.0f, 20.0f);
        fields.Float("swordEmberScale", "振っている剣先の火の粉の光の大きさ（倍、0 は出さない）", swordEmberScale, 0.0f,
                     10.0f);
        fields.Float("kamaeEmberScale", "構えで剣先に灯る火の粉の光の大きさ（倍）", kamaeEmberScale, 0.0f, 10.0f);
        fields.Note(
            "剣先の霞（煙）は場の霧に入って残ります。濃くすると技のたびにボスの頭上へ溜まり、長く戦うと視界を塞ぎます。"
            "構えを目立たせたい時は、煙ではなく火の粉の光を大きくします。");
        fields.Float("knifeEffectAmount", "ナイフの尾の霞の濃さ（倍）", knifeEffectAmount, 0.0f, 20.0f);
        fields.Vfx("knifeEffect", "投げナイフのエフェクト（刃と光点）", knifeEffect);
        fields.Vfx("ringEffect", "回し蹴りの霞の輪のエフェクト（広がる炎の輪。自前の流体域を持つ）", ringEffect);
        fields.Float("ringEffectRadius", "…エフェクトが作られている半径 (m。輪の半径 ÷ これ が大きさの倍率)",
                     ringEffectRadius, 0.5f, 50.0f);
        fields.Float("ringFadeSeconds", "…輪が消えた後、炎が消えるまで残す時間 (秒)", ringFadeSeconds, 0.0f, 3.0f);
        fields.Float("ringHeight", "…輪の高さ（地面から m）", ringHeight, 0.0f, 10.0f);
        fields.Note("炎の輪は流体域を 1 つ使います。回し蹴りの間は、砲撃の炎の枠がその分だけ減ります。"
                    "炎・煙の濃さや色は、エフェクト BossKickRing の Fire Domain と各層で調整します。");
        fields.Float("ringBurstScale", "回し蹴りの足元の爆ぜの大きさ（当たった所の爆発の倍率）", ringBurstScale, 0.0f,
                     5.0f);
        fields.Float("ringPush", "霞の輪が足元の霧を外へ押す速さ (m/秒、0 は押さない)", ringPush, 0.0f, 100.0f);
        fields.Float("ringPushRadius", "…押す 1 点の半径 (m)", ringPushRadius, 0.1f, 20.0f);
        fields.Vfx("puffEffect", "霞隠れ・分身が消える／現れる所の煙の塊のエフェクト", puffEffect);
        fields.Float("puffEffectScale", "…煙の塊の大きさ (倍)", puffEffectScale, 0.0f, 5.0f);
        fields.Note("飛ぶ斬撃の霞はエフェクトではなく、場の霧の域へ直接入れます（Emit）。域は増やしません。");
    }
    if (fields.Section("確かめ用（正式シーンでは使わない）")) {
        fields.Text("testMove", "試す技（行の名前。空は使わない）", testMove);
        fields.Bool("testSequence", "表の技を順番に全部出す", testSequence);
        fields.Bool("testLoop", "最後まで出したら最初から", testLoop);
        fields.Int("testStartFrames", "最初の技までの待ち (F)", testStartFrames, 0, 7200);
        fields.Int("testGapFrames", "技と技の間の待ち (F)", testGapFrames, 0, 7200);
        fields.Bool("testHoldStill", "待機中に横歩きしない", testHoldStill);
        fields.Int("testDamageAtFrame", "ボスに確かめのダメージを入れる F（-1 は使わない）", testDamageAtFrame, -1,
                   72000);
        fields.Float("testDamageAmount", "確かめのダメージの量", testDamageAmount, 0.0f, 100000.0f);
        fields.Int("testDamageRepeatFrames", "確かめのダメージの間隔 (F、0 は 1 回)", testDamageRepeatFrames, 0, 7200);
        fields.Int("testDamageRepeatCount", "確かめのダメージの回数", testDamageRepeatCount, 1, 7200);
        fields.Int("traceEveryFrames", "状態をログに出す間隔 (F、0 は出さない)", traceEveryFrames, 0, 7200);
        fields.Int("randomSeed", "乱数の種（移動の長さ・浮く高さ）", randomSeed, 0, 1000000);
    }
}

} // namespace Project_TD2_2
