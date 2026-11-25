#include "BossGourdLink.h"

#include <algorithm>
#include <cstdio>

#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BossFrame.h"
#include "BossWaves.h"
#include "../Gourd/GourdRules.h"
#include "../Gourd/GourdShotComponent.h"
#include "../Gourd/GourdShotRules.h"

namespace Project_TD2_2 {
namespace {

// 煙の量の換算が取れないとき（ヒョウタンがいない）の値。ヒョウタンの既定値と同じ。
constexpr float kFallbackSmokePerVolume = 4.0f;

// プレイヤーの砲撃（最初の 1 つ）を、砲撃の規則の読むだけの関数から見る。
void SeeShot(HIKARI::WorldView& world, BossGearSight& gear) {
    bool seen = false;
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   if (seen)
                                                       return;
                                                   seen = true;
                                                   gear.shotWindup = GourdShotRules::IsWindingUp(shot);
                                                   gear.shotChaining = GourdShotRules::IsChaining(shot);
                                                   gear.shotsFired = GourdShotRules::GetShotsFired(shot);
                                                   gear.shotCost = GourdShotRules::GetSmokeCost(shot);
                                                   gear.aimOrigin = GourdShotRules::GetAimOrigin(shot);
                                                   gear.aimDirection = GourdShotRules::GetAimDirection(shot);
                                               });
}

} // namespace

BossGourdSight BossGourdLink::Sense(HIKARI::WorldView& world) {
    // 吸い込み中のヒョウタン（飛んで来た斬撃・ナイフを吸える）。煙がもう入らなければ（満杯）、吸えずに当たる。
    // 捕まえられるかは、ヒョウタンの空きから、口へ引き込み中の斬撃・ナイフが入りきった時に足す煙を先に引いて決める
    // （1 本で満杯になる時は、続けて来た次の 1 本が当たる。引き込み中の分は満杯になっても最後まで入る）。
    // 満杯でも吸える設定（Gourd.absorbWhenFull）なら、煙は増えないが斬撃は吸い取れる（前の扱い）。
    // ボスがプレイヤーの様子として見るのは、煙が入るかどうかに関わらず吸い込みの構えをしているか。
    BossGourdSight sight;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            if (!sight.hasMouth) {
                sight.mouth = GourdRules::GetMouthPosition(gourd);
                sight.hasMouth = true;
                const float capacity = GourdRules::GetCapacity(gourd);
                sight.gear.hasGourd = true;
                sight.gear.smokePercent = capacity > 0.0f ? GourdRules::GetSmoke(gourd) / capacity * 100.0f : 0.0f;
                sight.gear.capacity = capacity;
                sight.gear.gourdFull = !GourdRules::HasRoomToAbsorb(gourd);
            }
            if (!GourdRules::IsAbsorbing(gourd))
                return;
            sight.inhaling = true;
            sight.catchRoom = std::max(sight.catchRoom, GourdRules::GetCatchRoom(gourd));
        });
    sight.gear.absorbing = sight.inhaling;
    SeeShot(world, sight.gear);
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType),
        [&](HIKARI::ObjectView, BossComponent& boss) { sight.catchRoom -= BossWaves::GetPullingSmoke(boss); });
    return sight;
}

void BossGourdLink::HandOff(HIKARI::WorldView& world, BossCatchHandoff& caught, int pulling) {
    if (caught.smoke <= 0.0f && caught.count == 0 && pulling == 0)
        return;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            // 引き込みの間と、入りきった直後は、吸い込みをやめていても口が飲み込み続ける。
            GourdRules::NotifySwallowing(gourd);
            if (caught.smoke <= 0.0f && caught.count == 0)
                return;
            // 吸った合図は、ヒョウタン自身が次の描画フレームで見せる（ボスはカメラに触れない）。
            GourdRules::NotifyCatch(gourd, caught.count);
            // 火の刃を吸い取ったら火弾になる（捕まえた時に空きがあった分。入りきった時に満杯になっていても）。
            GourdRules::NotifyFireCatch(gourd, caught.fireCount);
            const float added = GourdRules::AddSmoke(gourd, caught.smoke);
            char message[192]{};
            std::snprintf(message, sizeof(message), "AbsorbSmoke amount=%.3f added=%.3f smoke=%.3f caught=%d fire=%d",
                          static_cast<double>(caught.smoke), static_cast<double>(added),
                          static_cast<double>(GourdRules::GetSmoke(gourd)), caught.count, caught.fireCount);
            BossLog::Write(message);
            caught = {};
        });
}

void BossGourdLink::ReserveFluidDomains(HIKARI::WorldView& world, size_t demand) {
    if (demand == 0)
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType),
        [&](HIKARI::ObjectView, GourdShotComponent& shot) { GourdShotRules::ReserveFluidDomains(shot, demand); });
}

float BossGourdLink::SmokePerVolume(HIKARI::WorldView& world) {
    float smokePerVolume = kFallbackSmokePerVolume;
    bool found = false;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView, GourdComponent& gourd) {
                                               if (!found) {
                                                   smokePerVolume = GourdRules::GetSmokePerVolume(gourd);
                                                   found = true;
                                               }
                                           });
    return smokePerVolume;
}

} // namespace Project_TD2_2
