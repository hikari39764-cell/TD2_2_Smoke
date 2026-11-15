#pragma once

#include "BossComponent.h"

namespace Project_TD2_2 {

// 剣の向き（度）。x は前後の傾き（正で下）、y は左右（正でボスの左）。試作の POSE と同じ約束。
struct BossSwordPose {
    float x = 70.0f;
    float y = -10.0f;
};

// 剣の根元・霞の内側・霞の先（世界座標）。
struct BossSwordPoints {
    HIKARI::GameProjectFloat3 base{};
    HIKARI::GameProjectFloat3 inner{};
    HIKARI::GameProjectFloat3 outer{};
};

// 剣の線：付け根（体の向きで回した Boss.swordShoulder。世界座標）と、剣の向き（長さ 1）。
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
    static BossSwordLine SwordLine(const BossComponent& boss, BossSwordPose pose);
    static BossSwordPoints SwordPoints(const BossComponent& boss, BossSwordPose pose);
    // 地面から足元までの高さ（跳躍・浮遊から、霧へ沈んだ深さを引く。見た目の上下のゆれは含めない）。
    static float RootHeight(const BossComponent& boss);
};

} // namespace Project_TD2_2
