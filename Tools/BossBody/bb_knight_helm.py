"""騎士の兜（角付き・写実の貼図）を作る。c-08863a。

形は総監の模型（knight_helm/TD2_No2_boss.fbx）で、面の向きの直し・貼図・焼いた UV は knight_helm/bake_knight_helm.py が
作る（KnightHelm.obj と Assets/Models/KasumiBushi/KnightHelm_*.png）。ここでは向き・大きさ・高さをエンジンの座標に直す。
置き方は新しい兜そのものを基準にする（前の兜の鉢や中心には合わせない）。

部品の座標（エンジン：+Y 上、+Z 前、+X ボスの左。bb_armor と同じ）：
- KnightHelm        ：原点＝頭の置き場所（コードの Boss.bodyHeadCenter）。兜はそこから HELM_DROP だけ下げて、下の縁を
                      胴の襟の中へ沈める。向きはボスと同じ。
- KnightHelmHot     ：同じ形。フェーズ 2 で差し替える（深い隙間が熾火に光る）。
兜の中の光（面頬の 5 本の隙間から透ける一筋の光と、頭が動くと隙間から引き出される尾）は模型に入れず、エンジンの
エフェクト Assets/Vfx/BossEyes.hvfx が受け持つ。その置き場所は visor_points()（make_kasumi.py が VISOR_* として出す）。
"""
import math
import pathlib

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = pathlib.Path(__file__).resolve().parent
MESH = HERE / "knight_helm" / "KnightHelm.obj"
TEX_DIR = HERE.parents[1] / "Assets" / "Models" / "KasumiBushi"

# 元の模型の兜の中心（元の座標：前 +X・上 +Z）。
SOURCE_CENTER = Vector((0.0, 0.0, 1.699))
# 元の模型に掛ける大きさ。0.58 で兜の本体の幅 0.59 m・高さ 0.82 m、角の先から先まで 1.12 m（総監の見立て）。
# 体の上での大きさは、これにコードの Boss.bodyHeadScale と体の大きさが掛かる。
FIT_SCALE = 0.58
# 兜を頭の置き場所から下げる量（エンジンの m。FIT_SCALE を掛けた後）。兜の下の縁が胴の襟の中に沈み、首が空いて
# 浮いて見えないようにする。
HELM_DROP = 0.07
# 兜の中の光（エフェクト）の置き場所（元の模型の大きさの m・エンジンの軸。兜の中心から。FIT_SCALE と HELM_DROP を掛けて
# 置く）。面頬の 5 本の縦の隙間：横の中心は VISOR_SLITS_X、5 本ともに掛かる高さの帯は VISOR_BAND_TOP〜VISOR_BAND_BOTTOM。
# 隙間の底は z≈0.47（外の 2 本）〜0.52（真ん中）、面の表は z≈0.57〜0.62 なので、光る面（エフェクトの Visor Glow）と
# 尾の始まり（Visor Trail）は VISOR_INSIDE_Z（底より前・面の表より奥）に置き、面頬の板に隠れて隙間からだけ見える。
# 閃き（Visor Flare）は面の表の前 VISOR_FRONT_Z。
VISOR_SLITS_X = (-0.25, -0.1393, 0.0, 0.1393, 0.25)
VISOR_BAND_TOP = -0.15
VISOR_BAND_BOTTOM = -0.33
VISOR_INSIDE_Z = 0.538
VISOR_FRONT_Z = 0.63
# フェーズ 2 の熾火（深い隙間）の明るさ（物理の量。灼けた鎧の EmberVoid と同じ）。色は貼図 KnightHelm_Ember に焼いてある。
EMBER_STRENGTH = 2500.0


def _image(name, color_space):
    image = bpy.data.images.load(str(TEX_DIR / (name + ".png")), check_existing=True)
    image.name = name
    image.colorspace_settings.name = color_space
    return image


def _textured_material(name, ember=False):
    """貼図の材質。glTF の書き出しが読める形（色・metallicRoughness・法線・occlusion・発光）で繋ぐ。"""
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    bsdf = nodes["Principled BSDF"]

    def tex(image_name, color_space):
        node = nodes.new("ShaderNodeTexImage")
        node.image = _image(image_name, color_space)
        return node

    links.new(tex("KnightHelm_BaseColor", "sRGB").outputs["Color"], bsdf.inputs["Base Color"])
    orm = nodes.new("ShaderNodeSeparateColor")
    links.new(tex("KnightHelm_ORM", "Non-Color").outputs["Color"], orm.inputs[0])
    links.new(orm.outputs["Green"], bsdf.inputs["Roughness"])
    links.new(orm.outputs["Blue"], bsdf.inputs["Metallic"])
    normal = nodes.new("ShaderNodeNormalMap")
    links.new(tex("KnightHelm_Normal", "Non-Color").outputs["Color"], normal.inputs["Color"])
    links.new(normal.outputs["Normal"], bsdf.inputs["Normal"])
    # occlusion は書き出しの決まりの節「glTF Material Output」の Occlusion へ。
    group = bpy.data.node_groups.get("glTF Material Output")
    if group is None:
        group = bpy.data.node_groups.new("glTF Material Output", "ShaderNodeTree")
        group.interface.new_socket("Occlusion", in_out="INPUT", socket_type="NodeSocketFloat")
    settings = nodes.new("ShaderNodeGroup")
    settings.node_tree = group
    links.new(orm.outputs["Red"], settings.inputs["Occlusion"])
    if ember:
        links.new(tex("KnightHelm_Ember", "sRGB").outputs["Color"], bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = EMBER_STRENGTH
    return mat


def blender(x, y, z):
    """エンジンの (x, y, z) → Blender の座標。"""
    return Vector((x, -z, y))


def _fit_matrix():
    """元の座標（前 +X・上 +Z）→ Blender でのエンジンの座標（前 -Y・上 +Z）。中心を原点へ寄せて FIT_SCALE を掛け、
    HELM_DROP だけ下げる。"""
    return (Matrix.Translation(blender(0.0, -HELM_DROP, 0.0)) @ Matrix.Scale(FIT_SCALE, 4) @
            Matrix.Rotation(math.radians(-90.0), 4, "Z") @ Matrix.Translation(-SOURCE_CENTER))


def to_engine(x, y, z):
    """元の模型の大きさで兜の中心から測った (x, y, z) → 置いた後のエンジンの座標。"""
    return x * FIT_SCALE, y * FIT_SCALE - HELM_DROP, z * FIT_SCALE


def build():
    """兜（KnightHelm）。形と UV は KnightHelm.obj のまま、向き・大きさ・高さだけ直す。"""
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.obj_import(filepath=str(MESH), forward_axis="Y", up_axis="Z")
    helm = bpy.context.selected_objects[0]
    helm.name = helm.data.name = "KnightHelm"
    helm.data.transform(_fit_matrix() @ helm.matrix_world)
    helm.matrix_world = Matrix.Identity(4)
    helm.data.materials.clear()
    helm.data.materials.append(_textured_material("KnightHelm"))
    return helm


def hot_copy(helm):
    """灼けた兜（KnightHelmHot。フェーズ 2）：同じ形で、深い隙間（KnightHelm_Ember）が熾火に光る。"""
    hot = helm.copy()
    hot.data = helm.data.copy()
    hot.name = hot.data.name = "KnightHelmHot"
    helm.users_collection[0].objects.link(hot)
    hot.data.materials.clear()
    hot.data.materials.append(_textured_material("KnightHelmHot", ember=True))
    return hot


def front_hit(helm, x, y):
    """正面（+Z）から当てた射線の、兜の最初の当たりの z（エンジンの座標。当たらなければ None）。"""
    hit, location, _, _ = helm.ray_cast(blender(x, y, 3.0), Vector((0.0, 1.0, 0.0)))
    return -location.y if hit else None


def visor_points():
    """エフェクトの置き場所（エンジンの座標）：光る面と尾の始まりの高さ y、隙間の中の z、閃きの z、5 本の隙間の x。"""
    y = to_engine(0.0, (VISOR_BAND_TOP + VISOR_BAND_BOTTOM) * 0.5, 0.0)[1]
    return {"y": y, "inside_z": VISOR_INSIDE_Z * FIT_SCALE, "front_z": VISOR_FRONT_Z * FIT_SCALE,
            "slits_x": [x * FIT_SCALE for x in VISOR_SLITS_X],
            "band_width": (VISOR_SLITS_X[-1] - VISOR_SLITS_X[0] + 0.07) * FIT_SCALE,
            "band_height": (VISOR_BAND_TOP - VISOR_BAND_BOTTOM) * FIT_SCALE}
