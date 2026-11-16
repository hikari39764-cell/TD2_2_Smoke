#include "CloudMountComponent.h"

namespace Project_TD2_2 {

void CloudMountComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("召喚", true)) {
        fields.Note("Gameplay.Mount（F／LB）を押すたびに、乗る・降りるを切り替えます。地上でも落下中でも呼べます"
                    "（通常の状態で、砲撃などの動作の最中でない時）。");
        fields.Int("summonFrames", "浮き上がるまで (F)", summonFrames, 1, 600);
        fields.Float("rideHeight", "浮き上がる高さ（足元から地面まで m）", rideHeight, 0.0f, 20.0f);
        fields.Float("summonEasePower", "浮き上がりの出だしの速さ（1 は等速、大きいほど初め速くあとゆっくり）",
                     summonEasePower, 1.0f, 5.0f);
        fields.Float("summonGroundReach", "足元の地面を探す距離 (m。これより高い所ではその場で乗る)", summonGroundReach,
                     0.0f, 100.0f);
        fields.Int("dismissFrames", "降りる時にその場で止まる長さ (F。その後はいつもの重力で落ちる)", dismissFrames, 0,
                   600);
    }
    if (fields.Section("飛行", true)) {
        fields.Note("入力が無ければその場に浮きます（高さも変わりません）。前後はカメラの見ている向き"
                    "（見上げ・見下ろしを含む）、左右は水平の横、Jump は真上、Descend は真下です。"
                    "地形や物には当たって止まります。");
        fields.Float("cruiseSpeed", "前後左右の速さ (m/s)", cruiseSpeed, 0.0f, 100.0f);
        fields.Float("verticalSpeed", "上昇・下降の速さ (m/s)", verticalSpeed, 0.0f, 100.0f);
        fields.Float("accelSeconds", "最高速になるまで (秒、0 で一瞬)", accelSeconds, 0.0f, 5.0f);
        fields.Float("brakeSeconds", "入力を離して止まるまで (秒、0 で一瞬)", brakeSeconds, 0.0f, 5.0f);
        fields.Float("turnSpeed", "体を進む向きへ回す速さ (度/s)", turnSpeed, 1.0f, 5000.0f);
        fields.Float("levelPitch", "水平に飛ぶカメラの見下ろし角度 (度。0 でカメラの向きのまま)", levelPitch, -45.0f,
                     45.0f);
        fields.Note("前を押した時の上り下りの角度＝カメラの見下ろし角度 − この値。始めのカメラ（16 度見下ろす）で"
                    "水平に飛ぶよう 16 にしています。");
        fields.Float("maxFlightSpeed", "どの状態でも超えない速さ (m/s)", maxFlightSpeed, 1.0f, 100.0f);
        fields.Note("雲の流体（⑤）は 1 フレームに 60 m/s を超える動きを瞬間移動と見なすので、60 未満にしてください。");
        fields.Float("groundSkimSpeed", "地面に触れたまま速く進む時に上へ出す速さ (m/s、0 で切る)", groundSkimSpeed,
                     0.0f, 20.0f);
        fields.Float("groundSkimMinSpeed", "上へ出し始める水平の速さ (m/s)", groundSkimMinSpeed, 0.0f, 100.0f);
        fields.Float("groundSkimHoldSeconds", "上へ出した後、下へ向かわない長さ (秒)", groundSkimHoldSeconds, 0.0f,
                     2.0f);
        fields.Note("地面に触れたまま速く進むと、平らに近い所でも止まってしまう（エンジンの件）ので、騎乗中に地面に"
                    "触れたら下向きの速さを捨てて少し上へ出し、起伏を滑るように越えます。");
    }
    if (fields.Section("ブースト", true)) {
        fields.Note("Gameplay.Sprint（Shift／L3）を押している間、前後左右の入力があればブーストします。押した瞬間に"
                    "弾き出し、その後はブーストの速さで安定します。精力が尽きたら、入力を離すまでブーストしません。");
        fields.Float("boostSpeed", "ブーストの速さ (m/s)", boostSpeed, 0.0f, 100.0f);
        fields.Float("boostKickSpeed", "弾き出しの速さ (m/s。押した瞬間にここまで一気に上げる)", boostKickSpeed, 0.0f,
                     100.0f);
        fields.Float("boostKickSeconds", "弾き出しの長さ (秒、0 で一瞬)", boostKickSeconds, 0.0f, 2.0f);
        fields.Float("boostStaminaPerSecond", "使う精力 (/秒、0 で使い放題)", boostStaminaPerSecond, 0.0f, 200.0f);
    }
    if (fields.Section("急降下", true)) {
        fields.Note("前後左右の入力で下向きに進んでいる間、下向きの度合いに応じて速さが伸びます。引き起こした後は、"
                    "巡航（ブースト中はブースト）より速い分がゆっくり戻るので、速いまま上へ抜けられます。"
                    "Descend だけの下降は伸びません（降りる場所を選ぶ時の、決まった速さの下降）。");
        fields.Float("diveAccel", "度合い 1 の時の伸び (m/s²)", diveAccel, 0.0f, 100.0f);
        fields.Float("diveMaxSpeed", "急降下の最高速 (m/s)", diveMaxSpeed, 0.0f, 100.0f);
        fields.Float("diveStartDegrees", "伸び始める下向きの角度 (度。これより浅い下りは伸びない)", diveStartDegrees,
                     0.0f, 89.0f);
        fields.Float("diveFullDegrees", "度合いが 1 になる下向きの角度 (度)", diveFullDegrees, 1.0f, 90.0f);
        fields.Note("前で飛べる一番急な下りは、騎乗中のカメラの見下ろす限度 − levelPitch（既定 75 − 16 = 59 度）。"
                    "Descend を足すともっと急になります。");
        fields.Float("overspeedDecaySeconds", "速い分が戻るまで (秒、0 で一瞬)", overspeedDecaySeconds, 0.0f, 10.0f);
    }
    if (fields.Section("旋回", true)) {
        fields.Note("進む向きを入力の向きへ、毎秒この角度まで回します。巡航の速さ以下は巡航の値、ブーストの速さ以上は"
                    "ブーストの値、間は速さで補間します。旋回の半径＝速さ ÷ 回る速さ（巡航 16 m/s・180 度で約 5.1 m、"
                    "ブースト 32 m/s・90 度で約 20.4 m）。止まり方は「飛行」の値のままです。");
        fields.Float("turnRateCruise", "巡航の回る速さ (度/s)", turnRateCruise, 1.0f, 2000.0f);
        fields.Float("turnRateBoost", "ブーストの回る速さ (度/s)", turnRateBoost, 1.0f, 2000.0f);
    }
    if (fields.Section("受け止め", true)) {
        fields.Note("乗っていない時に足場を離れて落ちると、自動で筋斗雲を呼んで受け止めます（通常の状態の時）。"
                    "落ちた高さは足場を離れてから一番高い所から数えます。");
        fields.Bool("autoCatch", "落下を自動で受け止める", autoCatch);
        fields.Float("catchFallMeters", "受け止めるまでに落ちる高さ (m)", catchFallMeters, 0.0f, 500.0f);
        fields.Float("catchBrakeSeconds", "受け止めた後、落ちる勢いを止めるまで (秒、0 で一瞬)", catchBrakeSeconds,
                     0.0f, 5.0f);
        fields.Bool("catchAfterDismiss", "自分で降りた後の落下も受け止める", catchAfterDismiss);
    }
    if (fields.Section("範囲", true)) {
        fields.Note("騎乗中に飛べる範囲です。境界の手前から外向きの速さを弱め、境界の上で 0 にします。"
                    "境界に沿う向きの速さはそのまま残ります。高さは世界の Y（NimbusSky の島の床は 3400）。"
                    "下の限度は雲海の上端より上に置いてください。");
        fields.Float("boundsRadius", "場の原点からの水平の半径 (m)", boundsRadius, 1.0f, 100000.0f);
        fields.Float("boundsMinY", "下の限度 (世界の Y)", boundsMinY, -100000.0f, 100000.0f);
        fields.Float("boundsMaxY", "上の限度 (世界の Y)", boundsMaxY, -100000.0f, 100000.0f);
        fields.Float("boundsSoftMeters", "外向きの速さを弱め始める距離 (m)", boundsSoftMeters, 1.0f, 1000.0f);
    }
    if (fields.Section("カメラ")) {
        fields.Note("騎乗中だけ、Player のカメラへ値を渡します（姿勢を作るのは Player のカメラのまま）。降りると "
                    "cameraReleaseSeconds で普段の値へ戻ります。");
        fields.Float("cameraDistanceStop", "距離：止まっている (m)", cameraDistanceStop, 0.5f, 50.0f);
        fields.Float("cameraDistanceCruise", "距離：巡航 (m)", cameraDistanceCruise, 0.5f, 50.0f);
        fields.Float("cameraDistanceBoost", "距離：ブースト (m)", cameraDistanceBoost, 0.5f, 50.0f);
        fields.Float("cameraZoomSeconds", "距離・見下ろしのならし (秒)", cameraZoomSeconds, 0.0f, 3.0f);
        fields.Float("cameraFovStop", "画角：止まっている (度)", cameraFovStop, 10.0f, 120.0f);
        fields.Float("cameraFovCruise", "画角：巡航 (度)", cameraFovCruise, 10.0f, 120.0f);
        fields.Float("cameraFovBoost", "画角：ブースト (度)", cameraFovBoost, 10.0f, 120.0f);
        fields.Float("cameraFovSeconds", "画角のならし (秒)", cameraFovSeconds, 0.0f, 3.0f);
        fields.Note("止まっている → 巡航の速さ → ブーストの速さの間は速さで補間します。ブースト中はすぐブーストの値を"
                    "目標にし、急降下などで速い時もブーストの値です。");
        fields.Float("cameraBoostFovKick", "ブーストの出だしに行き過ぎる画角 (度、0 で無し)", cameraBoostFovKick, 0.0f,
                     30.0f);
        fields.Float("cameraBoostFovKickSeconds", "出だしの画角が戻るまで (秒)", cameraBoostFovKickSeconds, 0.0f, 2.0f);
        fields.Bool("cameraRollEnabled", "旋回で画面を傾ける", cameraRollEnabled);
        fields.Float("cameraRollCruise", "傾きの上限：巡航 (度、0 で傾けない)", cameraRollCruise, 0.0f, 45.0f);
        fields.Float("cameraRollBoost", "傾きの上限：ブースト (度、0 で傾けない)", cameraRollBoost, 0.0f, 45.0f);
        fields.Float("cameraRollSeconds", "傾きのならし (秒)", cameraRollSeconds, 0.0f, 3.0f);
        fields.Note(
            "傾き＝旋回の強さ（−1〜1。右へ回ると右へ傾く）× 上限。巡航より遅い時は速さに比例して小さくなります。"
            "酔いやすい人は切るか上限を 0 に。");
        fields.Float("cameraFollowSeconds", "水平に追うならし (秒)", cameraFollowSeconds, 0.0f, 1.0f);
        fields.Float("cameraFollowAccelSeconds", "加速中に伸ばすならし (秒)", cameraFollowAccelSeconds, 0.0f, 1.0f);
        fields.Float("cameraFollowBrakeSeconds", "減速中に縮めるならし (秒)", cameraFollowBrakeSeconds, 0.0f, 1.0f);
        fields.Float("cameraFollowFullAccel", "伸ばし切る加速 (m/s²)", cameraFollowFullAccel, 1.0f, 500.0f);
        fields.Float("cameraFollowRecoverSeconds", "伸ばした分を戻すならし (秒)", cameraFollowRecoverSeconds, 0.0f,
                     2.0f);
        fields.Note("速さが増えている間だけ水平に追うならしを伸ばし、体が先に画面の奥へ出てからカメラが追いつきます"
                    "（ブーストの弾き出しは約 80 m/s²、止まった所から巡航へは約 46 m/s²、急降下は 12 m/s²）。");
        fields.Float("cameraVerticalSeconds", "上下に追うならし：止まっている時 (秒)", cameraVerticalSeconds, 0.0f,
                     3.0f);
        fields.Float("cameraVerticalDeadZone", "上下を追わない幅 (m)", cameraVerticalDeadZone, 0.0f, 10.0f);
        fields.Note(
            "上下のならしは、水平の速さが巡航へ近づくほど水平のならしに寄せます（遅れが飛ぶ向きに沿い、体が画面の"
            "上下へずれずに奥へ離れます）。");
        fields.Float("diveLookDegrees", "急降下で足す見下ろし (度。見せるだけで進む向きは変えない)", diveLookDegrees,
                     0.0f, 30.0f);
        fields.Float("cameraMinPitch", "見上げる限度 (度、負で見上げ)", cameraMinPitch, -89.0f, 89.0f);
        fields.Float("cameraMaxPitch", "見下ろす限度 (度)", cameraMaxPitch, -89.0f, 89.0f);
        fields.Float("cameraEyeFloor", "見上げた時に視点が軸から下がれる限度 (m)", cameraEyeFloor, 0.0f, 50.0f);
        fields.Float("cameraBlurShutterDegrees", "動きのぼかしのシャッター角 (度、0 で切る)", cameraBlurShutterDegrees,
                     0.0f, 360.0f);
        fields.Float("cameraBlurMaxPixels", "ぼかしの長さの上限 (1080p の画素)", cameraBlurMaxPixels, 0.0f, 64.0f);
        fields.Float("cameraBlurSeconds", "ぼかしの強まり・弱まりのならし (秒)", cameraBlurSeconds, 0.0f, 3.0f);
        fields.Note("ぼかしはブースト中と急降下の度合いに合わせて強めます。TAA／DLAA／DLSS の時だけ効きます。"
                    "全画面の流れ（⑦）と重なるので控えめから。");
        fields.Float("cameraBoostShake", "ブースト中の震えの揺れ幅 (m、0 で切る)", cameraBoostShake, 0.0f, 0.1f);
        fields.Float("cameraBoostShakeFrequency", "震えの速さ (Hz)", cameraBoostShakeFrequency, 1.0f, 60.0f);
        fields.Float("cameraBoostShakeFadeSeconds", "離した後に震えが消えるまで (秒)", cameraBoostShakeFadeSeconds,
                     0.0f, 1.0f);
        fields.Float("cameraReleaseSeconds", "降りた後に普段のカメラへ戻すならし (秒)", cameraReleaseSeconds, 0.0f,
                     3.0f);
        fields.Note("降りた後、距離・画角・傾きと、騎乗中だけ許した見上げ・見下ろし（普段の限度の外）を一度に戻さず、"
                    "このならしで戻します。");
    }
    if (fields.Section("雲の見た目")) {
        fields.Note("足元に付いて動く流体の雲です。色・明るさ・形・尾の伸び方（相対風と消散）はエフェクト資産"
                    "（NimbusCloud）で、量と尾の量・揺れ・傾きはここで変えます。");
        fields.Vfx("cloudEffect", "雲のエフェクト（足元に付く流体。空なら出さない）", cloudEffect);
        fields.Float("cloudBelowFeet", "エフェクトの原点を足元から下げる量 (m)", cloudBelowFeet, -2.0f, 2.0f);
        fields.Float("cloudAmount", "体の雲の量（倍率）", cloudAmount, 0.0f, 5.0f);
        fields.Float("cloudAppearSeconds", "雲が湧き出るまで (秒、0 で一瞬)", cloudAppearSeconds, 0.0f, 5.0f);
        fields.Float("cloudVanishSeconds", "降りて雲の湧きが止まるまで (秒、0 で一瞬)", cloudVanishSeconds, 0.0f, 5.0f);
        fields.Float("cloudTailAmount", "尾の量（倍率。速さが下の値の時）", cloudTailAmount, 0.0f, 5.0f);
        fields.Float("cloudTailFullSpeed", "尾の量が倍率どおりになる速さ (m/s)", cloudTailFullSpeed, 1.0f, 100.0f);
        fields.Float("cloudTailLimit", "それより速い時の尾の量の上限（倍）", cloudTailLimit, 0.0f, 5.0f);
        fields.Note("尾の量＝速さ ÷ 上の速さ（上限まで）× 倍率。止まると尾の源は止まり、残った尾は消散で雲へ戻るように"
                    "消えます。");
        fields.Float("cloudBoostAmount", "ブースト中の吹き出しと金の縁の強さ", cloudBoostAmount, 0.0f, 5.0f);
        fields.Float("cloudBoostKickAmount", "弾き出しの間に足す強さ", cloudBoostKickAmount, 0.0f, 5.0f);
        fields.Float("cloudDiveAmount", "急降下の度合い 1 で足す強さ", cloudDiveAmount, 0.0f, 5.0f);
        fields.Float("cloudBoostSeconds", "強さのならし (秒)", cloudBoostSeconds, 0.0f, 2.0f);
        fields.Float("cloudBobHeight", "止まっている時の上下の揺れ幅 (m。雲だけ)", cloudBobHeight, 0.0f, 1.0f);
        fields.Float("cloudBobSeconds", "揺れの 1 往復 (秒)", cloudBobSeconds, 0.1f, 20.0f);
        fields.Float("cloudIdleSpeed", "揺れが無くなり、上り下りの傾きを全部掛ける水平の速さ (m/s)", cloudIdleSpeed, 0.1f,
                     100.0f);
        fields.Float("cloudBankDegrees", "旋回で雲を内側へ傾ける角度 (度。体は傾けない)", cloudBankDegrees, 0.0f, 60.0f);
        fields.Float("cloudBankSeconds", "旋回の傾きのならし (秒)", cloudBankSeconds, 0.0f, 2.0f);
        fields.Float("cloudPitchFollow", "上り下りで雲の前を進む向きへ傾ける割合 (0〜1)", cloudPitchFollow, 0.0f, 1.0f);
        fields.Float("cloudPitchMaxDegrees", "上り下りの傾きの上限 (度)", cloudPitchMaxDegrees, 0.0f, 89.0f);
        fields.Float("cloudPitchSeconds", "上り下りの傾きのならし (秒)", cloudPitchSeconds, 0.0f, 2.0f);
        fields.Note("急降下では、傾けないと相対風が雲を体の上へ吹き上げます。傾けると雲は進む向きに沿い、尾は後ろへ"
                    "流れます。真上・真下だけの上昇・下降では傾けません。");
        fields.Bool("cloudKeepAlive", "乗っていない間もエフェクトを出したままにする", cloudKeepAlive);
        fields.Float("cloudLingerSeconds", "出したままにしない時、雲が消えてから止めるまで (秒)", cloudLingerSeconds,
                     0.0f, 10.0f);
        fields.Note("出したままにすると、乗っていない間は中が空で域が眠り、ほぼ重さが無くなります。止めると、"
                    "召喚のたびにエフェクトを出し直す 1 フレームの詰まりが出ます。");
    }
    if (fields.Section("確かめ")) {
        fields.Int("traceEveryFrames", "位置と速度をログに出す間隔 (F、0 は出さない)", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
