#pragma once

#include <cstdint>

namespace Project_TD2_2 {

// 登場の煙（c-e0c500）の、流体へ出す番号と、拍ごとの形の決まり（調整は BossIntro の量・速さで。ここは拍の中の
// 割合と、点の並べ方）。使うのは BossIntroSmoke.cpp だけ。
// 番号は、ボスの物体を持ち主にするほかのコードと分け合っている（同じ番号の提出は、後から出した方で置き換わる）：ここは
// 1396〜1614 と 1640〜1659。火の舌 1600〜1608・胴の骨に付く煙 1620〜1630（BossBodyVisuals）、変わり目 1700〜1799
// （BossPhaseShiftVisuals）は使わない。
namespace BossIntroSmokeShape {

// 流体へ出す番号（登場の煙。体の物体が持ち主。ほかの注入と重ねない）。1 つの域が 1 フレームに受けられるのは 32 件まで：
// 煙の点と 1 回の押しが先に席を取り、続けて押す力と吸い込みは残りの席に入る（入り切らない分は効かない）。同じ時に流れる
// 拍の件数の合計を 32 より下に保つ（一番多いのは抜刀：煙の点 8 + 4 + 5、通り道 3、突き上げ 1、腕 6、力 4 で 31 件）。
inline constexpr uint64_t kStreakId = 1396; // +0..5 煙、+6 場の霧の押し
inline constexpr uint64_t kWrapId = 1404;   // +0..3
inline constexpr uint64_t kRiseId = 1408;   // +0..5 煙、+6 柱の力
inline constexpr uint64_t kArmId = 1416;    // 右 +0..3、左 +4..7
inline constexpr uint64_t kPuffId = 1424;   // 右 +0..4、左 +5..9
inline constexpr uint64_t kCallId = 1436;  // 右 +0..7、左 +8..15（+0..1 飛ぶ間の尾、+2..5 地を這う煙、+7 場の霧の押し）
inline constexpr uint64_t kLiftId = 1452;  // +0..3
inline constexpr uint64_t kSeatId = 1456;  // +0..7 襟、+8..10 面頬、+11 押し、+12 場の霧の押し
inline constexpr uint64_t kStepId = 1470;  // +0..4 煙、+5 押し
inline constexpr uint64_t kSeepId = 1476;  // +0..5
inline constexpr uint64_t kBurstId = 1482; // +0..7 柱、+12 柱の力、+15 場の霧の押し
inline constexpr uint64_t kTrailId = 1498; // +0..4
inline constexpr uint64_t kClearId = 1504; // +0..5
inline constexpr uint64_t kWarmId =
    1510; // +0..1 抜刀の柱の通り道、+2 抜刀の風、+3 地面の通り道、+4 鎧を起こす柱の通り道
// 地を走る煙：段のこの割合までで鎧に着き、残りは鎧を包んで薄れる。拍の後、尾が薄れるまで域を置いておく秒。
inline constexpr float kStreakRunShare = 0.62f;
inline constexpr float kStreakFadeSeconds = 1.5f;
inline constexpr float kWrapSeconds = 0.9f;
inline constexpr float kStreakWeave = 3.0f;
// 地を走る煙：点の数、道の 1 m あたりに置く煙の総量（項目の倍率 1 の時）、量を決める時の速さの下限 (m/秒。止まり際にも
// 出す)、入れた煙が進む向きへ持つ速さ (m/秒。尾が前へ転がる)、温度（尾の塊が少し持ち上がって巻く）。
inline constexpr int kStreakWisps = 6;
inline constexpr float kStreakPerMeter = 0.55f;
inline constexpr float kStreakSlowest = 5.0f;
inline constexpr float kStreakCarry = 6.0f;
inline constexpr float kStreakHeat = 1.4f;
// 地を走る煙の量を脈打たせる速さ（ラジアン/秒。約 8 回/秒）。
inline constexpr float kStreakBeat = 50.0f;
// 鎧を包む煙：点の数、鎧の周りを回る速さ（ラジアン/秒）。
inline constexpr int kWrapWisps = 4;
inline constexpr float kWrapTurn = 6.0f;
// 鎧を引き起こす柱：点の数、体の軸からの距離 (m)、軸の周りを回る速さ（ラジアン/秒）と煙が回る速さ (m/秒)、
// 上がる速さ (m/秒)、温度、胸より上へ伸ばす高さ (m)。
inline constexpr int kRiseWisps = 6;
inline constexpr float kRiseRing = 0.7f;
inline constexpr float kRiseTwist = 2.6f;
inline constexpr float kRiseSwirl = 4.5f;
inline constexpr float kRiseSpeed = 8.0f;
inline constexpr float kRiseHeat = 5.0f;
inline constexpr float kRiseAbove = 1.2f;
// 柱を引き上げる力：加速 (m/秒²) と届く半径 (m)。
inline constexpr float kRisePush = 26.0f;
inline constexpr float kRisePushRadius = 1.7f;
// 柱の通り道：胸の上を温める球の半径 (m) と温度。
inline constexpr float kRiseChimneyRadius = 2.4f;
inline constexpr float kRiseChimneyHeat = 3.0f;
// 腕の煙：肩から手首へ届くまでの秒、片腕の点の数（付く前・付いた後）、腕に沿って流れる速さ (m/秒)、温度。
inline constexpr float kArmReachSeconds = 1.1f;
inline constexpr int kArmWisps = 4;
inline constexpr int kArmWispsAttached = 3;
inline constexpr float kArmFlow = 2.5f;
inline constexpr float kArmHeat = 0.6f;
// 手甲を呼ぶ煙：痙攣の段の進みにこの倍率を掛けた所まで伸びる（1.6 なら、段の 6 割で手甲に届く）。点の数。
inline constexpr float kCallReachShare = 1.6f;
inline constexpr int kCallWisps = 4;
// 手甲の周りの場の霧を押しのける半径 (m) と速さ (m/秒)。地を走る煙の頭が場の霧をかき分ける半径と速さ。
inline constexpr float kCallFogRadius = 2.6f;
inline constexpr float kCallFogPush = 2.2f;
inline constexpr float kStreakFogRadius = 1.8f;
inline constexpr float kStreakFogPush = 6.0f;
// 手甲が付いた瞬間の弾け：入れる長さ (秒)、片手の点の数、継ぎ目から外へ噴く速さ (m/秒)。
inline constexpr float kPuffSeconds = 0.14f;
inline constexpr int kPuffWisps = 5;
inline constexpr float kPuffSpeed = 5.0f;
// 兜を持ち上げる柱：点の数、柱がねじれる速さ（ラジアン/秒）。
inline constexpr int kLiftWisps = 4;
inline constexpr float kLiftTwist = 3.0f;
// 兜が収まった噴き：入れる長さ (秒)、襟の点の数、顔の前（兜の向き）を空ける角度（度）、襟の高さ（兜の骨から下へ m）、
// 外へ噴く速さ (m/秒)。
inline constexpr float kSeatBurstSeconds = 0.16f;
inline constexpr int kSeatRing = 8;
inline constexpr float kSeatFrontGapDegrees = 80.0f;
inline constexpr float kSeatCollarDrop = 0.35f;
inline constexpr float kSeatRingSpeed = 7.0f;
// 面頬の隙間からの噴き：本数、長さ (秒)、速さ (m/秒)、兜の骨から前へ・上への置き場所 (m)、並べる幅 (m)、襟の量に
// 対する割合。
inline constexpr int kSeatJets = 3;
inline constexpr float kSeatJetSeconds = 0.10f;
inline constexpr float kSeatJetSpeed = 11.0f;
inline constexpr float kSeatJetForward = 0.85f;
inline constexpr float kSeatJetHeight = 0.55f;
inline constexpr float kSeatJetWidth = 0.9f;
inline constexpr float kSeatJetShare = 0.15f;
// 兜が収まった瞬間に、足元の場の霧を押しのける半径 (m) と速さ (m/秒)。
inline constexpr float kSeatFogRadius = 7.0f;
inline constexpr float kSeatFogPush = 6.0f;
// 一歩の煙：入れる長さ (秒。長くして、一歩ごとに噴かずに足元から滲み続ける)、点の数、足元から外へ寄る速さと上がる
// 速さ (m/秒)、温度（温かさで自分からゆっくり立ち上がる）、場の霧を押しのける半径 (m) と速さ (m/秒。黒い煙は押さない：
// 押すと一歩ごとに煙が外へ噴き出して、拍が立つ)。
inline constexpr float kStepSeconds = 0.7f;
inline constexpr int kStepWisps = 5;
inline constexpr float kStepSpeed = 0.6f;
inline constexpr float kStepRise = 0.5f;
inline constexpr float kStepHeat = 1.3f;
inline constexpr float kStepFogRadius = 3.6f;
inline constexpr float kStepFogPush = 1.5f;
// 場の霧を押すのは、一歩の初めのこの秒の間だけ（押しは 1 回の衝撃）。
inline constexpr float kStepPushSeconds = 0.02f;
// 刀の根元から滲む煙：地面の点の数と刀身を這い上がる点の数、根元の周りを巡る速さ・根元へ寄る速さ・上がる速さ (m/秒)、
// 温度（噴かせずに、温かさで自分からゆっくり立ち上がらせる）。
inline constexpr int kSeepGround = 4;
inline constexpr int kSeepBlade = 2;
inline constexpr float kSeepSwirl = 0.9f;
inline constexpr float kSeepInward = 0.5f;
inline constexpr float kSeepRise = 0.45f;
inline constexpr float kSeepHeat = 1.4f;
// 刀が抜けた噴き：柱の点の数と温度、地面を転がる煙の点の数と、ボスの体の反対側へ開く扇の半分の角度（度。体の側へは
// 噴かない）、柱を突き上げる 1 回の押しの半径 (m) と、柱の速さに対する割合。
inline constexpr int kBurstJet = 8;
// 噴く煙のうち柱へ回す割合（残りは地面）。地面を転がる煙の温度（少し温かい：転がった先で頭をもたげて、場の霧の上に出る）。
inline constexpr float kBurstHeat = 3.2f;
// 柱の形：伸び切った高さ (m)、伸び切るまでの秒、高さ 1 m ごとに体の反対側へ倒す量 (m)、軸の周りを回る速さ（ラジアン/秒）。
inline constexpr float kBurstColumnHeight = 2.6f;
inline constexpr float kBurstColumnGrow = 1.8f;
inline constexpr float kBurstColumnLean = 0.16f;
inline constexpr float kBurstColumnTwist = 2.2f;
// 柱の煙の量が満ちるまでの秒（抜けた直後のスローの寄りでは、まだ疎らな点の粒が見えやすいので、量を後から満たす）。
inline constexpr float kBurstColumnFill = 0.15f;
inline constexpr float kBurstColumnDelay = 0.0f;
// 抜けた瞬間に、根元に溜まった煙を突き上げる 1 回の押し：速さ (m/秒)、中心の高さ (m)、半径 (m)。
inline constexpr float kBurstKick = 7.0f;
inline constexpr float kBurstKickHeight = 0.8f;
inline constexpr float kBurstKickRadius = 2.4f;
// 噴き終わった柱を薄める半径 (m) と速さ (1/秒)。
inline constexpr float kBurstFadeRadius = 7.0f;
inline constexpr float kBurstFadeRate = 0.9f;
// 煙を流す風：流す向きに混ぜる「後ろ」の割合、加速 (m/秒²)、力の中心の高さ (m) と届く半径 (m)、噴き終わった後も
// 吹かせる秒。
inline constexpr float kBurstDriftBack = 0.6f;
inline constexpr float kBurstWind = 2.2f;
inline constexpr float kBurstWindHeight = 4.5f;
inline constexpr float kBurstWindRadius = 7.0f;
inline constexpr float kBurstWindSeconds = 3.0f;
// 風の向きの揺れ：流す向きから左右へ振れる大きさ（ラジアン）と速さ（ラジアン/秒）。柱の煙が一つ所に立ち続けず、
// 漂いながら散って行く。
inline constexpr float kBurstWindSway = 0.7f;
inline constexpr float kBurstWindSwaySpeed = 0.9f;
// 域の縁の手前で煙を薄める所：流す向きへ根元から何 m 先か、根元から何 m 上か、半径 (m)、速さ (1/秒)。
inline constexpr float kBurstEdgeAhead = 7.5f;
inline constexpr float kBurstEdgeAbove = 13.5f;
inline constexpr float kBurstEdgeRadius = 4.5f;
inline constexpr float kBurstEdgeRate = 3.0f;
// 抜けた瞬間に根元から走る煙の筋：本数、次の 1 本を出すまでの秒、1 本が飛ぶ秒と初めの速さ (m/秒。60 より下)、扇の
// 開き（度。真上から左右へ）、1 本の量（噴く煙の量に対する割合）、飛び出しの温度（域が光り始める 6 より上）。
inline constexpr uint64_t kStreamerId = 1520; // +0..5
// 握る間に根元へ集まる霧：番号（+0..3 煙、+4..6 引き寄せる力、+7 持ち上げる力）、点の数、外のどこから来るか (m)、1 秒に走る回数、力の数、
// 引き寄せる加速 (m/秒²。握り終わり)。
inline constexpr uint64_t kGatherId = 1530;
inline constexpr int kGatherWisps = 4;
inline constexpr float kGatherFrom = 4.4f;
inline constexpr float kGatherSpeed = 0.45f;
inline constexpr int kGatherPulls = 3;
inline constexpr float kGatherPull = 2.5f;
// 集まる霧の点の、根元へ寄る速さと巡る速さ (m/秒)。根元で霧を持ち上げる力（+7）の加速 (m/秒²)：寄った霧は、噴かずに
// ゆっくり立ち上がる。
inline constexpr float kGatherInward = 1.2f;
inline constexpr float kGatherSwirl = 0.8f;
inline constexpr float kGatherLift = 1.6f;
// 抜けた瞬間に、根元の霧を輪に押し開く半径 (m) と速さ (m/秒)。
inline constexpr float kBurstRingRadius = 3.2f;
inline constexpr float kBurstRingSpeed = 1.8f;
// 見得の黒い霧：番号（+0..7 壁、+8..9 地面、+10..14 力）、壁の点の数、抜けてから出し始める秒と満ちるまでの秒、主役からの
// 距離 (m)、点が弧を走る回数（1 秒あたり）、脇を後ろへ巻く加速・地面を前へ押す加速・後ろを持ち上げる加速 (m/秒²)。
inline constexpr uint64_t kBackdropId = 1540;
// 見得の終わりに霧を晴らす：番号（+0..5 外へ流す力、+6 煙の押し、+7 場の霧の押し、+8..12 薄める所）、流す力の数、
// 始めの 1 回の押しの半径 (m) と速さ (m/秒。弱く：強いと煙が解いていない所まで飛んで、流れて行く所が見えない)、
// 薄める速さ (1/秒)、演技の後に消す速さを何倍まで上げるか（域を止める時に、何も残さない）。
inline constexpr uint64_t kDisperseId = 1560;
inline constexpr int kDisperseBlows = 6;
inline constexpr float kDisperseRadius = 6.0f;
inline constexpr float kDisperseBurst = 0.8f;
inline constexpr float kDisperseFade = 2.5f;
inline constexpr float kLingerClearGain = 3.0f;
// 序幕の黒い煙：番号（+0..14 煙の筋：巡る所 5 つ × 席 3 つ（使うのは鎧 2 本・ほか 1 本）、+16..20 所ごとの輪を回す力、
// +24..28 場の霧の押し、+30..34 所ごとの尾を薄める所）、巡る所の数と、1 か所に取ってある筋の席の数。
inline constexpr uint64_t kAmbientId = 1580;
inline constexpr int kAmbientSites = 5;
inline constexpr int kAmbientWisps = 3;
// 筋の高さの揺れを決める時計の速さ（Feed の時計。ほかの拍の kWispRate より遅い）。舞台の向きが決まらなくても、この tick を
// 過ぎたら出し始める（プレイヤーのいない場面）。
inline constexpr float kAmbientWispRate = 1.1f;
inline constexpr uint64_t kAmbientWaitTicks = 3;
// 出し始めに量が満ちるまでの秒、拍の終わりで引いていく秒。輪の半径の下限 (m) と、ゆっくり伸び縮みする割合。入れた煙が
// 進む向きへ持つ速さ (m/秒)、輪を回す力が所の周りを巡る速さ（筋の速さに対する割合）。
inline constexpr float kAmbientIn = 1.5f;
inline constexpr float kAmbientOut = 1.6f;
inline constexpr float kAmbientRingMin = 0.5f;
inline constexpr float kAmbientBreathe = 0.2f;
inline constexpr float kAmbientCarry = 0.8f;
inline constexpr float kAmbientStirShare = 0.35f;
// 尾を薄める所の半径（巡る輪の半径に対する倍率。薄める速さは、中心から離れるほど弱い）。
inline constexpr float kAmbientFadeReach = 2.2f;
// 前触れが進んだ時：煙の温度（冷えて、地を這う。ふだんの温度は項目 introAmbientHeat）、筋が鎧の周りを巡る輪の半径 (m) と、
// 席ごとに足す半径 (m。筋を重ねない)。場の霧を押しのける半径 (m)。
inline constexpr float kOmenHeat = 0.05f;
inline constexpr float kOmenRing = 1.9f;
inline constexpr float kOmenRingStep = 0.32f;
inline constexpr float kAmbientFogRadius = 2.4f;
inline constexpr int kBackdropWisps = 8;
inline constexpr float kBackdropAt = 0.9f;
inline constexpr float kBackdropRise = 1.2f;
inline constexpr float kBackdropRadius = 3.6f;
inline constexpr float kBackdropSpeed = 0.18f;
inline constexpr float kBackdropSwirl = 3.0f;
inline constexpr float kBackdropCreep = 4.0f;
inline constexpr float kBackdropLift = 2.5f;
// 見得の霧を一つ所に留めない揺らぎ：壁の点の弧のずれ（度）、力の向き・強さが揺れる速さ（ラジアン/秒）。
inline constexpr float kBackdropWander = 22.0f;
inline constexpr float kBackdropWanderSpeed = 0.35f;
// 剣先の寄りの段（Sword.Tip。c-280b71）がある演出の、抜刀の煙の拍子（段が無い演出は、前の拍子のまま）。
// 地の割れ目（刀が刺さった所）から出る煙。点は地面より下に沈め、割れ目の線の上を落ち着きなく滑らせる：域（床は地面）
// に入るのは球の縁＝地面のすぐ上の 1 層の、細い線だけで、球の形は見えない。寄りの間は滲み出し、抜けた瞬間は同じ点から
// 溢れる（番号も動きも続き）。
// 割れ目：刀の幅の向きの主な 1 本（点の数と、長さの半分 m）と、根元から斜めへ出る短い 2 本（主な割れ目からの角度 度、
// 長さ m）。点を沈める深さ (m。点の半径 0.36 と格 0.18 から、0.27 より深いと 1 層目の格に届かず、何も出ない)、点が
// 割れ目を往復する速さ（ラジアン/秒）。
inline constexpr int kTipSlitMain = 4;
inline constexpr float kTipSlitHalf = 0.6f;
inline constexpr float kTipSlitSideAngle[2] = {55.0f, 215.0f};
inline constexpr float kTipSlitSideLength = 0.8f;
inline constexpr float kTipSlitDepth = 0.18f;
inline constexpr float kTipSlitSlide = 3.1f;
// 寄りの間の滲み出し：切り替わった所から出始め、刀が抜けるまで濃くなり続ける（出始めが速く、急に出そろわない）。量の
// 倍率（項目 introSeepAmount に掛ける。沈めた点は縁の重み 0.06 でしか入らないが、入る所が細い線なので、少しで濃く
// 見える：0.25 より上は、半透明の煙ではなく黒い染みになる）と、刀が抜けて行く終わりの所で足す倍率（進みの 4 乗に掛ける）。
inline constexpr float kTipSlitGain = 0.05f;
inline constexpr float kTipSlitSurge = 0.15f;
// 割れ目の上の空気を温める球（番号 +0..1。煙はほとんど足さない。握り始めから置く）：沈めた点からは温度がほとんど
// 入らず、冷たい煙は重さで床へ溜まって、墨のように広がる。温めておくと、滲み出した煙が巻きながら自分で立ちのぼる。
// 主な割れ目に沿って根元からずらす距離 (m)、高さ (m)、半径 (m)、温度（域が光り始める 6 より下）。
inline constexpr uint64_t kTipWarmId = 1526;
inline constexpr float kTipWarmAside = 0.35f;
inline constexpr float kTipWarmHeight = 0.2f;
inline constexpr float kTipWarmRadius = 0.9f;
inline constexpr float kTipSeepHeat = 3.0f;
// 寄りの間、滲み出した煙を刀身に沿って持ち上げる力：中心の高さ (m)、半径 (m)、加速 (m/秒²。段の終わり)。
inline constexpr float kTipLiftHeight = 0.35f;
inline constexpr float kTipLiftRadius = 1.1f;
inline constexpr float kTipLift = 6.0f;
// 抜けた後：溢れさせる長さ（項目 introDrawBurstSeconds に対する割合。短く溢れて、すぐ霧へ渡す）と、量の倍率（項目
// introDrawBurstAmount に掛ける。抜けた瞬間が最大で、細って行く）。溢れ終わった根元を薄める半径 (m) と、薄め始めの秒。
inline constexpr float kTipBurstShare = 0.6f;
inline constexpr float kTipSpillGain = 2.0f;
// 抜けてから、割れ目の上を温め続ける秒（溢れた煙が、盛り上がってから風に流れる）。
inline constexpr float kTipSpillWarm = 0.3f;
inline constexpr float kTipFadeRadius = 3.2f;
inline constexpr float kTipFadeIn = 0.5f;
// 風（番号 +0 根元の低い所、+1 舞台じゅう、+2 は湧き出し。刀のある側の外へ吹く）：根元の力は、中心を根元から風下へずらす距離 (m)・
// 高さ (m)・半径 (m)。舞台じゅうの力は、主役の足元からの高さ (m) と半径 (m。主役の両脇の、見得の壁の点まで届かせる)。
// 寄りの間のそよ風の加速 (m/秒²。根元だけ。段の終わり) と、向きに混ぜる「後ろ」の割合（寄りの画を横切る向き）。
// 抜けた瞬間の突風の加速（根元・舞台じゅう）と、向きに混ぜる「後ろ」の割合（主役の右脇から後ろへ流れて、見得の霧が
// 立つ所へ広がる）、強まる秒・保つ秒・引く秒、引いた後に残す割合（見得の霧の間も吹く）。
// 向きが左右へ振れる大きさ（ラジアン）と速さ（ラジアン/秒。2 つの力で位相をずらす：煙が板のまま流れず、ほぐれる）。
inline constexpr uint64_t kTipWindId = 1574;
inline constexpr float kTipWindLowAhead = 1.5f;
inline constexpr float kTipWindLowHeight = 1.0f;
inline constexpr float kTipWindLowRadius = 5.0f;
inline constexpr float kTipWindWideHeight = 3.0f;
inline constexpr float kTipWindWideRadius = 10.0f;
inline constexpr float kTipWindSeep = 4.0f;
inline constexpr float kTipWindSeepBack = 0.25f;
inline constexpr float kTipWindGustLow = 8.0f;
inline constexpr float kTipWindGustWide = 4.0f;
inline constexpr float kTipWindGustBack = 1.0f;
inline constexpr float kTipWindGustIn = 0.2f;
inline constexpr float kTipWindGustHold = 0.7f;
inline constexpr float kTipWindGustFall = 0.8f;
inline constexpr float kTipWindBreeze = 0.2f;
inline constexpr float kTipWindSway = 0.6f;
inline constexpr float kTipWindSwaySpeed = 1.3f;
// 刀の根元の細かい域（BossIntroTipSmoke。格 5 cm）へ入れる、滲み出す煙：番号（+0..7）、点の数と、そのうち主な割れ目の
// 上の数（残りは斜めの割れ目）、点の半径 (m。格 2 つ)、地面からの高さ (m)、割れ目を往復する速さ（ラジアン/秒）、
// 点の中心へ 1 秒に足す濃さ（段の終わり。項目 introTipSmokeAmount を掛ける）と、刀が抜けて行く終わりの所で足す倍率
// （進みの 4 乗に掛ける）、温度（浮力。域が光り始める 6 より下）、上向きの速さ (m/秒)。抜けてから域を置いておく秒。
inline constexpr uint64_t kTipFineId = 1640;
inline constexpr int kTipFinePoints = 8;
inline constexpr int kTipFineMain = 6;
inline constexpr float kTipFineRadius = 0.09f;
inline constexpr float kTipFineHeight = 0.05f;
inline constexpr float kTipFineSlide = 2.0f;
inline constexpr float kTipFineRate = 450.0f;
inline constexpr float kTipFineSurge = 1.0f;
inline constexpr float kTipFineHeat = 5.0f;
inline constexpr float kTipFineRise = 0.35f;
inline constexpr float kTipFineLinger = 0.15f;
// 手を握る拍（Hand.Fist）の煙：番号（+0..3 指の間、+4 拍の終わりに薄める所。細かい域の番号は 1640〜：胴の骨に付く煙
// 1620〜1630・火の舌 1600〜 と重ねない）、点の半径 (m)、温度、押し出す速さ (m/秒。握り込む勢いが満ちた時)。
// 握り込む勢い：手首から指先までが縮む速さがこの値 (m/秒) で満ちる。長さをならす速さ（1/秒：動捕の指の細かい震えを
// 拾わない）、勢いが立ち上がる・引く速さ（1/秒）。握っている深さ：開いた手からこれだけ (m) 縮むと、漏れが満ちる。
// 点の中心へ 1 秒に足す濃さ（勢いが満ちた時。項目 introFistSmokeAmount を掛ける）と、握っている間に漏れ続ける割合。
// 域を置く所：拍の始めの指の付け根から、舞台の向きでこれだけずらす（握る間に、手は上・内・後ろへ動く：その道を域の
// 中に収める。上は、握った拳から立ちのぼる煙の分を 1.4 m ほど空ける：足りないと、箱の上の面で煙が切れる）。拍の終わりに
// 煙を薄める秒と速さ（1/秒）。
inline constexpr uint64_t kFistSmokeId = 1650;
inline constexpr float kFistSmokeRadius = 0.09f;
inline constexpr float kFistSmokeHeat = 2.5f;
inline constexpr float kFistSmokeSpeed = 1.8f;
inline constexpr float kFistClosingFull = 0.45f;
inline constexpr float kFistCurlFollow = 10.0f;
inline constexpr float kFistSqueezeRise = 14.0f;
inline constexpr float kFistSqueezeFall = 5.0f;
inline constexpr float kFistHeldDepth = 0.18f;
inline constexpr float kFistSmokeRate = 1000.0f;
inline constexpr float kFistSmokeTrickle = 0.04f;
inline constexpr float kFistDomainSide = -0.5f;
inline constexpr float kFistDomainUp = 1.6f;
inline constexpr float kFistDomainBack = 0.4f;
inline constexpr float kFistFadeSeconds = 0.8f;
inline constexpr float kFistFadeRate = 5.0f;
// 域を置く時に、刀が入る所の実際の地面を探す光線：どれだけ上から落とすか (m) と、届く長さ (m)。
inline constexpr float kTipGroundAbove = 1.5f;
inline constexpr float kTipGroundReach = 4.0f;
// 湧き出し（番号 +2。根元から四方へ押し広げる）：中心の高さ (m)、半径 (m)、球面を流れ出す速さ (m/秒。抜けた瞬間)、
// 引き切るまでの秒。
inline constexpr float kTipSwellHeight = 0.4f;
inline constexpr float kTipSwellRadius = 1.6f;
inline constexpr float kTipSwell = 7.0f;
inline constexpr float kTipSwellSeconds = 0.7f;
// 煙が広がる先を、先に開けておく球（煙はほとんど足さない。流体は「煙があり得る所」だけを解く：速い煙は、まだ解いて
// いない所へ入ると消える）：数（根元の周りに並べる）、根元からの距離 (m)、高さ (m)、半径 (m)、温度（少し温かい：
// 広がった煙が、地面に張り付かずに盛り上がる）、置いておく秒（見得の霧の点が出る前まで：席を空ける）。
inline constexpr int kTipOpen = 4;
inline constexpr float kTipOpenAway = 3.0f;
inline constexpr float kTipOpenHeight = 1.5f;
inline constexpr float kTipOpenRadius = 3.5f;
inline constexpr float kTipOpenHeat = 0.6f;
inline constexpr float kTipOpenSeconds = 0.88f;
// 見得の黒い霧の出始め：壁の点・地面の点を、地面の下へ沈めておく（深さは割れ目の点と同じ。域へ入るのは地面のすぐ上の
// 細い線だけ：霧が、主役の周りの地面から立ちのぼる。空中の点から出すと、出始めが黒い球に見える）。沈めておく秒（霧が
// 出始めてから）と、その後に元の高さへ上げる秒、沈めている間の量の倍率（沈めた点は縁の重みでしか入らない分を補う）。
// 霧の立つ所の空気を温める球（番号は霧の +15..17：両脇と後ろ。煙はほとんど足さない）：主役からの距離 (m)、高さ (m)、
// 半径 (m)、温度。
inline constexpr float kTipWallSunkHold = 0.8f;
inline constexpr float kTipWallSunkRise = 1.2f;
inline constexpr float kTipWallSunkGain = 16.0f;
inline constexpr float kTipWallWarmAway = 3.4f;
inline constexpr float kTipWallWarmHeight = 1.5f;
inline constexpr float kTipWallWarmRadius = 3.5f;
inline constexpr float kTipWallWarmHeat = 3.0f;
// 刀身が引く煙の幕の量の割合（高い柱になって、主役の頭の上に黒い塊で残るので、控える）。
inline constexpr float kTipTrailShare = 0.5f;
// 縁で薄める所を出し始める秒（それまで煙は縁に届かない。刀の幕が終わって、席が空いてから）。
inline constexpr float kTipEdgeAt = 1.5f;
inline constexpr int kBurstStreamers = 3;
inline constexpr float kBurstStreamerGap = 0.04f;
inline constexpr float kBurstStreamerLife = 0.5f;
inline constexpr float kBurstStreamerSpeed = 24.0f;
inline constexpr float kBurstStreamerFan = 68.0f;
inline constexpr float kBurstStreamerShare = 0.07f;
inline constexpr float kBurstCoreHeat = 14.0f;
// 目の炎（既定では出さない）：番号、隙間の帯の中心から左右へ（兜の模型の寸法。2 本目と 4 本目の隙間）、面の表から前へ
// 出す長さ（同）、1 点の半径 (m。域の格 3 cm の 2 つ以上)、温度、目が灯る拍の初めに量を何倍足すか。
inline constexpr uint64_t kEyeId = 1516; // +0..1
inline constexpr float kEyeApart = 0.082f;
inline constexpr float kEyeForward = 0.02f;
inline constexpr float kEyeRadius = 0.1f;
inline constexpr float kEyeHeat = 8.0f;
inline constexpr float kEyeBurst = 3.0f;
// 柱を噴き上げる力：加速に使う高さ (m) と力の届く半径 (m)。地面の煙を扇へ押し出す力：数、根元からの距離 (m)、半径 (m)。
inline constexpr float kBurstJetRun = 3.2f;
inline constexpr float kBurstJetRadius = 1.7f;
// 柱の通り道（煙突）：温める球の数・半径 (m)・間隔 (m)・温度、噴き終わった後も温め続ける秒。地面の通り道の半径 (m)。
inline constexpr int kBurstChimney = 2;
inline constexpr float kBurstChimneyRadius = 3.0f;
inline constexpr float kBurstChimneyStep = 4.5f;
inline constexpr float kBurstChimneyHeat = 2.5f;
inline constexpr float kBurstChimneySeconds = 0.3f;
inline constexpr float kBurstFloorRadius = 3.6f;
// 刀身が煙を引く長さ (秒)、刀身に沿う点の数、引かれた幕が上がる温度。
inline constexpr float kTrailSeconds = 1.5f;
inline constexpr int kTrailWisps = 5;
inline constexpr float kTrailHeat = 2.5f;
// 刀身のどこからどこまでに煙の点を置くか（刀身の長さに対する割合）、点が刀身を追う速さの上限 (m/秒。エンジンが掃く
// 上限の 60 より下)。
inline constexpr float kTrailFrom = 0.2f;
inline constexpr float kTrailTo = 0.75f;
inline constexpr float kTrailChase = 42.0f;
// 漂う薄い煙を消し続ける所（胸から頭へこの割合の所）・半径 (m)・速さ (1/秒)。域の煙は長く残る設定（抜刀の柱を立ち
// 残らせるため）なので、ほかの間は、顔や手の寄りのカメラの前に溜まらないよう、ここで薄める。
inline constexpr float kHazeHead = 0.6f;
inline constexpr float kHazeRadius = 3.8f;
inline constexpr float kHazeRate = 4.0f;

} // namespace BossIntroSmokeShape
} // namespace Project_TD2_2
