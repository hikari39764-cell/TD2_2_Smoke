#pragma once

#include <array>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossIntroSettings.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出（c-8dd296）の拍：崩れる → 内から燃える → 火の輪が場を払う → 新しい構え → 済み。
// 規則（BossPhaseShift）が固定 tick ごとに進め、見た目（BossPhaseShiftVisuals）とカメラ（BossIntroCamera）が読む。
enum class BossShiftBeat { None, Collapse, Ignite, Burst, Settle, Done };

// フェーズの変わり目の演出（c-8dd296）：1 本目の条が空になった最後の当たりで時間が一瞬止まり、よろめいて膝をつき、刀を
// 地面に突き立てる（登場の「刺さった刀」と対）。鎧の継ぎ目から黒い煙が漏れ、胸の煙の中に火の粉が灯って内から外へ火が
// 回り、左手が刀身を根元から先へ拭って刀が灯る。立ち上がって刀を地面から斬り上げて抜くと、火の輪が場を払い（霧を
// 押しのけ、プレイヤーを中距離まで押し出す）、その閃光の中で鎧が灼け、場の色が変わる。最後にフェーズ 2 の構えに
// 収まり、HP の条が満ちる。位置は、変わり目に入った時のボスの足元から、プレイヤーのいる側を前にした局所
// （+X が左、+Z が前）。体の大きさを掛ける値は「1 倍の体での値」と書く。
struct BossPhaseShiftSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 演出をするか。切ると、変わり目は Boss.phaseShiftFrames の間、待機のまま（c-3e0ffa の形）。
    bool enabled = true;
    // 各拍の長さ (F)：崩れる・燃える・火の輪・構え。合わせた長さが変わり目の長さ（無敵で技を出さない）。
    int collapseFrames = 72;
    int igniteFrames = 108;
    int burstFrames = 72;
    int settleFrames = 108;
    // 演出の後、すぐには攻めて来ない間 (F。この間は技を選ばない。次の技は行動ツリーが決める)。
    int calmFrames = 45;
    // 飛ばす：演出の間にこの入力（行動 id）を押すと、フェーズ 2 の構えへ進む。飛ばせるか。
    bool skippable = true;
    std::string skipAction = "Gameplay.Menu";
    // 演出のカメラ：出すか、カットの表（Data/ からの場所）、演出の間にゲームのカメラが向くボスの高さ (m)。既定は切る
    // （オーナーの判断でいったん外す。仕組みと表は残す）。
    bool cameraEnabled = false;
    std::string cameraPath = "Boss/kasumi_bushi_phase_shift_camera.csv";
    float cameraFocusHeight = 4.0f;

    // 崩：最後の当たりで時間を止める長さ (F) と、その間の時間の速さ。よろめく（後ろへ反る）角度（度）・割合（崩の中で
    // ここまで）。膝をつく：腰を落とす量（立った高さに対する割合）・前へ傾く角度（度）・刀の側へ捻る角度（度。負が右）。
    // 刀が地面に刺さる割合（崩の中）。空中で 1 本目が空になった時、落ちる長さ（崩の中の割合）。
    int hitStopFrames = 9;
    float hitStopScale = 0.05f;
    float plantShare = 0.62f;
    float plantBuried = 1.6f;
    // 鎧の継ぎ目から漏れる黒い煙（胴の煙の域へ入れる）：中心の毎秒の密度、半径 (m。1 倍の体での値)、外へ吹く速さ (m/秒)。
    // 崩の始めから満ち、燃の終わりまでに火に替わって引く。
    float leakDensity = 90.0f;
    float leakRadius = 0.22f;
    float leakSpeed = 1.2f;
    // 燃：胸の煙の中の火の粉（燃料の注入）の強さと半径（1 倍の体での値）、胴の煙の火の強さを燃の終わりにここまで上げる（0〜1）。
    float chestFuel = 40.0f;
    float chestRadius = 0.2f;
    float igniteFire = 0.65f;
    float wipeStartShare = 0.30f;
    float wipeEndShare = 0.86f;
    float bladeFuel = 80.0f;
    float bladeFuelRadius = 0.28f;
    float bladeFuelHeat = 3.0f;
    // 爆（爆の中の割合）：立ち上がり、柄を握って力を込め終わる（ここで刀が地面を離れ、火の輪と閃光）・八相に構え切る。
    float drawShare = 0.34f;
    float ripShare = 0.56f;
    // 火の輪：資産（流体の炎の輪。Boss の「炎の輪」と同じ）、広がり切る半径 (m)、広がる長さ (秒)、光を下げて消す長さ (秒)。
    float ringRadius = 13.0f;
    float ringSeconds = 0.24f;
    float ringFadeSeconds = 0.15f;
    // 火の輪：資産の細い輪（シェーダーの層 Ring Haze・Ring Glow・Ring Edge）の明るさ（0〜1）。細い輪は地面の予告に
    // 見えるので、既定は 0（出さない。太い炎の帯＝流体の炎だけ）。
    float ringThinGlow = 0.0f;
    // 飛ばした時：演出の間に流体へ入れた火（突き立てた刀の刀身・柄とその上・膝をついた籠手と胸の所）を、この秒数だけ吸い消す
    // （構え直した後に元の場所で燃え残らない）。吸い消す球の半径（1 倍の体での m）と、消す速さ（1/秒）。
    float skipQuenchSeconds = 0.3f;
    float skipQuenchRadius = 0.4f;
    float skipQuenchRate = 40.0f;
    // 閃光：衝撃の輪の大きさ（1 つ目・遅れて出る 2 つ目。細い輪が残ると予告に見えるので、既定は出さない。火の輪が払う）、刀身を走る光、スローの長さ（実時間の F。時間の倍率は登場の
    // introDrawSlowScale）、場の色をこの割合まで一気に進める（閃光に隠す。残りは fieldWarmSeconds で）。
    float flashShock = 0.0f;
    float flashShockSecond = 0.0f;
    float flashGlint = 2.4f;
    int flashSlowFrames = 30;
    float flashWarm = 0.45f;
    // 地面の霧を外へ押す強さ (m/秒) と押す半径 (m)。
    float fogPush = 9.0f;
    float fogPushRadius = 12.0f;
    // プレイヤーを押し出す：ボスからこの距離 (m) まで（もう遠ければ押さない）、押す距離の上限 (m)、押す長さ (F)、出だしの
    // 速さ（1 は等速）、場の縁（Boss.arenaRadius）からこれだけ内側で止める (m)。
    float pushToDistance = 13.0f;
    float pushMaxDistance = 10.0f;
    int pushFrames = 26;
    float pushEasePower = 2.4f;
    float pushArenaMargin = 3.0f;
};

// フェーズの変わり目の演出の実行時の状態（保存しない）。
struct BossPhaseShiftState {
    BossShiftBeat beat = BossShiftBeat::None;
    // いまの拍に入ってからの F（規則）。
    int frame = 0;
    // 演出の基準：変わり目に入った時の足元と、プレイヤーのいる側の向き（BossPoseKit の基準）。始めの体の向き。
    HIKARI::GameProjectFloat3 origin{};
    float yaw = 0.0f;
    float startYaw = 0.0f;
    // 空中で 1 本目が空になった時の、体の高さ (m。崩の始めに、ここから落ちて膝をつく)。
    float fallHeight = 0.0f;
    // フェーズ 2 の見た目を待っている（閃光で切り替える。BossBodyMath::PhaseTwoLook）。
    bool lookPending = false;
    // 閃光が出た（爆の、刀が地面を離れた tick）。プレイヤーを押す頼み（次の tick に PlayerRules へ渡す）。
    bool flashed = false;
    bool pushPending = false;
    // 飛ばす：押された・飛ばした。演出の後の、技を選ばない残り (F)。
    bool skipRequested = false;
    bool skipped = false;
    int calmLeft = 0;
    // 見た目（BossPhaseShiftVisuals）：前に見た拍と、その拍に入ってからの秒（描画のフレームで進める）。閃光を出したか。
    BossShiftBeat seenBeat = BossShiftBeat::None;
    float beatSeconds = 0.0f;
    bool hitSeen = false;
    bool flashSeen = false;
    bool plantSeen = false;
    // 火の輪のエフェクトと、出してからの秒。
    HIKARI::GameProjectVfxPlaybackHandle ringVfx{};
    float ringAge = 0.0f;
    // 飛ばした時に吸い消す所（演出の間、刀を突き立てている間の最後のフレームの刀身・柄とその上・籠手・胸。世界）と、吸い消す
    // 残りの秒。飛ばしたのを見たか・そのフレームか（そのフレームは火の舌を入れない。籠手の火が元の所から引かれない）。
    std::array<HIKARI::GameProjectFloat3, 10> quenchAt{};
    int quenchCount = 0;
    float quenchLeft = 0.0f;
    bool skipSeen = false;
    bool skipFrame = false;
    // 変わり目に入ってから、規則の刀の火（BossRules::IsBladeLit）が点いたのを見たか。最初に点く時（閃光・飛ばした時）は
    // 剣先の火球（igniteFlashRadius）を出さない（刀身は拭う手が灯す。飛ばした時に火球が元の剣先の所で燃え残らない）。
    bool bladeLitSeen = false;
    // 演出のカメラのカット（表を読んだもの。at は変わり目の始めからの F）。
    std::vector<BossIntroCut> cuts;
};

} // namespace Project_TD2_2
