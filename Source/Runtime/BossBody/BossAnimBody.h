#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossSwordPose.h"

namespace Project_TD2_2 {

class BossComponent;
class BossBodyComponent;
struct BossMove;
struct BossBodyPose;

// 骨格の体（c-7b1a06）：ボスの技をモーション（骨格の動画）で見せるための体。登場の役者と同じ骨格の模型
// （Assets/Models/BossIntroActor）に動画の部品（AnimatorComponent）を付けたシーンの物体を、ボスの足元と向きに置いて
// 流す。待機のクリップはどの状態でも下の層で流す。流せるクリップが無い間は骨格の体を隠す。
// 技のクリップ（技の表の「アニメーション」。c-7b1a08）は、技の規則の時計（前隙・攻撃中・後隙の経過 F）に合わせた時刻の姿を、
// 骨格の根（Hips）から下へ重ねる層で出す（ヒットストップ・遅くする間も規則とずれない）。待機のクリップは下の層で繰り返す。
// 規則の剣の線（当たり・斬撃の面・刀の煙）は、クリップの刀か見えている刀の骨から取る（BossPose::Target）。
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

// 技のクリップの刀（規則の側。c-bd1a7a でボスの部品の骨格の体の状態から分けた）：クリップごとの刀の骨・左手の骨と、
// 読めなかったクリップ（何度も試さない）、待機のクリップの始めの刀。規則の tick が読む（PrepareSword が固定 tick の中で
// 読んでおく。描画のフレームの値ではない）。ボスの部品（BossComponent::clipSwords）が持つ。
struct BossClipSwords {
    std::vector<BossClipSwordTrack> tracks;
    std::vector<std::string> failed;
    // 待機のクリップの始め（時刻 0）の刀の骨（模型の空間。c-164cab）：クリップの刀で当たる技でない間の、規則の剣の構え。
    // PrepareSword が固定 tick の中で 1 度だけ読む。読めたか・読めなかったか（何度も試さない）。
    HIKARI::GameProjectFloat3 idlePosition{};
    HIKARI::GameProjectQuaternion idleRotation{0.0f, 0.0f, 0.0f, 1.0f};
    bool idleReady = false;
    bool idleFailed = false;
};

// 骨格の体の、描画のフレームごとの状態（シーンに保存しない。描画のフレームだけが書き、読む。体の見た目の部品
// BossBodyComponent::anim が持つ）。
struct BossAnimBodyState {
    // 物体の模型を出したか（-1 まだ聞いていない・0 隠した・1 出した）。
    int8_t shown = -1;
    // いま流しているクリップ（空は何も流していない）。
    std::string playing;
    // いま重ねている技のクリップ（空は重ねていない）。
    std::string layerClip;
    // 骨格の体が体を演じているか。
    bool active = false;
    // 流せなかったクリップ（同じクリップで何度も知らせない）。
    std::string failedClip;
    // 物体が無い・動画の機能が無い時に 1 度だけ知らせたか。
    bool warned = false;
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
    // 見えている刀の骨（世界。描画のフレーム。剣先の速さを出す。規則の tick は読まない）と、それを読めたか。
    HIKARI::GameProjectFloat3 swordGripWorld{};
    HIKARI::GameProjectFloat3 swordDirectionWorld{};
    bool swordSeen = false;
    // 前のフレームの剣先（剣先の速さを出す）。
    HIKARI::GameProjectFloat3 lastTip{};
    bool lastTipValid = false;
    // 骨の読み方を 1 度だけ記録に書いたか。
    bool bonesLogged = false;
    // 演出のクリップ（変わり目の演出が毎フレーム頼む。c-271cba）：流すクリップ（空は頼んでいない）、その時刻（0〜1）、
    // 外す時に待機の姿へ混ぜる長さ (秒)。いま重ねているか。
    std::string cinematicClip;
    float cinematicNormalized = 0.0f;
    float cinematicBlendOut = 0.35f;
    bool cinematicLayered = false;
    // 見えている腰の骨（世界。描画のフレーム。1 フレーム前に描いた姿）と、それを読めたか（演出の間、胴の煙の域を
    // 体に付いて行かせる）。
    HIKARI::GameProjectFloat3 hipsWorld{};
    bool hipsSeen = false;
    // 見えている胸の骨（Spine2）の所（世界。描画のフレーム。1 フレーム前に描いた姿。hipsSeen の時だけ意味がある）。
    HIKARI::GameProjectFloat3 chestWorld{};
    // 見えている頭の骨の所（同じ。戦いの間、胴の煙の域を倒す向きを 腰 → 頭 で決める）。
    HIKARI::GameProjectFloat3 headWorld{};
    // 胴の煙の域を倒す向き（世界。腰 → 胸をなめらかに追った向き。長さ 1）と、倒す重み（0 倒さない 〜 1。変わり目の
    // 演出で体を据えている間は 1、戦いの間は smokeTiltCombat）。
    HIKARI::GameProjectFloat3 smokeSpine{0.0f, 1.0f, 0.0f};
    float smokeTiltWeight = 0.0f;
    // 胴の煙の域の軸（世界。腰をなめらかに追った所。細かい揺れは追わない）と、それを持っているか。
    HIKARI::GameProjectFloat3 smokePivot{};
    bool smokePivotValid = false;
    // 戦いの間、域の軸と向きを体（根）の動きと一緒に運ぶための、前のフレームの体の置き場所と、それを持っているか。
    HIKARI::GameProjectFloat3 smokeBodyPosition{};
    HIKARI::GameProjectQuaternion smokeBodyRotation{0.0f, 0.0f, 0.0f, 1.0f};
    bool smokeBodyValid = false;
};

namespace BossAnimBody {

// 描画のフレームごと：流すクリップを決め、骨格の体をボスの足元 body（体の中心の変換）と向きに置いて流す。alpha は
// 固定 tick の間の補間の割合（技のクリップの時刻を tick の間でも滑らかにする）。
// 骨格の体が体を演じる時（流すクリップがある）は true。
// look は同じ物体の体の見た目の部品（見た目の状態を持つ）。
bool Update(HIKARI::WorldView& world, const BossComponent& boss, BossBodyComponent& look,
            const HIKARI::GameProjectTransform& body, float alpha, float seconds);
// 骨格の体を隠して止める（ボスが消える・Play が止まる）。
void Stop(HIKARI::WorldView& world, const BossComponent& boss, BossBodyComponent& look);
// いま骨格の体が体を演じているか（体の見た目の部品が無ければ false）。
bool Showing(const BossBodyComponent* look);
// 規則の tick ごと（技の規則より前）：表のクリップの技の刀の骨・左手の骨と、待機のクリップの始めの刀の骨を、まだ
// 読んでいなければ読んでおく。
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
// クリップの刀で当たる技でない間（待機・移動・気絶・演出）の規則の剣の構え：待機のクリップの始めの刀の骨を、規則の
// 剣の構え（足元が原点・体の向きの局所）にして true（BossPose::Target が使う。c-164cab：描画のフレームの骨は読まない。
// 同じ入力なら毎回同じ構えになる）。待機のクリップの刀の骨を読めていなければ false。
bool IdleSwordPose(const BossComponent& boss, BossSwordPose& out);
// 描画のフレーム（Update の後）：骨格の体が出ている間、体に付く効果の置き場所（刀・両手・袖口・肩・胴・兜・草摺）を、
// 見えている骨格の骨にする（c-bc5908）。クリップの刀で当たる技の間は、刀を
// 規則の時計の時刻のクリップの刀にする（alpha は tick の間の補間）。剣先の速さ（tipSpeed）も求める。
void ApplyPose(HIKARI::WorldView& world, const BossComponent& boss, BossBodyComponent& look, float alpha, float seconds,
               BossBodyPose& pose);

} // namespace BossAnimBody

} // namespace Project_TD2_2
