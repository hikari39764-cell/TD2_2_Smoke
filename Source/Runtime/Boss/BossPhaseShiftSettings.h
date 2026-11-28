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
