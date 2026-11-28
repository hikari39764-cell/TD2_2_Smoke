#pragma once

#include <array>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/SequenceTypes.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出（c-271cba で動捕の演出に作り直し）の段：演出の資産を流す → ゲームのカメラへ渡す → 済み。
// 規則（BossPhaseShift）が固定 tick ごとに進め、見た目（BossPhaseShiftVisuals）が読む。
enum class BossShiftBeat { None, Begin, Show, Settle, Done };

// 演出の資産（Sequence）の事件段の名前で決まる拍。名前は「Shift.<拍>」（Anim だけは「Anim.<クリップ名>」）。
// 演出の作業場で事件段を動かせば、効果の時刻が付いて来る。
enum class BossShiftCue {
    Anim,   // 骨格の体に流すクリップの置き場所（始め・長さ）
    Hit,    // 打ち退けられる：時間を一瞬止め、揺らす。胴の煙が体から弾け出る
    Fall,   // 崩れ落ちて、刀が地面を打つ：揺れ、胴の煙が弾ける
    Leak,   // 崩れて座り込む間：鎧の継ぎ目から黒い煙が漏れる
    Reach,  // 左手を太陽へ伸ばす間：太陽の光が強まる（画面の星）
    Grab,   // 握る：終わりの所で太陽を掴む（光を拳へ吸い込む・小さな揺れ・画が一瞬暗くなる）
    Ignite, // 拳の中で火が付き、燃え上がる
    Field,  // 場の色がフェーズ 2 の色へ変わっていく
    Spread, // 火が拳から腕・肩・胴へ回り、胴の煙が炎になる
    Eyes,   // 目が橙赤へ変わる
    Fuse,   // 左手が刀身を拭い、手の所から火が刀に移る
    Wall,   // 左手を前へ払った後、ボスの正面に炎の壁が立ち上がる（一閃で爆ぜる）
    Charge, // 刀を頭上へ振りかぶる：刃口が光り、気配が張りつめる
    Slash, // 一閃が地面に当たった所：閃光・火の輪・衝撃・時間の停止。ここでフェーズ 2 の見た目になり、HP の条が満ち始める
    Settle, // カメラをゲームのカメラへ渡し始める
    Count
};
inline constexpr size_t kBossShiftCueCount = static_cast<size_t>(BossShiftCue::Count);

// フェーズの変わり目の演出：1 本目の条が空になると、ボスが打ち退けられて崩れ、座り込み、左手を太陽へ伸ばして掴む。拳から
// 火が回って体の煙が炎になり、目が橙赤に、場の色が変わる。立ち上がって刀を拭い火を移し、一閃して待機の構えに戻ると、
// カメラがゲームのカメラへ戻る。動きは骨格の体のクリップ（総監の動捕）、カメラと拍の時刻は演出の資産。
// 舞台の向きは太陽で決め（太陽を掴むカットのため）、プレイヤーは演出の始めにボスの正面へ置き直す。
struct BossPhaseShiftSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 演出をするか。切ると、変わり目は Boss.phaseShiftFrames の間、待機のまま。
    bool enabled = true;
    // 演出の資産（時計・カメラ・事件段）と、骨格の体に流すクリップ。資産に Anim.<クリップ名> の段が無ければ、0 秒から
    // clipSeconds の長さで流す。資産を流す速さ。
    std::string sequence{"f417d9926c1bae81a991107f933b1e26"};
    std::string clip = "Shift_Fire";
    float clipSeconds = 18.933f;
    float playbackRate = 1.0f;
    // 資産のカメラが付く舞台の印の名前（資産のバインディングの名前。骨格の体の物体を結ぶ）。
    std::string stageSlot = "Stage";
    // 舞台の向き：太陽（環境の太陽の向き）が、ボスの正面からこの角度（度。正は左）に来るように向ける。切ると
    // プレイヤーのいる側を前にする。太陽が地平より下（夜）の時も、プレイヤーの側。
    bool faceSun = true;
    float sunStageYawDegrees = 0.0f;
    // 舞台の真ん中（ボスの足元）を、場の縁（Boss.arenaRadius）からこれだけ内側へ寄せる (m。カメラが場の外へ出ないように)。
    float stageEdgeMargin = 16.0f;
    // プレイヤーを置き直す：ボスの正面のこの距離 (m)、場の縁からの内側の余白 (m)、置く高さ（足元の高さに足す m）。
    bool placePlayer = true;
    float playerDistance = 13.0f;
    float playerArenaMargin = 3.0f;
    float playerLift = 0.5f;
    // 渡し：演出の最後のカメラの姿勢からゲームのカメラへ混ぜる長さ (秒)、その間ゲームのカメラが見ておく高さ (m)。
    // クリップの層を外す時に待機の姿へ混ぜる長さ (秒)。
    float settleSeconds = 1.4f;
    float cameraFocusHeight = 3.6f;
    // 演出の間のカメラの動きのぼけ（速いカメラの動き・速い体の動きが流れて見える）：シャッター角（度。180 が映画の標準、
    // 大きいほど強い）と、ぼけの長さの上限（1080p の画素。0〜64）。渡しの間は、ゲームのカメラへ戻るのに合わせて弱める。
    bool motionBlur = true;
    float motionBlurShutterDegrees = 270.0f;
    float motionBlurMaxPixels = 40.0f;
    float clipBlendOutSeconds = 0.35f;
    // 演出の後、すぐには攻めて来ない間 (F。この間は技を選ばない)。
    int calmFrames = 45;
    // 飛ばす：演出の間にこの入力（行動 id）を押すと、フェーズ 2 の構えへ進む。
    bool skippable = true;
    std::string skipAction = "Gameplay.Menu";

    // 打ち退け（Hit）：時間を止める長さ (F)・その間の速さ・揺れ（0〜1）。
    int hitStopFrames = 9;
    float hitStopScale = 0.05f;
    float hitShake = 0.7f;
    // 打ち退けの瞬間、胴の煙が体から弾け出る速さ (m/秒) と半径 (1 倍の体での m)。0 で出さない。
    float hitBurst = 3.5f;
    float hitBurstRadius = 0.9f;
    // 崩れ落ちた瞬間（刀が地面を打つ）：揺れ（0〜1）と、胴の煙が弾ける速さ (m/秒)。
    float fallShake = 0.45f;
    float fallBurst = 2.0f;
    // 継ぎ目から漏れる黒い煙（Leak。胴の煙の域）：中心の毎秒の密度・半径（1 倍の体での m）・外へ吹く速さ (m/秒)。
    float leakDensity = 35.0f;
    float leakRadius = 0.22f;
    float leakSpeed = 1.2f;
    // 打ち退けられてから火が回るまで、胴の煙の量をこの割合まで薄くする（0〜1。崩れて煙が抜けた姿：兜と胴が煙に隠れない）。
    float slumpSmoke = 0.35f;
    // 太陽へ伸ばす間（Reach）：太陽の所に出す画面の星の明るさ（終わりにここまで）。
    float reachStar = 0.9f;
    // 掴む（Grab の終わり）：小さな揺れ（0〜1）、拳へ吸い込む輪の強さと出始めの大きさ（画面の高さを 1）、吸い込む長さ (秒)、
    // 画が暗くなる強さ（0〜1）と長さ (秒)。
    float grabShake = 0.3f;
    float grabShock = 1.0f;
    float grabShockRadius = 0.9f;
    float grabShockSeconds = 0.32f;
    float grabDim = 0.55f;
    float grabDimSeconds = 0.7f;
    // 拳の火（Ignite）：燃料の強さ・半径（1 倍の体での m）・温度。
    float fistFuel = 150.0f;
    float fistFuelRadius = 0.3f;
    float fistFuelHeat = 3.0f;
    // 火が回る（Spread）：腕を伝う火の燃料の強さ、胴の煙の火の強さを Spread の終わりにここまで上げる（0〜1）。
    float spreadFuel = 240.0f;
    float spreadFire = 0.9f;
    // 火が回る（Spread）：胴の煙の柱を、胸から足元へ降りていく火の燃料の強さ（0 で出さない）と半径（1 倍の体での m）。
    // Spread が終わった後も、演出の終わりまで、この割合で燃やし続ける（0 で Spread と一緒に止む）。
    float columnFuel = 620.0f;
    float columnFuelRadius = 0.42f;
    float columnSustain = 0.3f;
    // 燃え上がり（火の先が胸に届いた瞬間。c-280b71）：胴の煙の柱へ一気に入れる燃料の強さ（0 で出さない）・半径（1 倍の
    // 体での m）・長さ (秒)、外へ膨らませる速さ (m/秒)、小さな揺れ、火の粉を何倍にするか・元へ戻るまでの秒。
    float flareFuel = 650.0f;
    float flareRadius = 0.6f;
    float flareSeconds = 0.5f;
    float flarePush = 2.0f;
    float flareShake = 0.5f;
    float flareEmbers = 3.0f;
    float flareEmbersSeconds = 1.4f;
    // 場の色（Field の終わりに、フェーズ 2 の色のこの割合まで）。
    float fieldWarm = 0.85f;
    // 刀に火を移す（Fuse）：火の先が左手より先へ進む割合（1 で手と同じ所、2 で倍の速さ）、刀身の燃料の強さ・半径 (m)・温度。
    float fuseLead = 1.1f;
    float bladeFuel = 90.0f;
    float bladeFuelRadius = 0.3f;
    float bladeFuelHeat = 3.0f;
    // 火の壁（Wall）：ボスの正面に立ち上がる炎の壁（場の火の域）。舞台の足元から正面へこの距離 (m)、壁の半分の幅 (m)、
    // 燃える地面として塗る幅 (m)、真ん中から端へ広がる長さ (秒)。炎を持ち上げる燃料：点の数・強さ・半径 (m)・温度・上へ
    // 吹く速さ (m/秒)。一閃で壁が爆ぜる：壁に沿って外へ走る炎の塊の半径 (m。0 で出さない) と、走る速さ (m/秒)。
    float wallDistance = 6.2f;
    float wallHalfWidth = 7.5f;
    float wallPaintWidth = 1.6f;
    float wallRiseSeconds = 0.4f;
    int wallPoints = 10;
    float wallFuel = 85.0f;
    float wallFuelRadius = 0.9f;
    float wallFuelHeat = 6.0f;
    float wallFuelRise = 5.0f;
    float wallBurstRadius = 2.0f;
    float wallBurstSpeed = 45.0f;
    // 振りかぶり（Charge）：刃口の光と体を包む気配（ゆがみ）の強さ、画の張りつめ（0〜1）。
    float chargeEdge = 2.4f;
    float chargeAura = 1.6f;
    float chargeTension = 0.55f;
    // 一閃（Slash の始め）：白黒の閃き・衝撃の輪（1 つ目・遅れて出る 2 つ目）・刀身を走る光・時間の停止 (F・速さ)・揺れ。
    bool slashMono = true;
    float slashShock = 1.3f;
    float slashShockSecond = 0.8f;
    float slashGlint = 2.6f;
    int slashHitStopFrames = 7;
    float slashHitStopScale = 0.04f;
    float slashShake = 1.0f;
    // 一閃の炎の塊（場の火の域。見た目だけ）：剣先の落ちた所の塊の半径 (m。0 で出さない)、刀身に沿って地面に並べる数と
    // その先へ続ける数、続けて出す回数と間 (秒)。
    float slashBlastRadius = 2.4f;
    int slashBlastAlong = 2;
    int slashBlastAhead = 3;
    int slashBlastWaves = 3;
    float slashBlastInterval = 0.06f;
    // 火の輪（Slash）：広がり切る半径 (m。0 で出さない)・広がる長さ (秒)・光を下げて消す長さ (秒)・資産の細い輪の明るさ。
    float ringRadius = 13.0f;
    float ringSeconds = 0.26f;
    float ringFadeSeconds = 0.18f;
    float ringThinGlow = 0.0f;
    // 一閃で地面の霧を外へ押す強さ (m/秒) と押す半径 (m)。
    float fogPush = 9.0f;
    float fogPushRadius = 12.0f;
    // 飛ばした時：演出で入れた火を吸い消す長さ (秒)・球の半径（1 倍の体での m）・消す速さ (1/秒)。
    float skipQuenchSeconds = 0.3f;
    float skipQuenchRadius = 0.5f;
    float skipQuenchRate = 40.0f;
};

// 事件段の時刻（演出の資産の秒。無ければ start が負）。
struct BossShiftCueTime {
    float start = -1.0f;
    float length = 0.0f;
};

// フェーズの変わり目の演出の実行時の状態（保存しない）。
struct BossPhaseShiftState {
    BossShiftBeat beat = BossShiftBeat::None;
    // いまの段に入ってからの tick。
    int frame = 0;
    // 舞台：ボスの足元と向き（ボスは演出の間ここに立つ。骨格の体はクリップの中で動く）。変わり目に入った時の向き。
    HIKARI::GameProjectFloat3 origin{};
    float yaw = 0.0f;
    float startYaw = 0.0f;
    // 空中で 1 本目が空になった時の、体の高さ (m。記録に書く)。
    float fallHeight = 0.0f;
    // 演出の資産：流している手綱、事件段（拍ごと）、演技の長さの上限 (tick)。
    HIKARI::GameProjectSequencePlaybackHandle sequence{};
    std::array<BossShiftCueTime, kBossShiftCueCount> cues{};
    int showTicks = 0;
    // 規則の時計（tick ごとに読む資産の時刻）と、見た目の時計（描画のフレームで、資産の時刻の 1 歩先）。
    float ruleSeconds = 0.0f;
    float showSeconds = 0.0f;
    float sequenceSeen = -1.0f;
    float sequenceStall = 0.0f;
    bool sequenceStalled = false;
    // 規則が見た拍（一閃でフェーズ 2 の見た目へ）。プレイヤーを置き直す頼み（BossSystem が次に実行する）。
    bool slashed = false;
    bool placePending = false;
    HIKARI::GameProjectFloat3 placeAt{};
    // 置いた次の tick に、足が地面より下に埋まっていないかを 1 度だけ見る頼み（c-7d3039）。
    bool placeCheckPending = false;
    // 舞台を決めた時の太陽の向き（太陽へ向かう。読めなければ sunValid が偽）。
    HIKARI::GameProjectFloat3 sunDirection{0.0f, 1.0f, 0.0f};
    bool sunValid = false;
    // 飛ばす：押された・飛ばした。演出の後の、技を選ばない残り (F)。
    bool skipRequested = false;
    bool skipped = false;
    int calmLeft = 0;

    // 見た目（BossPhaseShiftVisuals）：拍ごとに一度だけ出す効果を出したか。
    std::array<bool, kBossShiftCueCount> fired{};
    // 掴んだ時の、太陽の所（拳の所。画面の星と吸い込む輪の中心）と、吸い込みの残り・暗さの残り (秒)。
    HIKARI::GameProjectFloat3 sunPoint{};
    bool sunPointValid = false;
    float grabLeft = 0.0f;
    float dimLeft = 0.0f;
    // 目の熱さ（0〜1：ふだんの色 → フェーズ 2 の色）、胴の火の下限、場の色の下限（演出が決める）。
    float eyesHeat = 0.0f;
    float fireFloor = 0.0f;
    float fieldFloor = 0.0f;
    // 燃え上がりの残り (秒)、火の粉を増やしている残り (秒)、もう燃え上がったか。
    float flareLeft = 0.0f;
    float embersLeft = 0.0f;
    bool flared = false;
    // 刀身に灯った火の、根元から届いた所（握りから剣先へ m。灯っていなければ負）。灯り切った（刀の火を出す）。
    float bladeLit = -1.0f;
    bool bladeDone = false;
    // 渡し：演出の資産が最後に見せたカメラの姿勢と、混ぜ始めてからの秒。
    bool filmValid = false;
    HIKARI::GameProjectFloat3 filmEye{};
    HIKARI::GameProjectFloat3 filmTarget{};
    float filmFov = 1.0f;
    float settleClock = 0.0f;
    // 一閃の炎の塊：残りの回数・次までの秒・落ちた所と刀身の地面の上の向き・刀身の長さ。
    int blastLeft = 0;
    float blastWait = 0.0f;
    HIKARI::GameProjectFloat3 blastAt{};
    HIKARI::GameProjectFloat3 blastAlong{0.0f, 0.0f, 1.0f};
    float blastReach = 0.0f;
    // 火の壁が爆ぜる：落ちた所から壁に沿って走った距離 (m。負は走っていない)。
    float wallBurst = -1.0f;
    // 火の輪のエフェクトと、出してからの秒。
    HIKARI::GameProjectVfxPlaybackHandle ringVfx{};
    float ringAge = 0.0f;
    // 飛ばした時に吸い消す所（拳・腕・胴・刀身）と、吸い消す残りの秒。飛ばしたのを見たか・そのフレームか。
    std::array<HIKARI::GameProjectFloat3, 10> quenchAt{};
    int quenchCount = 0;
    float quenchLeft = 0.0f;
    bool skipSeen = false;
    bool skipFrame = false;
    // 変わり目に入ってから、規則の刀の火（BossRules::IsBladeLit）が点いたのを見たか（最初に点く時は剣先の火球を出さない）。
    bool bladeLitSeen = false;
    // 演出の作業場のプレビュー（編集中）：骨格の体を資産の時刻の姿にしているか、読んだプレビューの手綱。
    bool previewing = false;
    HIKARI::GameProjectSequencePlaybackHandle previewHandle{};
};

} // namespace Project_TD2_2
