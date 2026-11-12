#include "BurnMap.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {
namespace {

// 格子の一辺のセル数の上限。調整で極端に細かいセルを入れても、確保と走査が膨らみすぎないようにする。
constexpr int kMaxCellsPerSide = 1024;
// 割り算の丸め誤差で、ちょうど割り切れる大きさ（48 m ÷ 0.5 m）が 1 セル増えたり減ったりしないための余裕。
constexpr float kRoundingSlack = 1e-4f;
constexpr float kPi = 3.14159265f;

bool Finite(HIKARI::GameProjectFloat3 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

} // namespace

void BurnMap::Reset(HIKARI::GameProjectFloat3 center, float floorY, float fieldSize, float cellSize) {
    cells_.clear();
    candidates_.clear();
    width_ = height_ = 0;
    burningCells_ = 0;
    minX_ = minZ_ = 0;
    maxX_ = maxZ_ = -1;
    if (!Finite(center) || !std::isfinite(floorY) || !std::isfinite(fieldSize) || !std::isfinite(cellSize) ||
        fieldSize <= 0.0f || cellSize <= 0.0f)
        return;
    const int side =
        std::clamp(static_cast<int>(std::ceil(fieldSize / cellSize - kRoundingSlack)), 1, kMaxCellsPerSide);
    width_ = height_ = side;
    cellSize_ = cellSize;
    floorY_ = floorY;
    originX_ = center.x - static_cast<float>(side) * cellSize * 0.5f;
    originZ_ = center.z - static_cast<float>(side) * cellSize * 0.5f;
    cells_.assign(static_cast<size_t>(side) * static_cast<size_t>(side), 0.0f);
    candidates_.reserve(cells_.size());
}

bool BurnMap::CellRange(float minX, float maxX, float minZ, float maxZ, int& x0, int& x1, int& z0, int& z1) const {
    if (!IsReady())
        return false;
    // セル i の中心は origin + (i + 0.5) × cellSize。中心が [min, max] に入る i の範囲。
    x0 = std::max(0, static_cast<int>(std::ceil((minX - originX_) / cellSize_ - 0.5f)));
    x1 = std::min(width_ - 1, static_cast<int>(std::floor((maxX - originX_) / cellSize_ - 0.5f)));
    z0 = std::max(0, static_cast<int>(std::ceil((minZ - originZ_) / cellSize_ - 0.5f)));
    z1 = std::min(height_ - 1, static_cast<int>(std::floor((maxZ - originZ_) / cellSize_ - 0.5f)));
    return x0 <= x1 && z0 <= z1;
}

void BurnMap::Include(int ix, int iz) {
    ++burningCells_;
    if (minX_ > maxX_) {
        minX_ = maxX_ = ix;
        minZ_ = maxZ_ = iz;
        return;
    }
    minX_ = std::min(minX_, ix);
    maxX_ = std::max(maxX_, ix);
    minZ_ = std::min(minZ_, iz);
    maxZ_ = std::max(maxZ_, iz);
}

void BurnMap::Paint(int ix, int iz, float seconds) {
    float& cell = cells_[Index(ix, iz)];
    if (cell <= 0.0f)
        Include(ix, iz);
    cell = std::max(cell, seconds);
}

void BurnMap::PaintSegment(HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b, float width, float seconds) {
    if (!Finite(a) || !Finite(b) || !std::isfinite(width) || !std::isfinite(seconds) || width <= 0.0f ||
        seconds <= 0.0f)
        return;
    const float half = width * 0.5f;
    int x0, x1, z0, z1;
    if (!CellRange(std::min(a.x, b.x) - half, std::max(a.x, b.x) + half, std::min(a.z, b.z) - half,
                   std::max(a.z, b.z) + half, x0, x1, z0, z1))
        return;
    // 線分から幅の半分以内（端は丸い）。続けて塗る線分の継ぎ目に隙間ができない。
    const float abX = b.x - a.x, abZ = b.z - a.z;
    const float lengthSquared = abX * abX + abZ * abZ;
    for (int iz = z0; iz <= z1; ++iz) {
        for (int ix = x0; ix <= x1; ++ix) {
            const HIKARI::GameProjectFloat3 c = CellCenter(ix, iz);
            const float t = lengthSquared > 0.0f
                                ? std::clamp(((c.x - a.x) * abX + (c.z - a.z) * abZ) / lengthSquared, 0.0f, 1.0f)
                                : 0.0f;
            const float dx = c.x - (a.x + abX * t), dz = c.z - (a.z + abZ * t);
            if (dx * dx + dz * dz <= half * half)
                Paint(ix, iz, seconds);
        }
    }
}

void BurnMap::PaintRing(HIKARI::GameProjectFloat3 center, float radius, float width, float seconds) {
    if (!Finite(center) || !std::isfinite(radius) || !std::isfinite(width) || !std::isfinite(seconds) ||
        radius < 0.0f || width <= 0.0f || seconds <= 0.0f)
        return;
    const float outer = radius + width * 0.5f;
    const float inner = std::max(0.0f, radius - width * 0.5f);
    int x0, x1, z0, z1;
    if (!CellRange(center.x - outer, center.x + outer, center.z - outer, center.z + outer, x0, x1, z0, z1))
        return;
    for (int iz = z0; iz <= z1; ++iz) {
        for (int ix = x0; ix <= x1; ++ix) {
            const HIKARI::GameProjectFloat3 c = CellCenter(ix, iz);
            const float d2 = (c.x - center.x) * (c.x - center.x) + (c.z - center.z) * (c.z - center.z);
            if (d2 <= outer * outer && d2 >= inner * inner)
                Paint(ix, iz, seconds);
        }
    }
}

void BurnMap::PaintDisc(HIKARI::GameProjectFloat3 center, float radius, float seconds) {
    if (!Finite(center) || !std::isfinite(radius) || !std::isfinite(seconds) || radius <= 0.0f || seconds <= 0.0f)
        return;
    int x0, x1, z0, z1;
    if (!CellRange(center.x - radius, center.x + radius, center.z - radius, center.z + radius, x0, x1, z0, z1))
        return;
    for (int iz = z0; iz <= z1; ++iz) {
        for (int ix = x0; ix <= x1; ++ix) {
            const HIKARI::GameProjectFloat3 c = CellCenter(ix, iz);
            if ((c.x - center.x) * (c.x - center.x) + (c.z - center.z) * (c.z - center.z) <= radius * radius)
                Paint(ix, iz, seconds);
        }
    }
}

void BurnMap::RecomputeBounds() {
    const int oldMinX = minX_, oldMaxX = maxX_, oldMinZ = minZ_, oldMaxZ = maxZ_;
    minX_ = minZ_ = 0;
    maxX_ = maxZ_ = -1;
    burningCells_ = 0;
    for (int iz = oldMinZ; iz <= oldMaxZ; ++iz) {
        for (int ix = oldMinX; ix <= oldMaxX; ++ix) {
            if (cells_[Index(ix, iz)] > 0.0f)
                Include(ix, iz);
        }
    }
}

void BurnMap::Step(float dt) {
    if (burningCells_ == 0 || !std::isfinite(dt) || dt <= 0.0f)
        return;
    for (int iz = minZ_; iz <= maxZ_; ++iz) {
        for (int ix = minX_; ix <= maxX_; ++ix) {
            float& cell = cells_[Index(ix, iz)];
            if (cell > 0.0f)
                cell = std::max(0.0f, cell - dt);
        }
    }
    RecomputeBounds();
}

bool BurnMap::CellOf(HIKARI::GameProjectFloat3 point, int& ix, int& iz) const {
    if (!IsReady() || !Finite(point))
        return false;
    ix = static_cast<int>(std::floor((point.x - originX_) / cellSize_));
    iz = static_cast<int>(std::floor((point.z - originZ_) / cellSize_));
    return ix >= 0 && iz >= 0 && ix < width_ && iz < height_;
}

bool BurnMap::IsBurning(HIKARI::GameProjectFloat3 point) const {
    int ix = 0, iz = 0;
    return CellOf(point, ix, iz) && cells_[Index(ix, iz)] > 0.0f;
}

float BurnMap::ClearFan(HIKARI::GameProjectFloat3 origin, HIKARI::GameProjectFloat3 forward, float range,
                        float halfAngleRadians, float nearRadius, float maxArea) {
    if (burningCells_ == 0 || !Finite(origin) || !Finite(forward) || !std::isfinite(range) ||
        !std::isfinite(halfAngleRadians) || !std::isfinite(nearRadius) || !std::isfinite(maxArea) || range <= 0.0f ||
        maxArea < CellArea())
        return 0.0f;
    const float forwardLength = std::hypot(forward.x, forward.z);
    if (forwardLength <= 0.0f)
        return 0.0f;
    const float fx = forward.x / forwardLength, fz = forward.z / forwardLength;
    const float minimumCos = std::cos(std::clamp(halfAngleRadians, 0.0f, kPi));
    int x0, x1, z0, z1;
    if (!CellRange(origin.x - range, origin.x + range, origin.z - range, origin.z + range, x0, x1, z0, z1))
        return 0.0f;
    // 燃えている範囲の外は見ない。
    x0 = std::max(x0, minX_);
    x1 = std::min(x1, maxX_);
    z0 = std::max(z0, minZ_);
    z1 = std::min(z1, maxZ_);
    candidates_.clear();
    for (int iz = z0; iz <= z1; ++iz) {
        for (int ix = x0; ix <= x1; ++ix) {
            const size_t index = Index(ix, iz);
            if (cells_[index] <= 0.0f)
                continue;
            const HIKARI::GameProjectFloat3 c = CellCenter(ix, iz);
            const float dx = c.x - origin.x, dz = c.z - origin.z;
            const float d2 = dx * dx + dz * dz;
            if (d2 > range * range)
                continue;
            const float distance = std::sqrt(d2);
            // 足元（nearRadius 以内）は向きを問わない。距離 0 のセルは向きが決まらないので、これも扇の中とみなす。
            if (distance > std::max(0.0f, nearRadius) && (dx * fx + dz * fz) / distance < minimumCos)
                continue;
            candidates_.push_back({d2, static_cast<uint32_t>(index)});
        }
    }
    std::sort(candidates_.begin(), candidates_.end(),
              [](const Candidate& a, const Candidate& b) { return a.distanceSquared < b.distanceSquared; });
    const size_t limit = static_cast<size_t>(std::floor(maxArea / CellArea() + kRoundingSlack));
    const size_t count = std::min(limit, candidates_.size());
    for (size_t k = 0; k < count; ++k)
        cells_[candidates_[k].index] = 0.0f;
    if (count > 0)
        RecomputeBounds();
    return static_cast<float>(count) * CellArea();
}

float BurnMap::ClearAll() {
    const float area = BurningArea();
    if (burningCells_ > 0)
        std::fill(cells_.begin(), cells_.end(), 0.0f);
    burningCells_ = 0;
    minX_ = minZ_ = 0;
    maxX_ = maxZ_ = -1;
    return area;
}

HIKARI::GameProjectFloat3 BurnMap::CellCenter(int ix, int iz) const {
    return {originX_ + (static_cast<float>(ix) + 0.5f) * cellSize_, floorY_,
            originZ_ + (static_cast<float>(iz) + 0.5f) * cellSize_};
}

float BurnMap::Remaining(int ix, int iz) const {
    if (ix < 0 || iz < 0 || ix >= width_ || iz >= height_)
        return 0.0f;
    return cells_[Index(ix, iz)];
}

} // namespace Project_TD2_2
