#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossSwordPose.h"

namespace Project_TD2_2 {

class BossComponent;
struct BossMove;
struct BossBodyPose;

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
    // 技のクリップの刀（c-7b1a09）：クリップのある技は、刀の当たり・刀の線（斬撃・刀の煙・刀光）を、クリップの刀の骨の
    // 所にする（当たりの窓は区間の表どおり）。切ると今までの構えの刀の線のまま。
    bool clipSword = true;
    // 刀の骨・左手の骨の名前（模型の節点の名前。左手の技の弾はこの骨の所から出る）。
    std::string swordNode = "Sword";
    std::string leftHandNode = "LeftHand";
    // 両手の霧の刀（c-40f6c1）：左手が刀の柄（刀の骨の原点）からこの距離 (m、模型の大きさ) まで近づくと両手で握った
    // とみなす（twoHandNear 以下で 1、twoHandFar 以上で 0、間はなめらかに）。握っている間は当たりの線を剣先の先へ
    // twoHandReach m 伸ばし、刀身に沿って場の霧へ霧を毎秒 twoHandMist（剣の煙と同じ単位。0 は出さない）、半径
    // twoHandMistRadius m で入れる。
    float twoHandNear = 0.9f;
    float twoHandFar = 1.4f;
    float twoHandReach = 2.5f;
    float twoHandMist = 0.0f;
    float twoHandMistRadius = 1.2f;
    // 左手にまとう霧（c-40f6c1）：骨格の体が出ている間、見えている左手の所から場の霧へ霧を毎秒 leftHandMist（剣の煙と
    // 同じ単位。0 は出さない）、半径 leftHandMistRadius m で入れる。霧は左手の周りを leftHandMistSwirl m/秒 で回る向きに
    // 吹き出す（手に巻き付いて見える）。
    float leftHandMist = 0.0f;
    float leftHandMistRadius = 0.9f;
    float leftHandMistSwirl = 3.0f;
};

// 技のクリップの、規則の時計の F（60 Hz）ごとの刀の骨と左手の骨の置き場所（模型の空間：足元が原点、+X 左・+Z 前、
// 骨格の体の大きさの倍率は掛けていない）。初めて要った時に、クリップをその時刻で 1 度だけ読む（c-7b1a09）。
struct BossClipSwordTrack {
    std::string clip;
    std::vector<HIKARI::GameProjectFloat3> swordPosition;
    std::vector<HIKARI::GameProjectQuaternion> swordRotation;
    std::vector<HIKARI::GameProjectFloat3> leftHand;
    std::vector<HIKARI::GameProjectQuaternion> leftHandRotation;
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
    // 技のクリップの刀の骨・左手の骨（クリップごと）と、読めなかったクリップ（何度も試さない）。
    std::vector<BossClipSwordTrack> swordTracks;
    std::vector<std::string> swordTrackFailed;
    // 最後に骨格の体を置いた足元・向き・大きさ（描画のフレーム。刀光を見える刀と同じ所に出す）。
    HIKARI::GameProjectTransform placed{};
    // 見えているクリップの刀の剣先の速さ (m/s。描画のフレーム。刀光・振りのゆがみを出す目安)。
    float tipSpeed = 0.0f;
    // 見えている左手の骨の所（世界。描画のフレーム。1 フレーム前に描いた姿）と、それを読めたか。
    HIKARI::GameProjectFloat3 leftHandWorld{};
    bool leftHandSeen = false;
    // 技のクリップの層の重み（0〜1。c-bc5908：待機の姿から重ね始める時は、混ぜる長さで 0 から上げる。いきなり 1 に
    // すると待機の姿から技の始めの姿へ跳ぶ）、待機の姿から重ね始めたか、層を外してからの秒。
    float layerWeight = 0.0f;
    bool layerFromIdle = false;
    float layerOffSeconds = 1000.0f;
    // 見えている刀の骨（世界。描画のフレーム。待機・移動の間の刀光・刀の霞と規則の剣の構えが使う）と、それを読めたか。
    HIKARI::GameProjectFloat3 swordGripWorld{};
    HIKARI::GameProjectFloat3 swordDirectionWorld{};
    bool swordSeen = false;
    // 前のフレームの剣先（剣先の速さを出す）。
    HIKARI::GameProjectFloat3 lastTip{};
    bool lastTipValid = false;
    // 骨の読み方を 1 度だけ記録に書いたか。
    bool bonesLogged = false;
};

namespace BossAnimBody {

// 描画のフレームごと：流すクリップを決め、骨格の体をボスの足元 body（体の中心の変換）と向きに置いて流す。alpha は
// 固定 tick の間の補間の割合（技のクリップの時刻を tick の間でも滑らかにする）。
// 骨格の体が体を演じる時（流すクリップがある）は true（呼ぶ側は戦いの体の外形を出さない）。
bool Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body, float alpha,
            float seconds);
// 骨格の体を隠して止める（ボスが消える・Play が止まる）。
void Stop(HIKARI::WorldView& world, BossComponent& boss);
// いま骨格の体が体を演じているか。
bool Showing(const BossComponent& boss);
// 規則の tick ごと（技の規則より前）：表のクリップの技の刀の骨・左手の骨を、まだ読んでいなければ読んでおく。
void PrepareSword(HIKARI::WorldView& world, BossComponent& boss);
// move がクリップの刀で当たる技か（クリップがあり、その刀の骨を読めた）。当たりの窓は区間の表どおりになる。
bool ClipMove(const BossComponent& boss, const BossMove& move);
// いまの技がクリップの刀で当たる技なら、規則の時計のいまの時刻のクリップの刀を、規則の剣の構え（足元が原点・体の向きの
// 局所）にして true（BossPose::Target が使う。当たり・斬撃の面・刀の煙がクリップの刀に付いて行く）。
bool ClipSwordPose(const BossComponent& boss, BossSwordPose& out);
// 左手の骨の所（世界。規則の tick）。いまの技がクリップの刀で当たる技でなければ false（左手の技の弾を出す所）。
bool LeftHandPoint(const BossComponent& boss, HIKARI::GameProjectFloat3& out);
// 両手で握っている度合い（0〜1。規則の tick のいまの時刻。クリップの刀で当たる技の間だけ。c-40f6c1）。
float TwoHand(const BossComponent& boss);
// 骨格の体が待機の姿（クリップの刀で当たる技でない間）を見せている時、見えている刀の骨を規則の剣の構え（足元が原点・
// 体の向きの局所）にして true（BossPose::Target が使う。待機・移動の間の刀の煙・剣先の霞が見える刀に付く。c-bc5908）。
bool BoneSwordPose(const BossComponent& boss, BossSwordPose& out);
// 描画のフレーム（Update の後）：骨格の体が出ている間、体に付く効果の置き場所（刀・両手・袖口・肩・胴・兜・草摺）を、
// 見えている骨格の骨にする（c-bc5908。外形の体の置き場所に効果が残らない）。クリップの刀で当たる技の間は、刀を
// 規則の時計の時刻のクリップの刀にする（alpha は tick の間の補間）。剣先の速さ（tipSpeed）も求める。
void ApplyPose(HIKARI::WorldView& world, BossComponent& boss, float alpha, float seconds, BossBodyPose& pose);

} // namespace BossAnimBody

} // namespace Project_TD2_2
