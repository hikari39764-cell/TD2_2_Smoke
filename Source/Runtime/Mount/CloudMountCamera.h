#pragma once

#include "../Player/PlayerComponent.h"
#include "CloudMountComponent.h"

namespace Project_TD2_2 {

// 筋斗雲に乗っている間のカメラの演出の規則（速さで引く・画角が広がる・旋回で傾く・加速で遅れる・急降下で見下ろす・
// ぼかしの強さ）。世界を知らない。姿勢は作らず（作るのは PlayerCamera だけ）、PlayerCamera への要求の値と、
// 動きのぼかしの強さを決める。値は CloudMount の項目（節：カメラ）。
class CloudMountCamera final {
public:
    // 騎乗中の描画フレームごとに呼ぶ。dt は実時間（秒）。加速の遅れとブーストの出だしの画角を進め、要求を返す。
    static PlayerCameraRequest BuildRequest(CloudMountComponent& mount, float dt);
    // 乗っていない間に呼ぶ。次に乗った時に、前の騎乗の加速の遅れ・出だしの画角を持ち越さない。
    static void Reset(CloudMountComponent& mount);
    // 降りた後、地面に着くまでの落下のカメラ：上下は騎乗中と同じく素早く追い（いつもの 0.35 秒では落ちる体に置いて
    // 行かれて、体が画面の下へ外れる）、見上げ・見下ろしの限度も騎乗中のまま。距離・画角・傾きは Player の値へ、
    // 要求が終わった時と同じならしで戻す。
    static PlayerCameraRequest BuildFallRequest(const CloudMountComponent& mount);
    // 動きのぼかしの強さ（0〜1）を、ブースト中と急降下の度合いへ cameraBlurSeconds でならして返す。乗っていない間も
    // 呼ぶ（降りた後は消えていく）。
    static float StepBlur(CloudMountComponent& mount, float dt);
    // 速さの段階：0＝止まっている、1＝巡航の速さ、2＝ブースト中かブーストの速さ以上。間は速さで補間する。
    static float SpeedStage(const CloudMountComponent& mount);
    // ブーストを始めてから age 秒の、ならさずに足す画角（度）。初めの半分で上がり、残りで戻る。
    static float FovKick(const CloudMountComponent& mount, float age);
};

} // namespace Project_TD2_2
