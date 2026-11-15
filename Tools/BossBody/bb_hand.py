"""籠手の手（手甲・指）と、手首から先の筒（籠手の腕の所）を作る。指は節ごとの箱で、骨は作らない。形（握り・開き・二本指・拳）は
値で作り分け、コードは模型のノードを差し替える。

手の座標 H（右手。右手系）：x＝S（親指の側）、y＝U（手の甲の側）、z＝F（指先の向き）、原点＝手首。
- 右手の握り（HandR）：刀と同じ座標に置く（原点＝握りの中央、+Z＝剣先、-Y＝刃）。柄は掌を斜めに通る。
- 左手（HandL_*）：H を左右反転したものが、そのまま部品の座標（原点＝手首、+Z＝指先、+Y＝手の甲、親指は -X の側）。
- 筒（Cuff。左右で同じ形）：原点＝手首、-Z＝肘の向き、+Y＝腕の甲の側。
"""
import math

import bmesh
from mathutils import Matrix, Vector

import bb_common as bb

# ---- 大きさ（m）。下の値は実寸の約 1.35 倍で書いてあり、最後に HAND_SCALE を掛ける（体 4.0 m に対して実寸の約 1.2 倍。
# 遠くから読めるよう大きめ。2026-11-15：0.88 → 1.0）。----
HAND_SCALE = 1.0
PALM_LENGTH = 0.30
PALM_WIDTH_WRIST = 0.215
PALM_WIDTH_KNUCKLE = 0.275
PALM_THICK = 0.085
# 指：(付け根の s、付け根の f、3 節の長さ、太さ)
FINGERS = {
    "index": (0.098, 0.300, (0.112, 0.072, 0.060), 0.064),
    "middle": (0.033, 0.308, (0.124, 0.080, 0.064), 0.066),
    "ring": (-0.032, 0.300, (0.114, 0.074, 0.060), 0.062),
    "pinky": (-0.094, 0.280, (0.088, 0.056, 0.050), 0.054),
}
THUMB_THICK = 0.072
HANDLE_RADIUS = 0.070      # 握る柄の半径（柄の太い方。bb_katana の TSUKA_HALF_Y＋柄巻）
GRIP_DIAGONAL = math.radians(26.0)   # 柄が掌を斜めに通る角度（S の向きから F の側へ）
GRIP_ROLL = math.radians(22.0)       # 手の甲が、刀の右（-X）から棟の側（+Y）へ回っている角度
GRIP_AXIS_F = 0.235        # 柄の軸が通る所（掌の中央の s=0 での f）


def H(s, u, f):
    return Vector((s, u, f))


def E(v):
    """エンジンの座標 → Blender の座標。"""
    return Vector((v.x, -v.z, v.y))


def blender_matrix(engine_matrix):
    """エンジンの座標での 4x4 を、Blender の座標での 4x4 にする。"""
    conv = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    return conv @ engine_matrix @ conv.inverted()


def basis(origin, x, y, z):
    m = Matrix.Identity(4)
    for i, axis in enumerate((x, y, z)):
        m[0][i], m[1][i], m[2][i] = axis.x, axis.y, axis.z
    m[0][3], m[1][3], m[2][3] = origin.x, origin.y, origin.z
    return m


def rounded_box(bm, size, matrix, bevel):
    made = bmesh.ops.create_cube(bm, size=1.0, matrix=matrix @ Matrix.Diagonal((*size, 1.0)))
    verts = made["verts"]
    edges = list({e for v in verts for e in v.link_edges})
    if bevel > 0.0:
        bmesh.ops.bevel(bm, geom=edges, offset=bevel, segments=2, profile=0.6, affect="EDGES")


def segment_matrix(p0, p1, up):
    """p0→p1 を局所 +Z、up（その直交成分）を局所 +Y とする変換。原点は中点。"""
    z = (p1 - p0).normalized()
    x = up.cross(z)
    x = x.normalized() if x.length > 1e-6 else Vector((1, 0, 0))
    y = z.cross(x)
    return basis((p0 + p1) * 0.5, x, y, z)


def wrap_joints(base, lengths, center, radius):
    """(f, u) の面で、付け根 base から、中心 center・半径 radius の円に沿って節を巻き付ける。戻り値は節の端の点の列。"""
    points = [base]
    current = base
    for length in lengths:
        d = current - center
        dist = d.length
        angle_now = math.atan2(d.y, d.x)
        # 円（center, radius）と、円（current, length）の交点のうち、時計回り（指を曲げる向き）の方を取る。
        a = (dist * dist + radius * radius - length * length) / (2.0 * dist * radius) if dist > 1e-6 else 2.0
        if abs(a) > 1.0:
            # 届かない：円の接線の方へ伸ばす
            target = angle_now - math.radians(70.0)
            nxt = current + Vector((math.cos(target - math.pi / 2), math.sin(target - math.pi / 2))) * length
        else:
            nxt = center + Vector((math.cos(angle_now - math.acos(a)), math.sin(angle_now - math.acos(a)))) * radius
        points.append(nxt)
        current = nxt
    return points


def curl_joints(base, lengths, angles):
    """(f, u) の面で、曲げ角（度。F から掌の側へ、節ごとに足す）で節を並べる。"""
    points = [base]
    total = 0.0
    for length, angle in zip(lengths, angles):
        total += math.radians(angle)
        points.append(points[-1] + Vector((math.cos(total), -math.sin(total))) * length)
    return points


class HandBuilder:
    def __init__(self, mats):
        self.mats = mats
        self.meshes = {key: bmesh.new() for key in ("leather", "lacquer", "gold", "silk", "mail")}

    # ---- 部品 ----
    def palm(self):
        bm = self.meshes["leather"]
        rings = []
        for f, width, thick in ((-0.02, PALM_WIDTH_WRIST * 0.92, PALM_THICK * 0.80), (0.0, PALM_WIDTH_WRIST, PALM_THICK * 0.9),
                                (0.12, 0.250, PALM_THICK), (0.24, PALM_WIDTH_KNUCKLE, PALM_THICK),
                                (PALM_LENGTH, PALM_WIDTH_KNUCKLE * 0.97, PALM_THICK * 0.9),
                                (PALM_LENGTH + 0.018, PALM_WIDTH_KNUCKLE * 0.90, PALM_THICK * 0.6)):
            ring = []
            for i in range(20):
                a = 2.0 * math.pi * i / 20
                # 角の丸い四角（超楕円）
                cx, cy = math.cos(a), math.sin(a)
                x = math.copysign(abs(cx) ** 0.5, cx) * width * 0.5
                y = math.copysign(abs(cy) ** 0.5, cy) * thick * 0.5
                ring.append(H(x, y, f))
            rings.append(ring)
        bb.loft(bm, rings)

    def finger(self, name, joints_fu, spread_degrees=0.0, plates=True):
        s0, _f0, lengths, thick = FINGERS[name]
        spread = Matrix.Rotation(math.radians(spread_degrees), 4, "Y")
        pivot = H(s0, 0.0, joints_fu[0].x)

        def place(p):
            local = H(s0, p.y, p.x) - pivot
            return pivot + spread @ local

        pts = [place(p) for p in joints_fu]
        for k, (p0, p1) in enumerate(zip(pts, pts[1:])):
            t = thick * (1.0, 0.92, 0.84)[k]
            side = spread @ Vector((1, 0, 0))
            up = (p1 - p0).normalized().cross(side)
            m = segment_matrix(p0, p1, up)
            rounded_box(self.meshes["leather"], (t, t * 0.92, (p1 - p0).length * 1.04), m, t * 0.22)
            # 節の継ぎ目（丸い玉でつなぐ）
            bmesh.ops.create_icosphere(self.meshes["leather"], subdivisions=2, radius=t * 0.52,
                                       matrix=Matrix.Translation(p0))
            if plates and k < 2:
                # 指の甲の小札（黒漆の板。縁は金の下敷きが見える）
                top = m @ Matrix.Translation((0.0, t * 0.50, 0.0))
                rounded_box(self.meshes["lacquer"], (t * 1.08, 0.012, (p1 - p0).length * 0.80), top, 0.004)
                rounded_box(self.meshes["gold"], (t * 1.16, 0.005, (p1 - p0).length * 0.88),
                            m @ Matrix.Translation((0.0, t * 0.47, 0.0)), 0.002)
        bmesh.ops.create_icosphere(self.meshes["leather"], subdivisions=2, radius=thick * 0.40,
                                   matrix=Matrix.Translation(pts[-1]))
        return pts

    def thumb(self, joints):
        """joints：H の点の列（付け根→指先。3 節）。"""
        for k, (p0, p1) in enumerate(zip(joints, joints[1:])):
            t = THUMB_THICK * (1.15, 1.0, 0.88)[k]
            up = Vector((0.6, 0.8, 0.0))
            m = segment_matrix(p0, p1, up)
            rounded_box(self.meshes["leather"], (t, t * 0.92, (p1 - p0).length * 1.04), m, t * 0.22)
            bmesh.ops.create_icosphere(self.meshes["leather"], subdivisions=2, radius=t * 0.52,
                                       matrix=Matrix.Translation(p0))
            if k >= 1:
                rounded_box(self.meshes["lacquer"], (t * 1.06, 0.012, (p1 - p0).length * 0.78),
                            m @ Matrix.Translation((0.0, t * 0.50, 0.0)), 0.004)
                rounded_box(self.meshes["gold"], (t * 1.14, 0.005, (p1 - p0).length * 0.86),
                            m @ Matrix.Translation((0.0, t * 0.47, 0.0)), 0.002)
        bmesh.ops.create_icosphere(self.meshes["leather"], subdivisions=2, radius=THUMB_THICK * 0.42,
                                   matrix=Matrix.Translation(joints[-1]))

    def tekko(self):
        """手の甲の板（手甲）。先が 3 つの丸みに分かれ、指の付け根を覆う。金の縁、中央に金の紋と鋲。"""
        def plate(bm, grow, lift, thick):
            cols, rows = 36, 12
            grid = []
            for j in range(rows + 1):
                t = j / rows
                row = []
                for i in range(cols + 1):
                    x = -1.0 + 2.0 * i / cols
                    half = (0.118 + 0.030 * t + grow) * (1.0 - 0.10 * (1.0 - t) ** 2)
                    s = x * half
                    # 先の縁は 3 つの丸い山（山と山の間が浅い谷）
                    front = PALM_LENGTH + 0.030 + grow + 0.034 * abs(math.cos(x * math.pi * 1.5)) ** 0.6 - 0.030 * x * x
                    f = 0.005 - grow + (front - 0.005 + grow) * t
                    arch = 0.030 * (1.0 - x * x) + 0.008 * math.sin(t * math.pi)
                    row.append(bm.verts.new(H(s, PALM_THICK * 0.5 + lift + arch, f)))
                grid.append(row)
            faces = []
            for j in range(rows):
                for i in range(cols):
                    faces.append(bm.faces.new((grid[j][i], grid[j][i + 1], grid[j + 1][i + 1], grid[j + 1][i])))
            # 下へ厚みを付ける
            geom = bmesh.ops.extrude_face_region(bm, geom=faces)
            moved = [v for v in geom["geom"] if isinstance(v, bmesh.types.BMVert)]
            bmesh.ops.translate(bm, verts=moved, vec=Vector((0.0, -thick, 0.0)))

        plate(self.meshes["lacquer"], 0.0, 0.014, 0.014)
        plate(self.meshes["gold"], 0.012, 0.006, 0.010)
        # 中央の稜（盛り上がった筋）
        rounded_box(self.meshes["lacquer"], (0.026, 0.016, 0.26),
                    basis(H(0.0, PALM_THICK * 0.5 + 0.052, 0.17), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))),
                    0.006)
        # 紋（丸に二つ引きの簡略：金の丸い板と、その上の黒い輪）
        for radius, depth, lift, key in ((0.046, 0.008, 0.050, "gold"), (0.034, 0.008, 0.055, "lacquer"),
                                         (0.016, 0.008, 0.060, "gold")):
            bmesh.ops.create_cone(self.meshes[key], cap_ends=True, segments=28, radius1=radius, radius2=radius * 0.92,
                                  depth=depth, matrix=Matrix.Translation(H(0.0, PALM_THICK * 0.5 + lift, 0.165)) @
                                  Matrix.Rotation(math.radians(-90), 4, "X"))
        # 鋲（縁に沿って）
        for s, f in ((-0.105, 0.05), (0.105, 0.05), (-0.125, 0.19), (0.125, 0.19), (-0.10, 0.31), (0.10, 0.31),
                     (0.0, 0.335)):
            x = s / 0.15
            arch = 0.030 * (1.0 - x * x)
            bmesh.ops.create_icosphere(self.meshes["gold"], subdivisions=1, radius=0.011,
                                       matrix=Matrix.Translation(H(s, PALM_THICK * 0.5 + 0.016 + arch, f)))

    def wrist_cord(self):
        """手首を締める紅の紐。"""
        bm = self.meshes["silk"]
        rings = []
        for f in (-0.012, 0.012):
            rings.append([H(math.cos(2 * math.pi * i / 20) * PALM_WIDTH_WRIST * 0.56,
                            math.sin(2 * math.pi * i / 20) * PALM_THICK * 0.62, f + 0.004) for i in range(20)])
        bb.loft(bm, rings)

    # ---- 仕上げ ----
    def finish(self, name, transform, mirror=False):
        """H の形を部品の座標へ移して 1 つのノードにする。transform はエンジンの座標での 4x4。"""
        objects = []
        m = blender_matrix(transform)
        h_to_blender = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))  # H を「エンジンの座標」と見て E を掛ける
        for key, bm in self.meshes.items():
            if not bm.verts:
                bm.free()
                continue
            if mirror:
                bmesh.ops.scale(bm, vec=Vector((-1.0, 1.0, 1.0)), verts=bm.verts)
            bmesh.ops.transform(bm, matrix=m @ h_to_blender, verts=bm.verts)
            objects.append(bb.new_object(name + "_" + key, bm, self.mats[key], smooth=True))
        return bb.join(name, objects)


def grip_frame():
    """握りの右手：H → 刀の座標（エンジン）の 4x4 と、柄の軸が掌を通る点（H）。"""
    a, b = GRIP_DIAGONAL, GRIP_ROLL
    z = Vector((0, 0, 1))
    u = Vector((-math.cos(b), math.sin(b), 0.0))
    w = u.cross(z)
    s = z * math.cos(a) + w * math.sin(a)
    f = z * math.sin(a) - w * math.cos(a)
    # 手は最後に HAND_SCALE 倍に縮めるので、柄の太さは先に割っておく（縮めた後に本当の太さになる）。
    axis_point = H(0.0, -(PALM_THICK * 0.5 + HANDLE_RADIUS / HAND_SCALE + 0.004), GRIP_AXIS_F)
    rotation = basis(Vector((0, 0, 0)), s, u, f)
    return Matrix.Scale(HAND_SCALE, 4) @ rotation @ Matrix.Translation(-axis_point), axis_point


def build_grip(mats, name="HandR"):
    """柄を握った右手。戻り値：(物体, 手首の位置（刀の座標）, 肘の向き（刀の座標）, 手の甲の向き（刀の座標）)。"""
    hand = HandBuilder(mats)
    hand.palm()
    hand.tekko()
    hand.wrist_cord()
    transform, axis_point = grip_frame()
    slope = math.tan(GRIP_DIAGONAL)
    for finger_name, (s0, f0, lengths, thick) in FINGERS.items():
        center = Vector((axis_point.z + slope * s0, axis_point.y))
        joints = wrap_joints(Vector((f0, 0.0)), lengths, center, HANDLE_RADIUS / HAND_SCALE + thick * 0.5 + 0.004)
        hand.finger(finger_name, joints)
    # 親指：掌の脇から柄の向こうへ回り、人差し指・中指の先に重なる。
    hand.thumb([H(0.128, -0.030, 0.050), H(0.150, -0.085, 0.150), H(0.118, -0.150, 0.215), H(0.060, -0.185, 0.262)])
    obj = hand.finish(name, transform)
    wrist = transform @ Vector((0, 0, 0))
    elbow = (transform.to_3x3() @ Vector((0, 0, -1))).normalized()
    back = (transform.to_3x3() @ Vector((0, 1, 0))).normalized()
    return obj, wrist, elbow, back


FREE_POSES = {
    # 指ごとの曲げ角（付け根・中・先。度）と、開き（度）。親指は H の点の列。
    "Fist": dict(curl={"index": (82, 100, 62), "middle": (84, 102, 64), "ring": (84, 100, 62), "pinky": (82, 98, 60)},
                 spread={}, thumb=[H(0.128, -0.030, 0.050), H(0.150, -0.075, 0.150), H(0.105, -0.120, 0.225),
                                   H(0.040, -0.128, 0.262)]),
    "Open": dict(curl={"index": (10, 16, 12), "middle": (6, 14, 10), "ring": (10, 18, 12), "pinky": (14, 22, 14)},
                 spread={"index": 9, "middle": 1, "ring": -8, "pinky": -19},
                 thumb=[H(0.128, -0.030, 0.050), H(0.215, -0.045, 0.125), H(0.265, -0.050, 0.215),
                        H(0.290, -0.058, 0.295)]),
    "Seal": dict(curl={"index": (4, 5, 4), "middle": (3, 5, 4), "ring": (84, 100, 62), "pinky": (82, 98, 60)},
                 spread={"index": 2, "middle": -1},
                 thumb=[H(0.128, -0.030, 0.050), H(0.150, -0.080, 0.150), H(0.085, -0.118, 0.220),
                        H(0.010, -0.122, 0.250)]),
}


def build_free(mats, pose, name):
    """刀を持たない左手（H を左右反転）。"""
    spec = FREE_POSES[pose]
    hand = HandBuilder(mats)
    hand.palm()
    hand.tekko()
    hand.wrist_cord()
    for finger_name, (s0, f0, lengths, _thick) in FINGERS.items():
        joints = curl_joints(Vector((f0, 0.0)), lengths, spec["curl"][finger_name])
        hand.finger(finger_name, joints, spec["spread"].get(finger_name, 0.0))
    hand.thumb(spec["thumb"])
    return hand.finish(name, Matrix.Scale(HAND_SCALE, 4), mirror=True)


def build_cuff(mats, name="Cuff"):
    """手首から肘の方へ伸びる筒（籠手）。先（肘の側）は開いていて、中から煙が出る。
    部品の座標：原点＝手首、-Z＝肘の向き、+Y＝腕の甲の側。H と同じ向き（z＝F）で作る。"""
    meshes = {key: bmesh.new() for key in ("leather", "lacquer", "gold", "silk", "mail")}
    length = 0.62
    # 家地（布の筒）。厚みを付けて、開いた先から内側が見えるようにする。
    def tube(bm, grow, f_far):
        rings = []
        for k in range(9):
            t = k / 8.0
            f = -0.035 - (f_far - 0.035) * t
            rx = (0.120 + 0.052 * t ** 0.8 + grow)
            ry = (0.070 + 0.070 * t ** 0.8 + grow)
            rings.append([H(math.cos(2 * math.pi * i / 28) * rx, math.sin(2 * math.pi * i / 28) * ry, f)
                          for i in range(28)])
        bb.loft(bm, rings, cap_start=False, cap_end=False)
    sleeve_bm = bmesh.new()
    tube(sleeve_bm, 0.0, length)
    # 鎖の帯（篠と篠の間に見える）
    tube(meshes["mail"], 0.004, length * 0.86)
    # 篠（腕の甲の側に並ぶ縦長の板）と、その金の下敷き
    for k in range(-3, 4):
        angle = math.radians(90.0 + k * 24.0)
        for t0, t1 in ((0.04, 0.47), (0.50, 0.93)):
            p = []
            for t in (t0, t1):
                f = -0.035 - (length - 0.035) * t
                rx = 0.120 + 0.052 * t ** 0.8 + 0.012
                ry = 0.070 + 0.070 * t ** 0.8 + 0.012
                p.append(H(math.cos(angle) * rx, math.sin(angle) * ry, f))
            out = H(math.cos(angle), math.sin(angle) * 1.3, 0.0).normalized()
            m = segment_matrix(p[0], p[1], out)
            width = 0.050 + 0.016 * (t0 + t1) * 0.5
            rounded_box(meshes["lacquer"], (width, 0.014, (p[1] - p[0]).length), m, 0.005)
            rounded_box(meshes["gold"], (width + 0.012, 0.006, (p[1] - p[0]).length + 0.012),
                        m @ Matrix.Translation((0.0, -0.006, 0.0)), 0.002)
    # 手首・中ほど・先の帯（紅の紐と金の輪）
    for t, mat, grow, half in ((0.0, "gold", 0.016, 0.016), (0.485, "silk", 0.020, 0.012), (0.97, "gold", 0.018, 0.02)):
        f = -0.035 - (length - 0.035) * t
        rx = 0.120 + 0.052 * t ** 0.8 + grow
        ry = 0.070 + 0.070 * t ** 0.8 + grow
        rings = [[H(math.cos(2 * math.pi * i / 28) * rx * k, math.sin(2 * math.pi * i / 28) * ry * k, f + df)
                  for i in range(28)] for df, k in ((half, 0.96), (half * 0.5, 1.0), (-half * 0.5, 1.0), (-half, 0.96))]
        bb.loft(meshes[mat], rings, cap_start=False, cap_end=False)
    objects = []
    h_to_blender = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
    h_to_blender = h_to_blender @ Matrix.Scale(HAND_SCALE, 4)
    bmesh.ops.transform(sleeve_bm, matrix=h_to_blender, verts=sleeve_bm.verts)
    sleeve = bb.new_object(name + "_sleeve", sleeve_bm, mats["leather"], smooth=True)
    solid = sleeve.modifiers.new("Solid", "SOLIDIFY")
    solid.thickness = 0.014
    solid.offset = -1.0
    bb.apply_modifiers(sleeve)
    objects.append(sleeve)
    for key, bm in meshes.items():
        if not bm.verts:
            bm.free()
            continue
        bmesh.ops.transform(bm, matrix=h_to_blender, verts=bm.verts)
        objects.append(bb.new_object(name + "_" + key, bm, mats[key], smooth=True))
    return bb.join(name, objects)
