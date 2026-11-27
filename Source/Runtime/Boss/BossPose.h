#pragma once

#include "BossComponent.h"
#include "BossSwordPose.h"

namespace Project_TD2_2 {

// 剣の根元・霞の内側・霞の先（世界座標）。
struct BossSwordPoints {
    HIKARI::GameProjectFloat3 base{};
    HIKARI::GameProjectFloat3 inner{};
    HIKARI::GameProjectFloat3 outer{};
};

// 剣の線：付け根（構えの grip を体の向きで回した所。世界座標）と、剣の向き（長さ 1）。
struct BossSwordLine {
    HIKARI::GameProjectFloat3 root{};
    HIKARI::GameProjectFloat3 direction{};
};

// 「攻撃中」のいまの区間。tIn はその区間の何 F 目（1 から）。
struct BossSegmentProgress {
    int segment = 0;
    int hitIndex = 0;
    int tIn = 0;
    int swingLength = 1;
    float progress = 0.0f;
    bool hit = false;
};

// 剣の構え・振り（試作の windup／swingPose／swordTarget）。見た目と剣の当たりの両方がこれを使う。
class BossPose final {
public:
    static BossSegmentProgress Progress(const BossComponent& boss, const BossMove& move);
    static BossSwordPose Target(const BossComponent& boss);
    // 下ろした構え（待機・後隙の戻り先）。
    static BossSwordPose Rest(const BossComponent& boss);
    static BossSwordLine SwordLine(const BossComponent& boss, const BossSwordPose& pose);
    // 刀の点（世界）。体ごとの回転（spin。回し蹴り・体の回転の技）も含める（見える刀と同じ所。SwordLine は体の向きだけ）。
    static BossSwordPoints SwordPoints(const BossComponent& boss, const BossSwordPose& pose);
    // 見えている体の向き（rad）：プレイヤーの方（yaw）に、体の回転（spin）と狙いのずれのねじり（aimTwist。c-884095）を
    // 足したもの。体の物体・骨格の体・刀の当たり・左手の骨は、どれもこの向きで置く（置く所ごとに式を書かない）。
    static float BodyYaw(const BossComponent& boss);
    // 振りの進み（0〜1。振りの速さの曲線。tIn は振りに入ってからの F）。
    static float SwingShare(const BossComponent& boss, int tIn, int length);
    // 地面から足元までの高さ（跳躍・浮遊から、霧へ沈んだ深さを引く。見た目の上下のゆれは含めない）。
    static float RootHeight(const BossComponent& boss);
};

} // namespace Project_TD2_2
