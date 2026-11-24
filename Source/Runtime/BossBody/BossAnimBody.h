#pragma once

#include <cstdint>
#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2 {

class BossComponent;

// 骨格の体（c-7b1a06）：ボスの技をモーション（骨格の動画）で見せるための体。登場の役者と同じ骨格の模型
// （Assets/Models/BossIntroActor）に動画の部品（AnimatorComponent）を付けたシーンの物体を、ボスの足元と向きに置いて
// 流す。流すクリップがある間は、部品ごとに置く戦いの体の外形を出さない（骨格の体が体を演じる）。流すクリップが無い
// 間は骨格の体を隠し、今までどおりの外形で動く。
// 技のクリップ（技の表の「アニメーション」。c-7b1a08）は、技の規則の時計（前隙・攻撃中・後隙の経過 F）に合わせた時刻の姿を、
// 骨格の根（Hips）から下へ重ねる層で出す（ヒットストップ・遅くする間も規則とずれない）。待機のクリップは下の層で繰り返す。
// 当たりはまだ今までどおりの刀の線（c-7b1a09 でモーションの刀に合わせる）。
struct BossAnimBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 骨格の体の物体（模型と動画の部品を持つ）。0 なら使わない。
    uint64_t object = 0;
    // 骨格の体の大きさの倍率（物体の変換の scale に掛ける。1 で模型のまま）。
    float scale = 1.0f;
    // 技を出していない間（待機・歩き）に繰り返し流すクリップ。空なら、技のクリップを流す間だけ骨格の体を出す。
    std::string idleClip;
    // クリップを替える時に前の姿から混ぜる長さ (秒)。
    float blendSeconds = 0.15f;
};

// 骨格の体の、描画のフレームごとの状態（シーンに保存しない）。
struct BossAnimBodyState {
    // 物体の模型を出したか（-1 まだ聞いていない・0 隠した・1 出した）。
    int8_t shown = -1;
    // いま流しているクリップ（空は何も流していない）。
    std::string playing;
    // いま重ねている技のクリップ（空は重ねていない）。
    std::string layerClip;
    // 骨格の体が体を演じているか（外形を出さない）。
    bool active = false;
    // 流せなかったクリップ（同じクリップで何度も知らせない）。
    std::string failedClip;
    // 物体が無い・動画の機能が無い時に 1 度だけ知らせたか。
    bool warned = false;
};

namespace BossAnimBody {

// 描画のフレームごと：流すクリップを決め、骨格の体をボスの足元 body（体の中心の変換）と向きに置いて流す。alpha は
// 固定 tick の間の補間の割合（技のクリップの時刻を tick の間でも滑らかにする）。
// 骨格の体が体を演じる時（流すクリップがある）は true（呼ぶ側は戦いの体の外形を出さない）。
bool Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body, float alpha);
// 骨格の体を隠して止める（ボスが消える・Play が止まる）。
void Stop(HIKARI::WorldView& world, BossComponent& boss);
// いま骨格の体が体を演じているか。
bool Showing(const BossComponent& boss);

} // namespace BossAnimBody

} // namespace Project_TD2_2
