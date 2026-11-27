#pragma once

#include <cstdint>
#include <string_view>

#include "HIKARI/Project/ValueTypes.h"

// このファイルは Tools/BossBody/add_helm_glow.py が書き出す（手で直さない。模型を作り直したら流し直す）。
// 兜の面頬の隙間の中の光（模型に足した自発光の面）と、エフェクトを結ぶ場所。寸法は兜の模型のもの（兜の原点から、
// +Y 上・+Z 前。兜の大きさが掛かる）。
namespace Project_TD2_2::BossHelmGlowData {

// 戦いの体（KasumiBushi）のノード：ふだんの光と、フェーズ 2 の光。登場の役者（BossIntroActor）のノードと材質槽。
inline constexpr std::string_view kNode = "HelmGlow";
inline constexpr std::string_view kNodeHot = "HelmGlowHot";
inline constexpr std::string_view kActorNode = "HelmGlowMesh";
inline constexpr uint32_t kActorMaterial = 11u;
// 役者の刀の刃口の材質（BladeEdge）の槽。登場の間、ここを光らせる。
inline constexpr uint32_t kActorBladeEdgeMaterial = 6u;
// 落ちている兜（BossIntroHelm）の、隙間の中の面の材質槽（ふだんは黒。序幕の瞬きの間だけ、コードが光らせる）。
inline constexpr uint32_t kPartMaterial = 1u;
// 役者の、場所の印のノード（頭の骨の子）：隙間の帯の中心（面の表）と、兜の原点。
inline constexpr std::string_view kVisorSocket = "HelmVisor";
inline constexpr std::string_view kCoreSocket = "HelmCore";
// 隙間の帯の中心（面の表）と、帯の半分の幅・高さ。
inline constexpr HIKARI::GameProjectFloat3 kVisorCentre{-0.00100f, -0.24000f, 0.35053f};
inline constexpr float kVisorHalfWidth = 0.15300f;
inline constexpr float kVisorHalfHeight = 0.13600f;
// 役者の兜の大きさ（兜の模型の寸法に対する倍率）。
inline constexpr float kActorHelmScale = 1.95750f;

} // namespace Project_TD2_2::BossHelmGlowData
