"""霞武士の外形の模型（KasumiBushi.gltf）を Blender（背景モード）で作る。
使い方：blender --background --factory-startup --python make_kasumi.py -- <出力フォルダ> [preview=<名前,…>]
preview に katana・hand などを渡すと、その部品の画像を <出力>/preview_<名前>_*.png に出す。"""
import math
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
import bpy  # noqa: E402

import bb_common as bb  # noqa: E402
import bb_hand  # noqa: E402
import bb_katana  # noqa: E402
import bb_materials  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["."]
out = argv[0]
options = dict(a.split("=", 1) for a in argv[1:] if "=" in a)
previews = [p for p in options.get("preview", "").split(",") if p]

bb.reset_scene()
mats = bb_materials.make()
hilt, blade, blade_hot, katana_length = bb_katana.build(mats)
print("KATANA_LENGTH %.4f" % katana_length)

hand_r, wrist, elbow, back = bb_hand.build_grip(mats)
# コードが筒（籠手）を置くのに使う値（刀の座標）。BossBodySettings の既定値と合わせる。
print("GRIP_WRIST %.4f %.4f %.4f" % tuple(wrist))
print("GRIP_ELBOW %.4f %.4f %.4f" % tuple(elbow))
print("GRIP_BACK %.4f %.4f %.4f" % tuple(back))
hands_l = [bb_hand.build_free(mats, pose, "HandL_" + pose) for pose in ("Fist", "Open", "Seal")]
cuff = bb_hand.build_cuff(mats)

nodes = [hilt, blade, blade_hot, hand_r, cuff] + hands_l
for node in nodes:
    bb.finalize(node, recalc=node not in (blade, blade_hot))
for node in nodes:
    print("NODE %-16s %6d tris" % (node.name, bb.triangle_count([node])))
print("TRIANGLES", bb.triangle_count(nodes))
bb.export_gltf(out + "/KasumiBushi.gltf", nodes)


def show(only):
    for o in nodes:
        o.hide_render = o not in only


if "katana" in previews:
    show([hilt, blade])
    # Blender の座標：剣先は -Y の向き。全体・鍔の周り・切先を撮る。
    bb.render_preview(out + "/preview_katana_full.png", target=(0, -1.0, 0), distance=4.2, yaw_degrees=80,
                      pitch_degrees=8, size=(1400, 500), lens=50)
    bb.render_preview(out + "/preview_katana_hilt.png", target=(0, 0.05, 0), distance=1.5, yaw_degrees=62,
                      pitch_degrees=14, size=(1100, 800), lens=60)
    bb.render_preview(out + "/preview_katana_tip.png", target=(0, -2.25, 0.0), distance=0.9, yaw_degrees=85,
                      pitch_degrees=6, size=(1000, 600), lens=60)
    show([hilt, blade_hot])
    bb.render_preview(out + "/preview_katana_hot.png", target=(0, -1.0, 0), distance=4.2, yaw_degrees=80,
                      pitch_degrees=8, size=(1400, 500), lens=50)

if "hand" in previews:
    # 握り：刀と一緒に、手の甲の側・掌の側・上から
    show([hilt, blade, hand_r])
    for tag, yaw, pitch in (("back", 115, 12), ("palm", -70, 5), ("top", 100, 70), ("front", 15, 10)):
        bb.render_preview(out + "/preview_grip_%s.png" % tag, target=(0.05, 0.0, 0.05), distance=1.9, yaw_degrees=yaw,
                          pitch_degrees=pitch, size=(900, 800), lens=60)
    for hand in hands_l:
        show([hand])
        bb.render_preview(out + "/preview_%s_back.png" % hand.name, target=(0, -0.25, 0.0), distance=1.7,
                          yaw_degrees=40, pitch_degrees=50, size=(800, 700), lens=60)
        bb.render_preview(out + "/preview_%s_palm.png" % hand.name, target=(0, -0.25, 0.0), distance=1.7,
                          yaw_degrees=140, pitch_degrees=-35, size=(800, 700), lens=60)
    show([cuff])
    bb.render_preview(out + "/preview_cuff.png", target=(0, 0.3, 0.0), distance=2.0, yaw_degrees=140, pitch_degrees=35,
                      size=(800, 700), lens=60)
