#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdType = "Project_TD2_2.Gourd";
inline constexpr std::string_view kGourdSystem = "Project_TD2_2.GourdSystem";

class GourdComponent final : public HIKARI::IGameComponent {
  public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    uint64_t playerId = 0;
    uint64_t cameraId = 0;
    HIKARI::GameProjectFloat3 shoulderOffset{0.65f, 0.45f, -0.1f};
    float followFrequency = 14.0f;
    float teleportDistance = 10.0f;
    float height = 0.5f;
    float radius = 0.18f;
    float absorbRadius = 0.8f;
    float absorbStrength = 3.0f;
    float absorption = 60.0f;
    float absorbForwardDistance = 0.8f;
    float absorbHeightOffset = -0.45f;
    float absorbMoveScale = 0.5f;
    float capacity = 100.0f;
    float initialSmoke = 0.0f;
    float smokePerVolume = 25.0f;
    int traceEveryFrames = 0;

    // 累積回読値と所持量は別々に持ち、消費や容量超過を次の回読で戻さない。
    bool initialized = false;
    bool poseInitialized = false;
    bool absorbHeld = false;
    bool absorbing = false;
    bool playerValid = false;
    bool transformAccepted = true;
    bool fluidAccepted = true;
    bool readAccepted = true;
    float storedSmoke = 0.0f;
    double lastRemovedTotal = 0.0;
    double removedDelta = 0.0;
    uint64_t processedTick = 0;
    uint64_t renderFrameCount = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 followVelocity{};
    HIKARI::GameProjectFloat3 aimDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 mouthPosition{};
    HIKARI::GameProjectFloat3 sinkPosition{};
};

} // namespace Project_TD2_2
