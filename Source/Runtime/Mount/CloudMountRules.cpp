#include "CloudMountRules.h"

#include <algorithm>
#include <cmath>
#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;

using VectorMath::kTau;
using VectorMath::kRadiansPerDegree;
// 飛ぶ向きの上り下りの限度（度）。真上・真下では左右の向きが決まらないので少し手前で止める。
constexpr float kMaximumFlightPitch = 89.0f;
// これより遅い間は体の向きを変えない (m/s)。止まり際に向きが震えないようにする。速度の向きも、これより遅い間は
// 回さずに入力の向きへそろえる（止まった所からの出だし）。
constexpr float kTurnMinimumSpeed = 0.5f;
// 前後左右の入力として扱う最小の倒し量（ブースト・急降下の条件）。PlayerSystem の向きの入力と同じ値。
constexpr float kDirectionalInputDeadZone = 0.1f;
// 当たって止められた分の速さを持ち越さない判定の余裕 (m/s)。巡航・ブーストより速い分は「実際に動いた速さ＋急降下の
// 1 tick の伸び＋この値」までにする（地面へ急降下で押し付けたまま速さを溜め、引き起こして飛び出すことをさせない）。
constexpr float kAchievedSpeedMargin = 1.0f;
// 向きが真逆かを決める長さの下限。
constexpr float kParallelEpsilon = 1.0e-4f;

using VectorMath::Add;
using VectorMath::Cross;
using VectorMath::Dot;
using VectorMath::Length;
using VectorMath::Scale;

bool HasDirectionalInput(const CloudMountInput& input) {
    return std::hypot(input.strafe, input.forward) > kDirectionalInputDeadZone;
}

// 単位ベクトル from を、単位ベクトル to へ最大 maxAngle（ラジアン）回す。真逆なら水平に回す（横へ大回りする）。
Vec3 RotateToward(Vec3 from, Vec3 to, float maxAngle) {
    const float angle = std::acos(std::clamp(Dot(from, to), -1.0f, 1.0f));
    if (angle <= maxAngle)
        return to;
    Vec3 axis = Cross(from, to);
    float axisLength = Length(axis);
    if (axisLength < kParallelEpsilon) {
        // 真逆：水平の横（from と上の外積）へ向けて回す。真上・真下の時は x の向きへ。(from × side) × from = side。
        Vec3 side = Cross(from, Vec3{0.0f, 1.0f, 0.0f});
        if (Length(side) < kParallelEpsilon)
            side = {1.0f, 0.0f, 0.0f};
        axis = Cross(from, side);
        axisLength = Length(axis);
    }
    axis = Scale(axis, 1.0f / axisLength);
    // ロドリゲスの回転（軸は from と直交するので、軸方向の成分の項は 0）。
    const Vec3 rotated = Add(Scale(from, std::cos(maxAngle)), Scale(Cross(axis, from), std::sin(maxAngle)));
    const float length = Length(rotated);
    return length > 0.0f ? Scale(rotated, 1.0f / length) : to;
}

// 急降下の度合い（0〜1）：下向きの角度が diveStartDegrees で 0、diveFullDegrees で 1。
float DiveAmount(const CloudMountComponent& mount, Vec3 direction) {
    const float down = std::asin(std::clamp(-direction.y, -1.0f, 1.0f)) / kRadiansPerDegree;
    const float start = std::clamp(mount.diveStartDegrees, 0.0f, kMaximumFlightPitch);
    return std::clamp((down - start) / std::max(mount.diveFullDegrees - start, 1.0f), 0.0f, 1.0f);
}

// 前後左右の最高速（ブースト中は boostSpeed）。
float BaseSpeed(const CloudMountComponent& mount) {
    return mount.flight.boosting ? mount.boostSpeed : mount.cruiseSpeed;
}

// 前後左右の入力が無い（または解除中）：② の止まり方のまま、速度を 3 次元のまま wanted へ寄せる。
// 寄せる速さは「最高速に届くまでの時間」（accelSeconds、遅くする時は brakeSeconds）で決める。
void BrakeVelocity(CloudMountComponent& mount, Vec3 wanted, float dt) {
    mount.boostKickLeft = 0.0f;
    mount.overspeedDecayRate = 0.0f;
    mount.flight.turn = 0.0f;
    mount.flight.dive = 0.0f;
    const Vec3 gap{wanted.x - mount.velocity.x, wanted.y - mount.velocity.y, wanted.z - mount.velocity.z};
    const float distance = Length(gap);
    const float currentSpeed = Length(mount.velocity);
    const float wantedSpeed = Length(wanted);
    const float seconds = wantedSpeed < currentSpeed ? mount.brakeSeconds : mount.accelSeconds;
    // 最高速は巡航・上下・いまの速さ・欲しい速さの大きい方（斜めに上る時や、速い所から止まる時も、決めた時間で届く）。
    const float reference = std::max({mount.cruiseSpeed, mount.verticalSpeed, currentSpeed, wantedSpeed});
    const float step = seconds > 0.0f ? reference / seconds * dt : distance;
    if (distance <= step || distance <= 0.0f) {
        mount.velocity = wanted;
        return;
    }
    mount.velocity = Add(mount.velocity, Scale(gap, step / distance));
}

// 速さの 1 tick：弾き出し → 欲しい速さへ寄せる（速い分は overspeedDecaySeconds で戻す）→ 急降下の伸び →
// 当たって止められた分は持ち越さない。
float StepSpeed(CloudMountComponent& mount, float speed, float wantedSpeed, float dive, float dt) {
    bool kicked = false;
    if (mount.boostKickLeft > 0.0f) {
        // 残りの時間で boostKickSpeed に届くよう、一定の速さで上げる（その間は accelSeconds の加速を重ねない）。
        kicked = speed < mount.boostKickSpeed;
        if (kicked)
            speed += (mount.boostKickSpeed - speed) * std::min(1.0f, dt / mount.boostKickLeft);
        mount.boostKickLeft = std::max(0.0f, mount.boostKickLeft - dt);
    }
    if (!kicked && speed < wantedSpeed) {
        const float reference = std::max({mount.cruiseSpeed, mount.verticalSpeed, wantedSpeed});
        speed = mount.accelSeconds > 0.0f ? std::min(wantedSpeed, speed + reference / mount.accelSeconds * dt)
                                          : wantedSpeed;
    } else if (!kicked && speed > wantedSpeed && dive <= 0.0f) {
        // 速い分を overspeedDecaySeconds で戻す一定の減り。欲しい速さが下がって（ブーストや上昇を離した）速い分が
        // 今の減りで戻せる量を超えたら、その時の速い分から決め直す。
        const float excess = speed - wantedSpeed;
        if (mount.overspeedDecaySeconds > 0.0f) {
            if (excess > mount.overspeedDecayRate * mount.overspeedDecaySeconds)
                mount.overspeedDecayRate = excess / mount.overspeedDecaySeconds;
            speed = std::max(wantedSpeed, speed - mount.overspeedDecayRate * dt);
        } else {
            speed = wantedSpeed;
        }
    }
    if (dive > 0.0f) {
        mount.overspeedDecayRate = 0.0f;
        if (speed < mount.diveMaxSpeed)
            speed = std::min(mount.diveMaxSpeed, speed + mount.diveAccel * dive * dt);
    }
    if (speed <= wantedSpeed)
        mount.overspeedDecayRate = 0.0f;
    if (speed > wantedSpeed && mount.hasLastPosition) {
        const float reachable = mount.achievedSpeed + std::max(mount.diveAccel, 0.0f) * dt + kAchievedSpeedMargin;
        speed = std::min(speed, std::max(wantedSpeed, reachable));
    }
    return speed;
}

// 前後左右の入力がある：速度の向きを入力の向きへ旋回の速さで回し（速いほど大回り）、速さを寄せる。
void SteerVelocity(CloudMountComponent& mount, Vec3 wanted, float dt) {
    const float wantedSpeed = Length(wanted);
    const Vec3 wantedDirection = Scale(wanted, 1.0f / wantedSpeed);
    const float speed = Length(mount.velocity);
    Vec3 direction = wantedDirection;
    float turn = 0.0f;
    if (speed >= kTurnMinimumSpeed) {
        const Vec3 current = Scale(mount.velocity, 1.0f / speed);
        const float maxAngle = CloudMountRules::TurnRate(mount, speed) * kRadiansPerDegree * dt;
        direction = RotateToward(current, wantedDirection, maxAngle);
        // 旋回の強さは水平の向きの変わり方で測る。右は yaw が減る向き（WantedVelocity の右の取り方）。
        if (maxAngle > 0.0f && std::hypot(current.x, current.z) > kParallelEpsilon &&
            std::hypot(direction.x, direction.z) > kParallelEpsilon) {
            const float turned =
                std::remainder(std::atan2(direction.x, direction.z) - std::atan2(current.x, current.z), kTau);
            turn = std::clamp(-turned / maxAngle, -1.0f, 1.0f);
        }
    }
    const float dive = mount.state == CloudMountState::Riding ? DiveAmount(mount, direction) : 0.0f;
    mount.velocity = Scale(direction, StepSpeed(mount, speed, wantedSpeed, dive, dt));
    mount.flight.turn = turn;
    mount.flight.dive = dive;
}

// 召喚の持ち上げの輪郭：frame F たった時の高さ。出だしが速く、終わりで速さが 0 になる。
float SummonHeightAt(const CloudMountComponent& mount, int frame) {
    const int frames = std::max(1, mount.summonFrames);
    const float progress = std::clamp(static_cast<float>(frame) / static_cast<float>(frames), 0.0f, 1.0f);
    const float share = 1.0f - std::pow(1.0f - progress, std::max(mount.summonEasePower, 0.01f));
    return mount.summonStartY + (mount.summonTargetY - mount.summonStartY) * share;
}

} // namespace

void CloudMountRules::Enter(CloudMountComponent& mount, CloudMountState state) {
    mount.state = state;
    mount.stateFrame = 0;
    mount.catching = false;
}

CloudMountState CloudMountRules::NextOnToggle(CloudMountState state) {
    switch (state) {
    case CloudMountState::None: return CloudMountState::Summoning;
    case CloudMountState::Dismissing: return CloudMountState::Summoning;
    case CloudMountState::Summoning:
    case CloudMountState::Riding: return CloudMountState::Dismissing;
    }
    return CloudMountState::None;
}

CloudMountState CloudMountRules::TimedNext(const CloudMountComponent& mount) {
    if (mount.state == CloudMountState::Summoning && mount.stateFrame >= std::max(1, mount.summonFrames))
        return CloudMountState::Riding;
    if (mount.state == CloudMountState::Dismissing && mount.stateFrame >= std::max(0, mount.dismissFrames))
        return CloudMountState::None;
    return mount.state;
}

void CloudMountRules::AdvanceFrame(CloudMountComponent& mount) {
    if (mount.state != CloudMountState::None)
        ++mount.stateFrame;
}

void CloudMountRules::BeginSummon(CloudMountComponent& mount, float startY, bool hasGround, float groundY) {
    mount.summonStartY = startY;
    mount.summonTargetY =
        hasGround ? std::max(startY, groundY + mount.standOffset + std::max(0.0f, mount.rideHeight)) : startY;
    // 落下の勢いは止め、横の勢いは残して（歩いていた速さから）飛び始める。
    mount.velocity.y = 0.0f;
    Enter(mount, CloudMountState::Summoning);
}

float CloudMountRules::SummonVerticalSpeed(const CloudMountComponent& mount, float dt) {
    if (dt <= 0.0f)
        return 0.0f;
    return (SummonHeightAt(mount, mount.stateFrame + 1) - SummonHeightAt(mount, mount.stateFrame)) / dt;
}

Vec3 CloudMountRules::WantedVelocity(const CloudMountComponent& mount, Vec3 cameraLook) {
    if (mount.state == CloudMountState::Dismissing || mount.state == CloudMountState::None)
        return {};
    const float lookLength = Length(cameraLook);
    const Vec3 look = lookLength > 0.0f
                          ? Vec3{cameraLook.x / lookLength, cameraLook.y / lookLength, cameraLook.z / lookLength}
                          : Vec3{0.0f, 0.0f, 1.0f};
    const float yaw = std::atan2(look.x, look.z);
    // 見下ろしが正。levelPitch だけ見下ろしたカメラで水平に飛ぶ。
    const float limit = kMaximumFlightPitch * kRadiansPerDegree;
    float pitch = std::asin(std::clamp(-look.y, -1.0f, 1.0f)) - mount.levelPitch * kRadiansPerDegree;
    pitch = std::clamp(pitch, -limit, limit);
    const bool summoning = mount.state == CloudMountState::Summoning;
    if (summoning)
        pitch = 0.0f;
    const Vec3 forward{std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
    // 右は PlayerSystem の歩きと同じ取り方（水平の前を回した向き）。
    const Vec3 right{-std::cos(yaw), 0.0f, std::sin(yaw)};
    Vec3 direction{right.x * mount.input.strafe + forward.x * mount.input.forward, forward.y * mount.input.forward,
                   right.z * mount.input.strafe + forward.z * mount.input.forward};
    const float magnitude = Length(direction);
    if (magnitude > 1.0f)
        direction = {direction.x / magnitude, direction.y / magnitude, direction.z / magnitude};
    const float lift =
        summoning ? 0.0f
                  : static_cast<float>(mount.input.ascend ? 1 : 0) - static_cast<float>(mount.input.descend ? 1 : 0);
    const float speed = BaseSpeed(mount);
    return {direction.x * speed, direction.y * speed + lift * mount.verticalSpeed, direction.z * speed};
}

bool CloudMountRules::StepBoost(CloudMountComponent& mount, bool hasStamina) {
    if (!mount.input.boost)
        mount.boostExhausted = false;
    const bool wasBoosting = mount.flight.boosting;
    mount.flight.boosting = mount.state == CloudMountState::Riding && mount.input.boost && !mount.boostExhausted &&
                            hasStamina && HasDirectionalInput(mount.input);
    if (mount.flight.boosting && !wasBoosting)
        mount.boostKickLeft = std::max(0.0f, mount.boostKickSeconds);
    if (!mount.flight.boosting)
        mount.boostKickLeft = 0.0f;
    return mount.flight.boosting != wasBoosting;
}

void CloudMountRules::ExhaustBoost(CloudMountComponent& mount) { mount.boostExhausted = true; }

void CloudMountRules::StepFlight(CloudMountComponent& mount, Vec3 cameraLook, Vec3 position, float dt) {
    const bool catching = mount.catchBrakeLeft > 0.0f;
    // 受け止めのならしの間、縦は下のならしで決める。横の速さは、縦を除いて寄せる。
    if (catching)
        mount.velocity.y = 0.0f;
    const Vec3 wanted = WantedVelocity(mount, cameraLook);
    // 前後左右の入力が無い（止まる・上昇と下降だけ）時は ② のまま 3 次元で寄せる。旋回と急降下は前後左右の入力の間だけ。
    if (!HasDirectionalInput(mount.input) || Length(wanted) <= 0.0f || dt <= 0.0f)
        BrakeVelocity(mount, wanted, dt);
    else
        SteerVelocity(mount, wanted, dt);
    if (catching) {
        // 受け止めた時の下向きの速さから、catchBrakeSeconds で一定の割合で 0 にする（最初の tick は落ちていた速さ）。
        const float seconds = std::max(mount.catchBrakeSeconds, dt);
        mount.velocity.y = -mount.catchFallSpeed * std::min(mount.catchBrakeLeft / seconds, 1.0f);
        mount.catchBrakeLeft = std::max(0.0f, mount.catchBrakeLeft - dt);
    }
    ApplyBounds(mount, position);
    const float speed = Length(mount.velocity);
    const float limit = std::max(mount.maxFlightSpeed, 0.0f);
    if (speed > limit && speed > 0.0f)
        mount.velocity = Scale(mount.velocity, limit / speed);
    const float previousSpeed = mount.flight.speed;
    mount.flight.speed = std::min(speed, limit);
    mount.flight.acceleration = dt > 0.0f ? (mount.flight.speed - previousSpeed) / dt : 0.0f;
    mount.flight.overspeed = std::max(0.0f, mount.flight.speed - BaseSpeed(mount));
    mount.flight.boostKick = mount.boostKickSeconds > 0.0f ? mount.boostKickLeft / mount.boostKickSeconds : 0.0f;
}

void CloudMountRules::ApplyBounds(CloudMountComponent& mount, Vec3 position) {
    const float soft = std::max(mount.boundsSoftMeters, 1.0f);
    const float edgeSpeed = std::max(mount.maxFlightSpeed, 0.0f);
    // 境界までの距離 distance で許す外向きの速さ。弱め始める所で上限の速さ、境界の上で 0、外では内向き（上限の速さ
    // まで）。1 tick に進む外向きの距離は「距離 × 上限の速さ × dt ÷ soft」で距離より短いので、境界を越えない。
    const auto allowed = [&](float distance) { return std::max(-edgeSpeed, edgeSpeed * distance / soft); };
    const float radius = std::hypot(position.x, position.z);
    if (radius > 0.0f) {
        const Vec3 outward{position.x / radius, 0.0f, position.z / radius};
        const float out = mount.velocity.x * outward.x + mount.velocity.z * outward.z;
        const float cap = allowed(mount.boundsRadius - radius);
        if (out > cap) {
            mount.velocity.x -= outward.x * (out - cap);
            mount.velocity.z -= outward.z * (out - cap);
        }
    }
    mount.velocity.y = std::min(mount.velocity.y, allowed(mount.boundsMaxY - position.y));
    mount.velocity.y = std::max(mount.velocity.y, -allowed(position.y - mount.boundsMinY));
}

float CloudMountRules::TurnRate(const CloudMountComponent& mount, float speed) {
    const float span = mount.boostSpeed - mount.cruiseSpeed;
    const float share = span > 0.0f ? std::clamp((speed - mount.cruiseSpeed) / span, 0.0f, 1.0f)
                                    : (speed > mount.cruiseSpeed ? 1.0f : 0.0f);
    return std::max(0.0f, mount.turnRateCruise + (mount.turnRateBoost - mount.turnRateCruise) * share);
}

void CloudMountRules::RecordPosition(CloudMountComponent& mount, Vec3 position, float dt) {
    if (mount.hasLastPosition && dt > 0.0f) {
        const Vec3 moved{position.x - mount.lastPosition.x, position.y - mount.lastPosition.y,
                         position.z - mount.lastPosition.z};
        mount.achievedSpeed = Length(moved) / dt;
    }
    mount.lastPosition = position;
    mount.hasLastPosition = true;
}

void CloudMountRules::TrackFall(CloudMountComponent& mount, bool grounded, float y) {
    if (grounded) {
        mount.fallPeakY = y;
        mount.fallPeakKnown = true;
        mount.catchArmed = true;
        mount.fallingFromMount = false;
        return;
    }
    mount.fallPeakY = mount.fallPeakKnown ? std::max(mount.fallPeakY, y) : y;
    mount.fallPeakKnown = true;
}

bool CloudMountRules::ShouldCatch(const CloudMountComponent& mount, float y) {
    return mount.autoCatch && mount.state == CloudMountState::None && mount.catchArmed && mount.fallPeakKnown &&
           mount.fallPeakY - y >= std::max(0.0f, mount.catchFallMeters);
}

void CloudMountRules::BeginCatch(CloudMountComponent& mount, float y, float fallSpeed, Vec3 drift) {
    mount.summonStartY = y;
    mount.summonTargetY = y;
    mount.velocity = {drift.x, 0.0f, drift.z};
    Enter(mount, CloudMountState::Summoning);
    mount.catching = true;
    mount.catchFallSpeed = std::clamp(fallSpeed, 0.0f, std::max(mount.maxFlightSpeed, 0.0f));
    // ならしが 0 秒なら、その場で止める（手で呼んだ時と同じ）。
    mount.catchBrakeLeft = mount.catchFallSpeed > 0.0f ? std::max(0.0f, mount.catchBrakeSeconds) : 0.0f;
    if (mount.catchBrakeLeft <= 0.0f)
        mount.catchFallSpeed = 0.0f;
}

void CloudMountRules::OnDismounted(CloudMountComponent& mount, float y, bool deliberate) {
    mount.velocity = {};
    mount.flight = {};
    mount.boostKickLeft = 0.0f;
    mount.overspeedDecayRate = 0.0f;
    mount.catchBrakeLeft = 0.0f;
    mount.catchFallSpeed = 0.0f;
    mount.catching = false;
    mount.groundSkimHoldLeft = 0.0f;
    mount.fallPeakY = y;
    mount.fallPeakKnown = true;
    mount.catchArmed = !deliberate || mount.catchAfterDismiss;
    mount.fallingFromMount = true;
}

float CloudMountRules::VerticalSpeed(const CloudMountComponent& mount, float dt) {
    return mount.state == CloudMountState::Summoning && !mount.catching ? SummonVerticalSpeed(mount, dt)
                                                                        : mount.velocity.y;
}

float CloudMountRules::GravityForVerticalSpeed(float wanted, float baseVertical, float dt) {
    return dt > 0.0f ? (wanted - baseVertical) / dt : 0.0f;
}

Vec3 CloudMountRules::TurnFacing(Vec3 facing, Vec3 velocity, float degreesPerSecond, float dt) {
    if (std::hypot(velocity.x, velocity.z) < kTurnMinimumSpeed)
        return facing;
    const float current = std::atan2(facing.x, facing.z);
    const float target = std::atan2(velocity.x, velocity.z);
    const float limit = std::max(0.0f, degreesPerSecond) * kRadiansPerDegree * dt;
    const float yaw = current + std::clamp(std::remainder(target - current, kTau), -limit, limit);
    return {std::sin(yaw), 0.0f, std::cos(yaw)};
}

float CloudMountRules::ApplyGroundSkim(CloudMountComponent& mount, float vertical, bool grounded, float dt) {
    const bool riding = mount.state == CloudMountState::Riding && mount.groundSkimSpeed > 0.0f;
    mount.flight.groundSkim =
        riding && grounded &&
        std::hypot(mount.velocity.x, mount.velocity.z) >= std::max(0.0f, mount.groundSkimMinSpeed);
    if (mount.flight.groundSkim)
        mount.groundSkimHoldLeft = std::max(0.0f, mount.groundSkimHoldSeconds);
    else if (!riding)
        mount.groundSkimHoldLeft = 0.0f;
    const bool holding = mount.groundSkimHoldLeft > 0.0f;
    mount.groundSkimHoldLeft = std::max(0.0f, mount.groundSkimHoldLeft - dt);
    if (!mount.flight.groundSkim && !holding)
        return vertical;
    mount.velocity.y = std::max(mount.velocity.y, 0.0f);
    return std::max(vertical, 0.0f) + (mount.flight.groundSkim ? mount.groundSkimSpeed : 0.0f);
}

bool CloudMountRules::IsMounted(const CloudMountComponent& mount) { return mount.state != CloudMountState::None; }

const char* CloudMountRules::StateName(CloudMountState state) {
    switch (state) {
    case CloudMountState::None: return "None";
    case CloudMountState::Summoning: return "Summoning";
    case CloudMountState::Riding: return "Riding";
    case CloudMountState::Dismissing: return "Dismissing";
    }
    return "Unknown";
}

} // namespace Project_TD2_2
