#pragma once

#include <array>
#include <cstdint>

#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

// 登場で、体に付く前はシーンの独立した物体として置いてある部品（c-e0c500）。付いた後は、役者（骨格の動画で演じる
// 体）の同じ部品を見せる。
enum class BossIntroPart : uint8_t { GauntletRight, GauntletLeft, Helm, Count };
inline constexpr size_t kBossIntroPartCount = static_cast<size_t>(BossIntroPart::Count);

// 役者と部品の実行時の状態（保存しない）。見せる・隠すは同じ頼みを毎フレーム出さないよう、頼んだ値を覚える
// （-1＝まだ頼んでいない、0＝隠した、1＝見せた）。
struct BossIntroActorState {
    // 物体を探したか、役者がシーンにいて動画の機能が使えるか。
    bool searched = false;
    bool ready = false;
    int8_t actorShown = -1;
    int8_t lightOn = -1;
    std::array<int8_t, kBossIntroPartCount> partShown{-1, -1, -1};
    std::array<int8_t, kBossIntroPartCount> skinShown{-1, -1, -1};
    // 面頬の隙間の中の光（役者のノード HelmGlowMesh）を見せているか（-1 まだ頼んでいない）。場所の印を確かめたか。
    int8_t helmGlowShown = -1;
    bool helmSocketLogged = false;
    // 刃口の光を入れているか（消す時に 1 度だけ材質を戻す）。落ちている兜の隙間の瞬きを入れているか（同じ）。
    bool bladeEdgeLit = false;
    bool helmPartLit = false;
    // 演出の灯が、演出の資産に置かれていた所（世界。演技の間、毎フレーム控える）。演技の後、役者がまだ出ている間は、
    // ここに置き直す（資産は止まる時に灯を元の場所へ戻す。戻ったままだと、役者に当たる光が急に変わる）。
    bool keyLightSeen = false;
    HIKARI::GameProjectTransform keyLightAt{};
    // 演出の灯の、演技の最後の明るさ（灯の単位のまま）。演技の後、ここから 0 へ落として行く（資産は止まる時に、明るさも
    // 元の値へ戻す）。
    bool keyLightLevelSeen = false;
    float keyLightLevel = 0.0f;
    // 部品を骨へ寄せる偏移を頼んであるか（付いた時・済んだ時に外す）。
    std::array<bool, kBossIntroPartCount> offsetSet{};
    // 部品のいまの進み（0＝置いてある所、1＝骨の所）と、痙攣の段の進み（0〜1。段の外は 0）、付いたか。効果（煙・
    // 揺れ）が読む。
    std::array<float, kBossIntroPartCount> fly{};
    std::array<float, kBossIntroPartCount> twitch{};
    std::array<bool, kBossIntroPartCount> attached{};
    // 部品のいまの世界の位置（置いてある所から骨へ寄せた後）。
    std::array<HIKARI::GameProjectFloat3, kBossIntroPartCount> partAt{};
    // 役者に重ねてある姿の層を外す必要があるか（プレビューをやめた時）。記録に出した片段の事件段（変わった時に 1 行出す）。
    bool layerSet = false;
    const void* loggedClip = nullptr;
    std::array<bool, kBossIntroPartCount> twitchLogged{};
    // 編集器のプレビューで触ったか（やめた時に元へ戻す）。
    bool previewTouched = false;
    // 骨の世界の変換（1 フレーム前の姿）：腰・胸・兜・刀・両腕（肩・肘・手首）。取れたか。効果と体に付くエフェクトが読む。
    bool socketsValid = false;
    HIKARI::GameProjectTransform hips{};
    HIKARI::GameProjectTransform chest{};
    HIKARI::GameProjectTransform head{};
    HIKARI::GameProjectTransform sword{};
    HIKARI::GameProjectTransform rightShoulder{};
    HIKARI::GameProjectTransform rightElbow{};
    HIKARI::GameProjectTransform rightHand{};
    HIKARI::GameProjectTransform leftShoulder{};
    HIKARI::GameProjectTransform leftElbow{};
    HIKARI::GameProjectTransform leftHand{};
    // 地に立つ刀が地面に入る所（世界）。演技が始まる前の刀の骨から決めて、演技の間は変えない。
    bool swordEntryValid = false;
    HIKARI::GameProjectFloat3 swordEntry{};
};

} // namespace Project_TD2_2
