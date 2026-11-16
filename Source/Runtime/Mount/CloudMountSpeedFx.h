#pragma once

#include "HIKARI/Project/ValueTypes.h"
#include "CloudMountComponent.h"

namespace Project_TD2_2 {

// 画面に写すためのカメラ（直前に描いたフレームの姿勢と画角）。right・up は画面の右・上（傾きを含む）。
struct CloudMountSpeedFxView {
    HIKARI::GameProjectFloat3 eye{};
    HIKARI::GameProjectFloat3 forward{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 right{1.0f, 0.0f, 0.0f};
    HIKARI::GameProjectFloat3 up{0.0f, 1.0f, 0.0f};
    // 縦の画角の半分の tan と、横 ÷ 縦。
    float tanHalfFov = 0.57735f;
    float aspect = 16.0f / 9.0f;
};

// 速度感の全画面の効果（FlightSpeed：縁の放射状の流れ・ぼかし・わずかな色ずれ・縁の減光）へ送る値の規則。世界を知らない。
// 強さは速さ・ブースト・急降下から決め、上がり・下がりを別々にならし、上限を掛ける。流れの中心は進む向きを画面へ写した点、
// 体を守る楕円は体の中心を画面へ写した点。見た目（ぼかしの長さ・筋の数など）は効果資産 FlightSpeed.hposteffect の値。
// 値は CloudMount の項目（節：速度感）。
class CloudMountSpeedFx final {
public:
    // ならす前の狙いの強さ（0〜1 を超えることもある。上限の前）。乗っていなければ 0。
    static float TargetStrength(const CloudMountComponent& mount);
    // 描画フレームごとに呼ぶ。狙いへ speedFxRiseSeconds（強まる時）・speedFxFallSeconds（弱まる時）でならし、
    // 上限 speedFxLimit を掛けた強さ（0〜1）を返す。乗っていない間も呼ぶ（降りた後は弱まって消える）。
    static float StepStrength(CloudMountComponent& mount, float dt);
    // 進む向き（世界の単位ベクトル）を画面へ写す。uv は左上が 0、右下が 1（画面の外も取る）。画面の中心から
    // speedFxCenterLimit（画面の端が 1）より遠い時と、カメラの横・後ろへ向かう時は、その向きのまま限度の所へ置く。
    // 戻り値は前へ向かう度合い（1＝カメラの前、後ろへ向かうほど 0 へ。強さに掛ける）。
    static float HeadingOnScreen(const CloudMountComponent& mount, const CloudMountSpeedFxView& view,
                                 HIKARI::GameProjectFloat3 direction, float& outU, float& outV);
    // 中心を前の値から speedFxCenterSeconds でならす（first なら一度に置く）。
    static void StepCenter(CloudMountComponent& mount, float u, float v, bool first, float dt);
    // 世界の点を画面へ写し、半径 radius (m) の球が画面の高さに対して占める半径を返す。カメラの後ろなら false。
    static bool SphereOnScreen(const CloudMountSpeedFxView& view, HIKARI::GameProjectFloat3 center, float radius,
                               float& outU, float& outV, float& outRadius);
};

} // namespace Project_TD2_2
