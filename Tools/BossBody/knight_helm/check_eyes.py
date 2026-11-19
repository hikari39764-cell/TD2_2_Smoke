"""騎士の兜を、実機で崩れないか・兜の中の光（エフェクト）の置き場所が隙間に合っているかを、射線と面の向きで確かめる。
c-08863a。bake_knight_helm.py（形・面の向き）や bb_knight_helm.py（FIT_SCALE・HELM_DROP・VISOR_*）を変えたら流す。

使い方：blender --background --factory-startup --python Tools/BossBody/knight_helm/check_eyes.py -- [image=<画像.png>]
確かめること：
- 面の向き：全部の面が外を向いている（エンジンの glTF の読み込みは doubleSided を読まず片面で描くので、裏向きの面は
  実機で消える）。
- 光と尾の始まり（BossEyes の Visor Glow・Visor Trail）：5 本の隙間それぞれで、正面からの射線の最初の当たり（隙間の底）
  より前、左右の面の表より奥にある（面頬の板に隠れて、隙間の口からだけ見える）。
- 閃き（Visor Flare）：真ん中の面の表より前。
image を渡すと、正面から兜と置き場所（赤い小さな玉）を撮る。"""
import pathlib
import sys

sys.dont_write_bytecode = True
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import bmesh  # noqa: E402
import bpy  # noqa: E402

import bb_common as bb  # noqa: E402
import bb_knight_helm as kh  # noqa: E402
import bb_materials  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
options = dict(a.split("=", 1) for a in argv if "=" in a)
MARGIN = 0.004  # 光と隙間の底・面の表との最小の間（m）


def check_winding(helm):
    bm = bmesh.new()
    bm.from_mesh(helm.data)
    before = [f.normal.copy() for f in bm.faces]
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    inward = sum(1 for f, n in zip(bm.faces, before) if f.normal.dot(n) < 0.0)
    bm.free()
    print("FACES facing inward: %d / %d -> %s" % (inward, len(before), "OK" if inward == 0 else "NG"))
    return inward == 0


def plate_surface(helm, x, y):
    """隙間の左右 5 cm（元の大きさ）の所の、高い方の面の表の z。"""
    sides = [kh.front_hit(helm, x + d * kh.FIT_SCALE, y) for d in (-0.05, 0.05)]
    return max(s for s in sides if s is not None)


def check_visor(helm):
    v = kh.visor_points()
    ok = True
    for x in v["slits_x"]:
        floor = kh.front_hit(helm, x, v["y"])
        surface = plate_surface(helm, x, v["y"])
        slit_ok = floor is not None and floor + MARGIN < v["inside_z"] < surface - MARGIN
        ok = ok and slit_ok
        print("VISOR slit x %+.4f: floor z %.4f < light z %.4f < plate surface z %.4f -> %s" %
              (x, floor if floor is not None else -1.0, v["inside_z"], surface, "OK" if slit_ok else "NG"))
    surface = max(plate_surface(helm, 0.0, v["y"]), kh.front_hit(helm, 0.0, v["y"]) or -1.0)
    flare_ok = v["front_z"] > surface
    print("VISOR flare z %.4f in front of plate surface z %.4f -> %s" % (v["front_z"], surface, "OK" if flare_ok else "NG"))
    return ok and flare_ok


def picture(path):
    marker = bpy.data.materials.new("Marker")
    marker.use_nodes = True
    bsdf = marker.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Emission Color"].default_value = (1.0, 0.05, 0.05, 1.0)
    bsdf.inputs["Emission Strength"].default_value = 30.0
    v = kh.visor_points()
    for x, z in [(x, v["inside_z"]) for x in v["slits_x"]] + [(0.0, v["front_z"])]:
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.006, location=kh.blender(x, v["y"], z))
        bpy.context.object.data.materials.append(marker)
    bb.render_preview(path, target=kh.blender(0.0, v["y"], 0.0), distance=1.0, yaw_degrees=0,
                      pitch_degrees=0, size=(900, 700), lens=50)


bb.reset_scene()
bb_materials.make()
helm = kh.build()
bpy.context.view_layer.update()
results = [check_winding(helm), check_visor(helm)]
print("HELM_OK" if all(results) else "HELM_NOT_OK")
if "image" in options:
    picture(options["image"])
