"""Substance Painter の中で走る：籠手（KnightGauntlet）の工程を作り、焼き、層を積み、エンジン用の貼図を書き出す。c-8b643c。

流し方：Painter を開き（td2_bridge が 127.0.0.1:9882 で待つ）、手元から
  python Library/Handover/boss_mat/bridge.py painter -c "STEP='new'; exec(open(r'<このファイル>', encoding='utf-8').read())"
STEP：new（工程を作る。非同期）→ bake（焼く。非同期、終わりは td2_bake_state）→ layers（層を積む）→ export（書き出す）。
入る物（ROOT 下）：
- Library/Handover/c-8b643c/build/KnightGauntlet_sp.fbx（build_knight_gauntlet.py。物体 GauntletSteel・GauntletLeather、材質 1 つ）
- Library/Handover/c-8b643c/sd/out/*.png（Designer の sd_gauntlet.py → render_sd.sh：彫りの 4 枚は貼図と同じ UV、鍛えの 2 枚は繰り返し）
出す物：EXPORT_DIR に KnightGauntlet_BaseColor・_ORM（R＝AO、G＝粗さ、B＝金属）・_Normal（OpenGL、高さ込み）。2048。
工程は Library/Handover/c-8b643c/sp/KnightGauntlet.spp に保存する（layers の終わり）。
色は sRGB で書く（騎士の兜・胸当ての焼いた貼図と同じ黒ずんだ鋼の色。bake_knight_helm.py の COAT_* を sRGB にした値の近く）。
"""
import math

import substance_painter.baking as baking
import substance_painter.event as event
import substance_painter.export as ex
import substance_painter.layerstack as ls
import substance_painter.project as project
import substance_painter.resource as res
import substance_painter.textureset as ts
from substance_painter.colormanagement import Color
from substance_painter.textureset import ChannelType as C
from substance_painter.textureset import MeshMapUsage

ROOT = "D:/PJ/TD2_2"
WORK = ROOT + "/Library/Handover/c-8b643c"
MESH = WORK + "/build/KnightGauntlet_sp.fbx"
SD_OUT = WORK + "/sd/out/"
EXPORT_DIR = globals().get("EXPORT_DIR", WORK + "/sp/tex")
SET = "KnightGauntlet"
DOC_RES = 2048          # 4096 で作って 2048 へ書き出すと、書き出しの途中で Painter が落ちた（2026-11-26）
SPP = WORK + "/sp/KnightGauntlet.spp"
BAKE_LOG2 = 11

# ---------------------------------------------------------------- 色・値（sRGB）
STEEL_BASE = (0.190, 0.182, 0.174)      # 黒ずんだ鋼の膜（胸当ての貼図の中央値 0.20/0.19/0.18 に合わせる）
STEEL_WARM = (0.175, 0.158, 0.142)      # 膜のむら（焼けた茶。弱く）
STEEL_POLISH = (0.30, 0.29, 0.28)       # 薄く磨かれた所
STEEL_BARE = (0.58, 0.56, 0.53)         # 縁の擦れ（地の鋼）
RUST = (0.27, 0.115, 0.05)
SOOT = (0.055, 0.050, 0.046)
DUST = (0.31, 0.29, 0.26)
ENGRAVE_BLACK = (0.035, 0.032, 0.030)   # 溝に焼き付けた黒
ENGRAVE_LIP = (0.46, 0.45, 0.43)
LEATHER = (0.105, 0.082, 0.066)
LEATHER_DARK = (0.045, 0.036, 0.030)
LEATHER_WORN = (0.20, 0.16, 0.12)


# ---------------------------------------------------------------- 道具
_ALL = None


def find(name, kind):
    global _ALL
    if _ALL is None:
        _ALL = res.search("")
    for r in _ALL:
        try:
            n = r.identifier().name
            usages = [str(u).split(".")[-1].lower() for u in r.handle.usages()]
        except RuntimeError:
            _ALL = None
            return find(name, kind)
        except Exception:
            continue
        if (n == name or n.split("/")[0] == name) and kind in usages:
            return r.identifier()
    raise KeyError((name, kind))


_imported = globals().setdefault("_td2_imported", {})


def texture(name):
    """Designer の書き出した画像を、この工程の資源として取り込む（同じ名前は 1 度だけ）。"""
    if name not in _imported:
        _imported[name] = res.import_project_resource(SD_OUT + name + ".png", res.Usage.TEXTURE).identifier()
    return _imported[name]


def stack():
    return [t for t in ts.all_texture_sets() if t.name() == SET][0].get_stack()


def srgb(c):
    return Color(*c)


def gray(v):
    return Color(v, v, v)


def project_mode(node, mode="tri", scale=1.0):
    node.set_projection_mode(ls.ProjectionMode.Triplanar if mode == "tri" else ls.ProjectionMode.UV)
    p = node.get_projection_parameters()
    p.uv_transformation.scale = [scale, scale]
    node.set_projection_parameters(p)


def group(name, meshes):
    g = ls.insert_group(ls.InsertPosition.from_textureset_stack(stack()))
    g.set_name(name)
    g.set_geometry_mask_type(ls.GeometryMaskType.Mesh)
    g.set_geometry_mask_enabled_meshes(meshes)
    return g


def layer(where, name, color=None, rough=None, metal=None, height=None, height_amount=1.0, height_mode=None,
          opacity=1.0, smart_mask=None, mask_tex=None, mask_proc=None, mask_proj=("tri", 1.0),
          material=None, mat_proj=("tri", 1.0), levels=None):
    """塗りの層を 1 枚、where の一番上へ。height は数（一様）か画像の名前（Designer の書き出し、UV で貼る）。"""
    node = ls.insert_fill(ls.InsertPosition.inside_node(where, ls.NodeStack.Substack))
    node.set_name(name)
    chans = set()
    for value, ch in ((color, C.BaseColor), (rough, C.Roughness), (metal, C.Metallic), (height, C.Height)):
        if value is not None:
            chans.add(ch)
    if material:
        chans.add(C.Height)
    node.active_channels = chans
    if material:
        node.set_material_source(find(material, "basematerial"))
        project_mode(node, *mat_proj)
        if height_amount != 1.0 and height is None:
            node.set_opacity(height_amount, C.Height)
    if color is not None:
        node.set_source(C.BaseColor, srgb(color))
    if rough is not None:
        node.set_source(C.Roughness, gray(rough))
    if metal is not None:
        node.set_source(C.Metallic, gray(metal))
    if height is not None:
        if isinstance(height, str):
            node.set_source(C.Height, texture(height))
            project_mode(node, "uv" if height.startswith("engrave") else "tri", 1.0 if height.startswith("engrave") else 2.0)
        else:
            node.set_source(C.Height, gray(height))
        if height_mode is not None:
            node.set_blending_mode(height_mode, C.Height)
        if height_amount != 1.0:
            node.set_opacity(height_amount, C.Height)
    if opacity != 1.0:
        for ch in chans - {C.Height}:
            node.set_opacity(opacity, ch)
    if smart_mask or mask_tex or mask_proc:
        node.add_mask(ls.MaskBackground.Black)
        inside = ls.InsertPosition.inside_node(node, ls.NodeStack.Mask)
        if smart_mask:
            ls.insert_smart_mask(inside, find(smart_mask, "smartmask"))
        else:
            m = ls.insert_fill(inside)
            m.set_source(None, texture(mask_tex) if mask_tex else find(mask_proc, "procedural"))
            project_mode(m, *(("uv", 1.0) if mask_tex and mask_tex.startswith("engrave") else mask_proj))
        if levels:
            lv = ls.insert_levels_effect(ls.InsertPosition.inside_node(node, ls.NodeStack.Mask))
            p = lv.get_parameters()
            for k, v in levels.items():
                setattr(p, k, v)
            lv.set_parameters(p)
    return node


# ---------------------------------------------------------------- 手順
def step_new():
    if project.is_open():
        project.close()
    settings = project.Settings(normal_map_format=project.NormalMapFormat.OpenGL,
                                tangent_space_mode=project.TangentSpace.PerFragment,
                                default_texture_resolution=DOC_RES)
    project.create(MESH, settings=settings)
    return "creating"


def step_info():
    return {t.name(): {"res": (t.get_resolution().width, t.get_resolution().height),
                       "meshes": list(t.all_mesh_names())} for t in ts.all_texture_sets()}


def step_bake():
    state = globals().setdefault("td2_bake_state", {})
    state.clear()
    state["status"] = "running"
    if not globals().get("td2_bake_hooked"):
        event.DISPATCHER.connect(event.BakingProcessEnded, lambda e: state.__setitem__("status", str(getattr(e, "status", e))))
        globals()["td2_bake_hooked"] = True
    usages = [MeshMapUsage.AO, MeshMapUsage.Curvature, MeshMapUsage.Thickness, MeshMapUsage.WorldSpaceNormal,
              MeshMapUsage.Position]
    for t in ts.all_texture_sets():
        p = baking.BakingParameters.from_texture_set(t)
        common = p.common()
        baking.BakingParameters.set({
            common["LowAsHigh"]: True,
            common["OutputSize"]: (BAKE_LOG2, BAKE_LOG2),
            common["SubSampling"]: list(common["SubSampling"].enum_values().values())[1],
        })
        p.set_enabled_bakers(usages)
    baking.bake_selected_textures_async()
    return "baking"


def step_layers():
    st = stack()
    for node in ls.get_root_layer_nodes(st):
        ls.delete_node(node)
    # 下から：革 → 鋼（鋼の組の中も下から積む＝後に足した層が上）。
    leather = group("革", ["GauntletLeather"])
    for args in (
        dict(name="革 地", color=LEATHER, rough=0.62, metal=0.0),
        dict(name="革のしぼ", material="leather_grain", mat_proj=("tri", 16.0), height_amount=0.3),
        dict(name="色のむら", color=LEATHER_DARK, opacity=0.6, mask_proc="grunge_leather", mask_proj=("tri", 1.5)),
        dict(name="しわ", color=LEATHER_DARK, rough=0.7, opacity=0.5, smart_mask="Crumples"),
        dict(name="しわの汚れ", color=(0.02, 0.017, 0.015), rough=0.85, opacity=0.75, smart_mask="Dirt Cavities"),
        dict(name="縁の擦れ", color=LEATHER_WORN, rough=0.72, opacity=0.65, smart_mask="Edges Dusty"),
        dict(name="脂のつや", rough=0.38, opacity=0.45, mask_proc="grunge_fingerprints_smeared", mask_proj=("tri", 2.0)),
        dict(name="埃", color=DUST, rough=0.95, opacity=0.25, smart_mask="Dust Occlusion"),
    ):
        layer(leather, **args)
    steel = group("鋼", ["GauntletSteel"])
    for args in (
        dict(name="鋼 地（黒ずんだ膜）", color=STEEL_BASE, rough=0.74, metal=0.5),
        dict(name="鍛えの肌（Designer）", height="forged_height", height_amount=0.04),
        dict(name="膜のむら（Designer）", color=STEEL_WARM, rough=0.80, opacity=0.4, mask_tex="forged_oxide",
             mask_proj=("tri", 2.0)),
        dict(name="薄く磨かれた所", color=STEEL_POLISH, rough=0.52, metal=0.8, opacity=0.3, mask_proc="grunge_wipe_smudgy_soft",
             mask_proj=("tri", 0.8)),
        dict(name="細かい傷", color=STEEL_POLISH, rough=0.42, metal=0.9, opacity=0.3, smart_mask="Subtle Scratches"),
        dict(name="酸化", color=(0.13, 0.12, 0.11), rough=0.84, opacity=0.3, smart_mask="Oxydation"),
        # 彫り（手の甲の板だけ。Designer の画像は手の甲の島の外が 0）。
        dict(name="彫り：地の荒らし（Designer）", color=(0.13, 0.125, 0.12), rough=0.82, opacity=0.4,
             mask_tex="engrave_frost"),
        dict(name="彫り：溝（Designer）", height="engrave_groove", height_mode=ls.BlendingMode.Subtract,
             height_amount=0.9),
        dict(name="彫り：溝の黒（Designer）", color=ENGRAVE_BLACK, rough=0.86, metal=0.25, mask_tex="engrave_fill"),
        dict(name="彫り：縁の磨れ（Designer）", color=ENGRAVE_LIP, rough=0.30, opacity=0.65, mask_tex="engrave_lip"),
        # 使い込み
        dict(name="縁の擦れ（地の鋼）", color=STEEL_BARE, rough=0.34, metal=1.0, opacity=0.7, smart_mask="Edges Scratched"),
        dict(name="隙間の錆", color=RUST, rough=0.92, metal=0.0, opacity=0.35, smart_mask="Cavity Rust"),
        dict(name="錆の垂れ", color=(0.22, 0.10, 0.045), rough=0.9, metal=0.0, opacity=0.2, smart_mask="Rust Drips"),
        dict(name="下からの煤", color=SOOT, rough=0.82, metal=0.3, opacity=0.45, smart_mask="Dirt Ground"),
        dict(name="隙間の汚れ", color=SOOT, rough=0.86, metal=0.2, opacity=0.5, smart_mask="Dirt Cavities"),
        dict(name="上の面の埃", color=DUST, rough=0.95, metal=0.0, opacity=0.12, smart_mask="Dust Occlusion"),
    ):
        layer(steel, **args)
    project.save_as(SPP)
    return [n.get_name() for n in ls.get_root_layer_nodes(st)]


def step_export():
    def ch(dest, kind, name, src):
        return {"destChannel": dest, "srcChannel": src, "srcMapType": kind, "srcMapName": name}

    preset = {"name": "td2_engine", "maps": [
        {"fileName": "$textureSet_BaseColor", "channels": [ch(c, "documentMap", "basecolor", c) for c in "RGB"],
         "parameters": {"fileFormat": "png", "bitDepth": "8", "dithering": True}},
        {"fileName": "$textureSet_ORM",
         "channels": [ch("R", "virtualMap", "AO_Mixed", "L"), ch("G", "documentMap", "roughness", "L"),
                      ch("B", "documentMap", "metallic", "L")],
         "parameters": {"fileFormat": "png", "bitDepth": "8", "dithering": True}},
        {"fileName": "$textureSet_Normal", "channels": [ch(c, "virtualMap", "Normal_OpenGL", c) for c in "RGB"],
         "parameters": {"fileFormat": "png", "bitDepth": "8", "dithering": True}},
    ]}
    config = {"exportShaderParams": False, "exportPath": EXPORT_DIR, "defaultExportPreset": "td2_engine",
              "exportPresets": [preset], "exportList": [{"rootPath": SET}],
              "exportParameters": [{"parameters": {"paddingAlgorithm": "infinite"}}]}
    r = ex.export_project_textures(config)
    return {"status": str(r.status), "message": r.message,
            "files": sum((list(v) for v in r.textures.values()), [])}


STEP = globals().get("STEP", "info")
result = {"new": step_new, "info": step_info, "bake": step_bake, "layers": step_layers,
          "export": step_export}[STEP]()
if STEP == "bake":
    result = globals()["td2_bake_state"]
