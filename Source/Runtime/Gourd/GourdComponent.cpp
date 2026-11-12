#include "GourdComponent.h"

namespace Project_TD2_2 {

void GourdComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("追従と外形", true)) {
        fields.Bool("enabled", "有効", enabled);
        fields.SceneObject("player", "追従するプレイヤー", playerId);
        fields.SceneObject("camera", "照準用カメラ（制御視点が無い時）", cameraId);
        fields.Float3("shoulderOffset", "肩からの位置（右・上・前 m）", shoulderOffset);
        fields.Float("followFrequency", "ばね追従の速さ (/s)", followFrequency, 0.1f, 100.0f);
        fields.Note("砲撃の反動から戻る速さも、このばね追従の速さを使います。反動の距離と F 数は砲撃側で調整します。");
        fields.Float("teleportDistance", "瞬間移動とみなす距離 (m)", teleportDistance, 1.0f, 1000.0f);
        fields.Float("height", "高さ (m)", height, 0.02f, 10.0f);
        fields.Float("radius", "半径 (m)", radius, 0.01f, 5.0f);
        fields.Note("カプセルの +Y 側が口。基準メッシュは高さ 0.5 m・半径 0.18 m、物理の当たりは付けません。");
    }
    if (fields.Section("霧の吸い込み", true)) {
        fields.Float("absorbRadius", "口元で煙を回収する半径 (m)", absorbRadius, 0.01f, 10.0f);
        fields.Float("absorbStrength", "口元の吸入速度 (m/s)", absorbStrength, 0.0f, 100.0f);
        fields.Float("absorption", "口元で煙を消す率 (/s)", absorption, 0.0f, 1000.0f);
        fields.Float("gatherRadius", "霧を口へ運ぶ流れの半径 (m)", gatherRadius, 0.01f, 10.0f);
        fields.Float("gatherStrength", "霧を口へ運ぶ加速度 (m/s2・0 は停止)", gatherStrength, 0.0f, 100.0f);
        fields.Int("gatherRings", "霧を運ぶ流れの輪の数（1 は口の前だけ。奥へ伸ばすほど遠くの霧が来る）", gatherRings, 1, 4);
        fields.Float("gatherFanDegrees", "霧を集める扇の片側の角度 (度)", gatherFanDegrees, 0.0f, 80.0f);
        fields.Float("absorbForwardDistance", "霧を集める中心の前方距離 (m)", absorbForwardDistance, 0.0f, 10.0f);
        fields.Float("absorbHeightOffset", "霧を集める中心の高さ（プレイヤー原点から m）", absorbHeightOffset, -10.0f,
                     10.0f);
        fields.Note("プレイヤーの向きの前方の扇から、場の霧を輪から輪へ口の前まで運び、口元だけで回収します。"
                    "運ぶ力だけでは煙量は増えません。");
    }
    if (fields.Section("吸い込みの操作", true)) {
        fields.Int("absorbWindupFrames", "構えの前隙 (F。動けない)", absorbWindupFrames, 0, 600);
        fields.Int("absorbRecoveryFrames", "離した後の後隙 (F。動けない)", absorbRecoveryFrames, 0, 600);
        fields.Float("absorbMoveScale", "吸い込み中の歩行倍率（0.06 で約 0.3 m/秒）", absorbMoveScale, 0.0f, 1.0f);
        fields.Float("absorbTurnSpeed", "吸い込み中に向きを変えられる速さ (度/秒)", absorbTurnSpeed, 0.0f, 3600.0f);
        fields.Note("吸っている間は入力した方向へ向きを変えられます（ロックオン中でも）。吸う向きはプレイヤーの向きです。"
                    "構え・吸い込み・後隙の間は、ローリング・回復・砲撃を始められません。");
    }
    if (fields.Section("煙の残量", true)) {
        fields.Float("capacity", "煙の容量", capacity, 0.0f, 100000.0f);
        fields.Float("initialSmoke", "開始時の煙量", initialSmoke, 0.0f, 100000.0f);
        fields.Float("smokePerVolume", "吸った密度・体積から煙量への倍率", smokePerVolume, 0.0f, 100000.0f);
        fields.Note("回読は 1〜3 フレーム遅れます。容量に達しても吸い込み自体は続き、超過分は捨てます。");
    }
    if (fields.Section("吸い込みの演出と手応え")) {
        fields.Vfx("absorbEffect", "口の流体エフェクト（吸い込みの霧の筋）", absorbEffect);
        fields.Float("absorbEffectScale", "口のエフェクトの源の大きさ倍率（0 は無効）", absorbEffectScale, 0.0f, 8.0f);
        fields.Int("absorbRetireFrames", "吸い込みをやめてから口の域を消すまでの時間 (F)", absorbRetireFrames, 0, 600);
        fields.Float("absorbFovDegrees", "吸い込み中に狭める視野角 (度、0 は無効)", absorbFovDegrees, 0.0f, 20.0f);
        fields.Int("absorbFovEaseFrames", "視野角が寄る・戻る時間 (F)", absorbFovEaseFrames, 1, 120);
        fields.Float("absorbShakeAmplitude", "吸い込み中のカメラの細かい震え (m、0 は無効)", absorbShakeAmplitude, 0.0f,
                     0.2f);
        fields.Float("absorbShakeFrequency", "カメラの震えの周波数 (Hz)", absorbShakeFrequency, 0.0f, 60.0f);
        fields.Float("absorbPulseScale", "吸い込み中にヒョウタンが膨らむ割合（0 は無効）", absorbPulseScale, 0.0f, 1.0f);
        fields.Float("absorbPulseFrequency", "ヒョウタンの脈動の速さ (Hz)", absorbPulseFrequency, 0.0f, 30.0f);
        fields.Float("absorbRumble", "吸い込み中の振動の強さ (0〜1、0 は無効)", absorbRumble, 0.0f, 1.0f);
        fields.Int("fullRumbleFrames", "満杯になった時の振動時間 (F、0 は無効)", fullRumbleFrames, 0, 120);
        fields.Float("fullRumble", "満杯になった時の振動の強さ (0〜1)", fullRumble, 0.0f, 1.0f);
        fields.Note("エフェクト資産の +X は口の向く先。入力 input.on が吸い込み中です。使っていない間は域を消して、"
                    "同時に描ける流体の域の枠を空けます。");
    }
    if (fields.Section("検証")) {
        fields.Int("traceEveryFrames", "状態ログ間隔（0 は無効）", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
