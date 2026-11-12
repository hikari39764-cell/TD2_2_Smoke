#include "GourdComponent.h"

namespace Project_TD2_2 {

void GourdComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("追従と外形", true)) {
        fields.Bool("enabled", "有効", enabled);
        fields.SceneObject("player", "追従するプレイヤー", playerId);
        fields.SceneObject("camera", "照準用カメラ（制御視点が無い時）", cameraId);
        fields.Float3("shoulderOffset", "肩からの位置（右・上・前 m）", shoulderOffset);
        fields.Float("followFrequency", "ばね追従の速さ (/s)", followFrequency, 0.1f, 100.0f);
        fields.Float("teleportDistance", "瞬間移動とみなす距離 (m)", teleportDistance, 1.0f, 1000.0f);
        fields.Float("height", "高さ (m)", height, 0.02f, 10.0f);
        fields.Float("radius", "半径 (m)", radius, 0.01f, 5.0f);
        fields.Note("カプセルの +Y 側が口。基準メッシュは高さ 0.5 m・半径 0.18 m、物理の当たりは付けません。");
    }
    if (fields.Section("霧の吸い込み", true)) {
        fields.Float("absorbRadius", "吸う半径 (m)", absorbRadius, 0.01f, 10.0f);
        fields.Float("absorbStrength", "吸う強さ (m/s)", absorbStrength, 0.0f, 100.0f);
        fields.Float("absorption", "煙を消す率 (/s)", absorption, 0.0f, 1000.0f);
        fields.Float("absorbForwardDistance", "吸う中心の前方距離 (m)", absorbForwardDistance, 0.0f, 10.0f);
        fields.Float("absorbHeightOffset", "吸う中心の高さ（プレイヤー原点から m）", absorbHeightOffset, -10.0f, 10.0f);
        fields.Float("absorbMoveScale", "吸い込み中の歩行倍率", absorbMoveScale, 0.0f, 1.0f);
    }
    if (fields.Section("煙の残量", true)) {
        fields.Float("capacity", "煙の容量", capacity, 0.0f, 100000.0f);
        fields.Float("initialSmoke", "開始時の煙量", initialSmoke, 0.0f, 100000.0f);
        fields.Float("smokePerVolume", "吸った密度・体積から煙量への倍率", smokePerVolume, 0.0f, 100000.0f);
        fields.Note("回読は 1〜3 フレーム遅れます。容量に達しても吸い込み自体は続き、超過分は捨てます。");
    }
    if (fields.Section("検証")) {
        fields.Int("traceEveryFrames", "状態ログ間隔（0 は無効）", traceEveryFrames, 0, 7200);
    }
}

} // namespace Project_TD2_2
