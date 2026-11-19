"""ボスの外形の模型を Blender（背景モード）で作る時の共通の道具。

約束（エンジンは右手系・Y 上・+Z 前・+X がボスの左。glTF も同じ）：
- Blender は Z 上なので、書き出しで (x, y, z)_blender → (x, z, -y)_gltf になる。
  つまり Blender の -Y がエンジンの +Z（前）、Blender の +Z がエンジンの +Y（上）、+X はそのまま（ボスの左）。
- 単位は m。部品はノード（Blender の物体）で分け、ノードの変換は単位（位置 0・回転 0・拡大 1）にしておく。
  コードがノードごとに世界の変換を渡すので、形はノードの原点まわりに直接作る。
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector


def reset_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    return bpy.context.scene


def material(name, color, metallic=0.0, roughness=0.5, emission=None, emission_strength=0.0):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if emission is not None:
        bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def new_object(name, bm, mat, smooth=True, parent=None):
    """bmesh を物体にする。法線を外向きに揃え、材質を 1 つ付ける。"""
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = smooth
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    if mat is not None:
        mesh.materials.append(mat)
    if parent is not None:
        obj.parent = parent
    return obj


def join(name, objects):
    """複数の物体を 1 つのノードにまとめる（材質槽は物体ごとに残る）。"""
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.data.name = name
    return obj


def add_box(bm, size, center=(0, 0, 0), matrix=None):
    m = Matrix.Translation(center) @ Matrix.Diagonal((*size, 1.0))
    if matrix is not None:
        m = matrix @ m
    return bmesh.ops.create_cube(bm, size=1.0, matrix=m)["verts"]


def add_cylinder(bm, radius, depth, center=(0, 0, 0), axis="Y", segments=24, radius2=None, matrix=None):
    """軸に沿った円柱（radius2 を渡すと先細り）。"""
    rot = {"X": Matrix.Rotation(math.radians(90), 4, "Y"), "Y": Matrix.Rotation(math.radians(90), 4, "X"),
           "Z": Matrix.Identity(4)}[axis]
    m = Matrix.Translation(center) @ rot
    if matrix is not None:
        m = matrix @ m
    return bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius,
                                 radius2=radius if radius2 is None else radius2, depth=depth, matrix=m)["verts"]


def loft(bm, rings, cap_start=True, cap_end=True, closed=True):
    """rings：同じ点数の輪郭（Vector の列）の並び。隣り合う輪を面でつなぐ。"""
    vert_rings = [[bm.verts.new(p) for p in ring] for ring in rings]
    count = len(vert_rings[0])
    span = count if closed else count - 1
    for a, b in zip(vert_rings, vert_rings[1:]):
        for i in range(span):
            j = (i + 1) % count
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if cap_start:
        bm.faces.new(list(reversed(vert_rings[0])))
    if cap_end:
        bm.faces.new(vert_rings[-1])
    return vert_rings


def bevel_object(obj, width, segments=2, angle_degrees=30.0):
    """縁を面取りする（角にハイライトが乗って、作り物の角ばりが消える）。"""
    mod = obj.modifiers.new("Bevel", "BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(angle_degrees)
    mod.harden_normals = True
    return mod


def apply_modifiers(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    for mod in list(obj.modifiers):
        bpy.ops.object.modifier_apply(modifier=mod.name)


def finalize(obj, angle_degrees=38.0, recalc=True):
    """仕上げ：法線を外向きに揃え（閉じた殻になった後なので確実）、なめらかに塗り、角度の大きい辺だけ折れ目にする。
    板の縁がくっきり出て、丸い面はなめらかなまま。recalc=False は、帯ごとに頂点を分けて作った刀身のように、
    開いた面の向きを作った時のまま保ちたい物。"""
    mesh = obj.data
    if recalc:
        bm = bmesh.new()
        bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(mesh)
        bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    mesh.set_sharp_from_angle(angle=math.radians(angle_degrees))


def export_gltf(path, objects=None):
    """.gltf と .bin に分けて書き出す（エンジンの模型の取り込みは .gltf・.obj・.fbx だけで、.glb は読まない）。
    貼図（騎士の兜）は .gltf の隣の KasumiBushi フォルダへ出る（Assets/Models/KasumiBushi と同じ並び）。"""
    bpy.ops.object.select_all(action="DESELECT")
    for o in objects or bpy.context.scene.objects:
        o.select_set(True)
    bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLTF_SEPARATE", use_selection=True, export_yup=True,
                              export_apply=True, export_materials="EXPORT", export_tangents=True,
                              export_texture_dir="KasumiBushi", export_animations=False, export_skins=False)


def triangle_count(objects=None):
    total = 0
    deps = bpy.context.evaluated_depsgraph_get()
    for o in objects or bpy.context.scene.objects:
        if o.type != "MESH":
            continue
        mesh = o.evaluated_get(deps).to_mesh()
        total += sum(len(p.vertices) - 2 for p in mesh.polygons)
    return total


def render_preview(path, target=(0, 0, 0), distance=4.0, yaw_degrees=35.0, pitch_degrees=15.0, size=(900, 900),
                   lens=70.0, samples=32):
    """ざっと形を見る画像（Blender の中）。遊びの画面での判断は、エンジンの画像でやる。
    yaw は Blender の -Y（＝エンジンの前）から見て時計回り。"""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x, scene.render.resolution_y = size
    scene.render.film_transparent = False
    try:
        scene.eevee.taa_render_samples = samples
    except AttributeError:
        pass
    world = bpy.data.worlds.new("PreviewWorld")
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.36, 0.42, 1.0)
    world.node_tree.nodes["Background"].inputs[1].default_value = 0.7
    scene.world = world
    yaw, pitch = math.radians(yaw_degrees), math.radians(pitch_degrees)
    offset = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch))) * distance
    cam_data = bpy.data.cameras.new("PreviewCam")
    cam_data.lens = lens
    cam = bpy.data.objects.new("PreviewCam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = Vector(target) + offset
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    sun_data = bpy.data.lights.new("PreviewSun", "SUN")
    sun_data.energy = 4.0
    sun = bpy.data.objects.new("PreviewSun", sun_data)
    scene.collection.objects.link(sun)
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(yaw_degrees + 40))
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
    bpy.data.objects.remove(sun)
