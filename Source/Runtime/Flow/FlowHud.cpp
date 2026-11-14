#include "FlowHud.h"

#include <algorithm>

namespace Project_TD2_2 {
namespace {

// 単色の白い画像（エンジンが作る）。色は tint で付ける。
constexpr std::string_view kSolidTexture = "generated://solid/ffffffff";
// 矩形ごとの識別（同じ矩形は毎フレーム同じ番号）。
constexpr uint64_t kIdPlayerBack = 0x10;
constexpr uint64_t kIdHealth = 0x11;
constexpr uint64_t kIdStaminaBack = 0x12;
constexpr uint64_t kIdStamina = 0x13;
constexpr uint64_t kIdSmokeBack = 0x14;
constexpr uint64_t kIdSmoke = 0x15;
constexpr uint64_t kIdBossBack = 0x20;
constexpr uint64_t kIdBoss = 0x21;
constexpr uint64_t kIdHealSquare = 0x100;
constexpr uint64_t kIdFade = 0x1000;
// 重ね順。底、中身、小さな印、画面全体の色の順に上へ。
constexpr int kOrderBack = 0;
constexpr int kOrderFill = 1;
constexpr int kOrderFade = 100;
// 配置の基準にしている画面の幅（SubmitSprite の既定の参照画面と同じ）。
constexpr float kReferenceWidth = 1920.0f;

bool Rect(const HIKARI::UiView& ui, uint64_t id, int order, float x, float y, float width, float height,
          HIKARI::GameProjectFloat3 color, float opacity, uint32_t flags = 0u) {
    if (width <= 0.0f || height <= 0.0f)
        return true;
    HIKARI::GameProjectUiSpriteRequest request{};
    request.sourceId = id;
    request.order = order;
    request.flags = flags;
    request.x = x;
    request.y = y;
    request.width = width;
    request.height = height;
    request.tint = {color.x, color.y, color.z, opacity};
    return ui.SubmitSprite(kSolidTexture, request);
}

float Ratio(float value, float maximum) { return maximum > 0.0f ? std::clamp(value / maximum, 0.0f, 1.0f) : 0.0f; }

// 底の上に、割合だけの長さの中身を重ねる。
bool Bar(const HIKARI::UiView& ui, const FlowComponent& flow, uint64_t backId, uint64_t fillId, float x, float y,
         float width, float height, float ratio, HIKARI::GameProjectFloat3 color) {
    const bool back = Rect(ui, backId, kOrderBack, x, y, width, height, flow.backColor, flow.backOpacity);
    const bool fill = Rect(ui, fillId, kOrderFill, x, y, width * ratio, height, color, 1.0f);
    return back && fill;
}

} // namespace

bool FlowHud::DrawPlayer(const HIKARI::UiView& ui, const FlowComponent& flow, const FlowHudValues& values) {
    // 一番下の煙から上へ、回復の印、スタミナ、HP の順に積む。
    const float smokeY = flow.playerBarBottom - flow.smokeBarHeight;
    const float healY = smokeY - flow.barGap - flow.healSquareSize;
    const float staminaY = healY - flow.barGap - flow.staminaBarHeight;
    const float healthY = staminaY - flow.barGap - flow.healthBarHeight;
    bool accepted = Bar(ui, flow, kIdPlayerBack, kIdHealth, flow.playerBarX, healthY, flow.playerBarWidth,
                        flow.healthBarHeight, Ratio(values.playerHealth, values.playerMaxHealth), flow.healthColor);
    accepted = Bar(ui, flow, kIdStaminaBack, kIdStamina, flow.playerBarX, staminaY, flow.playerBarWidth,
                   flow.staminaBarHeight, Ratio(values.playerStamina, values.playerMaxStamina), flow.staminaColor) &&
               accepted;
    accepted = Bar(ui, flow, kIdSmokeBack, kIdSmoke, flow.playerBarX, smokeY, flow.playerBarWidth, flow.smokeBarHeight,
                   Ratio(values.gourdSmoke, values.gourdCapacity), flow.smokeColor) &&
               accepted;
    for (int index = 0; index < values.healCharges; ++index) {
        const float x = flow.playerBarX + static_cast<float>(index) * (flow.healSquareSize + flow.barGap);
        accepted = Rect(ui, kIdHealSquare + static_cast<uint64_t>(index), kOrderFill, x, healY, flow.healSquareSize,
                        flow.healSquareSize, flow.healColor, 1.0f) &&
                   accepted;
    }
    return accepted;
}

bool FlowHud::DrawBoss(const HIKARI::UiView& ui, const FlowComponent& flow, const FlowHudValues& values) {
    const float x = (kReferenceWidth - flow.bossBarWidth) * 0.5f;
    return Bar(ui, flow, kIdBossBack, kIdBoss, x, flow.bossBarTop, flow.bossBarWidth, flow.bossBarHeight,
               Ratio(values.bossHealth, values.bossMaxHealth), flow.bossColor);
}

bool FlowHud::DrawFade(const HIKARI::UiView& ui, const FlowComponent& flow, float fade) {
    if (fade <= 0.0f)
        return true;
    const bool lost = flow.phase == FlowPhase::Lost;
    return Rect(ui, kIdFade, kOrderFade, 0.0f, 0.0f, 1.0f, 1.0f, lost ? flow.loseColor : flow.winColor,
                fade * (lost ? flow.loseOpacity : flow.winOpacity), HIKARI::kGameProjectUiFillViewport);
}

} // namespace Project_TD2_2
