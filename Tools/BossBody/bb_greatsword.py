"""西洋の大剣（騎士の両手剣）を作る。エンジンの座標で考える：原点＝握りの中央、+Z＝剣先、±Y＝両刃、±X＝剣の平。
刀（bb_katana.py）の代わりに使う。握りの中央から剣先までの長さは刀と同じにする（コードと登場の動き（插し込み・肩の担ぎ）
が、その長さで剣を置いているため）。

ノード（名前は刀の頃のまま。コードがこの名前で探す）：
- KatanaHilt    ：鍔（十字の鍔）・握り（革巻き）・柄頭
- KatanaBlade   ：剣身（材質槽 BladeSteel＝平と樋、BladeEdge＝両の刃）
- KatanaBladeHot：同じ形の、灼けた剣身（発光する材質。火の技・フェーズ 2 でコードが差し替える）
"""
import math

import bmesh
from mathutils import Vector

import bb_common as bb

# ---- 形の値（m。刀の模型と同じ倍率でボスに付く）----
TIP_Z = 2.3411            # 握りの中央から剣先まで（刀と同じ。bosscommon.SWORD_MODEL_LENGTH）
GUARD_Z = 0.215           # 握りの中央から鍔の中央まで
GUARD_HALF_SPAN = 0.36    # 鍔の腕の長さ（中央から片方の端まで。刃の向き ±Y へ伸びる）
GUARD_HALF_X = 0.030      # 鍔の厚み（平の向きの半分）
GUARD_HALF_Z = 0.026      # 鍔の高さ（剣の向きの半分）
BLADE_WIDTH_BASE = 0.230  # 剣身の元幅（刃から刃まで）
BLADE_WIDTH_TIP = 0.165   # 切っ先が始まる所の幅
BLADE_THICK = 0.040       # 剣身の厚み（鎬の所）
POINT_LENGTH = 0.36       # 切っ先の長さ
FULLER_END = 0.62         # 樋が終わる所（剣身の長さに対する割合）
FULLER_DEPTH = 0.45       # 樋の深さ（厚みに対する割合）
RICASSO = 0.10            # 鍔の上の、刃の付いていない元の長さ
GRIP_BOTTOM = -0.50       # 握りの下の端（柄頭の上）
GRIP_RADIUS = 0.050       # 握りの太さ
GRIP_RIDGES = 9           # 革巻きの段の数
POMMEL_RADIUS = 0.095     # 柄頭（円盤）の半径
POMMEL_HALF_X = 0.040     # 柄頭の厚みの半分


def E(x, y, z):
    """エンジンの座標 → Blender の座標。"""
    return Vector((x, -z, y))


def blade_rings(z0, count=220):
    """剣身の断面の輪（14 点）。点の順は、+Y の刃 → +X の平 → -Y の刃 → -X の平（刀と同じ回りの向き）。"""
    rings = []
    length = TIP_Z - z0
    straight = length - POINT_LENGTH
    for i in range(count + 1):
        u = i / count
        # 切っ先は輪を細かくする。
        z = length * (1.0 - (1.0 - u) ** 1.3)
        if z <= straight:
            k = z / straight
            width = BLADE_WIDTH_BASE + (BLADE_WIDTH_TIP - BLADE_WIDTH_BASE) * k
            thick = BLADE_THICK * (1.0 - 0.25 * k)
        else:
            s = (z - straight) / POINT_LENGTH
            # 切っ先：両の刃が少し膨らんで一点へ寄る。
            width = max(0.004, BLADE_WIDTH_TIP * (1.0 - s) ** 0.85)
            thick = max(0.003, BLADE_THICK * 0.75 * (1.0 - s) ** 0.7)
        w = width * 0.5
        # 元の刃の付いていない所は、刃を立てない（平の厚みのまま角を落とすだけ）。
        edge = min(1.0, max(0.0, (z - RICASSO) / 0.06))
        bevel = 0.30 + 0.06 * edge  # 刃の面が始まる所（幅の半分に対する、刃からの割合）
        tip_t = thick * 0.5 * (0.55 - 0.45 * edge)
        # 樋（平の真ん中の溝）。終わりへ向けて浅くなる。
        f = min(1.0, max(0.0, (FULLER_END - z / length) / 0.08))
        groove = thick * 0.5 * (1.0 - FULLER_DEPTH * f)
        fw = w * 0.16 * (0.4 + 0.6 * f)
        half = [
            (tip_t, w * (1.0 - 0.02)),
            (thick * 0.5 * 0.80, w * (1.0 - bevel)),
            (thick * 0.5, w * 0.30),
            (groove, fw),
            (groove, -fw),
            (thick * 0.5, -w * 0.30),
            (thick * 0.5 * 0.80, -w * (1.0 - bevel)),
        ]
        pts = [(0.0, w)] + half[1:] + [(0.0, -w)] + [(-x, y) for x, y in reversed(half[1:])]
        rings.append([Vector((x, y, z0 + z)) for x, y in pts])
    return rings


def strip(bm, rings, a, b):
    """輪の a 番と b 番の点の間を、自前の頂点で帯にする（帯の境は折れ目、長さの方向はなめらか）。"""
    rows = [(bm.verts.new(E(*ring[a])), bm.verts.new(E(*ring[b]))) for ring in rings]
    for (p0, q0), (p1, q1) in zip(rows, rows[1:]):
        try:
            bm.faces.new((p0, q0, q1, p1))
        except ValueError:
            pass


def build_blade(name, z0, steel, edge):
    bm_steel, bm_edge = bmesh.new(), bmesh.new()
    rings = blade_rings(z0)
    n = len(rings[0])  # 14：0＝+Y の刃、7＝-Y の刃
    edge_strips = ((13, 0), (0, 1), (6, 7), (7, 8))
    for a in range(n):
        b = (a + 1) % n
        strip(bm_edge if (a, b) in edge_strips else bm_steel, rings, a, b)
    base = [bm_steel.verts.new(E(*p)) for p in rings[0]]
    bm_steel.faces.new(base)
    o1 = bb.new_object(name + "_steel", bm_steel, steel, smooth=True)
    o2 = bb.new_object(name + "_edge", bm_edge, edge, smooth=True)
    return bb.join(name, [o1, o2])


def box_ring(half_x, half_y, z, chamfer):
    """角を落とした四角の輪（8 点、刀の輪と同じ回りの向き）。"""
    c = chamfer
    pts = [(-half_x + c, half_y), (half_x - c, half_y), (half_x, half_y - c), (half_x, -half_y + c),
           (half_x - c, -half_y), (-half_x + c, -half_y), (-half_x, -half_y + c), (-half_x, half_y - c)]
    return [E(x, y, z) for x, y in pts]


def round_ring(radius_x, radius_y, z, count=28):
    return [E(math.cos(2.0 * math.pi * i / count) * radius_x, math.sin(2.0 * math.pi * i / count) * radius_y, z)
            for i in range(count)]


def build_guard(mats):
    """十字の鍔：中央の塊から ±Y へ腕を伸ばし、端を少し剣先の側へ反らせて太らせる。"""
    parts = []
    for side in (1.0, -1.0):
        bm = bmesh.new()
        rings = []
        steps = 14
        for i in range(steps + 1):
            s = i / steps
            y = side * GUARD_HALF_SPAN * s
            # 中ほどを細く、端を太く。端は剣先の側へ少し反る。
            fat = 1.0 - 0.28 * math.sin(math.pi * min(1.0, s * 1.15)) + 0.25 * max(0.0, s - 0.85) / 0.15
            lift = 0.05 * s ** 3
            hx, hz = GUARD_HALF_X * fat, GUARD_HALF_Z * fat
            ring = [E(hx * math.cos(a), y, GUARD_Z + lift + hz * math.sin(a))
                    for a in (2.0 * math.pi * j / 12 for j in range(12))]
            if side < 0:
                ring.reverse()
            rings.append(ring)
        bb.loft(bm, rings, cap_start=False, cap_end=True)
        parts.append(bb.new_object("guard_arm", bm, mats["iron"], smooth=True))
    # 中央の塊（剣身の元を抱く）。
    bm = bmesh.new()
    rings = [box_ring(GUARD_HALF_X * 1.55, BLADE_WIDTH_BASE * 0.62, GUARD_Z + dz, 0.012)
             for dz in (-GUARD_HALF_Z * 1.3, GUARD_HALF_Z * 1.3, GUARD_HALF_Z * 2.6)]
    rings[-1] = box_ring(GUARD_HALF_X * 0.9, BLADE_WIDTH_BASE * 0.52, GUARD_Z + GUARD_HALF_Z * 2.6, 0.008)
    bb.loft(bm, rings)
    parts.append(bb.new_object("guard_block", bm, mats["iron"], smooth=False))
    return parts


def build_grip(mats):
    """握り（革巻き、段が付く）と、鍔・柄頭へ続く金具。"""
    parts = []
    top = GUARD_Z - GUARD_HALF_Z * 1.3
    bm = bmesh.new()
    steps = GRIP_RIDGES * 4
    rings = []
    for i in range(steps + 1):
        s = i / steps
        z = top - (top - GRIP_BOTTOM) * s
        swell = 1.0 + 0.10 * math.sin(math.pi * s) + 0.05 * abs(math.sin(math.pi * GRIP_RIDGES * s))
        rings.append(round_ring(GRIP_RADIUS * 0.82 * swell, GRIP_RADIUS * swell, z))
    bb.loft(bm, rings)
    parts.append(bb.new_object("grip", bm, mats["leather"], smooth=True))
    # 握りの両端の金具。
    for z, k in ((top - 0.012, 1.18), (GRIP_BOTTOM + 0.012, 1.18)):
        bm = bmesh.new()
        rings = [round_ring(GRIP_RADIUS * 0.82 * k, GRIP_RADIUS * k, z + dz) for dz in (0.014, -0.014)]
        bb.loft(bm, rings)
        parts.append(bb.new_object("ferrule", bm, mats["iron"], smooth=True))
    # 柄頭：平たい円盤（車輪の形）と、下の小さな尻。
    zc = GRIP_BOTTOM - POMMEL_RADIUS * 0.92
    bm = bmesh.new()
    rings = []
    for dx, k in ((-1.0, 0.70), (-0.80, 0.97), (-0.35, 1.0), (0.35, 1.0), (0.80, 0.97), (1.0, 0.70)):
        x = POMMEL_HALF_X * dx
        rings.append([E(x, math.cos(a) * POMMEL_RADIUS * k, zc + math.sin(a) * POMMEL_RADIUS * k)
                      for a in (2.0 * math.pi * j / 32 for j in range(32))])
    bb.loft(bm, rings)
    parts.append(bb.new_object("pommel", bm, mats["iron"], smooth=True))
    bm = bmesh.new()
    rings = [round_ring(r, r, zc - POMMEL_RADIUS * 0.95 - dz) for dz, r in ((0.0, 0.030), (0.030, 0.026), (0.042, 0.010))]
    bb.loft(bm, rings)
    parts.append(bb.new_object("pommel_cap", bm, mats["iron"], smooth=True))
    return parts


def build(mats):
    """戻り値：(hilt, blade, blade_hot, 握りの中央から剣先までの長さ)。bb_katana.build と同じ形。"""
    z_blade = GUARD_Z + GUARD_HALF_Z * 1.3
    hilt = bb.join("KatanaHilt", build_guard(mats) + build_grip(mats))
    blade = build_blade("KatanaBlade", z_blade, mats["blade"], mats["blade_edge"])
    hot = build_blade("KatanaBladeHot", z_blade, mats["blade_hot"], mats["blade_hot_edge"])
    return hilt, blade, hot, TIP_Z
