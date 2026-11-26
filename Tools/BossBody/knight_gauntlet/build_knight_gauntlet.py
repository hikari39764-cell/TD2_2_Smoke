"""総監の新しい籠手（TD2_No2_boss_Hand.fbx の 立方体.005）を、骨格のボス（BossIntroActor）の手の骨に合わせて組み、肉付けし、
骨の重みと UV を付ける。c-8b643c。

使い方：blender --background --factory-startup --python build_knight_gauntlet.py -- [out=<フォルダ>]

元の模型は手の甲の側だけの鋼の板（袖口の浅い弧の板・手首の小札 4 枚・手の甲の板・親指の板と 3 節・4 本の指の小札 5〜6 枚）。
どれも閉じた薄い殻で、手のひら・指の腹・前腕の内側が空いている（ボスの腕は煙なので、そのままだと板が宙に浮いて見える）。
ここで肉付けする：
- 袖口：手の甲の側の板の左右の縁から、手のひらの側へ回る半楕円の板（同じ鋼）を足して、肘へ開く筒にする。
- 鋲：袖口の板の左右の縁・手首の小札の両端・手の甲の板の手首の側の角。指と親指の小札は少し寝かせる（TILE_LAY_DEG）。
- 手首・手のひら・指・親指の内側：革の手袋（メタボールで手の形にまとめた 1 つの網。手のひら・母指球・小指球・指の節・
  手首から前腕）を板の下に詰め、骨の区間への距離で滑らかに重みを付ける（関節で曲がる）。
骨への付け方（板は骨に剛体で付ける。動捕の握り・開きに付いて行く）：
- 袖口・手首の小札 → Cuff の骨（小札は手の骨へ少しずつ混ぜる）。手の甲の板 → 手の骨。
- 指の小札 → 指の骨（元の指の長さを、骨の 1〜3 節と指先の長さへ割り付け、真ん中の来る節に付ける）。親指も同じ。
座標：元の模型は前 +X・上 +Z・+Y がボスの左（兜・胸当ての組みと同じ）。骨はボスの glTF の結び姿（Y 上）を Blender の
座標（Z 上）に直して使う。出す網は Blender の座標で、splice_knight_gauntlet.py が glTF へ戻す。
出す物（out。既定 Library/Handover/c-8b643c/build）：
- KnightGauntlet.blend：組んだ両手（確かめ用）。
- KnightGauntlet_sp.fbx：Substance Painter に渡す網（物体 GauntletSteel・GauntletLeather、材質は 1 つ・UV 1 枚）。
- KnightGauntlet_mesh.npz：glTF に入れる網（ノードごとの頂点・法線・UV・骨と重み・三角）。
- KnightGauntlet_layout.json：手の甲の彫りの面の「板の座標 → UV」の変換と寸法（engrave_pattern.py がここへ彫りを描く）。
UV の詰め方は毎回わずかに変わるので、作り直したら engrave_pattern.py → render_sd.sh → Painter（sp/sp_gauntlet.py）も流し直す。
"""
import json
import math
import pathlib
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SOURCE = HERE / "TD2_No2_boss_Hand.fbx"
SOURCE_OBJECT = "立方体.005"
ACTOR = ROOT / "Assets" / "Models" / "BossIntroActor.gltf"

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
options = dict(a.split("=", 1) for a in argv if "=" in a)
OUT = pathlib.Path(options.get("out", ROOT / "Library" / "Handover" / "c-8b643c" / "build"))

# ---------------------------------------------------------------- 寸法（元の模型の m。骨の側は骨格の m）
SCALE = 1.10            # 元の模型 → ボスの骨格の大きさ（袖口の幅が前の籠手と同じくらい、手首〜指先が骨と合う）
HAND_SIDE_K = 0.86      # 手の甲の板の横幅の縮め（元の手は骨の指の並びより幅が広い）
CORE_DORSAL = 0.19      # 手の骨（手の厚みの真ん中）から手の甲の板の内側まで
FINGER_CORE = 0.070     # 指の骨（指の真ん中）から指の小札の内側まで
THUMB_CORE = 0.075      # 親指の骨から親指の板の内側まで
FINGER_WIDTH_K = 0.95   # 指の小札の横幅の縮め（骨の指の間隔に合わせる）
TIP_RATIO = 0.85        # 指先の節の長さ＝その手前の節の長さ × これ（骨格に指先の点が無い）
# 袖口を閉じる板（手のひらの側の半楕円）
CUFF_SLICES = 14
CUFF_ARC = 20
CUFF_THICK = 0.032
CUFF_DEPTH_MIN = 0.72   # 手のひらの側へのふくらみ ≧ 半幅 × これ
CUFF_BELOW_AXIS = 0.24  # 手のひらの側へのふくらみが、腕の芯からこれだけ下まで届く
CUFF_TUCK = 0.975       # 閉じる板を手の甲の側の板の縁の内へ入れる割合（縁が重なって継ぎ目に見える）
THUMB_PLATE_K = 1.25    # 親指の板の長さ＝親指の 1 節の長さ × これ
THUMB_PLATE_BACK = 0.06 # 親指の板を 1 節の付け根より手首の側へ出す長さ
TILE_LAY_DEG = 10.0     # 指・親指の小札を寝かせる角度（元は先の端が高く立ち、横から見ると板が浮いて見えた）
# 鋲（元の模型の m）：袖口の板の左右の縁に 3 つずつ、手首の小札の両端、手の甲の板の手首の側の角。
RIVET_R = 0.026
RIVET_FLAT = 0.55       # 頭の高さ＝半径 × これ
RIVET_SINK = 0.15       # 面へ沈める（半径 × これ）
RIVET_INSET = 0.045     # 板の縁から内へ
# 革の手袋（骨格の m。見える寸法）。メタボールで 1 つの手の形にまとめる。
GLOVE_RES = 0.024       # 網の細かさ
GLOVE_DECIMATE = 0.35   # メタボールの網を縮約する割合（片手 6 千三角くらいへ）
JOINT_OVERLAP = 0.6     # 指の節のカプセルを、隣の節へ半径 × これだけ重ねる
PALM_SPLIT = 0.45       # 手のひらの 2 つの箱の境（手首 0 → 指の付け根 1）
PALM_HALF_WIDTH_WRIST = 0.215
MB_STIFFNESS = 4.0      # 指どうしがくっつかない強さ
MB_VISIBLE = 0.68       # この強さでの、見える半径 ÷ メタボールの半径（Blender 5.1 で測った）
GLOVE_BLEND = 0.045     # 骨の重みを混ぜる幅（近い骨との距離の差がこれくらいまで混ざる）
PALM_TOP = 0.165        # 手のひらの甲の側の面（手の甲の板の内側より下。板の縁が下がるので低め）
PALM_BELOW = 0.215      # 手のひらの面（手の骨から）
PALM_BACK = 0.10        # 手首より肘の側へ伸ばす長さ
PALM_HALF_WIDTH = 0.27
PALM_ROUND = 0.13
METACARPAL_R, METACARPAL_LIFT = 0.085, 0.02
THENAR_R, THENAR_SHAPE, THENAR_DROP, THENAR_IN = 0.13, (1.7, 0.95, 1.05), 0.075, 0.02   # 母指球
HYPO_R, HYPO_SHAPE, HYPO_DROP = 0.11, (2.4, 0.85, 1.0), 0.09                          # 小指球
WRIST_BACK = 0.95       # 手首から肘の側へ（袖口の中）
WRIST_HALF_THICK, WRIST_HALF_WIDTH, WRIST_ROUND, WRIST_LIFT = 0.17, 0.235, 0.12, 0.0
FINGER_RADII = (0.080, 0.073, 0.066)   # 指の節の太さ（付け根・中・先）
FINGER_WIDTH = {"Index": 1.0, "Middle": 1.02, "Ring": 0.97, "Pinky": 0.88}
FINGER_LIFT = 0.012
THUMB_RADII = (0.098, 0.088, 0.078)
THUMB_LIFT = 0.01
LAME_CUFF_W = (0.85, 0.65, 0.4, 0.2)   # 手首の小札（上＝肘の側から）の Cuff の骨の重み。残りは手の骨
SHARP_ANGLE = 35.0
ENGRAVE_TEXEL = 3.0     # 手の甲の外の面の UV の画素を、ほかの面よりこれだけ多くする（彫りを細かく）
UV_MARGIN = 0.004

ID_STEEL, ID_LEATHER, ID_ENGRAVE = (0.55, 0.55, 0.58, 1.0), (0.45, 0.22, 0.08, 1.0), (0.9, 0.85, 0.2, 1.0)
SIDES = ("Left", "Right")
FINGERS = ("Index", "Middle", "Ring", "Pinky")   # 親指の側から
# glTF（Y 上）→ Blender（Z 上）
C4 = np.array([[1, 0, 0, 0], [0, 0, -1, 0], [0, 1, 0, 0], [0, 0, 0, 1]], float)


def nrm(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


# ---------------------------------------------------------------- 骨格（結び姿）
def read_bones():
    doc = json.loads(ACTOR.read_text(encoding="utf-8"))
    blob = (ACTOR.parent / doc["buffers"][0]["uri"]).read_bytes()
    skin = doc["skins"][0]
    acc = doc["accessors"][skin["inverseBindMatrices"]]
    view = doc["bufferViews"][acc["bufferView"]]
    offset = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    ibm = np.frombuffer(blob, np.float32, acc["count"] * 16, offset).reshape(-1, 4, 4).transpose(0, 2, 1)
    names = [doc["nodes"][i]["name"] for i in skin["joints"]]
    return {n: C4 @ np.linalg.inv(m.astype(float)) for n, m in zip(names, ibm)}


BONES = read_bones()


def head(name):
    return BONES[name][:3, 3].copy()


def frame(name, along_col=1, dorsal_col=2):
    """骨の向き（長さの向き・甲の向き・横＝長さ×甲）。"""
    m = BONES[name][:3, :3]
    a = nrm(m[:, along_col])
    d = m[:, dorsal_col] - np.dot(m[:, dorsal_col], a) * a
    d = nrm(d)
    return np.column_stack([a, d, np.cross(a, d)])


# ---------------------------------------------------------------- 元の模型
def import_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # Blender 5.1 の FBX 読み込みは、模型に入っているライトの cast_shadow で止まるので、読む前に項目を足しておく。
    light = bpy.data.lights.new("_probe", "POINT")
    if not hasattr(light.cycles, "cast_shadow"):
        type(light.cycles).cast_shadow = bpy.props.BoolProperty(default=True)
    bpy.data.lights.remove(light)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE))
    src = bpy.data.objects[SOURCE_OBJECT]
    for o in list(bpy.data.objects):
        if o is not src:
            bpy.data.objects.remove(o)
    src.data.transform(src.matrix_world)
    src.matrix_world = Matrix.Identity(4)
    if src.data.has_custom_normals:
        bpy.context.view_layer.objects.active = src
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    src.data.materials.clear()
    while src.data.uv_layers:
        src.data.uv_layers.remove(src.data.uv_layers[0])
    bpy.ops.object.select_all(action="DESELECT")
    src.select_set(True)
    bpy.context.view_layer.objects.active = src
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
    return [o for o in bpy.data.objects if o.type == "MESH"]


def co(ob):
    a = np.empty(len(ob.data.vertices) * 3)
    ob.data.vertices.foreach_get("co", a)
    return a.reshape(-1, 3)


def classify(parts):
    """左右の手ごとに、部品を見分ける（元の模型の x・z は左右で同じ。y の符号が左右）。"""
    hands = {}
    for side, sgn in (("Left", 1.0), ("Right", -1.0)):
        mine = [o for o in parts if np.sign(co(o)[:, 1].mean()) == sgn]
        info = {o: (co(o).mean(0), np.ptp(co(o), 0)) for o in mine}
        cuff = max(mine, key=lambda o: info[o][0][2])
        rest = [o for o in mine if o is not cuff]
        lames = sorted([o for o in rest if info[o][1][0] > 0.45 and info[o][1][2] < 0.25], key=lambda o: -info[o][0][2])
        back = [o for o in rest if info[o][1][0] > 0.6 and 0.4 < info[o][1][2] < 0.85]
        thumb_plate = [o for o in rest if info[o][0][0] > 0.35 and info[o][1][2] > 0.5]
        thumb_segs = sorted([o for o in rest if info[o][0][0] > 0.38 and info[o][1][2] <= 0.3], key=lambda o: -info[o][0][2])
        used = set([cuff] + lames + back + thumb_plate + thumb_segs)
        segs = sorted([o for o in rest if o not in used], key=lambda o: -info[o][0][0])
        columns, current = [], []
        for o in segs:
            if current and abs(info[o][0][0] - info[current[-1]][0][0]) > 0.08:
                columns.append(current)
                current = []
            current.append(o)
        columns.append(current)
        assert len(lames) == 4 and len(back) == 1 and len(thumb_plate) == 1 and len(thumb_segs) == 3, side
        assert len(columns) == 4, (side, [len(c) for c in columns])
        fingers = {f: sorted(c, key=lambda o: -info[o][0][2]) for f, c in zip(FINGERS, columns)}
        hands[side] = dict(sgn=sgn, cuff=cuff, lames=lames, back=back[0], thumb=[thumb_plate[0]] + thumb_segs,
                           fingers=fingers)
        print("[gauntlet] %s: cuff, lames %d, back, thumb %d, fingers %s" % (
            side, len(lames), 1 + len(thumb_segs), [len(fingers[f]) for f in FINGERS]))
    return hands


# ---------------------------------------------------------------- 当てはめ
def affine(target_origin, target_frame, k, source_frame, source_origin):
    """元の点 p → 骨の側：O_t + R_t · diag(k) · R_sᵀ · (p − O_s)。"""
    m = np.eye(4)
    m[:3, :3] = target_frame @ np.diag(k) @ source_frame.T
    m[:3, 3] = target_origin - m[:3, :3] @ source_origin
    return m


def apply(ob, m):
    ob.data.transform(Matrix(m.tolist()))


def source_frame(sgn, dorsal=None):
    a = np.array([0.0, 0.0, -1.0])
    d = np.array([0.0, sgn, 0.0]) if dorsal is None else nrm(dorsal - np.dot(dorsal, a) * a)
    return np.column_stack([a, d, np.cross(a, d)])


def chain(names):
    """骨の並び（付け根 → 指先）の点と、節ごとの長さ。最後の節の先は TIP_RATIO で決める。"""
    pts = [head(n) for n in names]
    last = np.linalg.norm(pts[-1] - pts[-2]) * TIP_RATIO
    pts.append(pts[-1] + frame(names[-1])[:, 0] * last)
    lengths = [np.linalg.norm(pts[i + 1] - pts[i]) for i in range(len(names))]
    return pts, lengths


def fit_chain(objs, names, axis_xy, sgn, core, dorsal=None, width_k=1.0, first_on_bone=False):
    """指・親指の部品を、骨の並びへ割り付ける。axis_xy＝元の指の芯（x, y）。戻り値：部品 → 骨の名前。
    first_on_bone：最初の部品（親指の板）を最初の骨の長さ × THUMB_PLATE_K に合わせ、骨の付け根から置く（元の親指の板は
    手首から指の付け根まであり、骨格の親指の 1 節よりずっと長い）。残りは、その先から指先までへ割り付ける。"""
    sf = source_frame(sgn, dorsal)
    pts, lengths = chain(names)
    out = {}
    if first_on_bone:
        plate = objs[0]
        p = co(plate)
        tf = frame(names[0])
        length = lengths[0] * THUMB_PLATE_K
        k = length / (SCALE * np.ptp(p[:, 2]))
        src_origin = np.array([axis_xy[0], axis_xy[1], p[:, 2].max()])
        apply(plate, affine(pts[0] - tf[:, 0] * THUMB_PLATE_BACK, tf, SCALE * np.array([k, 1.0, width_k]), sf,
                            src_origin))
        out[plate] = names[0]
        objs = objs[1:]
        # 残りは 2 節目から。
        pts, lengths, names = pts[1:], lengths[1:], names[1:]
    z0 = max(co(o)[:, 2].max() for o in objs)
    z1 = min(co(o)[:, 2].min() for o in objs)
    total = sum(lengths)
    along_k = total / (SCALE * (z0 - z1))
    for o in objs:
        zc = co(o)[:, 2].mean()
        t = (z0 - zc) / (z0 - z1) * total
        k = 0
        while k < len(names) - 1 and t > sum(lengths[:k + 1]):
            k += 1
        local = t - sum(lengths[:k])
        tf = frame(names[k])
        origin = pts[k] + tf[:, 0] * local
        src_origin = np.array([axis_xy[0], axis_xy[1], zc])
        # 芯から板の内側まで：元は core（元の m）、骨の側は core × SCALE。芯どうしを合わせるので、そのまま写る。
        apply(o, affine(origin, tf, SCALE * np.array([along_k, 1.0, width_k]), sf, src_origin))
        out[o] = names[k]
    return out


def dorsal_inner(ob, sgn, x_center=None, band=0.1):
    p = co(ob)
    q = p[:, 1] * sgn
    if x_center is not None:
        sel = np.abs(p[:, 0] - x_center) < band
        if sel.any():
            q = q[sel]
    return q.min()


# ---------------------------------------------------------------- 袖口を閉じる板
def cuff_closer(cuff, sgn, axis_q):
    """袖口の板（手の甲の側の浅い弧）の左右の縁から、手のひらの側へ回る半楕円の板を作る（元の模型の座標）。"""
    p = co(cuff)
    edges = np.array([e.vertices[:] for e in cuff.data.edges])
    a, b = p[edges[:, 0]], p[edges[:, 1]]
    zt, zb = p[:, 2].max(), p[:, 2].min()
    zs = np.linspace(zb + 0.02, zt - 0.02, CUFF_SLICES)
    bm = bmesh.new()
    rings_out, rings_in = [], []
    for z in zs:
        # 板と z の面の交わり（辺ごとの交点）の左右の端。
        cross = (a[:, 2] - z) * (b[:, 2] - z) < 0.0
        t = ((z - a[cross, 2]) / (b[cross, 2] - a[cross, 2]))[:, None]
        pts = a[cross] + (b[cross] - a[cross]) * t
        order = np.argsort(pts[:, 0])
        xl, xr = pts[order[0], 0], pts[order[-1], 0]
        ql = (pts[order[:3], 1] * sgn).mean()
        qr = (pts[order[-3:], 1] * sgn).mean()
        xc, rx = (xl + xr) / 2, (xr - xl) / 2 * CUFF_TUCK
        qe = (ql + qr) / 2
        ry = max(rx * CUFF_DEPTH_MIN, qe - axis_q + CUFF_BELOW_AXIS)
        out_ring, in_ring = [], []
        for i in range(CUFF_ARC + 1):
            th = math.pi * i / CUFF_ARC
            for r_off, ring in ((0.0, out_ring), (CUFF_THICK, in_ring)):
                x = xc + (rx - r_off) * math.cos(th)
                qq = qe - (ry - r_off) * math.sin(th) + (0.0 if i in (0, CUFF_ARC) else 0.0)
                ring.append(bm.verts.new((x, qq * sgn, z)))
        rings_out.append(out_ring)
        rings_in.append(in_ring)
    for s in range(len(zs) - 1):
        for i in range(CUFF_ARC):
            bm.faces.new((rings_out[s][i], rings_out[s][i + 1], rings_out[s + 1][i + 1], rings_out[s + 1][i]))
            bm.faces.new((rings_in[s][i + 1], rings_in[s][i], rings_in[s + 1][i], rings_in[s + 1][i + 1]))
        for i in (0, CUFF_ARC):  # 縁（左右）
            bm.faces.new((rings_out[s][i], rings_out[s + 1][i], rings_in[s + 1][i], rings_in[s][i]))
    for s in (0, len(zs) - 1):  # 上下の縁
        for i in range(CUFF_ARC):
            bm.faces.new((rings_out[s][i + 1], rings_out[s][i], rings_in[s][i], rings_in[s][i + 1]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("CuffCloser")
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new("CuffCloser", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


# ---------------------------------------------------------------- 革の手袋（骨格の側で作る）
def glove(side):
    """革の手袋：手のひら（角の丸い箱）・中手・母指球・小指球・手首から前腕・指と親指の節を、メタボールで 1 つの手の形に
    まとめて網へ直す（指どうしはくっつかない強さ）。大きさは見える寸法で書き、MB_VISIBLE で割ってメタボールの半径にする。"""
    hb = f"{side}Hand"
    hf, hh = frame(hb), head(hb)
    a, d = hf[:, 0], hf[:, 1]
    thumb_dir = hf[:, 2] if side == "Left" else -hf[:, 2]
    mb = bpy.data.metaballs.new(f"Glove{side}")
    mb.resolution = mb.render_resolution = GLOVE_RES
    mb.threshold = 0.6
    ob = bpy.data.objects.new(f"Glove{side}", mb)
    bpy.context.scene.collection.objects.link(ob)

    def put(kind, center, x_axis, up, rounding, half=(0.0, 0.0, 0.0)):
        """rounding＝見える丸みの半径。half＝箱・カプセルのまっすぐな所の半分の長さ（x・y・z）。"""
        x = nrm(x_axis)
        y = nrm(up - np.dot(up, x) * x)
        e = mb.elements.new(type=kind)
        e.co = Vector(center)
        e.radius = rounding / MB_VISIBLE
        e.stiffness = MB_STIFFNESS
        e.rotation = Matrix(np.column_stack([x, y, np.cross(x, y)]).tolist()).to_quaternion()
        if kind == "ELLIPSOID":
            e.size_x, e.size_y, e.size_z = half
        else:
            e.size_x, e.size_y, e.size_z = (max(h, 1e-4) for h in half)

    def box(center, half_extent, rounding, x_axis=a, up=d):
        put("CUBE", center, x_axis, up, rounding, tuple(max(h - rounding, 0.0) for h in half_extent))

    def segment(p0, p1, r, up):
        put("CAPSULE", (p0 + p1) / 2, p1 - p0, up, r, (max(np.linalg.norm(p1 - p0) / 2 - r, 1e-4), 0.0, 0.0))

    knuckles = {f: head(f"{side}Hand{f}1") for f in FINGERS}
    lat = [np.dot(k - hh, thumb_dir) for k in knuckles.values()]
    mid_lat = (max(lat) + min(lat)) / 2
    k_along = float(np.mean([np.dot(k - hh, a) for k in knuckles.values()]))
    top, bottom = PALM_TOP, -PALM_BELOW
    # 手のひら：手首の側（細い）と指の付け根の側（広い）の 2 つの丸い箱を溶け合わせて、台形の手のひらにする。
    split = k_along * PALM_SPLIT
    box(hh + a * ((split - PALM_BACK) / 2) + thumb_dir * mid_lat + d * ((top + bottom) / 2 - 0.01),
        ((split + PALM_BACK) / 2, (top - bottom) / 2 - 0.015, PALM_HALF_WIDTH_WRIST), PALM_ROUND)
    box(hh + a * ((split + k_along) / 2) + thumb_dir * mid_lat + d * ((top + bottom) / 2),
        ((k_along - split) / 2, (top - bottom) / 2, PALM_HALF_WIDTH), PALM_ROUND * 0.9)
    # 中手：付け根の骨から指の付け根の関節まで（付け根の節のこぶ）。
    for f in FINGERS:
        m0 = head(f"{side}Hand{f}0")
        segment(m0 + d * METACARPAL_LIFT, knuckles[f] + d * METACARPAL_LIFT, METACARPAL_R, d)
    # 母指球：親指の 1 節の手のひらの側。小指球：手のひらの小指の側。
    t1, t2 = head(f"{side}HandThumb1"), head(f"{side}HandThumb2")
    put("ELLIPSOID", t1 + (t2 - t1) * 0.45 - d * THENAR_DROP - thumb_dir * THENAR_IN, t2 - t1, d, THENAR_R,
        THENAR_SHAPE)
    put("ELLIPSOID", hh + a * (k_along * 0.5) - thumb_dir * (np.max(np.abs(lat)) * 0.85) - d * HYPO_DROP, a, d,
        HYPO_R, HYPO_SHAPE)
    # 手首から前腕（袖口の中）。
    box(hh - a * (WRIST_BACK / 2) + d * WRIST_LIFT, (WRIST_BACK / 2 + 0.02, WRIST_HALF_THICK, WRIST_HALF_WIDTH),
        WRIST_ROUND)
    # 指・親指：節ごとのカプセル（太さは付け根から細く、小指は細く）。最後の節は指先の点で終わる。
    for f in FINGERS + ("Thumb",):
        names = [f"{side}Hand{f}{k}" for k in (1, 2, 3)]
        pts, _ = chain(names)
        radii = THUMB_RADII if f == "Thumb" else tuple(r * FINGER_WIDTH.get(f, 1.0) for r in FINGER_RADII)
        for k, name in enumerate(names):
            up = frame(name)[:, 1]
            lift = up * (THUMB_LIFT if f == "Thumb" else FINGER_LIFT)
            along = nrm(pts[k + 1] - pts[k])
            # 隣の節と重ねて 1 本の指にする（端が接するだけだと数珠に見えた）。指先だけは指先の点で止める。
            p0 = pts[k] - along * radii[k] * JOINT_OVERLAP
            p1 = pts[k + 1] + (along * radii[k] * JOINT_OVERLAP if k < 2 else 0.0)
            segment(p0 + lift, p1 + lift, radii[k], up)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.context.view_layer.update()
    bpy.ops.object.convert(target="MESH")
    out = bpy.context.view_layer.objects.active
    out.name = out.data.name = f"Glove{side}"
    bm = bmesh.new()
    bm.from_mesh(out.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=GLOVE_RES * 0.2)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(out.data)
    bm.free()
    # 面を減らす（形を保つ縮約）。
    mod = out.modifiers.new("decimate", "DECIMATE")
    mod.ratio = GLOVE_DECIMATE
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return out


def glove_weights(ob, side):
    """手袋の頂点を、近い骨の区間（前腕・手首・中手・指の節）へ、距離で混ぜて付ける（関節で滑らかに曲がる）。"""
    hb, cb = f"{side}Hand", "CuffL" if side == "Left" else "CuffR"
    hf, hh = frame(hb), head(hb)
    a = hf[:, 0]
    segs = [(cb, hh - a * 1.2, hh - a * 0.10), (hb, hh - a * 0.10, hh + a * 0.22)]
    for f in FINGERS:
        segs.append((f"{side}Hand{f}0", head(f"{side}Hand{f}0"), head(f"{side}Hand{f}1")))
    for f in FINGERS + ("Thumb",):
        names = [f"{side}Hand{f}{k}" for k in (1, 2, 3)]
        pts, _ = chain(names)
        segs += [(n, pts[k], pts[k + 1]) for k, n in enumerate(names)]
    P = co(ob)
    dist = np.empty((len(P), len(segs)))
    for i, (_, p0, p1) in enumerate(segs):
        v = p1 - p0
        t = np.clip((P - p0) @ v / np.dot(v, v), 0.0, 1.0)
        dist[:, i] = np.linalg.norm(P - (p0 + t[:, None] * v), axis=1)
    w = np.exp(-((dist - dist.min(1, keepdims=True)) / GLOVE_BLEND) ** 2)
    order = np.argsort(-w, 1)[:, :3]
    for vi in range(len(P)):
        top = [(segs[j][0], w[vi, j]) for j in order[vi] if w[vi, j] > 0.03]
        total = sum(x for _, x in top)
        merged = {}
        for name, x in top:
            merged[name] = merged.get(name, 0.0) + x / total
        for name, x in merged.items():
            g = ob.vertex_groups.get(name) or ob.vertex_groups.new(name=name)
            g.add([vi], x, "REPLACE")


# ---------------------------------------------------------------- 鋲・小札を寝かせる（元の模型の座標）
def rivet_into(ob, centre, normal, r=RIVET_R):
    """部品の網へ、丸い鋲の頭を足す（面に少し沈める）。"""
    n = nrm(normal)
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    new = bmesh.ops.create_uvsphere(bm, u_segments=10, v_segments=6, radius=1.0)["verts"]
    x = nrm(np.cross(n, [0.0, 0.0, 1.0]) if abs(n[2]) < 0.9 else np.cross(n, [1.0, 0.0, 0.0]))
    y = np.cross(n, x)
    m = np.eye(4)
    m[:3, :3] = np.column_stack([x * r, y * r, n * r * RIVET_FLAT])
    m[:3, 3] = centre - n * r * RIVET_SINK
    bmesh.ops.transform(bm, matrix=Matrix(m.tolist()), verts=new)
    bm.to_mesh(ob.data)
    bm.free()


def outer_point(ob, x, z, sgn, reach=0.03):
    """部品の外の面（甲の側）の、(x, z) の所の点。近い頂点のうち一番外のもの。"""
    p = co(ob)
    near = (np.abs(p[:, 0] - x) < reach) & (np.abs(p[:, 2] - z) < reach * 2)
    if not near.any():
        near = np.argsort(np.hypot(p[:, 0] - x, p[:, 2] - z))[:6]
    q = (p[near, 1] * sgn).max()
    return np.array([x, q * sgn, z])


def add_rivets(h, sgn, axis_q, x_c):
    def radial(pt):
        return np.array([pt[0] - x_c, pt[1] - sgn * axis_q, 0.0])

    cuff = h["cuff"]
    p = co(cuff)
    zt, zb = p[:, 2].max(), p[:, 2].min()
    for z in np.linspace(zb + 0.14, zt - 0.14, 3):
        band = np.abs(p[:, 2] - z) < 0.08
        for x in (p[band, 0].min() + RIVET_INSET, p[band, 0].max() - RIVET_INSET):
            pt = outer_point(cuff, x, z, sgn)
            rivet_into(cuff, pt, radial(pt))
    for lame in h["lames"]:
        lp = co(lame)
        z = lp[:, 2].mean()
        for x in (lp[:, 0].min() + RIVET_INSET, lp[:, 0].max() - RIVET_INSET):
            pt = outer_point(lame, x, z, sgn)
            rivet_into(lame, pt, radial(pt), RIVET_R * 0.8)
    back = h["back"]
    bp = co(back)
    for x in (bp[:, 0].min() + RIVET_INSET * 1.6, bp[:, 0].max() - RIVET_INSET * 1.6):
        pt = outer_point(back, x, bp[:, 2].max() - 0.07, sgn)
        rivet_into(back, pt, radial(pt))


def lay_down(ob, sgn, degrees):
    """斜めに立った小札を、付け根（上・内側）を軸に寝かせる（先の端を手のひらの側へ下げる）。"""
    p = co(ob)
    q = p[:, 1] * sgn
    z0, q0 = p[:, 2].max(), q.min()
    u, v = z0 - p[:, 2], q - q0
    th = math.radians(degrees)
    u2, v2 = u * math.cos(th) + v * math.sin(th), -u * math.sin(th) + v * math.cos(th)
    p[:, 2], p[:, 1] = z0 - u2, (q0 + v2) * sgn
    ob.data.vertices.foreach_set("co", p.ravel())
    ob.data.update()

# ---------------------------------------------------------------- 重み・属性
def weigh(ob, weights):
    """weights：{骨の名前: 重み} を全部の頂点に、または関数（頂点の座標 → {骨: 重み}）。"""
    if callable(weights):
        for v in ob.data.vertices:
            for bone, w in weights(np.array(v.co)).items():
                g = ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
                g.add([v.index], w, "REPLACE")
    else:
        for bone, w in weights.items():
            g = ob.vertex_groups.get(bone) or ob.vertex_groups.new(name=bone)
            g.add([v.index for v in ob.data.vertices], w, "REPLACE")


def tag(ob, node, color):
    me = ob.data
    attr = me.attributes.new("node", "INT", "FACE")
    attr.data.foreach_set("value", [node] * len(me.polygons))
    ca = me.color_attributes.new("ID", "FLOAT_COLOR", "CORNER")
    ca.data.foreach_set("color", list(color) * len(me.loops))


def build_hand(side, h, node_hand, node_cuff):
    sgn = h["sgn"]
    back = h["back"]
    p_back = co(back)
    x_c = p_back[:, 0].mean()
    q_core = dorsal_inner(back, sgn, x_c) - CORE_DORSAL
    z_w = p_back[:, 2].max()
    z_k = max(co(o)[:, 2].max() for col in h["fingers"].values() for o in col[:1])
    wrist = np.array([x_c, sgn * q_core, z_w])
    sf = source_frame(sgn)
    hand_bone, cuff_bone = f"{side}Hand", "CuffL" if side == "Left" else "CuffR"
    hf, hh = frame(hand_bone), head(hand_bone)
    knuckle_along = np.mean([np.dot(head(f"{side}Hand{n}1") - hh, hf[:, 0]) for n in FINGERS])
    k_along = knuckle_along / (SCALE * (z_w - z_k))
    print("[gauntlet] %s: wrist %s knuckle line %.3f (source %.3f, along stretch %.3f)" % (
        side, np.round(wrist, 3), knuckle_along, z_w - z_k, k_along))

    # 元の模型の座標のうちに：袖口を閉じる板を作り（鋲より先に。縁の形を読む）、鋲を打ち、指と親指の小札を少し寝かせる。
    closer = cuff_closer(h["cuff"], sgn, q_core)
    add_rivets(h, sgn, q_core, x_c)
    for col in list(h["fingers"].values()) + [h["thumb"][1:]]:
        for o in col:
            lay_down(o, sgn, TILE_LAY_DEG)

    # 袖口（手の甲の側の板＋閉じる板）・手首の小札：Cuff の骨の向き（肘へ向く y の逆が長さの向き）。
    cf = frame(cuff_bone)
    cf = np.column_stack([-cf[:, 0], cf[:, 1], -cf[:, 2]])
    m_cuff = affine(head(cuff_bone), cf, SCALE * np.ones(3), sf, wrist)
    steel, leather = [], []
    for o in [h["cuff"], closer] + h["lames"]:
        apply(o, m_cuff)
    weigh(h["cuff"], {cuff_bone: 1.0})
    weigh(closer, {cuff_bone: 1.0})
    for o, w in zip(h["lames"], LAME_CUFF_W):
        weigh(o, {cuff_bone: w, hand_bone: 1.0 - w})
    for o in [h["cuff"], closer] + h["lames"]:
        tag(o, node_cuff, ID_STEEL)
        steel.append(o)

    # 手の甲の板：手の骨。長さは指の付け根の線まで伸ばし、横幅は骨の指の並びに寄せる。
    apply(back, affine(hh, hf, SCALE * np.array([k_along, 1.0, HAND_SIDE_K]), sf, wrist))
    weigh(back, {hand_bone: 1.0})
    tag(back, node_hand, ID_STEEL)
    steel.append(back)

    # 指の小札：指ごとの芯（元の x と、小札の内側から FINGER_CORE 下）を骨の並びへ。
    for f in FINGERS:
        col = h["fingers"][f]
        p = np.concatenate([co(o) for o in col])
        axis = (p[:, 0].mean(), sgn * (min(dorsal_inner(o, sgn) for o in col) - FINGER_CORE))
        names = [f"{side}Hand{f}{k}" for k in (1, 2, 3)]
        for o, bone in fit_chain(col, names, axis, sgn, FINGER_CORE, width_k=FINGER_WIDTH_K).items():
            weigh(o, {bone: 1.0})
            tag(o, node_hand, ID_STEEL)
            steel.append(o)

    # 親指：板と 3 節。甲の向き＝親指の板の面の向き（板の点の広がりの一番薄い向き。手の芯から外へ向ける）。
    plate = h["thumb"][0]
    pp = co(plate)
    pc = pp.mean(0)
    xy = pp[:, :2] - pc[:2]
    _, vecs = np.linalg.eigh(xy.T @ xy)
    normal = np.array([vecs[0, 0], vecs[1, 0], 0.0])
    if np.dot(normal, pc - np.array([x_c, sgn * q_core, pc[2]])) < 0.0:
        normal = -normal
    t_axis_pt = pc - normal * (np.ptp(pp @ normal) / 2 + THUMB_CORE)
    names = [f"{side}HandThumb{k}" for k in (1, 2, 3)]
    for o, bone in fit_chain(h["thumb"], names, t_axis_pt[:2], sgn, THUMB_CORE, dorsal=normal,
                             first_on_bone=True).items():
        weigh(o, {bone: 1.0})
        tag(o, node_hand, ID_STEEL)
        steel.append(o)

    # 革の手袋：手のひら・指・親指・手首から前腕まで 1 つの網。
    hand_glove = glove(side)
    glove_weights(hand_glove, side)
    tag(hand_glove, node_hand, ID_LEATHER)
    leather.append(hand_glove)

    # 手の甲の彫りの面（外を向いた面）。
    me = back.data
    me.update()
    outward = largest_patch(me, [poly.index for poly in me.polygons if np.dot(np.array(poly.normal), hf[:, 1]) > 0.55])
    ca = me.color_attributes["ID"]
    for i in outward:
        for li in me.polygons[i].loop_indices:
            ca.data[li].color = ID_ENGRAVE
    eng = me.attributes.new("engrave", "BOOLEAN", "FACE")
    eng.data.foreach_set("value", [poly.index in set(outward) for poly in me.polygons])
    return steel, leather, dict(origin=hh.tolist(), frame=hf.tolist())


def largest_patch(me, faces):
    """面の集まりのうち、頂点でつながった一番大きなまとまり（手の甲の板の上の鋲の頭を除く）。"""
    vmap = {}
    for f in faces:
        for v in me.polygons[f].vertices:
            vmap.setdefault(v, []).append(f)
    seen, best = set(), []
    for f in faces:
        if f in seen:
            continue
        stack, comp = [f], []
        seen.add(f)
        while stack:
            x = stack.pop()
            comp.append(x)
            for v in me.polygons[x].vertices:
                for g in vmap[v]:
                    if g not in seen:
                        seen.add(g)
                        stack.append(g)
        if len(comp) > len(best):
            best = comp
    return best


def finish_normals(obs, angle):
    for o in obs:
        me = o.data
        for poly in me.polygons:
            poly.use_smooth = True
        me.set_sharp_from_angle(angle=math.radians(angle))


def join(obs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in obs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = obs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = ob.data.name = name
    return ob


# ---------------------------------------------------------------- UV
def unwrap(ob, plates):
    """全体を角度で分けて展開し、手の甲の外の面だけは手の甲の向きに平らに写し直してから、まとめて詰める。
    詰めると島は動く（大きさ・向き・場所）ので、詰めた後に「板の座標 → UV」の変換を島ごとに測って layout に書く
    （engrave_pattern.py がそれで彫りを描く）。手の甲の島は ENGRAVE_TEXEL 倍に広げてから詰め、彫りの画素を多くする。"""
    me = ob.data
    me.uv_layers.new(name="UVMap")
    me.uv_layers.active = me.uv_layers["UVMap"]
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(55), island_margin=UV_MARGIN, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    # 編集の切り替えで UV の層の参照は古くなるので、ここで取り直す。
    uv = me.uv_layers["UVMap"]
    node = np.empty(len(me.polygons), np.int32)
    me.attributes["node"].data.foreach_get("value", node)
    eng = np.empty(len(me.polygons), bool)
    me.attributes["engrave"].data.foreach_get("value", eng)
    # 展開の島の大きさの目安：全体の UV の面積と 3D の面積の比。
    uv_all = np.empty(len(me.loops) * 2)
    uv.data.foreach_get("uv", uv_all)
    layout, planar = {}, {}
    for side, info in plates.items():
        hf = np.array(info["frame"])
        faces = [p for p in me.polygons if eng[p.index] and node[p.index] == info["node"]]
        loops = [li for p in faces for li in p.loop_indices]
        pts = np.array([me.vertices[me.loops[li].vertex_index].co[:] for li in loops])
        lat = pts @ hf[:, 2]
        alo = pts @ hf[:, 0]
        span = max(np.ptp(lat), np.ptp(alo))
        # 横を x、長さ（手首 → 指の付け根）を y。右手は横を反し、左右で同じ絵が同じ向きに乗るようにする。
        flip = -1.0 if side == "Right" else 1.0
        lat_c, alo_c = (lat.max() + lat.min()) / 2, (alo.max() + alo.min()) / 2
        xy = np.column_stack([0.5 + flip * (lat - lat_c) / span, 0.5 + (alo - alo_c) / span])
        # 仮の置き場所：smart_project と同じ縮尺（m → UV）× ENGRAVE_TEXEL。
        k = smart_scale(me, uv) * span * ENGRAVE_TEXEL
        for li, (x, y) in zip(loops, xy):
            uv.data[li].uv = (x * k, y * k)
        planar[side] = (loops, xy)
        layout[side] = dict(span_m=float(span), lateral_center=float(lat_c), along_center=float(alo_c),
                            lateral_flip=flip, frame=info["frame"],
                            lateral_extent=[float(lat.min() - lat_c), float(lat.max() - lat_c)],
                            along_extent=[float(alo.min() - alo_c), float(alo.max() - alo_c)])
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.select_all(action="SELECT")
    bpy.ops.uv.pack_islands(rotate=True, rotate_method="CARDINAL", scale=True, margin=UV_MARGIN,
                            shape_method="CONCAVE")
    bpy.ops.object.mode_set(mode="OBJECT")
    uv = me.uv_layers["UVMap"]
    for side, (loops, xy) in planar.items():
        got = np.array([uv.data[li].uv[:] for li in loops])
        X = np.column_stack([xy, np.ones(len(xy))])
        A = np.linalg.lstsq(X, got, rcond=None)[0]
        err = float(np.abs(X @ A - got).max())
        assert err < 1e-4, ("手の甲の島が詰める時に崩れた", side, err)
        layout[side]["uv_affine"] = A.T.tolist()   # uv = A · (x, y, 1)
        layout[side]["uv_bounds"] = [got.min(0).tolist(), got.max(0).tolist()]
        print("[gauntlet] %s back-plate island at %s..%s" % (side, np.round(got.min(0), 3), np.round(got.max(0), 3)))
    return layout


def smart_scale(me, uv):
    """smart_project の縮尺（UV の長さ ÷ 3D の長さ）の中央値。"""
    ratios = []
    for poly in list(me.polygons)[:4000:7]:
        li = list(poly.loop_indices)
        a, b = me.loops[li[0]].vertex_index, me.loops[li[1]].vertex_index
        d3 = (me.vertices[a].co - me.vertices[b].co).length
        d2 = (uv.data[li[0]].uv - uv.data[li[1]].uv).length
        if d3 > 1e-5:
            ratios.append(d2 / d3)
    return float(np.median(ratios))


# ---------------------------------------------------------------- 書き出し
def export_npz(ob, path):
    """ノードごとに、三角・角ごとの法線と UV で頂点を分け、骨 4 本までの重みを付ける（Blender の座標）。"""
    tmp = ob.copy()
    tmp.data = ob.data.copy()
    bpy.context.scene.collection.objects.link(tmp)
    bm = bmesh.new()
    bm.from_mesh(tmp.data)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    bm.to_mesh(tmp.data)
    bm.free()
    me = tmp.data
    me.update()
    groups = [g.name for g in tmp.vertex_groups]
    vw = []
    for v in me.vertices:
        ws = sorted(((g.weight, groups[g.group]) for g in v.groups if g.weight > 1e-4), reverse=True)[:4]
        total = sum(w for w, _ in ws)
        vw.append([(n, w / total) for w, n in ws])
    node = np.empty(len(me.polygons), np.int32)
    me.attributes["node"].data.foreach_get("value", node)
    normals = np.array([n.vector[:] for n in me.corner_normals])
    uvs = np.empty(len(me.loops) * 2)
    me.uv_layers["UVMap"].data.foreach_get("uv", uvs)
    uvs = uvs.reshape(-1, 2)
    data = {}
    for nid in sorted(set(node.tolist())):
        keys, verts, tris = {}, [], []
        for poly in me.polygons:
            if node[poly.index] != nid:
                continue
            tri = []
            for li in poly.loop_indices:
                vi = me.loops[li].vertex_index
                key = (vi, tuple(np.round(normals[li], 5)), tuple(np.round(uvs[li], 6)))
                if key not in keys:
                    keys[key] = len(verts)
                    verts.append((vi, normals[li], uvs[li]))
                tri.append(keys[key])
            tris.append(tri)
        pos = np.array([me.vertices[vi].co[:] for vi, _, _ in verts], np.float32)
        nor = np.array([n for _, n, _ in verts], np.float32)
        uv = np.array([u for _, _, u in verts], np.float32)
        jn = np.zeros((len(verts), 4), object)
        jw = np.zeros((len(verts), 4), np.float32)
        for i, (vi, _, _) in enumerate(verts):
            for k, (n, w) in enumerate(vw[vi]):
                jn[i, k], jw[i, k] = n, w
            for k in range(len(vw[vi]), 4):
                jn[i, k] = vw[vi][0][0]
        data[f"n{nid}_pos"] = pos
        data[f"n{nid}_nor"] = nor
        data[f"n{nid}_uv"] = uv
        data[f"n{nid}_jn"] = jn.astype(str)
        data[f"n{nid}_jw"] = jw
        data[f"n{nid}_idx"] = np.array(tris, np.uint32)
        print("[gauntlet] node %d: %d verts, %d tris" % (nid, len(verts), len(tris)))
    np.savez(path, **data)
    bpy.data.objects.remove(tmp)


def export_sp(ob, path):
    """Substance Painter に渡す網：鋼と革を別の物体（GauntletSteel・GauntletLeather）にして、同じ材質（1 つの貼図の組）
    のまま書き出す。Painter では層の組の形の型（物体ごと）で、鋼の層と革の層を分ける。"""
    me = ob.data
    ca = me.color_attributes["ID"]
    leather = set()
    for poly in me.polygons:
        c = ca.data[poly.loop_indices[0]].color
        if abs(c[0] - ID_LEATHER[0]) < 0.02 and abs(c[1] - ID_LEATHER[1]) < 0.02:
            leather.add(poly.index)
    parts = []
    for name, keep_leather in (("GauntletSteel", False), ("GauntletLeather", True)):
        part = ob.copy()
        part.data = me.copy()
        part.name = part.data.name = name
        bpy.context.scene.collection.objects.link(part)
        bm = bmesh.new()
        bm.from_mesh(part.data)
        bm.faces.ensure_lookup_table()
        bmesh.ops.delete(bm, geom=[f for f in bm.faces if (f.index in leather) != keep_leather], context="FACES")
        bm.to_mesh(part.data)
        bm.free()
        part.vertex_groups.clear()
        parts.append(part)
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"}, mesh_smooth_type="FACE",
                             use_mesh_modifiers=False, add_leaf_bones=False, axis_forward="-Z", axis_up="Y",
                             colors_type="LINEAR", bake_space_transform=False)
    for part in parts:
        bpy.data.objects.remove(part)


NODES = {0: "HandL", 1: "CuffLMesh", 2: "HandR", 3: "CuffRMesh"}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    parts = import_source()
    hands = classify(parts)
    plates, steel_all, leather_all = {}, [], []
    for side, (nh, nc) in (("Left", (0, 1)), ("Right", (2, 3))):
        steel, leather, info = build_hand(side, hands[side], nh, nc)
        info["node"] = nh
        plates[side] = info
        steel_all += steel
        leather_all += leather
    finish_normals(steel_all, SHARP_ANGLE)
    finish_normals(leather_all, 70.0)
    ob = join(steel_all + leather_all, "KnightGauntlet")
    mat = bpy.data.materials.new("KnightGauntlet")
    ob.data.materials.append(mat)
    layout = unwrap(ob, plates)
    layout["nodes"] = NODES
    (OUT / "KnightGauntlet_layout.json").write_text(json.dumps(layout, indent=1), encoding="utf-8")
    export_npz(ob, OUT / "KnightGauntlet_mesh.npz")
    export_sp(ob, OUT / "KnightGauntlet_sp.fbx")
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / "KnightGauntlet.blend"))
    print("[gauntlet] wrote", OUT)


main()
