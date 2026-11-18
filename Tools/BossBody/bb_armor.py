"""具足（兜・面頬・胴・大袖・草摺）を作る。体は高さ 4.0 m・半径 0.8 m のカプセルで、胴の中は煙。脚は作らない。
板（小札の段）は 1 段ずつ下へ重なり、段と段を紅の糸（素懸縅）でつなぐ。板は片面の格子で作り、最後に厚みを付ける。

部品の座標（エンジン：+Y 上、+Z 前、+X ボスの左）：
- Do            ：原点＝胴の中心（ボスの足元から DO_CENTER_Y）。向きはボスと同じ。
- Kabuto        ：原点＝頭の中心。向きはボスと同じ。
- EyesMist・EyesFire：Kabuto と同じ座標（目の光。霞の色と火の色。コードが差し替える）。
- Sode          ：原点＝上の縁の中央（肩に吊る所）。+Z＝外向き、-Y へ垂れる。左右で同じ形。
- KusazuriPanel ：原点＝上の縁の中央（腰に吊る所）。+Z＝外向き、-Y へ垂れる。腰の周りに同じ形を並べる。
"""
import math

import bmesh
from mathutils import Matrix, Vector

import bb_common as bb

DO_CENTER_Y = 2.50
DO_WAIST_Y = 1.95
DO_SIDE_TOP_Y = 2.86
HEAD_CENTER_Y = 3.46
PLATE_THICK = 0.016


def E(x, y, z):
    return Vector((x, -z, y))


def superpoint(angle, rx, rz, power=2.6):
    """角の丸い四角の輪郭。angle 0＝前（+Z）、π/2＝左（+X）。"""
    s, c = math.sin(angle), math.cos(angle)
    e = 2.0 / power
    return math.copysign(abs(s) ** e, s) * rx, math.copysign(abs(c) ** e, c) * rz


class Parts:
    """材質ごとの bmesh をためて、最後に 1 つのノードにする。"""

    def __init__(self, mats, plate_keys=("lacquer", "lacquer_red", "gold")):
        self.mats = mats
        self.sheets = {}      # 厚みを付ける板（片面の格子）
        self.solids = {}      # そのままの立体
        self.plate_keys = plate_keys

    def sheet(self, key):
        return self.sheets.setdefault(key, bmesh.new())

    def solid(self, key):
        return self.solids.setdefault(key, bmesh.new())

    def grid(self, key, fn, nu, nv, closed_u=False):
        """fn(u, v)（0〜1）→ エンジンの点。板の表の面を張る。"""
        bm = self.sheet(key)
        cols = nu if closed_u else nu + 1
        rows = [[bm.verts.new(E(*fn(i / nu, j / nv))) for i in range(cols)] for j in range(nv + 1)]
        for j in range(nv):
            for i in range(nu):
                i1 = (i + 1) % cols
                bm.faces.new((rows[j][i], rows[j][i1], rows[j + 1][i1], rows[j + 1][i]))

    def box(self, key, size, center, x_axis=(1, 0, 0), y_axis=(0, 1, 0)):
        """エンジンの座標の箱。x_axis・y_axis で向きを決める。"""
        x = Vector(x_axis).normalized()
        y = Vector(y_axis)
        y = (y - x * y.dot(x)).normalized()
        z = x.cross(y)
        m = Matrix.Identity(4)
        for i, axis in enumerate((x, y, z)):
            m[0][i], m[1][i], m[2][i] = axis.x, axis.y, axis.z
        m[0][3], m[1][3], m[2][3] = center
        conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        bmesh.ops.create_cube(self.solid(key), size=1.0, matrix=conv @ m @ Matrix.Diagonal((*size, 1.0)))

    def ball(self, key, radius, center, squash=(1.0, 1.0, 1.0), subdivisions=1):
        conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        bmesh.ops.create_icosphere(self.solid(key), subdivisions=subdivisions, radius=radius,
                                   matrix=conv @ Matrix.Translation(center) @ Matrix.Diagonal((*squash, 1.0)))

    def cone(self, key, radius1, radius2, depth, center, axis=(0, 0, 1), segments=6):
        """center を中心に、axis の向きへ細くなる円錐（radius1 が根元）。"""
        z = Vector(axis).normalized()
        x = Vector((0, 1, 0)).cross(z)
        x = x.normalized() if x.length > 1e-4 else Vector((1, 0, 0))
        y = z.cross(x)
        m = Matrix.Identity(4)
        for i, a in enumerate((x, y, z)):
            m[0][i], m[1][i], m[2][i] = a.x, a.y, a.z
        m[0][3], m[1][3], m[2][3] = center
        conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        bmesh.ops.create_cone(self.solid(key), cap_ends=True, segments=segments, radius1=radius1, radius2=radius2,
                              depth=depth, matrix=conv @ m)

    def finish(self, name, thickness=PLATE_THICK):
        objects = []
        for key, bm in self.sheets.items():
            obj = bb.new_object(name + "_sheet_" + key, bm, self.mats[key], smooth=True)
            solid = obj.modifiers.new("Solid", "SOLIDIFY")
            solid.thickness = thickness
            solid.offset = -1.0
            objects.append(obj)
        for key, bm in self.solids.items():
            objects.append(bb.new_object(name + "_solid_" + key, bm, self.mats[key], smooth=True))
        for obj in objects:
            bb.apply_modifiers(obj)
        node = bb.join(name, objects)
        bb.finalize(node)
        return node


def hot_copy(node, mats):
    """灼けた鎧（フェーズ 2 でコードが差し替える。名前は元の名前 + Hot）：同じ形で、紅の糸（Silk）と鎧の中の闇（Void）を
    熾火の材質に替える。板・金具・面頬の色はそのまま（光るのは継ぎ目と隙間だけ）。c-857ae3。"""
    hot = node.copy()
    hot.data = node.data.copy()
    hot.name = node.name + "Hot"
    hot.data.name = node.data.name + "Hot"
    node.users_collection[0].objects.link(hot)
    swap = {"Silk": mats["ember_silk"], "Void": mats["ember_void"]}
    for slot, material in enumerate(hot.data.materials):
        if material is not None and material.name in swap:
            hot.data.materials[slot] = swap[material.name]
    return hot


def cord_between(parts, p0, p1, normal, width=0.016, key="silk"):
    """2 点の間の糸（平たい箱）。normal は板の外向き。"""
    p0, p1, normal = Vector(p0), Vector(p1), Vector(normal).normalized()
    along = p1 - p0
    length = along.length
    if length < 1e-5:
        return
    mid = (p0 + p1) * 0.5 + normal * 0.006
    side = along.normalized().cross(normal)
    parts.box(key, (width, length + 0.01, 0.008), mid, x_axis=side, y_axis=along)


# ---------------------------------------------------------------- 板の段（袖・草摺）
def hanging_panel(mats, name, lames, width_top, width_bottom, lame_height, curve_radius, lace_pairs, top_plate_gold=False,
                  suspend=0.0):
    """上の縁の中央が原点で、-Y へ垂れる板の段。+Z が外向き。段は下へ行くほど少し外へ出る。"""
    parts = Parts(mats)
    overlap = 0.018
    step_out = 0.010

    def lame_point(j, u, v):
        # v：0＝段の上の縁、1＝下の縁。上の縁は前の段の下にもぐる。
        width = width_top + (width_bottom - width_top) * (j + v) / lames
        x = (u - 0.5) * width
        phi = x / curve_radius
        top = -suspend - j * (lame_height - overlap)
        y = top - v * lame_height
        out = j * step_out + 0.012 * v + (0.010 * math.sin(v * math.pi))
        radius = curve_radius + out
        return math.sin(phi) * radius, y, math.cos(phi) * radius - curve_radius

    for j in range(lames):
        key = "lacquer"
        parts.grid(key, lambda u, v, j=j: lame_point(j, u, v), 14, 3)
        # 段の上の縁の金の覆輪（いちばん上の板だけ）と、下の縁の細い金の筋（いちばん下の板）
        if j == 0 and top_plate_gold:
            parts.grid("gold", lambda u, v: tuple(Vector(lame_point(0, u, v * 0.30)) + Vector((0, 0.004, 0.006))), 14, 1)
        if j == lames - 1:
            parts.grid("gold", lambda u, v, j=j: tuple(Vector(lame_point(j, u, 0.84 + v * 0.18)) + Vector((0, 0, 0.006))),
                       14, 1)
        # 鋲
        for k in range(5):
            u = (k + 0.5) / 5.0
            p = Vector(lame_point(j, u, 0.16))
            parts.ball("gold", 0.011, tuple(p + Vector((0, 0, 0.010))))
    # 素懸縅：2 本ずつの紅の糸が、段から次の段へ渡る
    for j in range(lames - 1):
        for k in range(lace_pairs):
            center = (k + 0.5) / lace_pairs
            for du in (-0.018, 0.018):
                u = center + du
                p0 = Vector(lame_point(j, u, 0.52))
                p1 = Vector(lame_point(j + 1, u, 0.42))
                cord_between(parts, p0 + Vector((0, 0, 0.012)), p1 + Vector((0, 0, 0.012)), (0, 0, 1))
            # 糸の結び目（横に渡る小さな玉）
            knot = Vector(lame_point(j, center, 0.50)) + Vector((0, 0, 0.016))
            parts.box("silk", (0.060, 0.016, 0.010), tuple(knot))
    if suspend > 0.0:
        # 吊る糸（腰・肩から最初の板まで）
        for k in range(lace_pairs):
            u = (k + 0.5) / lace_pairs
            p1 = Vector(lame_point(0, u, 0.30)) + Vector((0, 0, 0.012))
            p0 = Vector((p1.x * 0.94, 0.0, p1.z - 0.02))
            cord_between(parts, p0, p1, (0, 0, 1), width=0.020)
    return parts.finish(name)


def build_sode(mats):
    return hanging_panel(mats, "Sode", lames=6, width_top=0.72, width_bottom=0.78, lame_height=0.180, curve_radius=0.80,
                         lace_pairs=5, top_plate_gold=True)


def build_kusazuri(mats):
    return hanging_panel(mats, "KusazuriPanel", lames=5, width_top=0.40, width_bottom=0.52, lame_height=0.215,
                         curve_radius=0.70, lace_pairs=3, suspend=0.07)


# ---------------------------------------------------------------- 胴
def torso_radii(y):
    """胴の輪郭（ボスの足元からの高さ y での、横と前後の半径）。"""
    table = ((1.90, 0.53, 0.46), (1.95, 0.54, 0.47), (2.25, 0.60, 0.52), (2.60, 0.70, 0.60), (2.86, 0.71, 0.57),
             (3.10, 0.63, 0.50))
    for (y0, a0, b0), (y1, a1, b1) in zip(table, table[1:]):
        if y <= y1:
            t = max(0.0, min(1.0, (y - y0) / (y1 - y0)))
            t = t * t * (3 - 2 * t)
            return a0 + (a1 - a0) * t, b0 + (b1 - b0) * t
    return table[-1][1], table[-1][2]


def torso_point(angle, y, out=0.0):
    rx, rz = torso_radii(y)
    x, z = superpoint(angle, rx + out, rz + out)
    # 前の中央の稜（鳩胸）：胸の高さで少し前へ張り出す
    front = max(0.0, math.cos(angle)) ** 6
    chest = math.exp(-((y - 2.55) / 0.35) ** 2)
    z += 0.05 * front * chest
    return x, y - DO_CENTER_Y, z


def build_do(mats):
    parts = Parts(mats)
    lames = 6
    height = (DO_SIDE_TOP_Y - DO_WAIST_Y) / lames
    overlap = 0.02
    for j in range(lames):
        top = DO_SIDE_TOP_Y - j * height
        def fn(u, v, top=top):
            y = top + overlap - v * (height + overlap)
            return torso_point(u * 2.0 * math.pi, y, out=0.004 + 0.016 * v)
        parts.grid("lacquer", fn, 72, 3, closed_u=True)
        for k in range(24):
            a = (k + 0.5) / 24 * 2.0 * math.pi
            p = Vector(torso_point(a, top - 0.035, out=0.016))
            parts.ball("gold", 0.012, tuple(p))
    # 胸板（前）と押付の板（後ろ）：脇の上の縁から首の周りへ立ち上がる
    def upper(center, half, top_y, scoop, scoop_width):
        def fn(u, v):
            a = center + (u - 0.5) * 2.0 * half
            edge = 1.0 - (abs(u - 0.5) * 2.0) ** 3 * 0.35            # 端は少し低い
            neck = scoop * math.exp(-(((u - 0.5) * 2.0 * half) / scoop_width) ** 2)
            y_top = DO_SIDE_TOP_Y + (top_y - DO_SIDE_TOP_Y) * edge - neck
            y = DO_SIDE_TOP_Y - 0.02 + (y_top - DO_SIDE_TOP_Y + 0.02) * v
            return torso_point(a, y, out=0.022 - 0.006 * v)
        parts.grid("lacquer", fn, 28, 5)
        # 上の縁の金の覆輪
        def rim(u, v):
            p = Vector(fn(u, 0.86 + 0.16 * v))
            a = center + (u - 0.5) * 2.0 * half
            return tuple(p + Vector((math.sin(a) * 0.007, 0.0, math.cos(a) * 0.007)))
        parts.grid("gold", rim, 28, 1)
    upper(0.0, math.radians(58.0), 3.06, 0.17, 0.30)
    upper(math.pi, math.radians(62.0), 3.12, 0.07, 0.34)
    # 肩上（肩を越えて前後をつなぐ帯）
    for side in (-1.0, 1.0):
        def strap(u, v, side=side):
            t = u * math.pi
            x = side * (0.34 + 0.13 * v)
            y = 3.04 + 0.11 * math.sin(t) - DO_CENTER_Y
            z = 0.44 * math.cos(t) - 0.02
            return x, y, z
        parts.grid("lacquer", strap, 12, 2)
        # 胸の鋲と、肩上を留める紅の紐
        for z_sign, y in ((1.0, 2.95), (-1.0, 3.00)):
            a = (0.0 if z_sign > 0 else math.pi) + side * z_sign * math.radians(38.0)
            p = Vector(torso_point(a, y, out=0.03))
            parts.box("silk", (0.05, 0.11, 0.02), tuple(p), y_axis=(0, 1, 0))
            parts.ball("gold", 0.02, tuple(p + Vector((0, 0.03, 0.012 * z_sign))))
    # 胸の金の紋（丸に輪）
    chest = Vector(torso_point(0.0, 2.62, out=0.02))
    for radius, lift, key in ((0.105, 0.0, "gold"), (0.080, 0.006, "lacquer"), (0.040, 0.012, "gold")):
        bm = parts.solid(key)
        conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        bmesh.ops.create_cone(bm, cap_ends=True, segments=32, radius1=radius, radius2=radius * 0.94, depth=0.012,
                              matrix=conv @ Matrix.Translation(chest + Vector((0, 0, lift))))
    # 上帯（腰の紅の帯）と前の結び
    def belt(u, v):
        return torso_point(u * 2.0 * math.pi, 2.06 - 0.12 * v, out=0.03 + 0.012 * math.sin(v * math.pi))
    parts.grid("silk", belt, 72, 2, closed_u=True)
    knot = Vector(torso_point(0.0, 2.0, out=0.06))
    parts.box("silk", (0.16, 0.10, 0.06), tuple(knot))
    for sx in (-1, 1):
        parts.box("silk", (0.05, 0.34, 0.03), tuple(knot + Vector((sx * 0.07, -0.20, 0.0))),
                  x_axis=(1, 0.18 * sx, 0), y_axis=(-0.18 * sx, 1, 0))
    # 総角（背の紅の飾り結び）と垂れる房
    back = Vector(torso_point(math.pi, 2.74, out=0.05))
    parts.box("silk", (0.10, 0.10, 0.05), tuple(back))
    for sx in (-1, 1):
        parts.box("silk", (0.26, 0.07, 0.04), tuple(back + Vector((sx * 0.16, 0.02, 0.0))), x_axis=(1, 0.22 * sx, 0))
        parts.box("silk", (0.045, 0.42, 0.035), tuple(back + Vector((sx * 0.06, -0.26, -0.01))),
                  x_axis=(1, 0.10 * sx, 0), y_axis=(-0.10 * sx, 1, 0))
        parts.box("silk", (0.07, 0.12, 0.05), tuple(back + Vector((sx * 0.083, -0.50, -0.01))))
    # 中の闇（胴の中が空へ抜けて見えないように。煙はこの前に描かれる）
    void = parts.solid("void")
    rings = []
    for y in (1.93, 2.10, 2.50, 2.84, 2.98, 3.02):
        shrink = -0.035 if y < 2.9 else -0.12 - (y - 2.9) * 1.6
        rings.append([E(*torso_point(2.0 * math.pi * i / 40, y, out=shrink)) for i in range(40)])
    bb.loft(void, rings)
    return parts.finish("Do")


# ---------------------------------------------------------------- 兜・面頬
def build_kabuto(mats):
    parts = Parts(mats)
    ribs = 28
    rx, rz, top = 0.345, 0.385, 0.40
    rim_y = 0.055

    def bowl(u, v):
        a = u * 2.0 * math.pi
        t = v * math.pi * 0.5
        ridge = max(0.0, math.cos(a * ribs)) ** 10 * 0.014 * math.cos(t) ** 0.5
        r = math.cos(t) ** 0.85
        return math.sin(a) * (rx * r + ridge), rim_y + math.sin(t) ** 1.1 * top, math.cos(a) * (rz * r + ridge)
    parts.grid("lacquer", bowl, ribs * 6, 12, closed_u=True)
    # 腰巻（鉢の下の帯）と天辺の金具
    def koshimaki(u, v):
        a = u * 2.0 * math.pi
        return math.sin(a) * (rx + 0.012), rim_y + 0.035 - 0.06 * v, math.cos(a) * (rz + 0.012)
    parts.grid("gold", koshimaki, 64, 1, closed_u=True)
    parts.ball("gold", 0.07, (0.0, rim_y + top - 0.006, 0.0), squash=(1.0, 0.45, 1.0), subdivisions=2)
    parts.ball("gold", 0.03, (0.0, rim_y + top + 0.03, 0.0), subdivisions=2)
    # 眉庇（前のひさし）
    def visor(u, v):
        a = (u - 0.5) * math.radians(150.0)
        out = 0.15 * v * math.cos(a * 0.6) ** 1.5
        drop = 0.07 * v ** 1.4
        return math.sin(a) * (rx + 0.01 + out), rim_y + 0.02 - drop, math.cos(a) * (rz + 0.01 + out)
    parts.grid("lacquer", visor, 24, 4)
    parts.grid("gold", lambda u, v: tuple(Vector(visor(u, 0.86 + 0.16 * v)) + Vector((0, 0.008, 0.0))), 24, 1)
    # 錣（首の周りへ広がる板の段）
    lames = 4
    span = math.radians(118.0)          # 後ろの中央から左右へ
    for j in range(lames):
        def fn(u, v, j=j):
            a = math.pi + (u - 0.5) * 2.0 * span
            r0 = 0.40 + 0.090 * j
            r1 = r0 + 0.115
            y0 = rim_y - 0.005 - 0.085 * j
            r = r0 + (r1 - r0) * v
            y = y0 - 0.115 * v + 0.012 * math.sin(v * math.pi)
            return math.sin(a) * r * 0.94, y, math.cos(a) * r * 1.02 - 0.02
        parts.grid("lacquer", fn, 40, 3)
        if j == lames - 1:
            parts.grid("gold", lambda u, v, fn=fn: tuple(Vector(fn(u, 0.84 + 0.18 * v)) + Vector((0, 0.008, 0))), 40, 1)
        if j < lames - 1:
            for k in range(11):
                u = (k + 0.5) / 11
                for du in (-0.008, 0.008):
                    p0 = Vector(fn(u + du, 0.55))
                    def nxt(u2, v2, j=j):
                        a = math.pi + (u2 - 0.5) * 2.0 * span
                        r0 = 0.40 + 0.090 * (j + 1)
                        r = r0 + 0.115 * v2
                        y = rim_y - 0.005 - 0.085 * (j + 1) - 0.115 * v2 + 0.012 * math.sin(v2 * math.pi)
                        return math.sin(a) * r * 0.94, y, math.cos(a) * r * 1.02 - 0.02
                    p1 = Vector(nxt(u + du, 0.40))
                    a = math.pi + (u - 0.5) * 2.0 * span
                    normal = Vector((math.sin(a), 0.75, math.cos(a)))
                    cord_between(parts, p0 + normal * 0.01, p1 + normal * 0.01, normal, width=0.014)
    # 吹返（錣の前の端を外へ折り返した板）と金の紋
    for side in (-1.0, 1.0):
        a = math.pi + side * span
        base = Vector((math.sin(a) * 0.44, rim_y - 0.03, math.cos(a) * 0.46 - 0.02))
        out = Vector((side, 0.0, 0.35)).normalized()

        def wing(u, v, base=base, out=out, side=side):
            p = base + out * (0.02 + 0.20 * u) + Vector((0, 0.10 - 0.24 * v, 0.0))
            p += Vector((0.0, 0.0, 0.05 * math.sin(u * math.pi * 0.5)))
            round_off = 1.0 - 0.25 * (u ** 2) * (abs(v - 0.5) * 2.0) ** 2
            return tuple(base + (p - base) * round_off)
        parts.grid("lacquer", wing, 6, 6)
        center = Vector(wing(0.55, 0.5)) + Vector((0.0, 0.0, 0.022))
        parts.ball("gold", 0.055, tuple(center), squash=(1.0, 1.0, 0.25), subdivisions=2)
    # 前立（三日月）。兜より大きい金の月で、遠くからの輪郭を決める。両の先が上を向く。
    moon = parts.solid("gold")
    outer_r, inner_r, shift = 0.50, 0.465, 0.115
    segments = 64
    tilt = math.radians(-10.0)
    center_y, center_z = rim_y + 0.60, rz + 0.07
    rows = []
    for i in range(segments + 1):
        a = math.radians(168.0 + 204.0 * i / segments)       # 左上の先 → 下 → 右上の先
        ox, oy = math.cos(a) * outer_r, math.sin(a) * outer_r
        ix, iy = math.cos(a) * inner_r, math.sin(a) * inner_r + shift
        # 先を尖らせる：両端で内の弧を外の弧へ寄せる
        k = 1.0 - abs(2.0 * i / segments - 1.0) ** 5
        ix, iy = ox + (ix - ox) * k, oy + (iy - oy) * k
        row = []
        for z_off in (0.010, -0.010):
            for x, y in ((ox, oy), (ix, iy)):
                yy = center_y + y * math.cos(tilt) - z_off * math.sin(tilt)
                zz = center_z + y * math.sin(tilt) + z_off * math.cos(tilt)
                row.append(moon.verts.new(E(x, yy, zz)))
        rows.append(row)      # 表の外・表の内・裏の外・裏の内
    for r0, r1 in zip(rows, rows[1:]):
        for quad in ((r0[0], r1[0], r1[1], r0[1]), (r0[2], r0[3], r1[3], r1[2]), (r0[0], r0[2], r1[2], r1[0]),
                     (r0[1], r1[1], r1[3], r0[3])):
            try:
                moon.faces.new(quad)
            except ValueError:
                pass
    # 前立の台（眉庇の上の金の板）
    parts.box("gold", (0.16, 0.12, 0.03), (0.0, rim_y + 0.20, rz - 0.02), y_axis=(0, 1, -0.25))
    # 面頬（朱漆の頬当て）：目の下から顎まで
    def mask(u, v):
        a = (u - 0.5) * math.radians(170.0)
        y = -0.015 - 0.30 * v
        r = 0.275 - 0.10 * v ** 1.6
        chin = 0.07 * math.exp(-((v - 0.85) / 0.22) ** 2) * math.cos(a) ** 2
        cheek = 0.03 * math.exp(-((v - 0.25) / 0.2) ** 2) * math.sin(abs(a) * 1.4) ** 2
        return math.sin(a) * (r + cheek) * 0.98, y, math.cos(a) * (r + chin + cheek) + 0.05
    parts.grid("lacquer_red", mask, 28, 10)
    # 鼻・口・しわ・歯
    parts.cone("lacquer_red", 0.062, 0.020, 0.17, (0.0, -0.075, 0.365), axis=(0, -0.55, 1), segments=5)
    parts.box("lacquer_red", (0.060, 0.13, 0.05), (0.0, 0.035, 0.300), y_axis=(0, 1, -0.15))      # 鼻筋
    for side in (-1.0, 1.0):
        # 怒った眉（内の端が下がる）と、白いひげ、下の牙
        parts.box("lacquer_red", (0.17, 0.040, 0.06), (side * 0.118, 0.108, 0.268), x_axis=(1, 0.30 * side, -0.25 * side))
        parts.cone("same", 0.026, 0.004, 0.24, (side * 0.115, -0.185, 0.335), axis=(side * 1.0, -0.55, -0.12))
        parts.cone("same", 0.014, 0.002, 0.07, (side * 0.062, -0.212, 0.345), axis=(0, 1, 0.1), segments=5)
    parts.box("void", (0.19, 0.036, 0.05), (0.0, -0.215, 0.315))
    for k in range(-3, 4):
        parts.box("gold", (0.020, 0.030, 0.02), (k * 0.026, -0.205, 0.326))
    for side in (-1.0, 1.0):
        for row, (y, length) in enumerate(((-0.10, 0.13), (-0.15, 0.11))):
            a = side * math.radians(38.0 + row * 4.0)
            p = Vector((math.sin(a) * 0.268, y, math.cos(a) * 0.268 + 0.062))
            parts.box("lacquer_red", (length, 0.022, 0.03), tuple(p), x_axis=(math.cos(a), -0.45 * side, -math.sin(a)))
    # 垂（喉を守る小さな板の段）
    for j in range(3):
        def tare(u, v, j=j):
            a = (u - 0.5) * math.radians(120.0 + 12.0 * j)
            r = 0.215 + 0.045 * j + 0.05 * v
            return math.sin(a) * r, -0.315 - 0.075 * j - 0.095 * v, math.cos(a) * r + 0.05
        parts.grid("lacquer", tare, 16, 2)
        if j < 2:
            for k in range(5):
                u = (k + 0.5) / 5
                a = (u - 0.5) * math.radians(120.0)
                normal = Vector((math.sin(a), 0.3, math.cos(a)))
                p0 = Vector(tare(u, 0.6)) + normal * 0.012
                p1 = Vector((math.sin(a) * (0.26 + 0.045 * j), -0.315 - 0.075 * (j + 1) - 0.03,
                             math.cos(a) * (0.26 + 0.045 * j) + 0.05)) + normal * 0.012
                cord_between(parts, p0, p1, normal, width=0.013)
    # 兜の緒（顎の下で結ぶ紅の紐）
    for side in (-1.0, 1.0):
        cord_between(parts, (side * 0.30, 0.02, 0.10), (side * 0.10, -0.33, 0.24), (side, 0, 0.6), width=0.022)
    parts.box("silk", (0.12, 0.05, 0.05), (0.0, -0.335, 0.265))
    # 頭の中の闇（目の奥）
    parts.ball("void", 0.285, (0.0, 0.03, -0.01), squash=(1.0, 1.08, 1.05), subdivisions=3)
    return parts.finish("Kabuto")


def build_eyes(mats, name, key):
    """面頬と眉庇の間の闇に浮く、つり上がった 2 つの目の光。"""
    parts = Parts(mats)
    for side in (-1.0, 1.0):
        conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
        m = (conv @ Matrix.Translation((side * 0.118, 0.052, 0.272)) @ Matrix.Rotation(math.radians(side * 16.0), 4, "Z") @
             Matrix.Diagonal((0.074, 0.021, 0.03, 1.0)))
        bmesh.ops.create_icosphere(parts.solid(key), subdivisions=2, radius=1.0, matrix=m)
    return parts.finish(name)
