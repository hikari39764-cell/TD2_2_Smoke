"""霞武士の外形の模型（KasumiBushi.gltf）を Blender（背景モード）で作る。
使い方：blender --background --factory-startup --python make_kasumi.py -- <出力フォルダ> [preview=<名前,…>]
preview に katana・hand などを渡すと、その部品の画像を <出力>/preview_<名前>_*.png に出す。"""
import math
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, __file__.replace("\\", "/").rsplit("/", 1)[0])
import bpy  # noqa: E402

import bb_armor  # noqa: E402
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
do = bb_armor.build_do(mats)
kabuto = bb_armor.build_kabuto(mats)
eyes_mist = bb_armor.build_eyes(mats, "EyesMist", "eye_mist")
eyes_fire = bb_armor.build_eyes(mats, "EyesFire", "eye_fire")
sode = bb_armor.build_sode(mats)
kusazuri = bb_armor.build_kusazuri(mats)
nodes += [do, kabuto, eyes_mist, eyes_fire, sode, kusazuri]
# 灼けた鎧（フェーズ 2。継ぎ目の糸と隙間の闇が熾火に光る。c-857ae3）
nodes += [bb_armor.hot_copy(node, mats) for node in (do, kabuto, sode, kusazuri)]
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

if "figure" in previews:
    # 組み上げた姿（コードの待機の置き場所と同じ並べ方）。書き出しの後なので、ここで動かしても模型には影響しない。
    from mathutils import Matrix, Vector

    def place(obj, engine_matrix, copy=False):
        target = obj
        if copy:
            target = obj.copy()
            bpy.context.scene.collection.objects.link(target)
        target.matrix_world = bb_hand.blender_matrix(engine_matrix)
        target.hide_render = False
        return target

    def aim_z(direction, spine):
        z = Vector(direction).normalized()
        x = Vector(spine).cross(z).normalized()
        y = z.cross(x)
        return bb_hand.basis(Vector((0, 0, 0)), x, y, z)

    for o in nodes:
        o.hide_render = True
    T = Matrix.Translation
    RY = lambda deg: Matrix.Rotation(math.radians(deg), 4, "Y")
    RX = lambda deg: Matrix.Rotation(math.radians(deg), 4, "X")
    place(do, T((0, bb_armor.DO_CENTER_Y, 0)))
    head = T((0, bb_armor.HEAD_CENTER_Y + 0.04, 0.03)) @ Matrix.Scale(1.16, 4)
    place(kabuto, head)
    place(eyes_fire if "fire" in options.get("eyes", "mist") else eyes_mist, head)
    for side in (-1, 1):
        place(sode, T((side * 0.88, 3.14, 0.02)) @ RY(side * 76.0) @ RX(-24.0), copy=True)
    count = 7
    for k in range(count):
        a = 2.0 * math.pi * k / count
        x, y, z = bb_armor.torso_point(a, 2.02, out=0.03)
        place(kusazuri, T((x, y + bb_armor.DO_CENTER_Y, z)) @ RY(math.degrees(a)) @ RX(-15.0), copy=True)
    # 刀と右手（待機の構え）、左手
    grip = Vector((-0.855, 2.372, 0.186))
    direction = Vector((-0.06, -0.94, 0.337)).normalized()
    sword_m = T(grip + direction * 0.07) @ aim_z(direction, (0, 1, -1))
    for o in (hilt, blade, hand_r):
        place(o, sword_m)
    wrist_r = sword_m @ wrist
    place(cuff, T(wrist_r) @ aim_z(-(Vector((-0.80, 3.05, 0.0)) - wrist_r).normalized(), sword_m.to_3x3() @ back))
    wrist_l = Vector((1.0, 1.80, 0.35))
    place(hands_l[1], T(wrist_l) @ aim_z((0.10, -0.85, 0.50), (0.85, 0.25, 0.45)))
    place(cuff, T(wrist_l) @ aim_z(-(Vector((0.80, 3.05, 0.0)) - wrist_l).normalized(), (0.85, 0.25, 0.45)), copy=True)
    views = (("front", 0, 4, 10.5, (0, 0, 2.6)), ("quarter", 35, 8, 10.5, (0, 0, 2.6)),
             ("side", 90, 4, 10.5, (0, 0, 2.6)), ("back", 180, 6, 10.5, (0, 0, 2.6)),
             ("low", 20, -14, 9.0, (0, 0, 2.5)), ("head", 18, -8, 3.4, (0, 0.0, 3.55)))
    wanted = [v for v in options.get("views", "").split(",") if v]
    for tag, yaw, pitch, dist, target in [v for v in views if not wanted or v[0] in wanted]:
        bb.render_preview(out + "/preview_figure_%s.png" % tag, target=target, distance=dist, yaw_degrees=yaw,
                          pitch_degrees=pitch, size=(760, 900), lens=70)
