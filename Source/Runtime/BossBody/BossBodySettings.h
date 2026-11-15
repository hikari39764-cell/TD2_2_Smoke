#pragma once

#include <string>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

// 刀と手の動き（見た目だけ）。規則の剣の線（BossPose）・当たり・F 数は読むだけで変えない。
// 刀は「見せる構え」を目標にばねで追い、振っている間（命中の窓）だけ規則の剣の線へ重ねる（見える刀＝当たる刀）。
// 位置は体の足元から、局所 m（+X が左、+Z が前）。向きは局所の向き（長さは問わない）。
struct BossBodyMotionSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 刀の向きのばね（窓の外）：戻る速さ（1 秒あたりの揺れの回数）と収まりの強さ（小さいほど行き過ぎて戻る）。
    float swordFrequency = 2.4f;
    float swordDamping = 0.42f;
    // 振る直前（窓の少し前）に規則の線へ寄せるばねの速さ・収まり。
    float swordHitFrequency = 9.0f;
    float swordHitDamping = 0.9f;
    // 命中の窓の間に許す、剣先と規則の線のずれ (m)。
    float hitMaxTipGap = 0.06f;
    // 振り抜いた勢いを、窓の後へ持ち越す割合（大きいほど振り抜いた先へ行き過ぎる）。
    float overshootCarry = 0.30f;
    // 振りの何 F 前から規則の線へ寄せ始めるか（溜めの誇張もこの間に消す）。
    int lockLeadFrames = 8;
    // 窓が閉じた後、手と刀を体の傾きへ戻す時間 (秒)。
    float bodyBlendSeconds = 0.10f;

    // 待機の構え（下段）：握りの位置と刀の向き。規則の剣が下ろした構えからこの角度 (度) 離れるまでは、
    // この構えを混ぜる（待機・溜めの始め・後隙の後半）。
    HIKARI::GameProjectFloat3 idleGrip{-0.55f, 1.75f, 0.55f};
    HIKARI::GameProjectFloat3 idleDirection{0.30f, -0.40f, 0.87f};
    float restBlendDegrees = 40.0f;
    // 待機の息：握りの上下 (m)、剣先の上下 (度)、速さ (1 秒あたりの回数)。
    float breathMeters = 0.025f;
    float breathDegrees = 1.5f;
    float breathFrequency = 0.28f;
    // 後隙のうち、振り終わりの形を保つ割合（残りで待機の構えへ戻す）。
    float postHoldShare = 0.5f;

    // 溜め：握りを剣の向きへ送り出す長さ (m) と、持ち上げる高さ (m)。構えている間の小さな震え（握り m・剣先 度・速さ）。
    float chargeReach = 0.30f;
    float chargeLift = 0.15f;
    float chargeTremorMeters = 0.010f;
    float chargeTremorDegrees = 0.6f;
    float chargeTremorFrequency = 11.0f;
    // 握りのばね（窓の外）：速さ（1 秒あたりの揺れの回数）と収まりの強さ。
    float gripFrequency = 3.2f;
    float gripDamping = 0.75f;
    // 振りの中ほどで握りを剣の線に沿って外へ伸ばす長さ (m) と、振りのどこ（0〜1）で伸び切るか。
    float armReach = 0.35f;
    float armReachPeak = 0.55f;

    // 刃の向き：刃を動きへ向け始める剣先の速さ（度/秒。これより遅い時は前の向きを保つ）、合わせる時間 (秒)、
    // 待機の刃の向きへ戻る時間 (秒)。
    float edgeMinDegreesPerSecond = 40.0f;
    float edgeTurnSeconds = 0.04f;
    float edgeIdleSeconds = 0.6f;
    // 待機の刃の向き（局所。刀に直交する成分を使う）と、溜めの間に刃を向ける先（これから斬る向き。局所）。
    HIKARI::GameProjectFloat3 idleEdge{-0.5f, -1.0f, 0.35f};
    HIKARI::GameProjectFloat3 chargeEdgeToward{0.0f, -0.45f, 0.9f};

    // 胴の捻り：刀の横の向き（局所 x）1 あたりの角度 (度)、上限 (度)、追う時間 (秒)、草摺に掛ける割合。
    float twistDegrees = 26.0f;
    float twistMaxDegrees = 22.0f;
    float twistSeconds = 0.08f;
    float tassetTwistShare = 0.5f;
    // 兜がプレイヤーを向く：横の上限 (度) と追う時間 (秒)。
    float headTurnMaxDegrees = 50.0f;
    float headTurnSeconds = 0.25f;
    // 左手の釣り合い：刀の向きが待機の構えから外れた量 1 あたり、反対へ動く長さ (m)、上限 (m)、追う時間 (秒)。
    float leftBalanceMeters = 0.30f;
    float leftBalanceMax = 0.30f;
    float leftBalanceSeconds = 0.15f;

    // 確かめ用：命中の窓の間、毎フレーム刃の向き・規則の線とのずれを記録に出す。
    bool traceMotion = false;
};

// ボスの外形（模型の部品）の、人が調整する値。ボスの部品（BossComponent）が持ち、項目は「外形」の節に出る。
// 模型は 1 つの資産（glTF）で、部品がノードに分かれて入っている。コードがノードごとに置き場所を決めて出す。
struct BossBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 外形の模型の資産（GUID）。空なら何も出さない。
    std::string model{"b1a52cf03fe7473cbbe74c310e9c5c29"};
    bool visible = true;
    // 外形の模型が影を落とすか。切ると軽くなるが、鎧の影が地面と鎧自身に落ちなくなる。
    bool castShadow = true;
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。剣の長さ（Boss.swordLength）に合うように拡大する。
    // 模型を作るスクリプトが出す値（KATANA_LENGTH）に合わせる。
    float swordModelLength = 2.305f;
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    float swordGripAlong = 0.07f;
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    float handScale = 1.0f;
    // 握りの右手の模型の形（刀の座標、模型の実寸）：手首の位置・肘の向き・手の甲の向き。籠手の筒を手首に付けるのに使う。
    // 模型を作るスクリプトが出す値（GRIP_WRIST・GRIP_ELBOW・GRIP_BACK）に合わせる。
    HIKARI::GameProjectFloat3 gripWrist{-0.0249f, 0.2105f, -0.0907f};
    HIKARI::GameProjectFloat3 gripElbow{0.3367f, 0.8333f, -0.4384f};
    HIKARI::GameProjectFloat3 gripBack{-0.9272f, 0.3746f, 0.0f};
    // 肩の位置（体の足元から、局所 m。右肩。左肩は左右を反転）。籠手の筒は、手首からこの肩の方へ伸びる
    // （腕は無いが、筒が肩を向くと腕があるように読める）。
    HIKARI::GameProjectFloat3 shoulder{-0.80f, 3.05f, 0.0f};
    // 筒の向きに、握りの手の模型の肘の向きを混ぜる割合（0 で肩をまっすぐ向く、1 で手の模型どおり）。
    float cuffGripBlend = 0.25f;
    // 左手（刀を持たない手）の、何もしていない時の置き場所：手首の位置（体の足元から、局所 m）と、
    // 指先・手の甲の向き（体の向きに対する局所の向き。長さは問わない）。
    HIKARI::GameProjectFloat3 leftRestWrist{1.0f, 1.80f, 0.35f};
    HIKARI::GameProjectFloat3 leftRestFingers{0.10f, -0.85f, 0.50f};
    HIKARI::GameProjectFloat3 leftRestBack{0.85f, 0.25f, 0.45f};

    // 鎧の部品の置き場所（体の足元から、局所 m。+X が左、+Z が前）。鎧は体の傾き・屈みに付いて動く。
    // 胴（ノード Do）の中心。模型を作るスクリプトの DO_CENTER_Y と合わせる。
    HIKARI::GameProjectFloat3 torsoCenter{0.0f, 2.50f, 0.0f};
    // 兜（ノード Kabuto・目の光）の中心と大きさ。
    HIKARI::GameProjectFloat3 headCenter{0.0f, 3.50f, 0.03f};
    float headScale = 1.16f;
    // 大袖（ノード Sode）：吊る所（右の袖。左は左右を反転）、外へ向ける角度（真横が 90 度。小さいほど前へ向く）、
    // 裾を外へ開く角度。
    HIKARI::GameProjectFloat3 sodeHang{-0.88f, 3.14f, 0.02f};
    float sodeYawDegrees = 76.0f;
    float sodeFlareDegrees = 24.0f;
    // 草摺（ノード KusazuriPanel）：枚数、吊る高さ、腰の輪郭の半径（横・前後）、裾を外へ開く角度。
    int tassetCount = 7;
    float tassetHangHeight = 2.02f;
    float tassetRadiusSide = 0.57f;
    float tassetRadiusFront = 0.50f;
    float tassetFlareDegrees = 15.0f;

    // 胴の煙：ボスの足元に付いて動く流体のエフェクト（鎧の中を満たし、裾から流れ落ちる）。空なら出さない。
    HIKARI::GameProjectVfxAssetReference smokeEffect{"7c41e9d2a05b4f6e9b3d8a1f2c6e5d04",
                                                     "Assets/Vfx/BossBodySmoke.hvfx"};
    // 煙の量（倍率）と、フェーズ 2 で煙を熱くする倍率（熱い煙は橙に光る。1 で光らない）。
    float smokeAmount = 1.0f;
    float smokeHeatPhaseTwo = 5.0f;
    // 気絶している間の煙の量（倍率。煙が薄れて、鎧がからっぽに見える）。
    float smokeAmountStunned = 0.35f;

    // 垂れた部品（大袖・草摺）の揺れ。体が動くと、動きと反対へ遅れて振れ、止まると行き過ぎて戻る。
    // 体の速さ 1 m/s あたりの振れ（度）、振れの上限（度）、戻る速さ（1 秒あたりの揺れの回数）、収まりの強さ（1 で揺れ戻さない）。
    float swayDegreesPerSpeed = 4.0f;
    float swayMaxDegrees = 40.0f;
    float swayFrequency = 1.6f;
    float swayDamping = 0.38f;
    // 止まっている時の小さな揺らぎ（度）と速さ（1 秒あたりの回数）。
    float flutterDegrees = 1.5f;
    float flutterFrequency = 0.45f;
    // 撃たれた時に、後ろへ弾かれる振れ（度）。
    float hitKickDegrees = 16.0f;

    // 刀と手の動き。
    BossBodyMotionSettings motion;

private:
    void DescribeShape(HIKARI::PropertyFields& fields);
};

// 草摺の枚数の上限（表示の番号を先に取っておく数）。
inline constexpr int kBossBodyTassetCapacity = 12;

// 刀と手の動きの実行時の状態（保存しない）。向き・位置は体の向き（yaw）の局所（足元が原点）。
struct BossBodyMotionState {
    bool ready = false;
    // 規則の剣の向きと、その動く速さ（固定 tick ごとに測る。局所の接ベクトル rad/秒）。
    HIKARI::GameProjectFloat3 ruleDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 ruleVelocity{};
    uint64_t ruleTick = 0;
    float ruleClock = 0.0f;
    // 見せる刀の向き（剣先）・その速さ（接ベクトル rad/秒）・刃の向き（刀に直交）・刃を向けたい先。
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 velocity{};
    HIKARI::GameProjectFloat3 edge{0.0f, -1.0f, 0.0f};
    HIKARI::GameProjectFloat3 edgeGoal{0.0f, -1.0f, 0.0f};
    // 見せる握りの位置と速さ。
    HIKARI::GameProjectFloat3 grip{};
    HIKARI::GameProjectFloat3 gripVelocity{};
    // 規則の線（体の向きだけの枠）へ寄せる重み（0＝体の傾きに付く、1＝規則の線）。前のフレームが窓だったか。
    float lock = 0.0f;
    bool wasWindow = false;
    // 溜めの重み（見た目の震えに使う）。
    float charge = 0.0f;
    // 胴の捻り・兜の向き（体の向きからの rad）と、左手の釣り合いのずれ（局所 m）。
    float twist = 0.0f;
    float headYaw = 0.0f;
    HIKARI::GameProjectFloat3 leftOffset{};
    // 瞬間移動を見分ける、前のフレームの足元。
    HIKARI::GameProjectFloat3 ground{};
};

// 外形の実行時の状態（保存しない）。
struct BossBodyState {
    // 模型を出せなかった知らせは、1 回だけ記録に出す（毎フレーム出さない）。
    bool submitWarned = false;
    HIKARI::GameProjectVfxPlaybackHandle smokeVfx{};
    // 垂れた部品の振れ（度。世界座標の x・z が水平の向き、y が持ち上がり）と、その速さ。
    HIKARI::GameProjectFloat3 sway{};
    HIKARI::GameProjectFloat3 swayVelocity{};
    HIKARI::GameProjectFloat3 previousPosition{};
    bool swayReady = false;
    float clock = 0.0f;
    // 撃たれた数（増えたら、振れを弾く）。
    uint64_t seenHits = 0;
    // 兜が向く先：プレイヤーの位置（BossSystem が固定 tick ごとに書く。見た目だけが読む）。
    HIKARI::GameProjectFloat3 lookTarget{};
    bool lookTargetSeen = false;
    BossBodyMotionState motion;
};

} // namespace Project_TD2_2
