"""騎士の胸当て（首当て付き）と左右の肩当て（写実の貼図）を作る。c-584825。

形は総監が組んだ模型（knight_helm/TD2_No2_boss_Body.fbx。兜・胸当て・左右の肩当てを並べた物）で、面の向きの直し・貼図・
焼いた UV は knight_helm/bake_knight_helm.py が作る（KnightBody.obj・KnightPauldron.obj と
Assets/Models/KasumiBushi/KnightBody_*.png・KnightPauldron_*.png）。OBJ は兜の元の模型の座標に乗っているので、ここでは兜と
同じ変換（bb_knight_helm の FIT_SCALE・HEAD_SCALE・HELM_DROP）で体の上へ置き、各ノードの原点へ寄せる。こうすると兜・胸当て・
肩当ての大きさと間隔が総監の組みのままになる（兜のうつむきだけはゲームの値で、胸当てと肩当ては傾けない）。

部品の座標（エンジン：+Y 上、+Z 前、+X ボスの左。1 倍の体の m。コードが Boss.bodyScale を掛ける）：
- KnightBody        ：原点＝TORSO_CENTER（コードの Boss.bodyTorsoCenter）。向きはボスと同じ。
- KnightPauldronR/L ：原点＝肩に吊る所 PAULDRON_HANG（右。左は左右を反転。コードの Boss.bodySodeHang）。+Z＝外向き（右は
                      ボスの右 -X、左は +X）。コードが外へ 90 度（Boss.bodySodeYawDegrees）回して置く。左右は鏡の形なので、
                      エンジンの置き方（拡大は一様・鏡にできない）に合わせてノードを分ける。
- 〜Hot              ：同じ形。フェーズ 2 で差し替える（深い隙間が熾火に光る）。
"""
import math
import pathlib

import bmesh
import bpy
from mathutils import Matrix, Vector

import bb_knight_helm as kh

HERE = pathlib.Path(__file__).resolve().parent
BODY_MESH = HERE / "knight_helm" / "KnightBody.obj"
PAULDRON_MESH = HERE / "knight_helm" / "KnightPauldron.obj"

# 胸当ての原点（1 倍の体の局所 m）。コードの Boss.bodyTorsoCenter と合わせる。胸当ての外接箱の真ん中あたり。
TORSO_CENTER = (0.0, 2.34, 0.04)
# 右の肩当てを吊る所（1 倍の体の局所 m）。コードの Boss.bodySodeHang と合わせる。肩当ての上の縁の真ん中あたり（揺れの支点）。
PAULDRON_HANG = (-0.67, 2.97, -0.02)


def helm_center():
    """兜の元の模型の中心（SOURCE_CENTER）が来る所（1 倍の体の局所 m。うつむきは入れない）。"""
    x, y, z = kh.HEAD_CENTER
    return Vector((x, y - kh.HELM_DROP * kh.HEAD_SCALE, z))


def source_to_body():
    """兜の元の座標の点 → 1 倍の体の局所（Blender の座標で表したエンジンの座標）。兜と同じ大きさ・同じ回し方。"""
    scale = kh.FIT_SCALE * kh.HEAD_SCALE
    return (Matrix.Translation(kh.blender(*helm_center())) @ Matrix.Scale(scale, 4) @
            Matrix.Rotation(math.radians(-90.0), 4, "Z") @ Matrix.Translation(-kh.SOURCE_CENTER))


def _import(path, name):
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.wm.obj_import(filepath=str(path), forward_axis="Y", up_axis="Z")
    obj = bpy.context.selected_objects[0]
    obj.name = obj.data.name = name
    obj.data.transform(source_to_body() @ obj.matrix_world)
    obj.matrix_world = Matrix.Identity(4)
    return obj


def _place(obj, matrix, prefix):
    obj.data.transform(matrix)
    obj.data.materials.clear()
    obj.data.materials.append(kh.textured_material(prefix, prefix))
    return obj


def build_body():
    """胸当て（KnightBody）。原点を TORSO_CENTER へ。"""
    body = _import(BODY_MESH, "KnightBody")
    return _place(body, Matrix.Translation(-kh.blender(*TORSO_CENTER)), "KnightBody")


def _keep_side(obj, right):
    """肩当ての片方（右＝ボスの右＝-X）だけを残す。"""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if (v.co.x > 0.0) == right], context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()


def build_pauldrons():
    """左右の肩当て（KnightPauldronR・KnightPauldronL）。原点を吊る所へ寄せ、外向きを +Z に回す（右は外が -X なので
    縦の軸まわりに +90 度、左は -90 度）。"""
    nodes = []
    for name, side in (("KnightPauldronR", -1.0), ("KnightPauldronL", 1.0)):
        obj = _import(PAULDRON_MESH, name)
        _keep_side(obj, right=side < 0.0)
        hang = kh.blender(side * -PAULDRON_HANG[0], PAULDRON_HANG[1], PAULDRON_HANG[2])
        turn = Matrix.Rotation(math.radians(-90.0 * side), 4, "Z")
        nodes.append(_place(obj, turn @ Matrix.Translation(-hang), "KnightPauldron"))
    return nodes


def hot_copies(body, pauldrons):
    """灼けた胸当て・肩当て（KnightBodyHot・KnightPauldronRHot・KnightPauldronLHot）。"""
    return [kh.hot_copy(body, "KnightBody")] + [kh.hot_copy(p, "KnightPauldron") for p in pauldrons]


def report(body, pauldrons):
    """置き場所を決める目安（1 倍の体の局所 m）を出す：胸当て・右の肩当ての外接箱（体の上に置いた所）。"""
    def box(obj, origin, turn_back=None):
        points = [v.co.copy() for v in obj.data.vertices]
        if turn_back is not None:
            points = [turn_back @ p for p in points]
        points = [p + kh.blender(*origin) for p in points]
        lo = [min(p[i] for p in points) for i in range(3)]
        hi = [max(p[i] for p in points) for i in range(3)]
        # Blender (x, y, z) → エンジン (x, z, -y)
        return ((lo[0], lo[2], -hi[1]), (hi[0], hi[2], -lo[1]))
    for tag, (lo, hi) in (("BODY", box(body, TORSO_CENTER)),
                          ("PAULDRON_R", box(pauldrons[0], PAULDRON_HANG,
                                             Matrix.Rotation(math.radians(-90.0), 3, "Z")))):
        print("%s x %.3f..%.3f y %.3f..%.3f z %.3f..%.3f" % (tag, lo[0], hi[0], lo[1], hi[1], lo[2], hi[2]))
