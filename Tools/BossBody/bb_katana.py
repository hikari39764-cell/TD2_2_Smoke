"""刀（打刀）を作る。エンジンの座標で考える：原点＝握りの中央、+Z＝剣先、-Y＝刃、+Y＝棟、±X＝刀の平。
反りがあるので、握りの中央から剣先への直線が +Z になるように全体を回して仕上げる（コードは「剣の向き」の線の上に剣先を置く）。

ノード：
- KatanaHilt    ：はばき・切羽・鍔・柄（鮫皮・柄巻・目貫）・縁・柄頭
- KatanaBlade   ：刀身（材質槽 BladeSteel＝地・鎬地・棟、BladeEdge＝刃文から刃先）
- KatanaBladeHot：同じ形の、灼けた刀身（発光する材質。火の技・フェーズ 2 でコードが差し替える）
"""
import math

import bmesh
from mathutils import Matrix, Vector

import bb_common as bb

# ---- 形の値（m。体 4.0 m のボスが片手で持つ大きさ）----
GRIP_TO_GUARD = 0.19      # 握りの中央から鍔まで
BLADE_LENGTH = 2.08       # はばきの元から剣先まで（反りに沿わない直線の長さに近い）
SORI = 0.060              # 反り（弦からの最大の離れ）
WIDTH_BASE = 0.125        # 元幅（刃先から棟まで）。実物の比率より太い：遠くの遊びのカメラから刀が読めるように
WIDTH_YOKOTE = 0.092      # 先幅
THICK_BASE = 0.028        # 元重ね
THICK_YOKOTE = 0.018      # 先重ね
KISSAKI = 0.20            # 切先の長さ
HAMON_HEIGHT = 0.26       # 刃文の高さ（幅に対する割合）
HAMON_WAVE = 0.07         # 刃文のうねりの幅（幅に対する割合）
HAMON_COUNT = 26          # 刃文の山の数
TSUKA_LENGTH = 0.62       # 柄の長さ
TSUKA_HALF_Y = 0.055      # 柄の太さ（刃〜棟の方向の半分）
TSUKA_HALF_X = 0.038      # 柄の太さ（平の方向の半分）
TSUBA_RADIUS = 0.135      # 鍔の半径
TSUBA_THICK = 0.018
ITO_DIAMONDS = 12         # 柄巻の菱の数


def E(x, y, z):
    """エンジンの座標 → Blender の座標。"""
    return Vector((x, -z, y))


def blade_center(z):
    """刀身の棟の線（反り）。元の接線を +Z として、棟の側（+Y）へ反る。"""
    c = 4.0 * SORI / (BLADE_LENGTH * BLADE_LENGTH)
    return c * z * z


def blade_rings(z0, count=300):
    """刀身の断面の輪（8 点：棟の頂・棟角右・鎬右・刃文右・刃先・刃文左・鎬左・棟角左）。座標はエンジン。"""
    rings = []
    yokote = BLADE_LENGTH - KISSAKI
    for i in range(count + 1):
        u = i / count
        # 切先は輪を細かくする（丸みが出る）。
        z = BLADE_LENGTH * (1.0 - (1.0 - u) ** 1.35)
        k = min(1.0, z / yokote)
        width = WIDTH_BASE + (WIDTH_YOKOTE - WIDTH_BASE) * k
        thick = THICK_BASE + (THICK_YOKOTE - THICK_BASE) * k
        top = blade_center(z)
        if z > yokote:
            s = (z - yokote) / KISSAKI
            # ふくら：刃先が丸く棟へ上がる。重ねも剣先へ薄くなる。
            width = WIDTH_YOKOTE * math.sqrt(max(1e-4, 1.0 - s ** 1.8))
            thick = THICK_YOKOTE * (1.0 - 0.8 * s)
        wave = math.sin(z / BLADE_LENGTH * HAMON_COUNT * 2.0 * math.pi) * 0.6 + math.sin(z * 37.0 + 1.3) * 0.4
        hamon = HAMON_HEIGHT + HAMON_WAVE * wave
        shinogi_depth = 0.34
        def half(depth):  # 鎬から刃先へ、重ねがまっすぐ薄くなる
            return thick * 0.5 * (1.0 - depth) / (1.0 - shinogi_depth)
        hamon_depth = 1.0 - hamon
        pts = [
            (0.0, 0.0),
            (thick * 0.30, -width * 0.05),
            (thick * 0.50, -width * shinogi_depth),
            (half(hamon_depth), -width * hamon_depth),
            (0.0, -width),
            (-half(hamon_depth), -width * hamon_depth),
            (-thick * 0.50, -width * shinogi_depth),
            (-thick * 0.30, -width * 0.05),
        ]
        rings.append([Vector((x, top + y, z0 + z)) for x, y in pts])
    return rings


def strip(bm, rings, a, b, flip=False):
    """輪の a 番と b 番の点の間を、自前の頂点で帯にする（帯の境は折れ目、長さの方向はなめらか）。"""
    rows = [(bm.verts.new(E(*ring[a])), bm.verts.new(E(*ring[b]))) for ring in rings]
    for (p0, q0), (p1, q1) in zip(rows, rows[1:]):
        face = (p0, q0, q1, p1) if not flip else (p0, p1, q1, q0)
        try:
            bm.faces.new(face)
        except ValueError:
            pass


def build_blade(name, z0, steel, edge):
    bm_steel, bm_edge = bmesh.new(), bmesh.new()
    rings = blade_rings(z0)
    for a, b in ((0, 1), (1, 2), (2, 3), (5, 6), (6, 7), (7, 0)):
        strip(bm_steel, rings, a, b)
    for a, b in ((3, 4), (4, 5)):
        strip(bm_edge, rings, a, b)
    # 元の切り口（はばきの中に隠れる）にふたをする。
    base = [bm_steel.verts.new(E(*p)) for p in rings[0]]
    bm_steel.faces.new(base)
    o1 = bb.new_object(name + "_steel", bm_steel, steel, smooth=True)
    o2 = bb.new_object(name + "_edge", bm_edge, edge, smooth=True)
    return bb.join(name, [o1, o2])


def oval_ring(half_x, half_y, z, count=40, lobes=0, lobe_depth=0.0, center_y=0.0):
    ring = []
    for i in range(count):
        a = 2.0 * math.pi * i / count
        r = 1.0 + (lobe_depth * math.cos(lobes * a) if lobes else 0.0)
        ring.append(E(math.cos(a) * half_x * r, center_y + math.sin(a) * half_y * r, z))
    return ring


def build_tsuba(z, iron, gold):
    """木瓜形（4 つの丸み）の鍔。縁（耳）を金で一段高くする。"""
    parts = []
    bm = bmesh.new()
    half = TSUBA_THICK * 0.5
    r = TSUBA_RADIUS
    profile = [(0.20, half * 0.85), (0.55, half), (0.92, half), (0.92, -half), (0.55, -half), (0.20, -half * 0.85)]
    rings = []
    for scale, dz in profile:
        rings.append(oval_ring(r * scale, r * scale * 1.04, z + dz, count=64, lobes=4, lobe_depth=0.045))
    bb.loft(bm, rings, cap_start=True, cap_end=True)
    parts.append(bb.new_object("tsuba_plate", bm, iron, smooth=False))
    bm = bmesh.new()
    rim = [(0.90, half * 0.6), (0.94, half * 1.5), (1.00, half * 1.5), (1.03, 0.0), (1.00, -half * 1.5),
           (0.94, -half * 1.5), (0.90, -half * 0.6)]
    rings = [oval_ring(r * s, r * s * 1.04, z + dz, count=64, lobes=4, lobe_depth=0.045) for s, dz in rim]
    bb.loft(bm, rings, cap_start=False, cap_end=False)
    parts.append(bb.new_object("tsuba_rim", bm, gold, smooth=True))
    return parts


def build_hilt(z_guard, mats):
    """鍔の位置 z_guard から下（-Z）へ柄を作る。"""
    parts = []
    # はばき（鍔の上、刀身の元を包む金具）
    bm = bmesh.new()
    rings = []
    for dz, sx, sy in ((0.0, 1.9, 1.10), (0.012, 1.9, 1.10), (0.075, 1.5, 1.04), (0.082, 1.2, 1.01)):
        z = z_guard + TSUBA_THICK * 0.5 + 0.006 + dz
        w = WIDTH_BASE * sy
        rings.append([E(0.0, 0.004, z), E(THICK_BASE * 0.5 * sx, -w * 0.04, z), E(THICK_BASE * 0.5 * sx, -w * 0.55, z),
                      E(0.0, -w, z), E(-THICK_BASE * 0.5 * sx, -w * 0.55, z), E(-THICK_BASE * 0.5 * sx, -w * 0.04, z)])
    bb.loft(bm, rings)
    parts.append(bb.new_object("habaki", bm, mats["gold"], smooth=False))
    center_y = -WIDTH_BASE * 0.5
    # 切羽（鍔の両側の薄い金具）
    for dz in (TSUBA_THICK * 0.5 + 0.003, -TSUBA_THICK * 0.5 - 0.003):
        bm = bmesh.new()
        rings = [oval_ring(TSUKA_HALF_X * 1.35, TSUKA_HALF_Y * 1.25, z_guard + dz + s * 0.003, center_y=center_y)
                 for s in (-1, 1)]
        bb.loft(bm, rings)
        parts.append(bb.new_object("seppa", bm, mats["gold"], smooth=False))
    parts += build_tsuba(z_guard, mats["iron"], mats["gold"])
    for part in parts[-2:]:
        part.location = E(0.0, center_y, 0.0)
    top = z_guard - TSUBA_THICK * 0.5 - 0.006
    bottom = top - TSUKA_LENGTH

    def waist(s):  # 柄の中ほどを少し絞る（立鼓）
        return 1.0 - 0.07 * math.sin(math.pi * s)

    # 縁（鍔の下の金具）と柄頭
    bm = bmesh.new()
    rings = [oval_ring(TSUKA_HALF_X * k, TSUKA_HALF_Y * k, top - dz, center_y=center_y)
             for dz, k in ((0.0, 1.10), (0.05, 1.10), (0.056, 1.02))]
    bb.loft(bm, rings)
    parts.append(bb.new_object("fuchi", bm, mats["iron"], smooth=True))
    bm = bmesh.new()
    rings = [oval_ring(TSUKA_HALF_X * k, TSUKA_HALF_Y * k, bottom + dz, center_y=center_y)
             for dz, k in ((0.056, 1.02), (0.05, 1.12), (0.012, 1.12), (0.0, 0.9), (-0.012, 0.55))]
    bb.loft(bm, rings)
    parts.append(bb.new_object("kashira", bm, mats["iron"], smooth=True))
    # 鮫皮（柄の芯。柄巻の菱から見える）
    bm = bmesh.new()
    steps = 24
    rings = [oval_ring(TSUKA_HALF_X * waist(i / steps), TSUKA_HALF_Y * waist(i / steps),
                       top - 0.05 - (TSUKA_LENGTH - 0.10) * i / steps, center_y=center_y) for i in range(steps + 1)]
    bb.loft(bm, rings)
    parts.append(bb.new_object("same", bm, mats["same"], smooth=True))
    # 柄巻：芯より少し外の面。両の平に菱の穴を開ける。菱の縁がまっすぐ出るよう、縁の上に頂点を置いて帯を張る。
    bm = bmesh.new()
    length = TSUKA_LENGTH - 0.10
    pitch = length / ITO_DIAMONDS
    half_angle = math.radians(64.0)
    columns = 8

    def surface(angle, depth):
        s = max(0.0, min(1.0, depth / length))
        return E(math.cos(angle) * (TSUKA_HALF_X + 0.006) * waist(s),
                 center_y + math.sin(angle) * (TSUKA_HALF_Y + 0.006) * waist(s), top - 0.05 - depth)

    def quad(a0, a1, d00, d01, d10, d11):
        """角度 a0→a1、深さ（柄の下向き）d00/d01（a0 の側の始めと終わり）、d10/d11（a1 の側）。"""
        if abs(d01 - d00) < 1e-6 and abs(d11 - d10) < 1e-6:
            return
        verts = [bm.verts.new(surface(a0, d00)), bm.verts.new(surface(a1, d10)), bm.verts.new(surface(a1, d11)),
                 bm.verts.new(surface(a0, d01))]
        bm.faces.new(verts)

    for k in range(ITO_DIAMONDS):
        d0, d1 = k * pitch, (k + 1) * pitch
        mid = (d0 + d1) * 0.5
        for face_center in (0.0, math.pi):
            for c in range(-columns, columns):
                a0 = face_center + half_angle * c / columns
                a1 = face_center + half_angle * (c + 1) / columns
                open0 = (1.0 - abs(c) / columns) * pitch * 0.42      # 菱の半分の高さ（その角度での）
                open1 = (1.0 - abs(c + 1) / columns) * pitch * 0.42
                quad(a0, a1, d0, mid - open0, d0, mid - open1)
                quad(a0, a1, mid + open0, d1, mid + open1, d1)
        # 棟の側と刃の側（平と平の間）は、切れ目なしで巻く。
        for start in (half_angle, math.pi + half_angle):
            for c in range(6):
                a0 = start + (math.pi - 2.0 * half_angle) * c / 6
                a1 = start + (math.pi - 2.0 * half_angle) * (c + 1) / 6
                quad(a0, a1, d0, d1, d0, d1)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    ito = bb.new_object("ito", bm, mats["ito"], smooth=True)
    solid = ito.modifiers.new("Solid", "SOLIDIFY")
    solid.thickness = 0.006
    solid.offset = -1.0
    bb.apply_modifiers(ito)
    parts.append(ito)
    # 目貫（柄巻の下の金の飾り。両の平に 1 つずつ、位置をずらす）
    for sign, s in ((1.0, 0.36), (-1.0, 0.64)):
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=2, radius=1.0,
                                   matrix=Matrix.Translation(E(sign * (TSUKA_HALF_X + 0.004), center_y,
                                                               top - 0.05 - length * s)) @
                                   Matrix.Diagonal((0.009, 0.060, 0.016, 1.0)))
        parts.append(bb.new_object("menuki", bm, mats["gold"], smooth=True))
    return parts


def build(mats):
    """戻り値：(hilt, blade, blade_hot, 握りの中央から剣先までの長さ)。"""
    z_guard = GRIP_TO_GUARD
    z_blade = z_guard + TSUBA_THICK * 0.5 + 0.006
    hilt_parts = build_hilt(z_guard, mats)
    hilt = bb.join("KatanaHilt", hilt_parts)
    blade = build_blade("KatanaBlade", z_blade, mats["blade"], mats["blade_edge"])
    hot = build_blade("KatanaBladeHot", z_blade, mats["blade_hot"], mats["blade_hot_edge"])
    # 刃〜棟の中心を軸に乗せ（柄の中心が原点を通る）、剣先が +Z の線に乗るように回す。
    tip_y = blade_center(BLADE_LENGTH) + WIDTH_BASE * 0.5
    tip_z = z_blade + BLADE_LENGTH
    tilt = math.atan2(tip_y, tip_z)
    # エンジンの X 軸まわり＝Blender の X 軸まわり。剣先を +Y から +Z の線へ下ろす。
    fix = Matrix.Rotation(tilt, 4, "X") @ Matrix.Translation(E(0.0, WIDTH_BASE * 0.5, 0.0))
    for obj in (hilt, blade, hot):
        obj.data.transform(fix @ obj.matrix_world)
        obj.matrix_world = Matrix.Identity(4)
    return hilt, blade, hot, math.hypot(tip_y, tip_z)
