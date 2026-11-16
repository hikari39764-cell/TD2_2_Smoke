#pragma once

namespace Project_TD2_2 {

// 剣の向き（度）。x は前後の傾き（正で下）、y は左右（正でボスの左）。試作の POSE と同じ約束。
// 既定値は規則の「下ろした構え」（BossPose の kRest）と同じ。
struct BossSwordPose {
    float x = 70.0f;
    float y = -10.0f;
};

} // namespace Project_TD2_2
