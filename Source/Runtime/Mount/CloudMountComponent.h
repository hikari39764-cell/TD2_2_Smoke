#pragma once

#include <cstdint>
#include <string>

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {

// 筋斗雲の段階。Gameplay.Mount を押すたびに、乗る（Summoning → Riding）と降りる（Dismissing → None）を切り替える。
enum class CloudMountState { None, Summoning, Riding, Dismissing };

// 飛行の入力。描画フレームの入力から入れ、固定 tick が使う。
struct CloudMountInput {
    // Gameplay.Move の左右（右が正）と前後（前が正）。
    float strafe = 0.0f;
    float forward = 0.0f;
    // Gameplay.Jump（上昇）と Gameplay.Descend（下降）を押している。
    bool ascend = false;
    bool descend = false;
    // Gameplay.Sprint（ブースト）を押している。
    bool boost = false;
};

// 飛行の手応えの値。カメラ（④ c-db5975）と見た目（⑤〜⑧）が読むために毎 tick 書く。読む側は書き換えない。保存しない。
struct CloudMountFlightReadout {
    // いまの速さ (m/s)。騎乗していなければ 0。
    float speed = 0.0f;
    // この tick の速さの変わり方 (m/s²。速くなる時が正)。カメラの加速の遅れに使う。
    float acceleration = 0.0f;
    // ブースト中（Gameplay.Sprint を押し、精力が残っている間）。
    bool boosting = false;
    // ブーストの弾き出しの残り（押した瞬間 1、boostKickSeconds で 0）。
    float boostKick = 0.0f;
    // 旋回の強さ（−1〜1、右へ回る時が正）。いまの速さで回れる最大の速さで回っている時が ±1。
    float turn = 0.0f;
    // 急降下の度合い（0〜1）。下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。前後左右の入力がある時だけ。
    float dive = 0.0f;
    // 巡航・ブーストの速さを超えている分 (m/s)。急降下の後に overspeedDecaySeconds かけて 0 へ戻る。
    float overspeed = 0.0f;
    // 地面すれすれの持ち上げをこの tick に掛けた（騎乗中に地面に触れたまま速く進んでいる）。
    bool groundSkim = false;
};

// 召喚と解除の演出の実行時の状態（CloudMountVisuals が描画フレームごとに進める。保存しない）。
struct CloudMountArrivalState {
    // 前の描画フレームに見た段階。段階の変わり目で、飛来・散りを始める。
    CloudMountState lastState = CloudMountState::None;
    // 飛んで来ている間か、飛び始めからの秒、飛ぶ長さ（秒）、飛び始めてからの描画フレームの数。
    bool flying = false;
    float age = 0.0f;
    float flySeconds = 0.0f;
    int flyFrames = 0;
    // 空中（落下の受け止め・持ち上げの無い召喚）の飛来か。下から受け止め、深く沈む。
    bool catching = false;
    // 足元から見た飛び始めの位置と、道の曲がりを決める点（世界の向き、m）。
    HIKARI::GameProjectFloat3 from{};
    HIKARI::GameProjectFloat3 bend{};
    // 前の描画フレームの根の位置と、そこから測った根の速さ (m/s。確かめ用)。
    HIKARI::GameProjectFloat3 lastRoot{};
    bool hasLastRoot = false;
    float rootSpeed = 0.0f;
    // 飛んでいる間の雲の向き（度）。着いた後はここから体の向きへ戻す。
    float yaw = 0.0f;
    float pitch = 0.0f;
    // 着いてからの秒（負は着いていない）と、その時の沈む深さ (m)。
    float landedAge = -1.0f;
    float sinkDepth = 0.0f;
    // 軌跡の帯の明るさ（0〜1）。
    float trail = 0.0f;
    // 散り始めてからの秒（負は散っていない）と、その間に留める根の変換。
    float scatterAge = -1.0f;
    HIKARI::GameProjectTransform scatterRoot{};
    // 前の描画フレームに送った根の変換。
    HIKARI::GameProjectTransform lastTransform{};
    // 着いた瞬間のカメラの沈み。
    Feedback::CameraShake cameraDip{};
    // 着いた瞬間の飾りの下準備を済ませた。
    bool prewarmed = false;
};

// プレイヤーの筋斗雲。Player と同じ物体に付ける。状態と飛行の運動の規則は CloudMountRules、入力を読んで運動を出すのは
// CloudMountSystem。見た目（CloudMountVisuals）とカメラの要求（CloudMountCamera）は別の所に置く。ここは調整値と実行時の状態だけ。
struct CloudMount : HIKARI::Component {
    HIKARI_COMPONENT(CloudMount, "筋斗雲")

    // 召喚：summonFrames F で、足元を地面から rideHeight 浮いた高さへ持ち上げる（今より低くはしない）。
    FIELD(
        int, summonFrames, 30, "浮き上がるまで (F)", HIKARI::Range(1, 600), HIKARI::Group("召喚"))
    NOTE("Gameplay.Mount（F／LB）を押すたびに、乗る・降りるを切り替えます。地上でも落下中でも呼べます"
         "（通常の状態で、砲撃などの動作の最中でない時）。")
    FIELD(float, rideHeight, 1.2f, "浮き上がる高さ（足元から地面まで m）", HIKARI::Range(0.0f, 20.0f))
    // 持ち上げの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）。
    FIELD(float, summonEasePower, 2.0f, "浮き上がりの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）",
          HIKARI::Range(1.0f, 5.0f))
    // 召喚の時に足元の地面を探す距離 (m)。これより下に地面が無ければ、その場の高さで乗る（落下中の召喚）。
    FIELD(float, summonGroundReach, 4.0f, "足元の地面を探す距離 (m。これより高い所ではその場で乗る)",
          HIKARI::Range(0.0f, 100.0f))
    // 解除：dismissFrames F その場で止まってから、いつもの重力で落ちる。
    FIELD(int, dismissFrames, 12, "降りる時にその場で止まる長さ (F。その後はいつもの重力で落ちる)",
          HIKARI::Range(0, 600))
    // 飛行：入力が無ければその場に浮く。前後はカメラの見ている向き、左右は水平の横、上昇・下降は真上・真下。
    FIELD(float, cruiseSpeed, 16.0f, "前後左右の速さ (m/s)", HIKARI::Range(0.0f, 100.0f), HIKARI::Group("飛行"))
    NOTE("入力が無ければその場に浮きます（高さも変わりません）。前後はカメラの見ている向き"
         "（見上げ・見下ろしを含む）、左右は水平の横、Jump は真上、Descend は真下です。"
         "地形や物には当たって止まります。")
    FIELD(float, verticalSpeed, 10.0f, "上昇・下降の速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, accelSeconds, 0.35f, "最高速になるまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, brakeSeconds, 0.5f, "入力を離して止まるまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 5.0f))
    // 体を水平の進む向きへ回す速さ（度/秒）。
    FIELD(float, turnSpeed, 540.0f, "体を進む向きへ回す速さ (度/s)", HIKARI::Range(1.0f, 5000.0f))
    // このカメラの見下ろし角度（度）を水平の飛行とみなす。0 でカメラの向きのまま（始めのカメラは 16 度見下ろす）。
    FIELD(
        float, levelPitch, 16.0f, "水平に飛ぶカメラの見下ろし角度 (度。0 でカメラの向きのまま)",
        HIKARI::Range(-45.0f, 45.0f))
    NOTE("前を押した時の上り下りの角度＝カメラの見下ろし角度 − この値。始めのカメラ（16 度見下ろす）で"
         "水平に飛ぶよう 16 にしています。")
    // どの状態でも超えない速さ (m/s)。⑤ の雲の流体の域は 1 フレームに 60 m/s を超える動きを瞬間移動と見なす。
    FIELD(
        float, maxFlightSpeed, 45.0f, "どの状態でも超えない速さ (m/s)", HIKARI::Range(1.0f, 100.0f))
    NOTE("雲の流体（⑤）は 1 フレームに 60 m/s を超える動きを瞬間移動と見なすので、60 未満にしてください。")
    // 地面すれすれの持ち上げ：騎乗中に地面に触れ、水平の速さが groundSkimMinSpeed 以上なら、その tick の縦の速さの
    // 下向きを捨てて groundSkimSpeed (m/s) 上へ出す。地面に触れたまま速く進むと運動学ソルバーが平らに近い所で止まる
    // 件（エンジンへの要望 e-dca15c）を避け、起伏を滑るように越える。groundSkimSpeed 0 で切る。
    // 持ち上げた後 groundSkimHoldSeconds の間は下へ向かわない（地面へ向けて飛び続けても、持ち上げと下降を毎 tick
    // 繰り返して震えず、地面に沿って滑る）。
    FIELD(float, groundSkimSpeed, 3.0f, "地面に触れたまま速く進む時に上へ出す速さ (m/s、0 で切る)",
          HIKARI::Range(0.0f, 20.0f))
    FIELD(float, groundSkimMinSpeed, 6.0f, "上へ出し始める水平の速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(
        float, groundSkimHoldSeconds, 0.2f, "上へ出した後、下へ向かわない長さ (秒)", HIKARI::Range(0.0f, 2.0f))
    NOTE("地面に触れたまま速く進むと、平らに近い所でも止まってしまう（エンジンの件）ので、騎乗中に地面に"
         "触れたら下向きの速さを捨てて少し上へ出し、起伏を滑るように越えます。")
    // ブースト：Gameplay.Sprint を押している間の最高速。押した瞬間は boostKickSeconds で boostKickSpeed まで弾き出す。
    FIELD(float, boostSpeed, 32.0f, "ブーストの速さ (m/s)", HIKARI::Range(0.0f, 100.0f), HIKARI::Group("ブースト"))
    NOTE("Gameplay.Sprint（Shift／L3）を押している間、前後左右の入力があればブーストします。押した瞬間に"
         "弾き出し、その後はブーストの速さで安定します。精力が尽きたら、入力を離すまでブーストしません。")
    FIELD(float, boostKickSpeed, 28.0f, "弾き出しの速さ (m/s。押した瞬間にここまで一気に上げる)",
          HIKARI::Range(0.0f, 100.0f))
    FIELD(float, boostKickSeconds, 0.15f, "弾き出しの長さ (秒、0 で一瞬)", HIKARI::Range(0.0f, 2.0f))
    // ブースト中に使う精力（/秒）。尽きたら入力を離すまでブーストしない。0 で使い放題。
    FIELD(float, boostStaminaPerSecond, 12.0f, "使う精力 (/秒、0 で使い放題)", HIKARI::Range(0.0f, 200.0f))
    // 急降下：下向きに進んでいる間、下向きの度合いに応じて速さが伸びる（度合い 1 で diveAccel、上限 diveMaxSpeed）。
    // 度合いは下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。浅い下りでは伸びない（速さが溜まり続け
    // ないように）。前で飛べる一番急な下りは「騎乗中のカメラの見下ろす限度 − levelPitch」（既定 75 − 16 = 59 度）。
    FIELD(float, diveAccel, 12.0f, "度合い 1 の時の伸び (m/s²)", HIKARI::Range(0.0f, 100.0f), HIKARI::Group("急降下"))
    NOTE("前後左右の入力で下向きに進んでいる間、下向きの度合いに応じて速さが伸びます。引き起こした後は、"
         "巡航（ブースト中はブースト）より速い分がゆっくり戻るので、速いまま上へ抜けられます。"
         "Descend だけの下降は伸びません（降りる場所を選ぶ時の、決まった速さの下降）。")
    FIELD(float, diveMaxSpeed, 45.0f, "急降下の最高速 (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, diveStartDegrees, 10.0f, "伸び始める下向きの角度 (度。これより浅い下りは伸びない)",
          HIKARI::Range(0.0f, 89.0f))
    FIELD(
        float, diveFullDegrees, 55.0f, "度合いが 1 になる下向きの角度 (度)", HIKARI::Range(1.0f, 90.0f))
    NOTE("前で飛べる一番急な下りは、騎乗中のカメラの見下ろす限度 − levelPitch（既定 75 − 16 = 59 度）。"
         "Descend を足すともっと急になります。")
    // 巡航・ブーストより速い分は、急降下をやめてからこの秒数で戻る（その間は速いまま上へ抜けられる）。
    FIELD(float, overspeedDecaySeconds, 1.5f, "速い分が戻るまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 10.0f))
    // 旋回：速度の向きを入力の向きへ回す速さ（度/秒）。巡航の速さ以下で turnRateCruise、ブーストの速さ以上で
    // turnRateBoost、間は速さで補間する。旋回の半径＝速さ ÷ 回る速さ（ラジアン/秒）なので速いほど大回りになる。
    FIELD(float, turnRateCruise, 180.0f, "巡航の回る速さ (度/s)", HIKARI::Range(1.0f, 2000.0f), HIKARI::Group("旋回"))
    NOTE("進む向きを入力の向きへ、毎秒この角度まで回します。巡航の速さ以下は巡航の値、ブーストの速さ以上は"
         "ブーストの値、間は速さで補間します。旋回の半径＝速さ ÷ 回る速さ（巡航 16 m/s・180 度で約 5.1 m、"
         "ブースト 32 m/s・90 度で約 20.4 m）。止まり方は「飛行」の値のままです。")
    FIELD(float, turnRateBoost, 90.0f, "ブーストの回る速さ (度/s)", HIKARI::Range(1.0f, 2000.0f))
    // 落下の受け止め：乗っていない時、足場を離れて catchFallMeters 落ちたら自動で召喚し、下向きの速さを
    // catchBrakeSeconds でならして止める。自分で降りた後の落下は、catchAfterDismiss が true の時だけ受け止める。
    FIELD(
        bool, autoCatch, true, "落下を自動で受け止める", HIKARI::Group("受け止め"))
    NOTE("乗っていない時に足場を離れて落ちると、自動で筋斗雲を呼んで受け止めます（通常の状態の時）。"
         "落ちた高さは足場を離れてから一番高い所から数えます。")
    FIELD(float, catchFallMeters, 12.0f, "受け止めるまでに落ちる高さ (m)", HIKARI::Range(0.0f, 500.0f))
    FIELD(float, catchBrakeSeconds, 0.4f, "受け止めた後、落ちる勢いを止めるまで (秒、0 で一瞬)",
          HIKARI::Range(0.0f, 5.0f))
    FIELD(bool, catchAfterDismiss, false, "自分で降りた後の落下も受け止める")
    // 飛べる範囲：場の原点から水平の半径 boundsRadius、高さ boundsMinY〜boundsMaxY（世界の Y）。境界の手前
    // boundsSoftMeters から外向きの速さを弱め、境界の上で 0 にする（壁に当たったようには止めない）。
    // 高さの既定は NimbusSky の島の床（Y 3400）から −150〜+300 m。
    FIELD(float, boundsRadius, 400.0f, "場の原点からの水平の半径 (m)", HIKARI::Range(1.0f, 100000.0f),
          HIKARI::Group("範囲"))
    NOTE("騎乗中に飛べる範囲です。境界の手前から外向きの速さを弱め、境界の上で 0 にします。"
         "境界に沿う向きの速さはそのまま残ります。高さは世界の Y（NimbusSky の島の床は 3400）。"
         "下の限度は雲海の上端より上に置いてください。")
    FIELD(float, boundsMinY, 3250.0f, "下の限度 (世界の Y)", HIKARI::Range(-100000.0f, 100000.0f))
    FIELD(float, boundsMaxY, 3700.0f, "上の限度 (世界の Y)", HIKARI::Range(-100000.0f, 100000.0f))
    FIELD(float, boundsSoftMeters, 30.0f, "外向きの速さを弱め始める距離 (m)", HIKARI::Range(1.0f, 1000.0f))
    // 距離 (m) と縦の画角（度）：止まっている・巡航・ブースト。間は速さで補間し、ブースト中はブーストの値。
    // 巡航・ブーストより速い時（急降下）はブーストの値。距離は cameraZoomSeconds、画角は cameraFovSeconds でならす。
    FIELD(float, cameraDistanceStop, 6.0f, "距離：止まっている (m)", HIKARI::Range(0.5f, 50.0f),
          HIKARI::Group("カメラ"))
    NOTE("騎乗中だけ、Player のカメラへ値を渡します（姿勢を作るのは Player のカメラのまま）。降りると "
         "cameraReleaseSeconds で普段の値へ戻ります。")
    FIELD(float, cameraDistanceCruise, 7.5f, "距離：巡航 (m)", HIKARI::Range(0.5f, 50.0f))
    FIELD(float, cameraDistanceBoost, 9.5f, "距離：ブースト (m)", HIKARI::Range(0.5f, 50.0f))
    FIELD(float, cameraZoomSeconds, 0.35f, "距離・見下ろしのならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, cameraFovStop, 60.0f, "画角：止まっている (度)", HIKARI::Range(10.0f, 120.0f))
    FIELD(float, cameraFovCruise, 68.0f, "画角：巡航 (度)", HIKARI::Range(10.0f, 120.0f))
    FIELD(float, cameraFovBoost, 82.0f, "画角：ブースト (度)", HIKARI::Range(10.0f, 120.0f))
    FIELD(
        float, cameraFovSeconds, 0.15f, "画角のならし (秒)", HIKARI::Range(0.0f, 3.0f))
    NOTE("止まっている → 巡航の速さ → ブーストの速さの間は速さで補間します。ブースト中はすぐブーストの値を"
         "目標にし、急降下などで速い時もブーストの値です。")
    // ブーストの出だしに、ならさずに足す画角（度）と、その長さ（秒）。初めの半分で上がり、残りで戻る。その間は画角の
    // ならしを短くしてブーストの画角へ素早く届かせるので、ブーストの画角を一度これだけ行き過ぎて戻る。
    FIELD(float, cameraBoostFovKick, 6.0f, "ブーストの出だしに行き過ぎる画角 (度、0 で無し)",
          HIKARI::Range(0.0f, 30.0f))
    FIELD(float, cameraBoostFovKickSeconds, 0.25f, "出だしの画角が戻るまで (秒)", HIKARI::Range(0.0f, 2.0f))
    // 傾き：旋回の強さ（−1〜1）× 上限（度）。上限は巡航の速さで cameraRollCruise、ブーストで cameraRollBoost、
    // 巡航より遅い時は速さに比例して小さくする。cameraRollSeconds でならす。
    FIELD(bool, cameraRollEnabled, true, "旋回で画面を傾ける")
    FIELD(float, cameraRollCruise, 12.0f, "傾きの上限：巡航 (度、0 で傾けない)", HIKARI::Range(0.0f, 45.0f))
    FIELD(float, cameraRollBoost, 20.0f, "傾きの上限：ブースト (度、0 で傾けない)", HIKARI::Range(0.0f, 45.0f))
    FIELD(
        float, cameraRollSeconds, 0.25f, "傾きのならし (秒)", HIKARI::Range(0.0f, 3.0f))
    NOTE("傾き＝旋回の強さ（−1〜1。右へ回ると右へ傾く）× 上限。巡航より遅い時は速さに比例して小さくなります。"
         "酔いやすい人は切るか上限を 0 に。")
    // 水平に追うならし（秒）。速さが増えている間は、増え方が cameraFollowFullAccel (m/s²) で cameraFollowAccelSeconds
    // まで伸ばし（体が先に画面の奥へ出る）、減っている間は cameraFollowBrakeSeconds まで縮める。伸ばした分は
    // cameraFollowRecoverSeconds で戻す（カメラが追いつく）。
    FIELD(float, cameraFollowSeconds, 0.09f, "水平に追うならし (秒)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, cameraFollowAccelSeconds, 0.22f, "加速中に伸ばすならし (秒)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, cameraFollowBrakeSeconds, 0.06f, "減速中に縮めるならし (秒)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, cameraFollowFullAccel, 60.0f, "伸ばし切る加速 (m/s²)", HIKARI::Range(1.0f, 500.0f))
    FIELD(float, cameraFollowRecoverSeconds, 0.05f, "伸ばした分を戻すならし (秒)", HIKARI::Range(0.0f, 2.0f))
    NOTE("速さが増えている間だけ水平に追うならしを伸ばし、体が先に画面の奥へ出てからカメラが追いつきます"
         "（ブーストの弾き出しは約 80 m/s²、止まった所から巡航へは約 46 m/s²、急降下は 12 m/s²）。")
    // 騎乗中のカメラ（PlayerCamera への要求。姿勢を作るのは PlayerCamera だけ）。上下の追い方と見上げの限度は
    // Player の「カメラ」の値の代わりに使う。cameraVerticalSeconds は止まっている時の上下のならしで、水平の速さが巡航へ
    // 近づくほど水平のならし（加速の遅れを含む）へ寄せる（体が飛ぶ向きのまま画面の奥へ離れ、上下へ外れない）。
    FIELD(float, cameraVerticalSeconds, 0.08f, "上下に追うならし：止まっている時 (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(
        float, cameraVerticalDeadZone, 0.0f, "上下を追わない幅 (m)", HIKARI::Range(0.0f, 10.0f))
    NOTE("上下のならしは、水平の速さが巡航へ近づくほど水平のならしに寄せます（遅れが飛ぶ向きに沿い、体が画面の"
         "上下へずれずに奥へ離れます）。")
    // 急降下の度合い 1 で足す、見せるだけの見下ろし（度。進む向きは変えない）。
    FIELD(float, diveLookDegrees, 6.0f, "急降下で足す見下ろし (度。見せるだけで進む向きは変えない)",
          HIKARI::Range(0.0f, 30.0f))
    FIELD(float, cameraMinPitch, -70.0f, "見上げる限度 (度、負で見上げ)", HIKARI::Range(-89.0f, 89.0f))
    FIELD(float, cameraMaxPitch, 75.0f, "見下ろす限度 (度)", HIKARI::Range(-89.0f, 89.0f))
    // 見上げた時に、視点が軸からどこまで下がってよいか (m)。Player の値（1.5）のままだと、見上げて上る時に距離が
    // 縮んで引きが効かない。
    FIELD(float, cameraEyeFloor, 4.0f, "見上げた時に視点が軸から下がれる限度 (m)", HIKARI::Range(0.0f, 50.0f))
    // 動きのぼかし：ブースト中と急降下の度合いに合わせて強める（TAA／DLAA／DLSS の時だけ効く）。強さは cameraBlurSeconds
    // でならす。シャッター角 0 で切る。
    FIELD(float, cameraBlurShutterDegrees, 90.0f, "動きのぼかしのシャッター角 (度、0 で切る)",
          HIKARI::Range(0.0f, 360.0f))
    FIELD(float, cameraBlurMaxPixels, 8.0f, "ぼかしの長さの上限 (1080p の画素)", HIKARI::Range(0.0f, 64.0f))
    FIELD(
        float, cameraBlurSeconds, 0.2f, "ぼかしの強まり・弱まりのならし (秒)", HIKARI::Range(0.0f, 3.0f))
    NOTE("ぼかしはブースト中と急降下の度合いに合わせて強めます。TAA／DLAA／DLSS の時だけ効きます。"
         "全画面の流れ（⑦）と重なるので控えめから。")
    // ブースト中だけの細かい震え：揺れ幅 (m)・速さ (Hz)。離すと cameraBoostShakeFadeSeconds で消える。0 で切る。
    FIELD(float, cameraBoostShake, 0.015f, "ブースト中の震えの揺れ幅 (m、0 で切る)", HIKARI::Range(0.0f, 0.1f))
    FIELD(float, cameraBoostShakeFrequency, 26.0f, "震えの速さ (Hz)", HIKARI::Range(1.0f, 60.0f))
    FIELD(float, cameraBoostShakeFadeSeconds, 0.15f, "離した後に震えが消えるまで (秒)", HIKARI::Range(0.0f, 1.0f))
    // 降りた後、距離・画角・傾きと、騎乗中だけ許した見上げ・見下ろしを普段の値へ戻すならし（秒）。
    FIELD(
        float, cameraReleaseSeconds, 0.4f, "降りた後に普段のカメラへ戻すならし (秒)", HIKARI::Range(0.0f, 3.0f))
    NOTE("降りた後、距離・画角・傾きと、騎乗中だけ許した見上げ・見下ろし（普段の限度の外）を一度に戻さず、"
         "このならしで戻します。")
    // 雲の見た目（CloudMountVisuals）：足元に付いて動く流体の雲のエフェクト。空なら出さない。資産の入力は
    // input.appear（体の雲の量）・input.speed（尾の量）・input.boost（吹き出しと金の縁の明るさ）。
    FIELD(
        HIKARI::VfxAsset, cloudEffect, (HIKARI::VfxAsset{"467fbd610a584de599d6946884fcdb67"}),
        "雲のエフェクト（足元に付く流体。空なら出さない）", HIKARI::Group("雲の見た目"))
    NOTE("足元に付いて動く流体の雲です。色・明るさ・形・尾の伸び方（相対風と消散）はエフェクト資産"
         "（NimbusCloud）で、量と尾の量・揺れ・傾きはここで変えます。")
    // エフェクトの原点（資産の足元）を、体の足元からどれだけ下げるか (m)。
    FIELD(float, cloudBelowFeet, 0.0f, "エフェクトの原点を足元から下げる量 (m)", HIKARI::Range(-2.0f, 2.0f))
    // 体の雲の量（倍率）。出す・消すは cloudAppearSeconds・cloudVanishSeconds で 0〜1 をならす（⑥ の演出の口）。
    FIELD(float, cloudAmount, 1.0f, "体の雲の量（倍率）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudAppearSeconds, 0.3f, "雲が湧き出るまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudVanishSeconds, 0.25f, "降りて雲の湧きが止まるまで (秒、0 で一瞬)", HIKARI::Range(0.0f, 5.0f))
    // 尾の量：速さ ÷ cloudTailFullSpeed（上限 cloudTailLimit）× cloudTailAmount。尾の長さは資産の相対風と消散で決まる。
    FIELD(float, cloudTailAmount, 1.0f, "尾の量（倍率。速さが下の値の時）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudTailFullSpeed, 32.0f, "尾の量が倍率どおりになる速さ (m/s)", HIKARI::Range(1.0f, 100.0f))
    FIELD(
        float, cloudTailLimit, 1.4f, "それより速い時の尾の量の上限（倍）", HIKARI::Range(0.0f, 5.0f))
    NOTE("尾の量＝速さ ÷ 上の速さ（上限まで）× 倍率。止まると尾の源は止まり、残った尾は消散で雲へ戻るように"
         "消えます。")
    // 吹き出しと金の縁の強さ：ブースト中 cloudBoostAmount、弾き出しの間 cloudBoostKickAmount を足し、急降下の度合い ×
    // cloudDiveAmount を足す。cloudBoostSeconds でならす。
    FIELD(float, cloudBoostAmount, 1.0f, "ブースト中の吹き出しと金の縁の強さ", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudBoostKickAmount, 1.0f, "弾き出しの間に足す強さ", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudDiveAmount, 0.6f, "急降下の度合い 1 で足す強さ", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, cloudBoostSeconds, 0.12f, "強さのならし (秒)", HIKARI::Range(0.0f, 2.0f))
    // 止まっている時の上下の揺れ（雲だけ。体は動かさない）：揺れ幅 (m) と 1 往復の秒。水平の速さ cloudIdleSpeed で 0 になる。
    FIELD(float, cloudBobHeight, 0.08f, "止まっている時の上下の揺れ幅 (m。雲だけ)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, cloudBobSeconds, 2.4f, "揺れの 1 往復 (秒)", HIKARI::Range(0.1f, 20.0f))
    FIELD(float, cloudIdleSpeed, 6.0f, "揺れが無くなり、上り下りの傾きを全部掛ける水平の速さ (m/s)",
          HIKARI::Range(0.1f, 100.0f))
    // 旋回で雲を内側へ傾ける角度（旋回の強さ ±1 で、度）と、そのならし（秒）。
    FIELD(float, cloudBankDegrees, 18.0f, "旋回で雲を内側へ傾ける角度 (度。体は傾けない)", HIKARI::Range(0.0f, 60.0f))
    FIELD(float, cloudBankSeconds, 0.2f, "旋回の傾きのならし (秒)", HIKARI::Range(0.0f, 2.0f))
    // 上り下りで雲の前を進む向きへ傾ける割合（0〜1）と上限（度）、ならし（秒）。水平の速さが cloudIdleSpeed 以上で全部掛け、
    // 真上・真下だけの上昇・下降では傾けない。急降下で相対風が雲を体の上へ吹き上げず、尾が進む向きの後ろへ流れる。
    FIELD(float, cloudPitchFollow, 0.8f, "上り下りで雲の前を進む向きへ傾ける割合 (0〜1)", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, cloudPitchMaxDegrees, 45.0f, "上り下りの傾きの上限 (度)", HIKARI::Range(0.0f, 89.0f))
    FIELD(
        float, cloudPitchSeconds, 0.25f, "上り下りの傾きのならし (秒)", HIKARI::Range(0.0f, 2.0f))
    NOTE("急降下では、傾けないと相対風が雲を体の上へ吹き上げます。傾けると雲は進む向きに沿い、尾は後ろへ"
         "流れます。真上・真下だけの上昇・下降では傾けません。")
    // 乗っていない間もエフェクトを出したままにする（中は空で域は眠る。召喚のたびに出し直す詰まりを避ける）。false なら
    // 雲が消えてから cloudLingerSeconds 後に止める。
    FIELD(bool, cloudKeepAlive, true, "乗っていない間もエフェクトを出したままにする")
    FIELD(
        float, cloudLingerSeconds, 1.5f, "出したままにしない時、雲が消えてから止めるまで (秒)",
        HIKARI::Range(0.0f, 10.0f))
    NOTE("出したままにすると、乗っていない間は中が空で域が眠り、ほぼ重さが無くなります。止めると、"
         "召喚のたびにエフェクトを出し直す 1 フレームの詰まりが出ます。")
    // 召喚の演出（CloudMountArrival・CloudMountVisuals）：雲（本体のエフェクトの根）が、体の向きの斜め後ろ上（後ろへ
    // summonFromDistance・上へ summonFromHeight、真後ろから横へ summonFromSideDegrees）から尾を引いて足元へ飛んで来る。
    // 着くのは召喚の summonArriveFraction（summonFrames の割合）の時。落下中の受け止めでは足元の下へ summonCatchDip 回り
    // 込んで下から受け止める。飛ぶ速さが summonFlySpeedLimit を超える時は飛び始めを近くする（流体は 1 フレームに 60 m/s を
    // 超える動きを瞬間移動と見なし、尾が引けない）。
    FIELD(float, summonFromDistance, 25.0f, "飛び始め：後ろへの水平の距離 (m)", HIKARI::Range(0.0f, 200.0f),
          HIKARI::Group("召喚の演出"))
    NOTE("乗る時（落下の受け止めも）、雲が体の向きの斜め後ろ上から尾を引いて足元へ飛んで来ます。雲は本体の"
         "エフェクトをそのまま飛ばすので、流体の域は増えません。着くのは召喚の summonFrames × 割合の時で、"
         "体の動き（規則）は変えません。")
    FIELD(float, summonFromHeight, 10.0f, "飛び始め：足元からの高さ (m)", HIKARI::Range(-50.0f, 100.0f))
    FIELD(float, summonFromSideDegrees, 60.0f, "飛び始め：真後ろから右へ回す角度 (度。負で左)",
          HIKARI::Range(-180.0f, 180.0f))
    FIELD(float, summonArriveFraction, 0.8f, "着く時（召喚の長さの割合）", HIKARI::Range(0.05f, 1.0f))
    FIELD(float, summonFlyEasePower, 1.3f, "飛ぶ速さのゆるみ（1 は等速、大きいほど初め速く足元の手前でゆっくり）",
          HIKARI::Range(1.0f, 4.0f))
    FIELD(float, summonFlySpeedLimit, 55.0f, "飛ぶ速さの上限 (m/s。超える時は飛び始めを近くする。0 で上限なし)",
          HIKARI::Range(0.0f, 200.0f))
    NOTE("流体は 1 フレームに 60 m/s を超える動きを瞬間移動と見なし、尾が引けません。既定（30 F の召喚の "
         "0.8 で着く＝0.4 秒）では、25 m・10 m からだと速すぎるので、上限 55 m/s に合わせて道を縮め、"
         "約 16 m・6 m の所から飛びます。遠くから飛ばすなら summonFrames（召喚）か割合を大きくします。")
    FIELD(float, summonApproachHeight, 1.0f, "地上：足元へ入る手前の道の高さ (m。0 で水平に滑り込む)",
          HIKARI::Range(-10.0f, 10.0f))
    FIELD(float, summonCatchDip, 1.5f, "落下中：足元の下へ回り込む深さ (m)", HIKARI::Range(0.0f, 20.0f))
    // 飛んで来る間の雲の量・尾の量（倍率）と、軌跡の帯の明るさ。帯は着いてから summonTrailFadeSeconds で消す。
    FIELD(float, summonFlyAmount, 0.7f, "飛んで来る間の雲の量（倍率）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, summonFlyTail, 0.4f, "飛んで来る間の尾の量（倍率）", HIKARI::Range(0.0f, 5.0f))
    // 飛んで来る間の渦の強さ（域の乱流の倍率）。相対風でなめらかに伸びる煙を巻いて、綿の縁を保つ。着いた後は膨らみと
    // 一緒に 1 へ戻る。
    FIELD(float, summonFlySwirl, 2.5f, "飛んで来る間の渦の強さ（乱流の倍率。1 で普段のまま）",
          HIKARI::Range(0.0f, 10.0f))
    FIELD(float, summonTrailAmount, 1.0f, "軌跡の帯の明るさ（0 で出さない）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, summonTrailFadeSeconds, 0.15f, "着いてから帯が消えるまで (秒)", HIKARI::Range(0.0f, 2.0f))
    // 着いた時：雲だけが沈んで戻る（体は動かさない。地上 summonSinkMeters、受け止め summonCatchSinkMeters を
    // summonSinkSeconds で）、雲の量を一度 summonPuffAmount 足して膨らませる（summonPuffSeconds で戻る）、向きを飛ぶ向きから
    // 体の向きへ summonSettleSeconds で戻す。
    FIELD(float, summonSinkMeters, 0.15f, "地上で着いた時に雲が沈む深さ (m。体は動かさない)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, summonCatchSinkMeters, 0.4f, "落下を受け止めた時に雲が沈む深さ (m)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, summonSinkSeconds, 0.3f, "沈んで戻るまで (秒)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, summonPuffAmount, 1.0f, "着いた瞬間に足す雲の量（膨らみ）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, summonPuffSeconds, 0.3f, "膨らみが戻るまで (秒)", HIKARI::Range(0.0f, 2.0f))
    FIELD(float, summonSettleSeconds, 0.2f, "飛んで来た向きから体の向きへ戻すまで (秒)", HIKARI::Range(0.0f, 2.0f))
    // 着いた瞬間の飾り（空気のゆがみの輪・光・粒。流体の域は持たない）と、輪の強さ・大きさの倍率。
    FIELD(HIKARI::VfxAsset, arriveEffect, (HIKARI::VfxAsset{"5c1e7a3f2b8d4e60a9f4d2c7b1e08a36"}),
          "着いた瞬間の飾り（空気のゆがみの輪・光の粒。空なら出さない）")
    FIELD(float, arriveRingStrength, 1.0f, "ゆがみの輪の強さ（倍率）", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, arriveRingScale, 1.0f, "飾りの大きさ（倍率）", HIKARI::Range(0.1f, 5.0f))
    // 場面の始めに飾りを見えない強さで一度出し、画面のゆがみの合成の初回の準備を済ませる（最初の召喚で止まらない）。
    FIELD(bool, arrivePrewarm, true, "場面の始めに飾りの下準備をする（最初の召喚の詰まりを防ぐ）")
    NOTE("ゆがみの輪は太く柔らかい輪をカメラへ向けて広げます（細い輪はぎざぎざに見えます）。強すぎると像が"
         "崩れて段が見えるので、倍率は 1.3 くらいまで。背景が空だけの時はほとんど見えません。")
    // 着いた瞬間のカメラの手応え：下へ summonCameraDip (m) 沈んで summonCameraDipSeconds で戻る。0 で切る。
    FIELD(float, summonCameraDip, 0.05f, "着いた瞬間にカメラを下へ沈める量 (m、0 で切る)", HIKARI::Range(0.0f, 0.5f))
    FIELD(float, summonCameraDipSeconds, 0.25f, "カメラが戻るまで (秒)", HIKARI::Range(0.0f, 2.0f))
    // 解除の演出：雲はその場に残り（体と一緒に落ちない）、disperseSeconds の間だけ外へ押し広げながら吸って散る。
    // disperseHoldSeconds 後に根を体の足元へ戻す（その時は空）。散る瞬間に disperseEffect（光の粒）を出す。
    FIELD(
        HIKARI::VfxAsset, disperseEffect, (HIKARI::VfxAsset{"a7d93e15c4f2480bb6e1f0a2d95c3b74"}),
        "散る瞬間の飾り（光の粒。空なら出さない）", HIKARI::Group("解除の演出"))
    NOTE("降りる時（被弾で解けた時も）、雲はその場に残り、外へほどけるように膨らみながら吸われて消えます。"
         "残った煙は域の消散（NimbusCloud）でも薄くなります。")
    FIELD(float, disperseSeconds, 0.5f, "散らす長さ (秒。外へ押し広げて吸う)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, disperseHoldSeconds, 1.0f, "雲を降りた所に留める長さ (秒。この後は体の足元へ戻る)",
          HIKARI::Range(0.0f, 5.0f))
    // 散らす間の渦の強さ（域の乱流の倍率）。煙を筋にちぎって散らす。
    FIELD(float, disperseSwirl, 3.0f, "散らす間の渦の強さ（乱流の倍率。煙を筋にちぎる）", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, speedFxLimit, 1.0f, "強さの上限 (0〜1。0 で出さない：酔いやすい人向け)", HIKARI::Range(0.0f, 1.0f),
          HIKARI::Group("速度感"))
    NOTE("速く飛ぶ間、進む向き（画面の中心近く）を澄ませたまま、縁を放射状に流します（ぼかし・細い筋・"
         "わずかな色ずれ・縁の減光）。巡航は弱く、ブーストと急降下で強くなります。歩き・止まっている時は"
         "一切掛かりません。ぼかしの長さ・筋・色ずれ・減光・中心の澄んだ広さは効果資産（FlightSpeed）で変えます。")
    FIELD(float, speedFxStartSpeed, 4.0f, "掛かり始める速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, speedFxCruise, 0.15f, "巡航の速さの強さ", HIKARI::Range(0.0f, 1.0f))
    FIELD(float, speedFxBoost, 0.75f, "ブースト（中・ブーストの速さ）の強さ", HIKARI::Range(0.0f, 1.0f))
    FIELD(
        float, speedFxDive, 1.0f, "急降下（度合い 1・急降下の上限の速さ）の強さ", HIKARI::Range(0.0f, 1.0f))
    NOTE("強さ＝速さの曲線（掛かり始める速さまで 0 → 巡航 → ブーストの速さ → 急降下の上限の速さ）と、"
         "ブースト中・急降下の度合いの大きい方。")
    FIELD(float, speedFxRiseSeconds, 0.25f, "強まる時のならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, speedFxFallSeconds, 0.5f, "弱まる時のならし (秒)", HIKARI::Range(0.0f, 3.0f))
    // 流れの中心（進む向きを画面へ写した点）のならし（秒）と、画面の中心からの限度（画面の端が 1）。横へ飛ぶ時は
    // 限度の所に置き、流れはほぼ平行になる。
    FIELD(float, speedFxCenterSeconds, 0.1f, "流れの中心（進む向き）のならし (秒)", HIKARI::Range(0.0f, 2.0f))
    FIELD(
        float, speedFxCenterLimit, 1.4f, "流れの中心を置ける、画面の中心からの限度（画面の端が 1）",
        HIKARI::Range(0.0f, 5.0f))
    NOTE("横へ飛ぶと進む向きは画面の外になります。その時は向きのまま限度の所に中心を置き、流れはほぼ"
         "平行になります。")
    // 体と雲を守る球：体の中心から speedFxGuardDrop (m) 下げた所の、半径 speedFxGuardRadius (m)。画面で守る楕円の
    // 内は流さない（縁のぼかし具合は効果資産の guardFeather）。半径 0 で守らない。
    FIELD(float, speedFxGuardRadius, 1.5f, "流さない体と雲の球の半径 (m。0 で守らない)", HIKARI::Range(0.0f, 10.0f))
    FIELD(float, speedFxGuardDrop, 0.35f, "その球の中心を体の中心から下げる量 (m。雲を含める)",
          HIKARI::Range(-5.0f, 5.0f))
    NOTE("動きのぼかし（節：カメラ）と重なります。流れを強める時は両方を見て決めてください。")
    // 速度感（全画面の効果 FlightSpeed：進む向きを中心に、縁が放射状に流れる・ぼかし・わずかな色ずれ・縁の減光）。
    // 強さは速さの曲線（speedFxStartSpeed まで 0 → 巡航 speedFxCruise → ブーストの速さ speedFxBoost → 急降下の上限の速さ
    // speedFxDive）と、ブースト中 speedFxBoost・急降下の度合い × speedFxDive の大きい方。強まる時 speedFxRiseSeconds、
    // 弱まる時 speedFxFallSeconds でならし、speedFxLimit で抑える（0 で出さない。酔いやすい人向け）。強さ 0 の間は
    // 効果を一切出さない。見た目（ぼかしの長さ・筋・色ずれ・減光・中心の澄んだ広さ）は効果資産の値。
    FIELD(HIKARI::PostProfile, speedFxProfile, (HIKARI::PostProfile{"0b62bd7baa7d48e4a65dc8867fd3d990"}),
          "効果のプロファイル（FlightSpeed。空なら出さない）")
    // 飛行の飾り（シェーダーの層だけ。流体の域は使わない）。trimEffect（NimbusTrail）を雲の根と同じ変換で出し続け、
    // 入力で出し入れする：長い尾の帯は、速さが trimStreakStartSpeed で 0 → trimStreakFullSpeed（巡航）で
    // trimStreakAmount、ブースト中は trimStreakBoostAmount（帯の長さは資産の秒 × 速さで、速いほど長い）。
    // 空気のゆがみと高速の気流の錐は、それぞれの StartSpeed で 0 → FullSpeed で量。どれも強まる時 trimRiseSeconds、
    // 弱まる時 trimFallSeconds でならす（秒の時定数）。騎乗中だけ出す（召喚の飛来・解除の散りの間は 0）。
    FIELD(HIKARI::VfxAsset, trimEffect, (HIKARI::VfxAsset{"3e8b1c94d27f4a05b6c0e9a7f15d2b48"}),
          "飛行の飾り（NimbusTrail。空なら出さない）", HIKARI::Group("飾り"))
    NOTE("騎乗中に、雲の左右の端から後ろへ伸びる長い尾の帯、雲の後ろの空気のゆがみ、速い時に体のまわりを"
         "前から後ろへ流れる気流の錐を出します（シェーダーの層だけ）。量は速さで決まり、0 にすると出ません。"
         "帯の長さ・幅・色、ゆがみの強さ、錐の筋の置き場所は効果資産（NimbusTrail）の値です。")
    FIELD(float, trimStreakAmount, 1.0f, "尾の帯の明るさ（巡航の速さで。0 で出さない）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, trimStreakBoostAmount, 1.8f, "ブースト中の尾の帯の明るさ", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, trimStreakStartSpeed, 6.0f, "尾の帯が出始める速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimStreakFullSpeed, 16.0f, "尾の帯が明るさに届く速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimHazeAmount, 1.0f, "空気のゆがみの強さ（倍率。資産の強さに掛かる。0 で出さない）",
          HIKARI::Range(0.0f, 1.5f))
    FIELD(float, trimHazeStartSpeed, 14.0f, "ゆがみが出始める速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimHazeFullSpeed, 32.0f, "ゆがみが強さに届く速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimConeAmount, 1.0f, "高速の気流の錐の明るさ（0 で出さない）", HIKARI::Range(0.0f, 5.0f))
    FIELD(float, trimConeStartSpeed, 24.0f, "錐が出始める速さ (m/s。巡航 16・ブースト 32・急降下の上限 45)",
          HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimConeFullSpeed, 38.0f, "錐が明るさに届く速さ (m/s)", HIKARI::Range(0.0f, 100.0f))
    FIELD(float, trimRiseSeconds, 0.15f, "飾りが強まる時のならし (秒)", HIKARI::Range(0.0f, 3.0f))
    FIELD(float, trimFallSeconds, 0.35f, "飾りが弱まる時のならし (秒)", HIKARI::Range(0.0f, 3.0f))
    // ブーストの衝撃の輪：ブーストを押した瞬間（騎乗中）に、体の中心へ boostRingEffect（NimbusBoost）を一度だけ出す。
    // 世界に置くので、前へ飛ぶ体から見て後ろへ抜ける。強さ（倍率。資産の 0.28 に掛かる。0.35 を超えると段が出る）と大きさ。
    FIELD(HIKARI::VfxAsset, boostRingEffect, (HIKARI::VfxAsset{"c72f05a1e94b4d3a8e6b1f0d2a7c9e53"}),
          "ブーストを押した瞬間の衝撃の輪（NimbusBoost。空なら出さない）")
    FIELD(float, boostRingStrength, 1.0f, "衝撃の輪のゆがみの強さ（倍率。1.25 くらいまで。上げすぎると段が出る）",
          HIKARI::Range(0.0f, 3.0f))
    FIELD(float, boostRingScale, 1.0f, "衝撃の輪の大きさ（倍率）", HIKARI::Range(0.1f, 5.0f))
    // 確かめ用：この F ごとに位置・速度をログに出す（0 は出さない）。
    FIELD(int, traceEveryFrames, 0, "位置と速度をログに出す間隔 (F、0 は出さない)", HIKARI::Range(0, 7200),
          HIKARI::Group("確かめ"))

    // 実行時のデータは保存しない。
    CloudMountState state = CloudMountState::None;
    // いまの段階に入ってから進んだ固定 tick の数。
    int stateFrame = 0;
    // Mount が押された（固定 tick が使ったら消す）。
    bool mountPressed = false;
    CloudMountInput input{};
    // 飛行の速度（世界 m/s）。縦はこの値をソルバーの重力の差し替えで出す。
    HIKARI::GameProjectFloat3 velocity{};
    // 外へ知らせる飛行の値（④〜⑧が読む）。
    CloudMountFlightReadout flight{};
    // ブーストが尽きた（入力を離すまでブーストしない）。
    bool boostExhausted = false;
    // 弾き出しの残り (秒)。
    float boostKickLeft = 0.0f;
    // 速い分を戻す速さ (m/s²)。戻し始めた時の「速い分 ÷ overspeedDecaySeconds」。戻していない間は 0。
    float overspeedDecayRate = 0.0f;
    // 前の固定 tick の体の中心と、そこから実際に動いた速さ（当たって止められた分の速さを持ち越さないために使う）。
    HIKARI::GameProjectFloat3 lastPosition{};
    bool hasLastPosition = false;
    float achievedSpeed = 0.0f;
    // 落下の受け止め：足場を離れてからの一番高い体の中心の高さ。受け止める落下か（自分で降りた後は catchAfterDismiss）。
    float fallPeakY = 0.0f;
    bool catchArmed = true;
    // 一番高い所を一度でも測ったか（測る前は受け止めない：場面の始めの 1 tick に、高さ 0 から落ちたと見なさない）。
    bool fallPeakKnown = false;
    // 降りてから（解除・被弾で解けた後）地面に着くまでの落下の間。この間は上下を素早く追うカメラを要求し続ける。
    bool fallingFromMount = false;
    // 受け止めた時の下向きの速さ (m/s、正) と、ならしの残り (秒)。catching は受け止めの召喚の間（縦は持ち上げの輪郭
    // ではなく、落ちる勢いのならしで出す）。
    float catchFallSpeed = 0.0f;
    float catchBrakeLeft = 0.0f;
    bool catching = false;
    // 地面すれすれの持ち上げの後、下へ向かわない残り (秒)。
    float groundSkimHoldLeft = 0.0f;
    // カメラの演出（CloudMountCamera が描画フレームごとに進める）。いまの水平に追うならし（負は騎乗の始め）、
    // ブーストの出だしからの秒（負は無し）、前のフレームのブースト、動きのぼかしの強さ（0〜1）、震え。
    float cameraFollowNow = -1.0f;
    float cameraKickAge = -1.0f;
    bool cameraWasBoosting = false;
    float cameraBlurWeight = 0.0f;
    Feedback::CameraShake cameraShake{};
    // 召喚の始めと終わりの体の中心の高さ。
    float summonStartY = 0.0f;
    float summonTargetY = 0.0f;
    // 立った時の、体の中心から足元までの高さ。地面に立っている間に測る（測る前は Player のカプセルの半分）。
    float standOffset = 0.85f;
    // ログ用：この tick に提出した縦の速さと重力の差し替え、提出が受け入れられたか。
    float submittedVertical = 0.0f;
    float submittedGravity = 0.0f;
    bool motionAccepted = true;
    // 雲の見た目（CloudMountVisuals が描画フレームごとに進める）：再生、出し入れ（0〜1）、吹き出しの強さ、旋回の傾きと上り下りの傾き（度）、
    // 揺れの時間、雲が消えてからの秒、送った入力が受け付けられたか。
    HIKARI::GameProjectVfxPlaybackHandle cloudVfx{};
    float cloudAppear = 0.0f;
    float cloudBoost = 0.0f;
    float cloudBank = 0.0f;
    float cloudPitch = 0.0f;
    float cloudBobTime = 0.0f;
    float cloudHiddenSeconds = 0.0f;
    bool cloudAccepted = true;
    // 召喚と解除の演出（CloudMountVisuals が描画フレームごとに進める）。
    CloudMountArrivalState arrival{};
    // 飛行の飾り（CloudMountVisuals が描画フレームごとに進める）：再生、ならした帯・ゆがみ・錐の量、前のフレームの
    // ブースト、出した衝撃の輪の数（確かめ用）、送った入力が受け付けられたか。
    HIKARI::GameProjectVfxPlaybackHandle trimVfx{};
    float trimStreak = 0.0f;
    float trimHaze = 0.0f;
    float trimCone = 0.0f;
    bool trimWasBoosting = false;
    int boostRings = 0;
    bool trimAccepted = true;
    // 速度感（CloudMountSystem が描画フレームごとに進める）：ならした強さ（上限の前）、流れの中心（UV）と置いたか、
    // 前へ向かう度合い、最後の提出が受け付けられたか、受け付けられた提出の数と、出していない所から出し始めた数
    // （確かめ用）。
    float speedFxStrength = 0.0f;
    float speedFxCenterU = 0.5f;
    float speedFxCenterV = 0.5f;
    bool speedFxHasCenter = false;
    // 前へ向かう度合い（CloudMountSpeedFx::HeadingOnScreen）。止まって向きが決まらない間は前の値のまま。
    float speedFxFacing = 1.0f;
    bool speedFxAccepted = false;
    uint64_t speedFxSubmits = 0;
    uint64_t speedFxStarts = 0;
};

} // namespace Project_TD2_2
