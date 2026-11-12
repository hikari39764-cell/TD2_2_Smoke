#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

// 燃えている地面の格子（XZ 平面の正方形、中心と床の高さは持ち主が決める）。世界には触らない純粋な規則なので、
// 当たり・吸い込み・見た目のどれからも同じ答えが返る。セルは残りの燃える秒数（0 は燃えていない）を持つ。
class BurnMap final {
public:
    // 格子を作り直す（燃えている所は全部消える）。fieldSize は一辺 (m)、cellSize はセルの一辺 (m)。
    void Reset(HIKARI::GameProjectFloat3 center, float floorY, float fieldSize, float cellSize);
    bool IsReady() const { return width_ > 0 && height_ > 0; }

    // 中心が形の中に入るセルを、残り = max(残り, seconds) にする（重ねて塗っても短くならない）。
    void PaintSegment(HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b, float width, float seconds);
    void PaintRing(HIKARI::GameProjectFloat3 center, float radius, float width, float seconds);
    void PaintDisc(HIKARI::GameProjectFloat3 center, float radius, float seconds);

    // 残りを dt 秒減らす（0 で止める）。燃えているセルの数も数え直す。
    void Step(float dt);

    bool IsBurning(HIKARI::GameProjectFloat3 point) const;
    size_t BurningCells() const { return burningCells_; }
    float BurningArea() const { return static_cast<float>(burningCells_) * CellArea(); }

    // 扇（origin から forward（XZ の単位方向）へ range m、片側 halfAngleRadians）の中の燃えているセルを、
    // 近い順に maxArea m² まで消す。origin から nearRadius m 以内は向きを問わない（足元の火）。
    // セルは丸ごと消すので、1 セルの面積に満たない残りは消さない。消した面積を返す。
    float ClearFan(HIKARI::GameProjectFloat3 origin, HIKARI::GameProjectFloat3 forward, float range,
                   float halfAngleRadians, float nearRadius, float maxArea);
    float ClearAll();

    // 見た目用の読み出し。
    int Width() const { return width_; }
    int Height() const { return height_; }
    float CellSize() const { return cellSize_; }
    float CellArea() const { return cellSize_ * cellSize_; }
    float FloorY() const { return floorY_; }
    // セルの中心の世界座標（Y は床の高さ）。
    HIKARI::GameProjectFloat3 CellCenter(int ix, int iz) const;
    // 点（高さは見ない）が入るセルの番号。格子の外なら false。
    bool CellOf(HIKARI::GameProjectFloat3 point, int& ix, int& iz) const;
    float Remaining(int ix, int iz) const;
    // 燃えているセルを訪ねる。visit(ix, iz, 残り秒)。燃えた所を囲む範囲だけを走査する。
    template <class Visitor>
    void ForEachBurning(Visitor&& visit) const {
        if (burningCells_ == 0)
            return;
        for (int iz = minZ_; iz <= maxZ_; ++iz) {
            for (int ix = minX_; ix <= maxX_; ++ix) {
                const float remaining = cells_[Index(ix, iz)];
                if (remaining > 0.0f)
                    visit(ix, iz, remaining);
            }
        }
    }

private:
    size_t Index(int ix, int iz) const {
        return static_cast<size_t>(iz) * static_cast<size_t>(width_) + static_cast<size_t>(ix);
    }
    // 世界の XZ の範囲を、中心がその範囲に入りうるセルの番号の範囲へ直す（格子の外は切る）。偽なら格子の外。
    bool CellRange(float minX, float maxX, float minZ, float maxZ, int& x0, int& x1, int& z0, int& z1) const;
    // 燃え始めたセルを数え、囲む範囲を広げる。
    void Include(int ix, int iz);
    void Paint(int ix, int iz, float seconds);
    void RecomputeBounds();

    int width_ = 0;
    int height_ = 0;
    float cellSize_ = 0.5f;
    float floorY_ = 0.0f;
    // 格子の (0,0) セルの角の世界 XZ。
    float originX_ = 0.0f;
    float originZ_ = 0.0f;
    std::vector<float> cells_;
    size_t burningCells_ = 0;
    // 燃えているセルを囲む範囲（無ければ min > max）。Step と走査をここだけに絞る。
    int minX_ = 0, maxX_ = -1, minZ_ = 0, maxZ_ = -1;
    // 扇の中のセルを近い順に並べる作業場所（Reset で一度だけ確保し、毎 tick は確保しない）。
    struct Candidate {
        float distanceSquared;
        uint32_t index;
    };
    std::vector<Candidate> candidates_;
};

} // namespace Project_TD2_2
