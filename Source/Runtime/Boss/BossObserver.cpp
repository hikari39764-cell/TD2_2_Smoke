#include "BossObserver.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossFrame.h"
#include "BossMath.h"
#include "../Burn/BurnFieldRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

const char* DirectionName(BossRollDirection direction) {
    return direction == BossRollDirection::Away     ? "away"
           : direction == BossRollDirection::Toward ? "toward"
                                                    : "sideways";
}

// 転がる向きを、プレイヤーからボスへの向きと比べて分ける。
BossRollDirection RollDirectionOf(const BossComponent& boss, const PlayerComponent& player, Vec3 playerCenter) {
    const Vec3 toBoss = FlatNormalize(Sub(boss.ground, playerCenter));
    const float along = Dot(FlatNormalize(player.actionDirection), toBoss);
    const float axis = std::cos(std::clamp(boss.rollAxisDegrees, 0.0f, 90.0f) * kRadiansPerDegree);
    if (along >= axis)
        return BossRollDirection::Toward;
    if (along <= -axis)
        return BossRollDirection::Away;
    return BossRollDirection::Sideways;
}

} // namespace

void BossObserver::Observe(HIKARI::WorldView& world, BossComponent& boss, const PlayerComponent* player,
                           HIKARI::GameProjectFloat3 playerCenter, bool absorbing) {
    BossPlayerWatch& watch = boss.watch;
    const float now = boss.clockSeconds;
    watch.burningPercent = BurnFieldRules::GetBurningFraction(world) * 100.0f;
    watch.present = player != nullptr && player->state != PlayerState::Dead;
    if (!watch.present) {
        watch.healing = watch.absorbing = watch.inFire = watch.wasRolling = false;
        return;
    }
    watch.healing = player->state == PlayerState::Heal;
    if (absorbing && !watch.absorbing)
        watch.absorbSince = now;
    watch.absorbing = absorbing;
    const bool inFire = BurnFieldRules::IsBurning(world, playerCenter);
    if (inFire && !watch.inFire)
        watch.fireSince = now;
    watch.inFire = inFire;
    // ローリングの始まり：転がっていなかった所から転がった、または続けてもう一度転がった（経過 F が戻った）。
    const bool rolling = player->state == PlayerState::Roll;
    if (rolling && (!watch.wasRolling || player->stateFrame < watch.rollFrame)) {
        watch.rollSeconds = now;
        watch.rollDirection = RollDirectionOf(boss, *player, playerCenter);
        char message[160]{};
        std::snprintf(message, sizeof(message), "SeeRoll tick=%llu direction=%s distance=%.3f",
                      static_cast<unsigned long long>(boss.tick), DirectionName(watch.rollDirection),
                      static_cast<double>(FlatLength(Sub(boss.ground, playerCenter))));
        BossLog::Write(message);
    }
    watch.wasRolling = rolling;
    watch.rollFrame = player->stateFrame;
}

} // namespace Project_TD2_2
