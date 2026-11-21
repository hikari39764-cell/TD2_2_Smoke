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
import bb_greatsword  # noqa: E402
import bb_knight_armor  # noqa: E402
import bb_knight_helm  # noqa: E402
import bb_materials  # noqa: E402

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else ["."]
out = argv[0]
options = dict(a.split("=", 1) for a in argv[1:] if "=" in a)
previews = [p for p in options.get("preview", "").split(",") if p]
# 組み上げた姿の画像の置き場所（コードの既定値と合わせる。1 倍の体の局所 m）：右肩（Boss.bodyShoulder）、左手の待機の
# 手首（Boss.bodyLeftRestWrist）、草摺（枚数・腰の半径の横・前後・吊る高さ・裾の開きの度）。
SHOULDER = (-0.70, 2.72, 0.0)
LEFT_REST_WRIST = (1.0, 1.80, 0.35)
TASSETS = (7, 0.56, 0.44, 2.14, 15.0)

bb.reset_scene()
mats = bb_materials.make()
# 剣は西洋の大剣（騎士の両手剣）。ノードの名前と握りから剣先までの長さは刀の頃のまま（bb_greatsword.py）。
hilt, blade, blade_hot, katana_length = bb_greatsword.build(mats)
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
# 兜は騎士の兜（総監の模型と焼いた貼図。c-08863a）。兜の中の光と尾はエフェクト（Assets/Vfx/BossEyes.hvfx）で、
# その置き場所をここで出す（エフェクト資産の emitterTransform.position と合わせる）。
helm = bb_knight_helm.build()
visor = bb_knight_helm.visor_points()
print("VISOR_Y %.4f INSIDE_Z %.4f FRONT_Z %.4f SLITS_X %s BAND %.4f x %.4f" % (
    visor["y"], visor["inside_z"], visor["front_z"], " ".join("%.4f" % x for x in visor["slits_x"]),
    visor["band_width"], visor["band_height"]))
# 胸当てと左右の肩当ては騎士の物（総監の組みの模型と焼いた貼図。c-584825）。大きさと間隔は兜を基準に総監の組みのまま。
body = bb_knight_armor.build_body()
pauldrons = bb_knight_armor.build_pauldrons()
bb_knight_armor.report(body, pauldrons)
kusazuri = bb_armor.build_kusazuri(mats)
nodes += [body, helm] + pauldrons + [kusazuri]
# 灼けた鎧（フェーズ 2）：草摺は継ぎ目の糸が熾火に光る（c-857ae3）。兜・胸当て・肩当ては深い隙間が光る。
nodes.append(bb_armor.hot_copy(kusazuri, mats))
nodes += [bb_knight_helm.hot_copy(helm)] + bb_knight_armor.hot_copies(body, pauldrons)
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
    # 置き場所はコードの既定値（BossBodySettings.h）と同じ。
    place(body, T(bb_knight_armor.TORSO_CENTER))
    # 兜のうつむき（度。コードの Boss.bodyHeadPitchDegrees と同じ向き：+ で顎を引く）。
    pitch = float(options.get("head_pitch", "12"))
    head = T(bb_knight_helm.HEAD_CENTER) @ RX(pitch) @ Matrix.Scale(bb_knight_helm.HEAD_SCALE, 4)
    place(helm, head)
    hang = bb_knight_armor.PAULDRON_HANG
    for pauldron, side in zip(pauldrons, (-1, 1)):
        # 外へ向ける角度は Boss.bodySodeYawDegrees（90）、裾の開きは Boss.bodySodeFlareDegrees（0）。
        place(pauldron, T((side * -hang[0], hang[1], hang[2])) @ RY(side * 90.0))
    # 草摺：Boss.bodyTassetCount・bodyTassetRadiusSide・bodyTassetRadiusFront・bodyTassetHangHeight・
    # bodyTassetFlareDegrees と同じ（腰の輪郭はコードの WaistPoint と同じ角の丸い四角）。
    count, radius_side, radius_front, hang_y, flare = TASSETS
    for k in range(count):
        a = 2.0 * math.pi * k / count
        x, z = bb_armor.superpoint(a, radius_side, radius_front)
        place(kusazuri, T((x, hang_y, z)) @ RY(math.degrees(a)) @ RX(-flare), copy=True)
    # 刀と右手（待機の構え）、左手
    grip = Vector((-0.855, 2.372, 0.186))
    direction = Vector((-0.06, -0.94, 0.337)).normalized()
    sword_m = T(grip + direction * 0.07) @ aim_z(direction, (0, 1, -1))
    for o in (hilt, blade, hand_r):
        place(o, sword_m)
    wrist_r = sword_m @ wrist
    shoulder = Vector(SHOULDER)
    place(cuff, T(wrist_r) @ aim_z(-(shoulder - wrist_r).normalized(), sword_m.to_3x3() @ back))
    wrist_l = Vector(LEFT_REST_WRIST)
    place(hands_l[1], T(wrist_l) @ aim_z((0.10, -0.85, 0.50), (0.85, 0.25, 0.45)))
    place(cuff, T(wrist_l) @ aim_z(-(Vector((-shoulder.x, shoulder.y, shoulder.z)) - wrist_l).normalized(),
                                   (0.85, 0.25, 0.45)), copy=True)
    views = (("front", 0, 4, 10.5, (0, 0, 2.6)), ("quarter", 35, 8, 10.5, (0, 0, 2.6)),
             ("side", 90, 4, 10.5, (0, 0, 2.6)), ("back", 180, 6, 10.5, (0, 0, 2.6)),
             ("low", 20, -14, 9.0, (0, 0, 2.5)), ("head", 18, -8, 3.4, (0, 0.0, 3.55)))
    wanted = [v for v in options.get("views", "").split(",") if v]
    for tag, yaw, pitch, dist, target in [v for v in views if not wanted or v[0] in wanted]:
        bb.render_preview(out + "/preview_figure_%s.png" % tag, target=target, distance=dist, yaw_degrees=yaw,
                          pitch_degrees=pitch, size=(760, 900), lens=70)
