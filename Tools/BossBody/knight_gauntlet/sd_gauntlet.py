"""Substance Designer の中で走る：籠手の材質に使う 2 つの図を作り、計算して画像に書き出す。c-8b643c。

流し方：Designer を開き（sd_mcp_plugin が localhost:9881 で待つ）、手元から
  python Library/Handover/c-8b643c/sd_run.py Tools/BossBody/knight_gauntlet/sd_gauntlet.py src=<彫りの線のフォルダ> sbs=<保存する .sbs>
（sd_run.py が ARGS に src・sbs を入れて execute_code で送る。）ここでは図を組んで .sbs に保存するだけ。4096 の計算は
Designer の中だと差し込みの待ち時間（120 秒）を越えるので、画像は render_sd.ps1（sbscooker → sbsrender）で書き出す。

図 1「Engraving」（4096、16 bit。engrave_pattern.py の線を、貼図と同じ UV のまま加工する）：
- engrave_groove：鏨の V 字の溝の深さ（線を Bevel で山にした高さ。線の真ん中で 1）。Painter で高さを引く。
- engrave_fill：溝に詰まった黒（焼き付けた黒漆・煤）。所々抜ける。
- engrave_lip：溝の縁の、磨れて光る細い帯（線をぼかした所 − 線）。
- engrave_frost：枠の内の地の細かな荒らし（線の所は除く）。
図 2「ForgedSteel」（2048、16 bit、繰り返し）：
- forged_height：槌で打った浅いへこみ・鍛えの細かな肌・延ばした向きの筋。
- forged_oxide：酸化の膜のむら（色と粗さのむらに使う）。
"""
import os

from sd.api.sbs.sdsbscompgraph import SDSBSCompGraph
from sd.api.sdbasetypes import ColorRGBA, float4, int2
from sd.api.sdproperty import SDPropertyCategory, SDPropertyInheritanceMethod
from sd.api.sdresource import EmbedMethod
from sd.api.sdresourcebitmap import SDResourceBitmap
from sd.api.sdvaluebool import SDValueBool
from sd.api.sdvaluefloat import SDValueFloat
from sd.api.sdvaluefloat4 import SDValueFloat4
from sd.api.sdvalueint import SDValueInt
from sd.api.sdvalueint2 import SDValueInt2
from sd.api.sdvaluestring import SDValueString

try:
    from sd.api.sdvalueenum import SDValueEnum
except ImportError:  # 古い Designer
    SDValueEnum = None

pm = app.getPackageMgr()
LIB = "D:/软件/SD/Adobe Substance 3D Designer/resources/packages"
SRC, SBS = ARGS["src"], ARGS["sbs"]

# ---------------------------------------------------------------- 値（調整はここ）
GROOVE_BEVEL = 0.004       # 溝の V の幅（Bevel の距離。線の半分より大きいと、線の真ん中が 1 に届かない山になる→自動の色階で 1 へ）
LIP_BLUR = 2.2             # 縁の光る帯の幅（ぼかしの強さ）
FILL_KEEP = 0.38           # 黒の詰まりが抜ける量（雲の模様のどこで切るか。大きいほど抜ける）
FROST_SCALE = 48           # 地の荒らしの粒の細かさ
DENT_SCALE = 7             # 槌のへこみの数（繰り返し 1 枚あたり）
GRAIN_LEVEL = 0.18         # 鍛えの肌の強さ
STREAK_LEVEL = 0.12        # 延ばした筋の強さ


# ---------------------------------------------------------------- 道具
def load(name):
    path = os.path.join(LIB, name + ".sbs").replace("\\", "/")
    for p in pm.getUserPackages():
        if p.getFilePath().replace("\\", "/").lower() == path.lower():
            return p
    return pm.loadUserPackage(path)


def resource(package, graph_id):
    for r in load(package).getChildrenResources(True):
        if r.getClassName() == "SDSBSCompGraph" and r.getIdentifier() == graph_id:
            return r
    raise KeyError((package, graph_id))


def setp(node, pid, value):
    prop = node.getPropertyFromId(pid, SDPropertyCategory.Input)
    tid = prop.getType().getId()
    if isinstance(value, bool):
        v = SDValueBool.sNew(value)
    elif tid == "float":
        v = SDValueFloat.sNew(float(value))
    elif tid == "int":
        v = SDValueInt.sNew(int(value))
    elif tid == "float4":
        v = SDValueFloat4.sNew(float4(*value))
    elif tid == "string":
        v = SDValueString.sNew(value)
    elif tid.startswith("sbs::compositing::") and SDValueEnum is not None:
        v = SDValueEnum.sFromValueId(tid, value) if isinstance(value, str) else SDValueInt.sNew(int(value))
    else:
        v = SDValueInt.sNew(int(value))
    node.setInputPropertyValueFromId(pid, v)


class Graph:
    def __init__(self, package, name, size_log2):
        self.g = SDSBSCompGraph.sNew(package)
        self.g.setIdentifier(name)
        # 大きさは親に対する相対ではなく絶対（相対のままだと書き出しの時に 2^(8+12) になり、sbsrender が落ちた）。
        size = self.g.getPropertyFromId("$outputsize", SDPropertyCategory.Input)
        self.g.setPropertyInheritanceMethod(size, SDPropertyInheritanceMethod.Absolute)
        self.g.setInputPropertyValueFromId("$outputsize", SDValueInt2.sNew(int2(size_log2, size_log2)))
        fmt = self.g.getPropertyFromId("$format", SDPropertyCategory.Input)
        self.g.setPropertyInheritanceMethod(fmt, SDPropertyInheritanceMethod.Absolute)
        try:
            self.g.setInputPropertyValueFromId("$format", SDValueInt.sNew(1))   # 16 bit
        except Exception as e:  # 型が合わない版では既定のまま
            print("format:", e)
        self.package = package
        self.exports = []

    def atom(self, kind, **params):
        n = self.g.newNode("sbs::compositing::" + kind)
        for k, v in params.items():
            setp(n, k, v)
        return n

    def lib(self, package, graph_id, **params):
        n = self.g.newInstanceNode(resource(package, graph_id))
        for k, v in params.items():
            setp(n, k, v)
        return n

    def bitmap(self, path):
        res = SDResourceBitmap.sNewFromFile(self.package, path, EmbedMethod.Linked)
        n = self.atom("bitmap")
        setp(n, "bitmapresourcepath", res.getUrl())
        gray = self.atom("grayscaleconversion", channelsweights=(1.0, 0.0, 0.0, 0.0))
        self.link(n, gray, "input1")
        return gray

    @staticmethod
    def out_id(node):
        return node.getProperties(SDPropertyCategory.Output)[0].getId()

    def link(self, src, dst, inp, out=None):
        src.newPropertyConnectionFromId(out or self.out_id(src), dst, inp)
        return dst

    def blend(self, fg, bg, mode, opacity=1.0, mask=None):
        n = self.atom("blend", blendingmode=mode, opacitymult=opacity)
        self.link(fg, n, "source")
        self.link(bg, n, "destination")
        if mask is not None:
            self.link(mask, n, "opacity")
        return n

    def uniform(self, v):
        n = self.atom("uniform", colorswitch=False)
        n.setInputPropertyValueFromId("outputcolor", __import__("sd.api.sdvaluecolorrgba", fromlist=["x"]).SDValueColorRGBA.sNew(ColorRGBA(v, v, v, 1.0)))
        return n

    def export(self, node, name, out=None):
        """図の出力（名前＝書き出す画像の名前）。"""
        o = self.atom("output")
        self.link(node, o, "inputNodeOutput", out)
        o.setAnnotationPropertyValueFromId("identifier", SDValueString.sNew(name))
        o.setAnnotationPropertyValueFromId("label", SDValueString.sNew(name))


# ---------------------------------------------------------------- 包（作り直す）
for p in list(pm.getUserPackages()):
    if p.getFilePath().replace("\\", "/").lower() == SBS.replace("\\", "/").lower():
        pm.unloadUserPackage(p)
package = pm.newUserPackage()

# ---------------------------------------------------------------- 図 1：彫り
E = Graph(package, "Engraving", 12)
lines = E.bitmap(os.path.join(SRC, "engrave_lines.png").replace("\\", "/"))
panel = E.bitmap(os.path.join(SRC, "engrave_panel.png").replace("\\", "/"))
# 溝：線の形を Bevel で山にする（縁 0 → 真ん中 1 の V 字）。
bevel = E.lib("bevel", "bevel", distance=GROOVE_BEVEL, smoothing=0.15)
E.link(lines, bevel, "input")
groove = E.lib("auto_levels", "auto_levels")
E.link(bevel, groove, "Input", "height")
E.export(groove, "engrave_groove")
# 黒の詰まり：線を少し細くし、雲の模様で所々抜く。
thin = E.lib("histogram_scan", "histogram_scan", Position=0.55, Contrast=0.6)
E.link(lines, thin, "Input_1")
clouds = E.lib("noise_clouds_2", "clouds_2", scale=6)
keep = E.lib("histogram_scan", "histogram_scan", Position=FILL_KEEP, Contrast=0.55)
E.link(clouds, keep, "Input_1")
fill = E.blend(keep, thin, "multiply")
E.export(fill, "engrave_fill")
# 縁の光る帯：ぼかした線 − 線。
soft = E.lib("blur_hq", "blur_hq_grayscale", Intensity=LIP_BLUR)
E.link(lines, soft, "Source")
lip_raw = E.blend(lines, soft, "substract")
lip = E.lib("histogram_scan", "histogram_scan", Position=0.75, Contrast=0.4)
E.link(lip_raw, lip, "Input_1")
E.export(lip, "engrave_lip")
# 地の荒らし：枠の内、線と縁の帯の外に、細かな粒。
spots = E.lib("noise_bnw_spots_3", "bnw_spots_3", scale=FROST_SCALE)
frost_in = E.blend(spots, panel, "multiply")
away = E.lib("invert", "invert_grayscale")
E.link(soft, away, "Source")
away_s = E.lib("histogram_scan", "histogram_scan", Position=0.35, Contrast=0.8)
E.link(away, away_s, "Input_1")
frost = E.blend(away_s, frost_in, "multiply")
E.export(frost, "engrave_frost")

# ---------------------------------------------------------------- 図 2：鍛えた鋼の肌（繰り返し）
F = Graph(package, "ForgedSteel", 11)
cells = F.lib("noise_cells_1", "cells_1", scale=DENT_SCALE)
dents_inv = F.lib("invert", "invert_grayscale")
F.link(cells, dents_inv, "Source")
dents = F.lib("blur_hq", "blur_hq_grayscale", Intensity=4.0)
F.link(dents_inv, dents, "Source")
grain = F.lib("noise_fractal_sum_base", "fractal_sum_base", Roughness=0.55, MinLevel=5, MaxLevel=11)
streak = F.lib("noise_directional_noise_1", "directional_noise_1", scale=6, angle=0.0)
h1 = F.blend(grain, dents, "addsub", opacity=GRAIN_LEVEL)
height = F.blend(streak, h1, "addsub", opacity=STREAK_LEVEL)
F.export(height, "forged_height")
oxide_c = F.lib("noise_clouds_2", "clouds_2", scale=3)
grunge = F.lib("grunge_map_005", "grunge_map_005")
oxide = F.blend(grunge, oxide_c, "overlay", opacity=0.7)
F.export(oxide, "forged_oxide")

pm.savePackageAs(package, SBS)
print("[sd] saved", SBS)
