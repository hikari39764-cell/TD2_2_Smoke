"""騎士の兜・胸当て・肩当て（総監の模型）の写実の貼図を、Blender（背景モード・Cycles）の手続きの材質から焼く。
c-08863a（兜）・c-584825（胸当て・肩当て）。
類魂（ダークファンタジー）の見た目：黒ずんだ鋼の地に、縁の擦れ（地の鋼が光る）・薄い磨き・引っかき傷・隙間の錆と下へ流れる
錆の筋・錆の穴・縁の欠け・煤（下ほど濃い）・上の面の埃・槌の凹みと打ち傷・鍛えた地の粒。粗さは高め。角は自然の角（根元は黒く、先へ行くほど骨の色。節の輪・縦の繊維・細い割れ）。

使い方：blender --background --factory-startup --python bake_knight_helm.py -- [part=helm|body|pauldron] [res=2048] [samples=64]
part ごとに出る物（<名前> は KnightHelm・KnightBody・KnightPauldron）：
- Assets/Models/KasumiBushi/<名前>_BaseColor.png（色。sRGB）
- Assets/Models/KasumiBushi/<名前>_Normal.png（接空間の法線。glTF と同じ OpenGL の向き＝緑が上）
- Assets/Models/KasumiBushi/<名前>_ORM.png（R＝AO、G＝粗さ、B＝金属。glTF の metallicRoughness と occlusion）
- Assets/Models/KasumiBushi/<名前>_Ember.png（フェーズ 2 の熾火の色。深い隙間だけ光る。sRGB）
- Tools/BossBody/knight_helm/<名前>.obj（焼いた UV の付いた模型。make_kasumi.py が読む。兜の元の模型の座標）
元の模型の UV は左右の半分（兜は 2 本の角も）が重なっていて、場所ごとの汚れを焼けないので、ここで作り直す（角度で分ける展開）。
座標はどの部品も兜の元の模型（TD2_No2_boss.fbx）の座標：前が +X、上が +Z、+Y がボスの左、兜の中心が (0, 0, 1.699)。
- helm：TD2_No2_boss.fbx の兜そのまま。
- body・pauldron：総監が組んだ TD2_No2_boss_Body.fbx（兜・胸当て・左右の肩当てを並べた物）から取り、その中の兜が元の兜に
  重なるように動かす（組みの兜の大きさで割り、位置を引く）。こうすると 3 つの部品の大きさと間隔が総監の組みのまま、兜と同じ
  座標に乗る（ゲームでの置き場所は bb_knight_armor.py）。肩当ては左右 2 つの塊を 1 つの貼図に焼き、ゲームの模型では分ける。"""
import math
import os
import pathlib
import sys

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
TEX_DIR = ROOT / "Assets" / "Models" / "KasumiBushi"
# 部品ごとの元の模型・その中の物体の名前（総監の組みの FBX の中の名前）・出す名前。
PARTS = {
    "helm": (HERE / "TD2_No2_boss.fbx", None, "KnightHelm"),
    "body": (HERE / "TD2_No2_boss_Body.fbx", "平面", "KnightBody"),
    "pauldron": (HERE / "TD2_No2_boss_Body.fbx", "立方体.001", "KnightPauldron"),
}
# 総監の組みの FBX の中の兜の物体の名前（これを元の兜に重ねる）。
SET_HELM = "円"

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
options = dict(a.split("=", 1) for a in argv if "=" in a)
PART = options.get("part", "helm")
SOURCE, SOURCE_OBJECT, NAME = PARTS[PART]
MESH_OUT = HERE / (NAME + ".obj")
RES = int(options.get("res", "2048"))
SAMPLES = int(options.get("samples", "64"))

# ---------------------------------------------------------------- 見た目の値（色は線形）
COAT_DARK = (0.014, 0.0135, 0.013)      # 黒ずんだ鋼（酸化の膜）の暗い所
COAT_LIGHT = (0.040, 0.037, 0.034)      # 同じ膜の明るい所
COAT_WARM = (0.046, 0.036, 0.028)       # 膜のむら（焼けた茶）
COAT_COOL = (0.030, 0.033, 0.038)       # 膜のむら（青み）
POLISH = (0.12, 0.116, 0.11)            # 薄く磨かれた所
BARE = (0.42, 0.40, 0.38)               # 縁の擦れ・傷（地の鋼）
DUST = (0.13, 0.115, 0.095)             # 上を向いた面に積もった埃と灰
RUST_DARK = (0.045, 0.016, 0.007)
RUST_LIGHT = (0.30, 0.095, 0.03)
GRIME = (0.012, 0.011, 0.010)           # 煤
HORN_STOPS = ((0.0, (0.008, 0.006, 0.005)), (0.55, (0.022, 0.014, 0.009)), (0.82, (0.075, 0.048, 0.026)),
              (1.0, (0.42, 0.36, 0.26)))  # 角：根元 0 → 先 1
EDGE_RADIUS = 0.012      # 縁を丸く見せる幅（m）。擦れの幅もこれで決まる
CAVITY_DISTANCE = 0.08   # 隙間の汚れを探す距離（m）
EMBER_DISTANCE = 0.06    # 熾火を入れる深い隙間を探す距離（m）
BUMP_STRENGTH = 1.0      # 凹凸（槌・打ち傷・錆の穴・欠け・傷・角の節）の強さ
# 粗さ（c-08863a の 2 回目で総監の指示により全体に上げた。油の切れた鋼で、光沢は縁の擦れにだけ少し残る）。
ROUGH_COAT = 0.70        # 黒ずんだ膜
ROUGH_COAT_VAR = 0.14    # 膜のむら
ROUGH_MICRO = 0.10       # 細かいむら（すすけ・指の脂・細かな擦り）
ROUGH_POLISH = 0.52      # 薄く磨かれた所
ROUGH_BARE = 0.36        # 擦れた縁・傷（前の 0.27 は夕方の逆光で冠が白く飛んだ）
ROUGH_HORN = 0.62        # 角
DUST_AMOUNT = 0.75       # 埃の濃さ（0 で無し）
HORN_RINGS = 11.0        # 角の節の数
SHARP_ANGLE = 35.0       # これより折れた辺は角を立てて塗る（度。元の模型の手で付けた法線の代わり）
EMBER_COLOR = (1.0, 0.24, 0.03)  # 熾火の色（前の灼けた胴の隙間の闇と同じ）。明るさは bb_knight_helm.EMBER_STRENGTH
# 煤が下ほど濃くなる高さの幅（元の座標の Z。下の値で最も濃く、上の値で無くなる）。兜は決めた値、胸当て・肩当ては部品の
# 高さから同じ割合（下の端から高さの 4 %〜54 %）で決める（main で入れる）。
GRIME_LOW = (1.05, 1.75)
AO_DISTANCE = 0.5        # 胸当て・肩当ての AO を探す距離（元の座標の m）


def import_source():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    # Blender 5.1 の FBX 読み込みは、模型に入っているライトの cast_shadow で止まるので、読む前に項目を足しておく。
    light = bpy.data.lights.new("_probe", "POINT")
    if not hasattr(light.cycles, "cast_shadow"):
        type(light.cycles).cast_shadow = bpy.props.BoolProperty(default=True)
    bpy.data.lights.remove(light)
    bpy.ops.import_scene.fbx(filepath=str(SOURCE))
    if SOURCE_OBJECT is not None:
        # 組みの兜を元の兜（位置 0・拡大 1・向きは同じ）に重ねる変換を、取り出す部品に掛ける。
        set_helm = bpy.data.objects[SET_HELM].matrix_world
        to_helm = (Matrix.Scale(1.0 / set_helm.to_scale()[0], 4) @ Matrix.Translation(-set_helm.translation))
        part = bpy.data.objects[SOURCE_OBJECT]
        part.data.transform(to_helm @ part.matrix_world)
        part.matrix_world = Matrix.Identity(4)
        print("[bake] set helm scale %.4f at %s" % (set_helm.to_scale()[0], tuple(round(v, 4) for v in set_helm.translation)))
        for o in list(bpy.data.objects):
            if o is not part:
                bpy.data.objects.remove(o)
    for o in list(bpy.data.objects):
        if o.type != "MESH":
            bpy.data.objects.remove(o)
    helm = [o for o in bpy.data.objects if o.type == "MESH"][0]
    helm.name = helm.data.name = NAME
    fix_winding(helm)
    return helm


def fix_winding(helm):
    """面の向きを外向きに揃える。元の模型は冠（前立）の 22 面が裏向きで、エンジンの glTF の読み込みは doubleSided を
    読まずに片面で描くので、実機で冠が消えて兜の真ん中が欠けて見えた（Blender の Cycles は裏も描くので気づきにくい）。
    裏返した面の手で付けた法線は向きが合わなくなるので捨て、角度（SHARP_ANGLE）で折れ目を付け直す。"""
    me = helm.data
    bm = bmesh.new()
    bm.from_mesh(me)
    before = [f.normal.copy() for f in bm.faces]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    flipped = sum(1 for f, n in zip(bm.faces, before) if f.normal.dot(n) < 0.0)
    bm.to_mesh(me)
    bm.free()
    if me.has_custom_normals:
        bpy.context.view_layer.objects.active = helm
        bpy.ops.mesh.customdata_custom_splitnormals_clear()
    for poly in me.polygons:
        poly.use_smooth = True
    me.set_sharp_from_angle(angle=math.radians(SHARP_ANGLE))
    print("[bake] faces turned outward:", flipped)


def loose_parts(bm):
    """つながった面のまとまり（大きい順）。いちばん大きいのが兜、残りが角。"""
    seen, parts = set(), []
    for face in bm.faces:
        if face.index in seen:
            continue
        stack, part = [face], []
        seen.add(face.index)
        while stack:
            f = stack.pop()
            part.append(f)
            for e in f.edges:
                for g in e.link_faces:
                    if g.index not in seen:
                        seen.add(g.index)
                        stack.append(g)
        parts.append(part)
    return sorted(parts, key=len, reverse=True)


def write_part_mask(helm):
    """色の属性 PartMask：R＝角か、G＝角の根元 0 → 先 1、B＝角の周りの角度（0〜1）。
    元の UV の角は先を中心にした扇なので、先からの UV の距離が角の長さの向きになる（節の輪がきれいに回る）。"""
    me = helm.data
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.faces.ensure_lookup_table()
    uv = me.uv_layers[0].data
    along, around, horn_faces = {}, {}, set()
    # 角は兜のいちばん大きい塊の外の塊。胸当て・肩当てには角が無い（塊はみな鋼）。
    for part in loose_parts(bm)[1:] if PART == "helm" else []:
        pole = max({v for f in part for v in f.verts}, key=lambda v: len(v.link_faces))
        loops = [li for f in part for li in me.polygons[f.index].loop_indices]
        tip_loops = [li for li in loops if me.loops[li].vertex_index == pole.index]
        tip = sum((Vector(uv[li].uv) for li in tip_loops), Vector((0, 0))) / len(tip_loops)
        reach = max((Vector(uv[li].uv) - tip).length for li in loops)
        for li in loops:
            d = Vector(uv[li].uv) - tip
            along[li] = 1.0 - min(1.0, d.length / reach)
            around[li] = (math.atan2(d.y, d.x) / (2.0 * math.pi)) % 1.0
        horn_faces.update(f.index for f in part)
    bm.free()
    attr = me.color_attributes.new("PartMask", "FLOAT_COLOR", "CORNER")
    for poly in me.polygons:
        horn = 1.0 if poly.index in horn_faces else 0.0
        for li in poly.loop_indices:
            attr.data[li].color = (horn, along.get(li, 0.0), around.get(li, 0.0), 1.0)


def unwrap(helm):
    me = helm.data
    old = me.uv_layers[0].name
    me.uv_layers.active = me.uv_layers.new(name="UVMap")
    bpy.context.view_layer.objects.active = helm
    helm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(60), island_margin=0.006, area_weight=0.0,
                             correct_aspect=True, scale_to_bounds=False)
    bpy.ops.uv.pack_islands(margin=0.004, rotate=True)
    bpy.ops.object.mode_set(mode="OBJECT")
    me.uv_layers.remove(me.uv_layers[old])
    me.uv_layers["UVMap"].active_render = True


class Graph:
    """材質のノードを短く書くための道具。"""

    def __init__(self, mat):
        self.nodes = mat.node_tree.nodes
        self.links = mat.node_tree.links
        self.nodes.clear()

    def node(self, kind, **props):
        n = self.nodes.new(kind)
        for key, value in props.items():
            setattr(n, key, value)
        return n

    def link(self, value, socket):
        if isinstance(value, (int, float)):
            socket.default_value = value
        elif isinstance(value, tuple):
            socket.default_value = (*value, 1.0) if len(value) == 3 else value
        else:
            self.links.new(value, socket)

    def math(self, op, a, b=None, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.link(a, n.inputs[0])
        if b is not None:
            self.link(b, n.inputs[1])
        return n.outputs[0]

    def mix(self, a, b, fac, blend="MIX"):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type=blend, clamp_result=True)
        self.link(fac, n.inputs[0])
        self.link(a, n.inputs[6])
        self.link(b, n.inputs[7])
        return n.outputs[2]

    def fmix(self, a, b, fac):
        n = self.node("ShaderNodeMix", data_type="FLOAT")
        self.link(fac, n.inputs[0])
        self.link(a, n.inputs[2])
        self.link(b, n.inputs[3])
        return n.outputs[0]

    def ramp(self, fac, stops):
        n = self.node("ShaderNodeValToRGB")
        elements = n.color_ramp.elements
        elements[0].position, elements[0].color = stops[0][0], (*stops[0][1], 1.0)
        elements[1].position, elements[1].color = stops[-1][0], (*stops[-1][1], 1.0)
        for position, color in stops[1:-1]:
            elements.new(position).color = (*color, 1.0)
        self.link(fac, n.inputs[0])
        return n.outputs[0]

    def step(self, fac, lo, hi, top=1.0):
        return self.ramp(fac, [(lo, (0, 0, 0)), (hi, (top, top, top))])

    def noise(self, vector, scale, detail=6.0, roughness=0.6, distortion=0.0):
        n = self.node("ShaderNodeTexNoise")
        for key, value in (("Scale", scale), ("Detail", detail), ("Roughness", roughness),
                           ("Distortion", distortion)):
            n.inputs[key].default_value = value
        self.link(vector, n.inputs["Vector"])
        return n.outputs["Fac"]


def masks(g):
    """汚れと傷の型（0〜1）。座標は元の模型の座標（前 +X・上 +Z）。"""
    m = {}
    tc = g.node("ShaderNodeTexCoord")
    geo = g.node("ShaderNodeNewGeometry")
    part = g.node("ShaderNodeSeparateColor")
    g.link(g.node("ShaderNodeAttribute", attribute_name="PartMask").outputs["Color"], part.inputs[0])
    m["horn"], m["along"], m["around"] = part.outputs[0], part.outputs[1], part.outputs[2]
    p = m["p"] = tc.outputs["Object"]
    world = geo.outputs["Position"]
    steel = g.math("SUBTRACT", 1.0, m["horn"])
    # 縁の擦れ：縁を丸めた法線と元の法線の差（出っ張った縁ほど大きい）を、ところどころ途切れさせる。
    bevel = g.node("ShaderNodeBevel", samples=16)
    bevel.inputs["Radius"].default_value = EDGE_RADIUS
    m["bevel"] = bevel.outputs["Normal"]
    dot = g.node("ShaderNodeVectorMath", operation="DOT_PRODUCT")
    g.link(bevel.outputs["Normal"], dot.inputs[0])
    g.link(geo.outputs["Normal"], dot.inputs[1])
    edge = g.math("SUBTRACT", g.math("MULTIPLY", g.math("SUBTRACT", 1.0, dot.outputs["Value"]), 22.0),
                  g.math("MULTIPLY", g.noise(p, 26.0, 8.0, 0.7, 0.6), 1.4))
    # 擦れは縁をなぞる白い線にしない：大きなむらで途切れさせ、幅も所々で変える。
    keep = g.ramp(g.noise(p, 4.0, 5.0, 0.6, 0.3), [(0.44, (0, 0, 0)), (0.62, (1, 1, 1))])
    m["edge"] = g.math("MULTIPLY", g.step(edge, 0.18, 0.50), keep, clamp=True)
    # 隙間：近い所だけの AO の暗さ。
    ao = g.node("ShaderNodeAmbientOcclusion", samples=16, only_local=True)
    ao.inputs["Distance"].default_value = CAVITY_DISTANCE
    cav = g.math("ADD", g.math("SUBTRACT", 1.0, ao.outputs["AO"]),
                 g.math("MULTIPLY", g.math("SUBTRACT", g.noise(p, 9.0, 6.0, 0.6, 0.2), 0.5), 0.35))
    m["cavity"] = g.step(cav, 0.08, 0.45)
    # 錆：隙間と、そこから下へ流れる筋。角には付けない。
    stretch = g.node("ShaderNodeVectorMath", operation="MULTIPLY")
    g.link(world, stretch.inputs[0])
    stretch.inputs[1].default_value = (14.0, 14.0, 1.6)
    streak = g.step(g.noise(stretch.outputs[0], 4.0, 4.0, 0.5), 0.52, 0.68)
    m["rust_noise"] = g.noise(p, 5.0, 10.0, 0.65, 0.3)
    rust = g.math("ADD", g.math("MULTIPLY", m["cavity"], 0.85),
                  g.math("MULTIPLY", streak, g.math("MULTIPLY", m["cavity"], 0.5)))
    rust = g.math("MULTIPLY", rust, g.ramp(m["rust_noise"], [(0.42, (0.15,) * 3), (0.62, (1, 1, 1))]), clamp=True)
    m["rust"] = g.math("MULTIPLY", rust, steel, clamp=True)
    # 煤：隙間・大きなむら・下ほど濃い。
    z = g.node("ShaderNodeSeparateXYZ")
    g.link(world, z.inputs[0])
    low = g.node("ShaderNodeMapRange", clamp=True)
    g.link(z.outputs["Z"], low.inputs[0])
    for i, v in ((1, GRIME_LOW[0]), (2, GRIME_LOW[1]), (3, 0.45), (4, 0.0)):
        low.inputs[i].default_value = v
    grime = g.math("ADD", g.math("ADD", g.math("MULTIPLY", m["cavity"], 0.6),
                                  g.math("MULTIPLY", g.noise(p, 2.2, 6.0, 0.55, 0.5), 0.55)), low.outputs[0])
    m["grime"] = g.step(grime, 0.35, 0.85)
    # 引っかき傷：1 方向に引き伸ばした細い線を、まばらに。
    mapping = g.node("ShaderNodeMapping")
    mapping.inputs["Rotation"].default_value = (0.3, 0.8, 0.2)
    mapping.inputs["Scale"].default_value = (120.0, 1.2, 1.2)
    g.link(p, mapping.inputs["Vector"])
    line = g.ramp(g.noise(mapping.outputs[0], 3.0, 2.0, 0.5),
                  [(0.0, (0, 0, 0)), (0.495, (0, 0, 0)), (0.5, (1, 1, 1)), (0.505, (0, 0, 0)), (1.0, (0, 0, 0))])
    sparse = g.step(g.noise(p, 3.0, 3.0, 0.5), 0.58, 0.72)
    m["scratch"] = g.math("MULTIPLY", g.math("MULTIPLY", g.math("MULTIPLY", line, 0.6), sparse), steel, clamp=True)
    mapping2 = g.node("ShaderNodeMapping")
    mapping2.inputs["Rotation"].default_value = (1.2, -0.4, 0.9)
    mapping2.inputs["Scale"].default_value = (160.0, 1.4, 1.4)
    g.link(p, mapping2.inputs["Vector"])
    line2 = g.ramp(g.noise(mapping2.outputs[0], 4.0, 2.0, 0.5),
                   [(0.0, (0, 0, 0)), (0.497, (0, 0, 0)), (0.5, (1, 1, 1)), (0.503, (0, 0, 0)), (1.0, (0, 0, 0))])
    sparse2 = g.step(g.noise(p, 4.0, 3.0, 0.5), 0.6, 0.74)
    m["scratch"] = g.math("MAXIMUM", m["scratch"], g.math("MULTIPLY", g.math("MULTIPLY", line2, sparse2), steel),
                          clamp=True)
    m["steel_var"] = g.noise(p, 4.0, 8.0, 0.6)
    m["tint"] = g.noise(p, 1.6, 4.0, 0.5, 0.3)
    m["micro"] = g.noise(p, 45.0, 8.0, 0.65)
    # 埃：上を向いた面（世界の Z）に、むらと隙間の近くで濃く積もる。
    up = g.node("ShaderNodeSeparateXYZ")
    g.link(geo.outputs["Normal"], up.inputs[0])
    dust = g.math("MULTIPLY", g.step(up.outputs["Z"], 0.25, 0.85), g.step(g.noise(p, 6.0, 6.0, 0.6, 0.3), 0.38, 0.70))
    dust = g.math("MULTIPLY", dust, g.math("ADD", 0.45, g.math("MULTIPLY", m["cavity"], 0.8)), clamp=True)
    m["dust"] = g.math("MULTIPLY", dust, DUST_AMOUNT, clamp=True)
    # 錆の穴（細かな点）：錆の所に多く、ほかにもまばらに。
    pits = g.node("ShaderNodeTexVoronoi", feature="F1")
    pits.inputs["Scale"].default_value = 90.0
    g.link(p, pits.inputs["Vector"])
    pit = g.ramp(pits.outputs["Distance"], [(0.0, (1, 1, 1)), (0.12, (0, 0, 0))])
    pit_where = g.math("ADD", g.math("MULTIPLY", m["rust"], 0.9), g.step(g.noise(p, 3.5, 3.0, 0.5), 0.6, 0.75, 0.35))
    m["pits"] = g.math("MULTIPLY", g.math("MULTIPLY", pit, pit_where), steel, clamp=True)
    # 縁の欠け：出っ張った縁に、ところどころ小さく欠けた打ち傷。
    chip = g.step(g.noise(p, 28.0, 4.0, 0.6, 0.5), 0.60, 0.70)
    m["chips"] = g.math("MULTIPLY", g.math("MULTIPLY", g.step(edge, 0.05, 0.25), chip), steel, clamp=True)
    m["polish"] = g.ramp(g.noise(p, 2.6, 5.0, 0.55, 0.2), [(0.56, (0, 0, 0)), (0.78, (0.45, 0.45, 0.45))])
    m["bare"] = g.math("MAXIMUM", g.math("MAXIMUM", m["edge"], g.math("MULTIPLY", m["scratch"], 0.55)),
                       g.math("MULTIPLY", m["chips"], 0.8), clamp=True)
    # 熾火（フェーズ 2）：深い隙間だけ。
    deep = g.node("ShaderNodeAmbientOcclusion", samples=32, only_local=True)
    deep.inputs["Distance"].default_value = EMBER_DISTANCE
    m["ember"] = g.math("MULTIPLY", g.step(g.math("SUBTRACT", 1.0, deep.outputs["AO"]), 0.30, 0.62), steel, clamp=True)
    horn_masks(g, m)
    return m


def horn_masks(g, m):
    """角の節の輪（根元ほどはっきり、先は滑らか）と、縦の繊維。"""
    wobble = g.math("MULTIPLY", g.noise(m["p"], 7.0, 3.0, 0.5), 0.9)
    rings = g.math("FRACT", g.math("ADD", g.math("MULTIPLY", m["along"], HORN_RINGS), wobble))
    shape = g.ramp(rings, [(0.0, (0, 0, 0)), (0.8, (1, 1, 1)), (1.0, (0, 0, 0))])
    m["ring_fade"] = g.math("POWER", g.math("SUBTRACT", 1.0, m["along"]), 0.3)
    m["ring"] = g.math("MULTIPLY", shape, m["ring_fade"])
    fiber = g.node("ShaderNodeCombineXYZ")
    g.link(g.math("MULTIPLY", m["around"], 60.0), fiber.inputs[0])
    g.link(g.math("MULTIPLY", m["along"], 3.0), fiber.inputs[1])
    m["fiber"] = g.noise(fiber.outputs[0], 1.0, 6.0, 0.6, 0.3)
    # 縦の細い割れ：繊維の向きに引き伸ばした細い線を、根元の方に多く。
    crack_v = g.node("ShaderNodeCombineXYZ")
    g.link(g.math("MULTIPLY", m["around"], 25.0), crack_v.inputs[0])
    g.link(g.math("MULTIPLY", m["along"], 1.2), crack_v.inputs[1])
    crack = g.ramp(g.noise(crack_v.outputs[0], 1.0, 3.0, 0.5),
                   [(0.0, (0, 0, 0)), (0.49, (0, 0, 0)), (0.5, (1, 1, 1)), (0.51, (0, 0, 0)), (1.0, (0, 0, 0))])
    m["crack"] = g.math("MULTIPLY", crack, m["ring_fade"])


def channels(g, m):
    """色・金属・粗さ・凹凸（法線）を組む。"""
    coat = g.mix(COAT_DARK, COAT_LIGHT, m["steel_var"])
    coat = g.mix(coat, g.mix(COAT_COOL, COAT_WARM, m["tint"]), 0.5)
    steel = g.mix(g.mix(coat, POLISH, m["polish"]), BARE, m["bare"])
    rust = g.mix(RUST_DARK, RUST_LIGHT, m["rust_noise"])
    steel = g.mix(g.mix(steel, rust, m["rust"]), GRIME, g.math("MULTIPLY", m["grime"], 0.6))
    steel = g.mix(steel, RUST_DARK, g.math("MULTIPLY", m["pits"], 0.8))
    groove = g.math("MULTIPLY", g.math("SUBTRACT", 1.0, m["ring"]), g.math("MULTIPLY", m["ring_fade"], 0.55))
    horn = g.mix(g.ramp(m["along"], HORN_STOPS), (0.006, 0.005, 0.004), groove)
    horn = g.mix(horn, (0.55, 0.5, 0.4), g.math("MULTIPLY", m["fiber"], 0.3), "OVERLAY")
    horn = g.mix(horn, (0.004, 0.003, 0.003), g.math("MULTIPLY", m["crack"], 0.8))
    horn = g.mix(horn, GRIME, g.math("MULTIPLY", m["cavity"], 0.5))
    color = g.mix(g.mix(steel, horn, m["horn"]), DUST, m["dust"])

    metal = g.fmix(g.fmix(0.45, 0.80, m["polish"]), 1.0, m["bare"])
    metal = g.fmix(g.fmix(metal, 0.0, m["rust"]), 0.2, g.math("MULTIPLY", m["grime"], 0.6))
    metal = g.fmix(g.fmix(metal, 0.2, m["pits"]), 0.0, m["horn"])
    metal = g.fmix(metal, 0.0, m["dust"])

    micro = g.math("MULTIPLY", g.math("SUBTRACT", m["micro"], 0.5), 2.0 * ROUGH_MICRO)
    rough = g.math("ADD", g.math("ADD", ROUGH_COAT, g.math("MULTIPLY", m["steel_var"], ROUGH_COAT_VAR)), micro)
    rough = g.fmix(g.fmix(g.fmix(rough, ROUGH_POLISH, m["polish"]), ROUGH_BARE, m["bare"]), 0.92, m["rust"])
    rough = g.fmix(g.fmix(rough, 0.9, g.math("MULTIPLY", m["grime"], 0.7)), 0.95, m["pits"])
    horn_rough = g.math("ADD", ROUGH_HORN, g.math("MULTIPLY", g.math("SUBTRACT", 1.0, m["ring"]), 0.25))
    horn_rough = g.math("SUBTRACT", horn_rough, g.math("MULTIPLY", m["along"], 0.12))
    rough = g.fmix(g.fmix(rough, horn_rough, m["horn"]), 0.96, m["dust"])
    rough = g.math("MINIMUM", g.math("MAXIMUM", rough, 0.0), 1.0)

    dents = g.node("ShaderNodeTexVoronoi", feature="SMOOTH_F1")
    dents.inputs["Scale"].default_value = 14.0
    dents.inputs["Smoothness"].default_value = 0.6
    g.link(m["p"], dents.inputs["Vector"])
    blows = g.node("ShaderNodeTexVoronoi", feature="SMOOTH_F1")  # 大きな打ち傷のへこみ
    blows.inputs["Scale"].default_value = 4.5
    blows.inputs["Smoothness"].default_value = 1.0
    g.link(m["p"], blows.inputs["Vector"])
    height = g.math("ADD", g.math("MULTIPLY", dents.outputs["Distance"], 0.6),
                    g.math("MULTIPLY", blows.outputs["Distance"], 0.8))
    height = g.math("ADD", height, g.math("MULTIPLY", g.noise(m["p"], 260.0, 6.0, 0.6), 0.06))  # 鍛えた地の粒
    height = g.math("ADD", height, g.math("MULTIPLY", g.math("MULTIPLY", m["rust_noise"], m["rust"]), 0.7))
    height = g.math("SUBTRACT", height, g.math("MULTIPLY", m["scratch"], 0.35))
    height = g.math("SUBTRACT", height, g.math("MULTIPLY", m["pits"], 0.5))
    height = g.math("SUBTRACT", height, g.math("MULTIPLY", m["chips"], 0.6))
    horn_height = g.math("ADD", g.math("MULTIPLY", m["ring"], 0.9), g.math("MULTIPLY", m["fiber"], 0.5))
    horn_height = g.math("SUBTRACT", horn_height, g.math("MULTIPLY", m["crack"], 0.4))
    bump = g.node("ShaderNodeBump")
    bump.inputs["Strength"].default_value = BUMP_STRENGTH
    bump.inputs["Distance"].default_value = 0.005
    g.link(g.fmix(height, horn_height, m["horn"]), bump.inputs["Height"])
    g.link(m["bevel"], bump.inputs["Normal"])
    return {"color": color, "metal": metal, "rough": rough, "normal": bump.outputs["Normal"],
            "ember": g.mix((0.0, 0.0, 0.0), EMBER_COLOR, m["ember"])}


def bake_all(g, ch):
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    scene.cycles.samples = SAMPLES
    prefs = bpy.context.preferences.addons["cycles"].preferences
    try:
        prefs.compute_device_type = "OPTIX"
        prefs.get_devices()
        for device in prefs.devices:
            device.use = True
        scene.cycles.device = "GPU"
    except Exception as error:  # GPU が無い PC では CPU で焼く
        print("[bake] CPU で焼く:", error)
    scene.render.bake.margin = 16
    scene.render.bake.margin_type = "EXTEND"
    bsdf = g.node("ShaderNodeBsdfPrincipled")
    for key, socket in (("color", "Base Color"), ("metal", "Metallic"), ("rough", "Roughness"), ("normal", "Normal")):
        g.link(ch[key], bsdf.inputs[socket])
    out = g.node("ShaderNodeOutputMaterial")
    emit = g.node("ShaderNodeEmission")
    target = g.node("ShaderNodeTexImage")
    g.nodes.active = target

    def bake(kind, source=None):
        image = bpy.data.images.new("bake_" + kind.lower(), RES, RES, alpha=False)
        image.colorspace_settings.name = "sRGB" if source is ch["color"] or source is ch["ember"] else "Non-Color"
        target.image = image
        for link in list(out.inputs["Surface"].links):
            g.links.remove(link)
        if source is None:
            g.link(bsdf.outputs[0], out.inputs["Surface"])
            bpy.ops.object.bake(type=kind)
        else:
            for link in list(emit.inputs["Color"].links):
                g.links.remove(link)
            g.link(source, emit.inputs["Color"])
            g.link(emit.outputs[0], out.inputs["Surface"])
            bpy.ops.object.bake(type="EMIT")
        print("[bake]", kind, "done")
        return image

    images = {key: bake("EMIT_" + key, ch[key]) for key in ("color", "metal", "rough", "ember")}
    if PART == "helm":
        images["ao"] = bake("AO")
    else:
        # 胸当て・肩当ては AO の節を光として焼く（距離は AO_DISTANCE）。肩当てで AO の焼き（type AO）が 15 分を越えて
        # 終わらなかったため（c-584825。原因は確かめていない。初めての焼きは GPU の準備に数分かかることがある）。
        ao = g.node("ShaderNodeAmbientOcclusion", samples=64)
        ao.inputs["Distance"].default_value = AO_DISTANCE
        images["ao"] = bake("EMIT_ao", ao.outputs["AO"])
    scene.render.bake.normal_space = "TANGENT"
    images["normal"] = bake("NORMAL")
    return images


def pixels(image):
    data = np.empty(image.size[0] * image.size[1] * 4, dtype=np.float32)
    image.pixels.foreach_get(data)
    return data.reshape(-1, 4)


def save(image_or_array, name, color_space="Non-Color"):
    if isinstance(image_or_array, np.ndarray):
        image = bpy.data.images.new(name, RES, RES, alpha=False)
        image.colorspace_settings.name = color_space
        image.pixels.foreach_set(image_or_array.reshape(-1))
    else:
        image = image_or_array
    image.filepath_raw = str(TEX_DIR / (name + ".png"))
    image.file_format = "PNG"
    image.save()
    print("[bake] wrote", image.filepath_raw)


def main():
    global GRIME_LOW
    helm = import_source()
    if PART != "helm":
        zs = [v.co.z for v in helm.data.vertices]
        low, high = min(zs), max(zs)
        GRIME_LOW = (low + 0.04 * (high - low), low + 0.54 * (high - low))
    write_part_mask(helm)
    unwrap(helm)
    mat = bpy.data.materials.new(NAME + "Bake")
    mat.use_nodes = True
    helm.data.materials.clear()
    helm.data.materials.append(mat)
    bpy.ops.object.select_all(action="DESELECT")
    helm.select_set(True)
    bpy.context.view_layer.objects.active = helm
    g = Graph(mat)
    images = bake_all(g, channels(g, masks(g)))
    TEX_DIR.mkdir(parents=True, exist_ok=True)
    save(images["color"], NAME + "_BaseColor")
    save(images["normal"], NAME + "_Normal")
    ao, rough, metal = pixels(images["ao"]), pixels(images["rough"]), pixels(images["metal"])
    save(np.stack([ao[:, 0], rough[:, 0], metal[:, 0], np.ones(len(ao), np.float32)], axis=1), NAME + "_ORM")
    save(images["ember"], NAME + "_Ember")
    # 焼いた UV の模型（元の座標のまま。軸の変換をしない）。色の属性は焼くためだけの物なので外す。
    helm.data.color_attributes.remove(helm.data.color_attributes["PartMask"])
    helm.data.materials.clear()
    bpy.ops.wm.obj_export(filepath=str(MESH_OUT), export_selected_objects=True, forward_axis="Y", up_axis="Z",
                          export_uv=True, export_normals=True, export_materials=False, export_colors=False,
                          apply_modifiers=True)
    print("[bake] wrote", MESH_OUT)


main()
