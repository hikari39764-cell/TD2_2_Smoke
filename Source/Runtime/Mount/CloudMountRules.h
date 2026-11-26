#pragma once

#include "CloudMountComponent.h"

namespace Project_TD2_2 {

// 筋斗雲の状態と飛行の運動の規則。世界（物理・入力・見た目）を知らない。呼び出し中に得た部品だけを渡す。
class CloudMountRules final {
public:
    static void Enter(CloudMount& mount, CloudMountState state);
    // Mount が押された時の次の段階：乗っていなければ召喚、召喚中・騎乗中なら解除、解除中なら召喚し直す。
    static CloudMountState NextOnToggle(CloudMountState state);
    // 時間で終わる段階（召喚・解除）の終わりに来ていれば、次の段階を返す。来ていなければ今の段階。
    // 段階の F は、その段階の運動を提出した固定 tick の数（AdvanceFrame で増やす）。
    static CloudMountState TimedNext(const CloudMount& mount);
    static void AdvanceFrame(CloudMount& mount);
    // 召喚を始める。startY は今の体の中心の高さ、groundY は足元の地面の高さ（見つからなければ false を渡す）。
    // 浮き上がった後の高さは、足元が地面から rideHeight の所。今より低くはしない（落下中の召喚はその場で乗る）。
    static void BeginSummon(CloudMount& mount, float startY, bool hasGround, float groundY);
    // 召喚のいまの F に欲しい縦の速さ (m/s)。この F の終わりの高さが、持ち上げの輪郭の次の F の高さになる。
    static float SummonVerticalSpeed(const CloudMount& mount, float dt);
    // 入力とカメラの見ている向き（単位ベクトル）から、欲しい速度（世界 m/s）。解除中は 0。前後左右の最高速は、
    // ブースト中は boostSpeed、それ以外は cruiseSpeed。召喚中は上昇・下降と前後の上り下りを使わない
    // （縦は持ち上げの輪郭が決める）。
    static HIKARI::GameProjectFloat3 WantedVelocity(const CloudMount& mount, HIKARI::GameProjectFloat3 cameraLook);
    // ブーストの 1 固定 tick。騎乗中に Sprint を押し、前後左右の入力があり、尽きておらず、精力が残っている
    // （hasStamina）間はブースト。始めた tick に弾き出しを始める。Sprint を離すと尽きた印を消す。
    // ブーストの始めと終わりの tick に true を返す。
    static bool StepBoost(CloudMount& mount, bool hasStamina);
    // 精力が尽きた：この tick まではブーストし、入力を離すまでブーストしない。
    static void ExhaustBoost(CloudMount& mount);
    // 飛行の速度の 1 固定 tick。入力があれば向きを旋回の速さで回し、速さを寄せる（弾き出し・急降下・速い分の戻り）。
    // 入力が無ければ accelSeconds・brakeSeconds で止める。受け止めのならし、範囲、上限の速さを掛け、flight の値を書く。
    // position は今の体の中心。
    static void StepFlight(CloudMount& mount, HIKARI::GameProjectFloat3 cameraLook, HIKARI::GameProjectFloat3 position,
                           float dt);
    // 範囲：境界の手前 boundsSoftMeters から外向きの速さを弱め、境界の上で 0、外にいれば内へ戻す。
    // 境界に沿う向きの速さは残す。
    static void ApplyBounds(CloudMount& mount, HIKARI::GameProjectFloat3 position);
    // いまの速さで、速度の向きを回せる速さ（度/秒）。巡航の速さ以下で turnRateCruise、ブーストの速さ以上で
    // turnRateBoost、間は速さで補間する。
    static float TurnRate(const CloudMount& mount, float speed);
    // 毎固定 tick の体の中心。前の tick から実際に動いた速さを測る（当たって止められた分の速さを持ち越さないため）。
    static void RecordPosition(CloudMount& mount, HIKARI::GameProjectFloat3 position, float dt);
    // 乗っていない間の落下の追跡。地面にいれば足場の高さを測り直し、空中では一番高い所を覚える。
    static void TrackFall(CloudMount& mount, bool grounded, float y);
    // 足場を離れて catchFallMeters 以上落ちたので受け止める（乗っていない・autoCatch・受け止める落下の時）。
    static bool ShouldCatch(const CloudMount& mount, float y);
    // 落下を受け止める召喚を始める。その場の高さで乗り、下向きの速さ fallSpeed (正) を catchBrakeSeconds でならして
    // 止める。drift は横の勢い（残す）。
    static void BeginCatch(CloudMount& mount, float y, float fallSpeed, HIKARI::GameProjectFloat3 drift);
    // 降りた（deliberate：自分で解除した、false：被弾などで解けた）。飛行の値を消し、落下の追跡を y から始める。
    // 自分で降りた後の落下は catchAfterDismiss の時だけ受け止める。
    static void OnDismounted(CloudMount& mount, float y, bool deliberate);
    // 地面すれすれの持ち上げ（e-dca15c を避ける）。騎乗中（Riding）に grounded で、水平の速さが groundSkimMinSpeed
    // 以上なら、提出する縦の速さ vertical の下向きを捨てて groundSkimSpeed を足した値を返す。その後
    // groundSkimHoldSeconds の間は、提出する縦の速さと速度の下向きを捨てる（次の tick にまた地面へ押し付けない）。
    // 持ち上げた tick を flight.groundSkim に書く。どちらでもなければ vertical のまま。
    static float ApplyGroundSkim(CloudMount& mount, float vertical, bool grounded, float dt);
    // この tick に提出する縦の速さ。召喚中は持ち上げの輪郭（受け止めの召喚は除く）、それ以外は飛行の速度の縦。
    static float VerticalSpeed(const CloudMount& mount, float dt);
    // 運動学ソルバーは、提出した速度の縦を使わず「重力を足す前の縦の速さ＋重力×dt」で縦を決める。
    // 欲しい縦の速さ wanted にするための、この tick の縦の重力の差し替え (m/s²)。
    static float GravityForVerticalSpeed(float wanted, float baseVertical, float dt);
    // 向き facing（水平）を、速度の水平の向きへ毎秒 degreesPerSecond まで回す。遅い間はそのまま。
    static HIKARI::GameProjectFloat3 TurnFacing(HIKARI::GameProjectFloat3 facing, HIKARI::GameProjectFloat3 velocity,
                                                float degreesPerSecond, float dt);
    static bool IsMounted(const CloudMount& mount);
    static const char* StateName(CloudMountState state);
};

} // namespace Project_TD2_2
