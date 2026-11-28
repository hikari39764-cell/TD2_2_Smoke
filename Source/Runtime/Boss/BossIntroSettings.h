#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/SequenceTypes.h"
#include "HIKARI/Project/VfxTypes.h"
#include "BossIntroActorState.h"

namespace Project_TD2_2 {

// 登場の段階（c-e0c500）。散らばって待つ（役者は最初の片段の最初の姿）→ 演技（役者が骨格の動画を演じる）→ 一息
// （戦いの体に替わった後、ゲームのカメラへ戻る間）→ 済み。規則（BossIntro）が固定 tick ごとに進め、見た目
// （BossIntroVisuals・BossIntroActor）が読む。
enum class BossIntroPhase { Scattered, Show, Settle, Done };


// 効果の拍（演出の資産の事件段）の種類。名前は読んだ後に 1 回だけ見分ける（毎フレーム文字を比べない）。
enum class BossIntroCueKind : uint8_t {
    Unread,
    Other,
    HelmSeat,
    SwordOut,
    Eyes,
    SmokeStreak,
    SmokeRise,
    SmokeTendril,
    SmokeTendrilLeft,
    HandRight,
    HandLeft,
    HelmLift,
    SwordGrab,
    SmokeClear,
    Step,
    SmokeDisperse,
    SmokeAmbient,
    Omen,
    HelmGlint,
    // カメラの効果（c-280b71）：ピント（そのカットの始め／前の段から続く）、動きのぼかしの強い所。
    Focus,
    FocusLink,
    Blur,
    // 剣先の寄りのカットの間（この段がある演出は、抜刀の煙をこの段に合わせて出す）。
    SwordTip,
    // 右手を握り込む間（指の間から煙が押し出される）。
    HandFist
};

// 役者（BossIntroActor）が使う事件段の役：片段を流す（Anim.<片段>）、部品が骨へ飛ぶ（Part.<部品>.Fly）、落ちている部品が
// 痙攣する（Part.<部品>.Twitch）。
enum class BossIntroCueRole : uint8_t { None, Anim, PartFly, PartTwitch };

// 効果の拍 1 つ：事件段（名前・始め・長さ）と、入った・出たか。役者が使う段は、役と部品の番号（BossIntroPart。無ければ負）。
struct BossIntroCue {
    HIKARI::GameProjectSequenceEventClip clip{};
    BossIntroCueKind kind = BossIntroCueKind::Unread;
    bool entered = false;
    bool left = false;
    BossIntroCueRole role = BossIntroCueRole::None;
    int8_t part = -1;
    // 名前に付いた数（Focus：距離 m・強さ。Blur：シャッター角・px）。
    float valueA = 0.0f;
    float valueB = 1.0f;
};

// 登場の実行時の状態（保存しない）。
struct BossIntroState {
    BossIntroPhase phase = BossIntroPhase::Done;
    // いまの段に入ってからの F（固定 tick）。
    int frame = 0;
    // 舞台の枠：登場を始めた時のボスの足元と向き（Initialize が入れ、演技の前にプレイヤーが見えた tick に 1 度だけ
    // プレイヤーの側へ向ける）。演技の間、ボスの物体はここに止める（演出の資産のカメラは物体に付く）。
    HIKARI::GameProjectFloat3 origin{};
    float yaw = 0.0f;
    // 戦いの流れ（Flow）に止められたのを見たか（止められる前の最初の数 tick で始めない）。
    bool heldSeen = false;
    // 舞台の枠を、プレイヤーのいる側へ向けたか。
    bool faced = false;
    // 登場が始まった。
    bool started = false;
    // 役者と部品（BossIntroActor）。
    BossIntroActorState actor;
    // 演技の段の長さ (固定 tick。演出の資産の長さから)。
    int showTicks = 0;
    // 演出の資産：流している手綱（0 は流していない・断られた）。
    HIKARI::GameProjectSequencePlaybackHandle sequence{};
    // 見た目：最後に見た資産の時刻、その時刻のまま進まない秒、進まないので自分の時計に切り替えたか。
    float sequenceSeen = -1.0f;
    float sequenceStall = 0.0f;
    bool sequenceStalled = false;
    // 効果の拍（読んだ数だけ使う。入れ物は Begin で確保する）。役者が使う段を見分けたか、役者と部品を隠したか。
    std::vector<BossIntroCue> cues;
    int cueCount = 0;
    bool cueRolesRead = false;
    bool actorHidden = false;
    // 演技の最後に役者が担いでいた刀の置き場所（体に付く効果の置き場所の約束）。一息の段で、刀の効果をここから
    // 待機の構えへ移す。
    bool carryValid = false;
    HIKARI::GameProjectTransform carrySword{};
    float settleClock = 0.0f;
    // 見た目（BossIntroVisuals）：前に見た段、演技の時計（表の秒。描画のフレームで進める）、一息の段の秒、飛ばしたのを
    // 見たか、目が灯ったか（Eyes の拍から）。
    BossIntroPhase seenPhase = BossIntroPhase::Done;
    float showSeconds = 0.0f;
    float settleSeconds = 0.0f;
    bool skipSeen = false;
    bool eyesLit = false;
    // 登場の黒い煙（BossIntroSmoke）：流体の域の再生、演技の後に消し続けた秒、外へ 1 回押す頼み（兜が収まった・刀が抜けた）。
    HIKARI::GameProjectVfxPlaybackHandle smokeHandle{};
    float smokeLinger = 0.0f;
    // 地を走る煙の域：再生、道の始めと終わり（世界。拍の始めに決める）、拍の後に置いておいた秒。
    HIKARI::GameProjectVfxPlaybackHandle streakHandle{};
    HIKARI::GameProjectVfxPlaybackHandle eyeFireHandle{};
    bool streakSet = false;
    HIKARI::GameProjectFloat3 streakFrom{};
    HIKARI::GameProjectFloat3 streakTo{};
    float streakFade = 0.0f;
    bool smokeSeatPush = false;
    bool smokeDrawPush = false;
    // 刀身が引く煙：刀身を追いかける点の位置（世界）、もう置いたか、前に進めた時の演技の時刻 (秒)。
    std::array<HIKARI::GameProjectFloat3, 5> smokeTrail{};
    bool smokeTrailSet = false;
    float smokeTrailAt = 0.0f;
    // 編集器のプレビュー（Play の外）：流しているか、拍を読んだプレビューの手綱。
    bool previewing = false;
    HIKARI::GameProjectSequencePlaybackHandle previewHandle{};
    // 全画面の衝撃の輪（刀が抜けた瞬間）：始まってからの秒（負なら出していない）と、中心（世界）。
    float shockSeconds = -1.0f;
    HIKARI::GameProjectFloat3 shockAt{};
    // 剣先の星（全画面の効果）：始まってからの秒（負なら出していない）。
    float starSeconds = -1.0f;
    // 見得の終わり：霧を外へ流し始める 1 回の押しを頼んだか。
    bool dispersePush = false;
    // 序幕の黒い霧：点を巡らせる時計（秒。演技の前から進む。演技の時計は、始まるまで 0 のまま）。
    float ambientClock = 0.0f;
    // 落ちている兜の隙間の瞬き：頼まれた強さ（このフレームだけ。0〜1）。
    float helmGlint = 0.0f;
    // 演出の資産が流れている間の、最後に見たカメラの姿勢（演技の後、ここからゲームのカメラへ滑らかに渡す）。
    bool filmValid = false;
    HIKARI::GameProjectFloat3 filmEye{};
    HIKARI::GameProjectFloat3 filmTarget{};
    float filmFov = 1.0f;
    // 飛ばす：押された（次の tick に進める）・飛ばした。
    bool skipRequested = false;
    bool skipped = false;
    // 演出の動きのぼかしを頼んで受け付けられたか（変わった時だけ記録に書く）。
    bool motionBlurAccepted = false;
    // カメラの効果（c-280b71）。ピント：このフレームに拍が入れた距離 (m) と強さ（拍が無ければ focusSeen が false）、
    // 掛かっている割合（0〜1。実時間で出入りする）、最後に出した距離と強さ、受け付けられたか。
    bool focusSeen = false;
    float focusWantDistance = 0.0f;
    float focusWantStrength = 0.0f;
    float focusWeight = 0.0f;
    float focusDistance = 0.0f;
    float focusStrength = 0.0f;
    bool focusAccepted = false;
    // 動きのぼかし：このフレームに拍が入れた値（無ければ負）と、いま出している値（負はまだ決めていない）。
    float blurWantShutter = -1.0f;
    float blurWantPixels = -1.0f;
    float blurShutter = -1.0f;
    float blurPixels = -1.0f;
    // 上下の黒帯の出ている割合（0〜1）。
    float bars = 0.0f;
    // 一度きりの特効の頼み（拍が立て、描画の前に出す）と、漂う塵の再生。
    bool dustPending = false;
    bool debrisPending = false;
    bool hazePending = false;
    HIKARI::GameProjectVfxPlaybackHandle motesHandle{};
    // 刀の根元の細かい煙の域の再生と、置いた所（刀が入る所の、実際の地面。域を置く時に 1 回決める）。
    HIKARI::GameProjectVfxPlaybackHandle tipSmokeHandle{};
    HIKARI::GameProjectFloat3 tipGround{};
    // 手を握る拍の細かい煙の域の再生と、置いた所（世界。拍の始めに決める）。手首から指先までの長さ：拍の始め（開いた
    // 手）・ならした今の値（負＝まだ読んでいない）、握り込む勢い（0〜1。速く立ち上がり、ゆっくり引く）。
    HIKARI::GameProjectVfxPlaybackHandle fistSmokeHandle{};
    HIKARI::GameProjectFloat3 fistAt{};
    float fistOpen = 0.0f;
    float fistCurl = -1.0f;
    float fistSqueeze = 0.0f;
};

} // namespace Project_TD2_2
