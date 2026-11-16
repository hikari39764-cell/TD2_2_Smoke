#pragma once

#include "CloudMountComponent.h"

namespace Project_TD2_2 {

// 筋斗雲の状態と飛行の運動の規則。世界（物理・入力・見た目）を知らない。呼び出し中に得た部品だけを渡す。
class CloudMountRules final {
public:
    static void Enter(CloudMountComponent& mount, CloudMountState state);
    // Mount が押された時の次の段階：乗っていなければ召喚、召喚中・騎乗中なら解除、解除中なら召喚し直す。
    static CloudMountState NextOnToggle(CloudMountState state);
    // 時間で終わる段階（召喚・解除）の終わりに来ていれば、次の段階を返す。来ていなければ今の段階。
    // 段階の F は、その段階の運動を提出した固定 tick の数（AdvanceFrame で増やす）。
    static CloudMountState TimedNext(const CloudMountComponent& mount);
    static void AdvanceFrame(CloudMountComponent& mount);
    // 召喚を始める。startY は今の体の中心の高さ、groundY は足元の地面の高さ（見つからなければ false を渡す）。
    // 浮き上がった後の高さは、足元が地面から rideHeight の所。今より低くはしない（落下中の召喚はその場で乗る）。
    static void BeginSummon(CloudMountComponent& mount, float startY, bool hasGround, float groundY);
    // 召喚のいまの F に欲しい縦の速さ (m/s)。この F の終わりの高さが、持ち上げの輪郭の次の F の高さになる。
    static float SummonVerticalSpeed(const CloudMountComponent& mount, float dt);
    // 入力とカメラの見ている向き（単位ベクトル）から、欲しい速度（世界 m/s）。解除中は 0。
    // 召喚中は上昇・下降と前後の上り下りを使わない（縦は持ち上げの輪郭が決める）。
    static HIKARI::GameProjectFloat3 WantedVelocity(const CloudMountComponent& mount,
                                                    HIKARI::GameProjectFloat3 cameraLook);
    // 速度を wanted へ寄せる。寄せる速さは「最高速に届くまでの時間」（accelSeconds、遅くする時は brakeSeconds）で決める。
    static void StepVelocity(CloudMountComponent& mount, HIKARI::GameProjectFloat3 wanted, float dt);
    // この tick に提出する縦の速さ。召喚中は持ち上げの輪郭、それ以外は飛行の速度の縦。
    static float VerticalSpeed(const CloudMountComponent& mount, float dt);
    // 運動学ソルバーは、提出した速度の縦を使わず「重力を足す前の縦の速さ＋重力×dt」で縦を決める。
    // 欲しい縦の速さ wanted にするための、この tick の縦の重力の差し替え (m/s²)。
    static float GravityForVerticalSpeed(float wanted, float baseVertical, float dt);
    // 向き facing（水平）を、速度の水平の向きへ毎秒 degreesPerSecond まで回す。遅い間はそのまま。
    static HIKARI::GameProjectFloat3 TurnFacing(HIKARI::GameProjectFloat3 facing, HIKARI::GameProjectFloat3 velocity,
                                                float degreesPerSecond, float dt);
    static bool IsMounted(const CloudMountComponent& mount);
    static const char* StateName(CloudMountState state);
};

} // namespace Project_TD2_2
