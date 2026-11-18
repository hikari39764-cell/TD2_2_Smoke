"""c-857ae3 1 段目：刀のエフェクト（Assets/Vfx/BossBlade.hvfx）に、フェーズ 2 の「いつも燃えている刀」の層を足す。
このカードの前のコミット（c9cb71a）の資産を土台に、毎回同じ結果を書く（何度流してもよい）。既にある層は値を変えない（熱のゆがみの層だけ強める）。
足す層（どれも灼けた刀身の間だけ。フェーズ 1 は入力 flame・embers が 0 で出ない）：
- Blade Flame 1〜4：刀身の両側へ立ち、根元から剣先へ流れて揺らめく炎（炎の形の貼図を刀身に沿って並べた帯を、
  並ぶ数と速さをずらして 4 枚重ねる）。
- Blade Embers：刀身から立つ火の粉（粒。量は入力 embers＝Float）。
変える層：Blade Heat Haze（熱のゆがみの強さと幅）。
使い方：python -X utf8 edit_blade_fire.py [名前=値 ...] [out=<書き出す先>]"""
import copy
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path("D:/PJ/TD2_2")
ASSET = "Assets/Vfx/BossBlade.hvfx"
# 幅・位置は刀の局所（エフェクトの拡大＝刃の長さ 約 4.3 m を掛ける前）。
P = dict(
    # 炎：帯の幅（刀身から炎の先まで）・根元を刀身へ寄せる割合（1 で帯の縁が刀身の軸）・模様・色・流れる速さ（A・B。
    # 負が剣先の向き）・刀身に沿って並ぶ数（A・B。ずらして同じ形が揃わないように）・ゆがみ（強さ、細かさ、速さ、0）・
    # 切る明るさ（LuminanceMask：これより暗い所を切る、柔らかさ）。
    flame_root=1.05, flame_texture="engine.vfx.texture.flame_mask", flame_color=[4.0, 1.4, 0.15], flame_blend="Alpha",
    flame_wobble=[0.22, 2.2, 1.6, 0.0], flame_cut=[0.35, 0.7],
    # 炎の帯ごとの [側（1＝A の側、-1＝反対）, 刀身に沿って並ぶ数, 流れる速さ（負が剣先の向き）, 幅, 明るさの倍率]。
    # 並ぶ数と速さをずらして重ね、同じ形が等間隔に揃わないようにする。
    flame_layers=[[1, 7.0, -1.1, 0.08, 1.0], [-1, 8.5, -1.4, 0.08, 1.0], [1, 4.3, -0.8, 0.11, 0.7],
                  [-1, 5.1, -0.95, 0.10, 0.6]],
    ember_rate=40.0, ember_life=[0.5, 1.1], ember_size=[0.014, 0.005], ember_half=[0.42, 0.01, 0.01],
    ember_speed=[0.1, 0.5], ember_rise=1.6,
    haze_strength=0.17, haze_width=0.7,
)
for arg in sys.argv[1:]:
    key, _, value = arg.partition("=")
    if key in P:
        P[key] = json.loads(value)
OUT = pathlib.Path(dict(a.split("=", 1) for a in sys.argv[1:] if a.startswith("out=")).get("out", str(ROOT / ASSET)))


# 土台は、このカードの前のコミット（c-3e0ffa の HP の条。コミットした後に流し直しても二重に足さない）。
BASE = "c9cb71a"


def head(path):
    return subprocess.run(["git", "show", BASE + ":" + path], cwd=ROOT, capture_output=True, check=True).stdout


def reid(node, tag):
    """写した層のモジュールと層の id を別のものにする。"""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "id" and isinstance(value, str) and value.split(".")[0] in ("module", "emitter"):
                stem, _, tail = value.rpartition(".")
                node[key] = stem + "." + hashlib.md5((tail + tag).encode()).hexdigest()
            else:
                reid(value, tag)
    elif isinstance(node, list):
        for item in node:
            reid(item, tag)


def link(layer, input_id, module, prop):
    return {"bias": 0.0, "id": f"{layer['id']}.link.{input_id}.{prop}", "inputId": input_id, "scale": 1.0,
            "target": {"emitterId": layer["id"], "moduleId": layer["modules"][module]["id"], "propertyId": prop}}


def effect(layer, kind):
    return next(e for e in layer["effects"] if e["kind"] == kind)


def set_color(layer, rgba):
    for stop in layer["modules"]["color"]["value"]["gradient"]["stops"]:
        stop["color"] = list(rgba)


doc = json.loads(head(ASSET).decode("utf-8"))
layers = {e["name"]: e for e in doc["emitters"]}
assert "Blade Flame" not in layers
fire = layers["Blade Fire"]
order = max(e["order"] for e in doc["emitters"]) + 1

# 炎の帯：刀身の両側へ立つ炎の形（flame_mask）を刀身に沿って並べ、根元から剣先へ流す（UvFlow の U が負＝剣先の向き。
# Blade Fire と同じ向き）。帯は刀身の軸まわりにカメラを向く帯で、アンカーで帯の縁を刀身に置いて片側へ伸ばし、
# 反対側へ伸ばす 2 枚目は模様を 180 度回す（どちらも炎の根元が刀身に来る）。雑音でゆがめて揺らめかせる。
# 遮罩の貼図は localUv で引かれ（タイル・回転・入れ替えの前）、刀身の真ん中だけになるので使わない。
flames = []
for k, (side, tiles, speed, width, bright) in enumerate(P["flame_layers"]):
    flame = copy.deepcopy(fire)
    reid(flame, f"bossblade.flame{k}")
    flame.update(name=f"Blade Flame {k + 1}", order=order + k)
    flame["anchor"] = [0.0, -side * 0.5 * width * P["flame_root"], 0.0]
    flame["modules"]["renderShape"].update(ribbonWidth=width, uvSwapAxes=False,
                                           uvRotationDegrees=0.0 if side > 0 else 180.0, taperStart=0.8,
                                           taperEnd=0.7)
    # 混ぜ方：足し算だと、明るい空の前で炎が桃色の綿に見えた。重ねて塗る（Alpha）と空の前でも橙の炎に見える。
    flame["modules"]["renderCommon"].update(blendMode=P["flame_blend"])
    flame["modules"]["renderCommon"].update(colorTextureAssetGuid=P["flame_texture"], maskTextureAssetGuid="",
                                            noiseTextureAssetGuid="engine.vfx.texture.noise_fractal_1",
                                            flowMapTextureAssetGuid="")
    flame["effects"] = [e for e in flame["effects"] if e["kind"] in ("UvFlow", "UvDistortion", "LuminanceMask")]
    # 暗い所（煙のようにぼやけた縁）を切って、炎の舌の縁を締める。
    effect(flame, "LuminanceMask")["parameters"] = P["flame_cut"] + [0.0, 0.0]
    set_color(flame, [c * bright for c in P["flame_color"]] + [1.0])
    effect(flame, "UvFlow")["parameters"] = [speed, 0.0, tiles, 1.0]
    effect(flame, "UvDistortion")["parameters"] = P["flame_wobble"]
    # 重ねて塗る層は、色だけ 0 にしても黒い炎が残る（フェーズ 1 で刀に黒い斑が出た）。不透明度も一緒に落とせるよう、
    # 専用の入力 flame（Color。RGB と A の両方に火の強さ）を掛ける。
    flame["inputLinks"] = [link(flame, "input.flame", "color", "valueMultiplier")]
    flames.append(flame)

# 火の粉：刃に沿った細い箱から、上へ漂って消える小さな光の点。
flame_doc = json.loads((ROOT / "Assets/Vfx/GourdFireShotFlame.hvfx").read_text(encoding="utf-8"))
embers = copy.deepcopy(next(e for e in flame_doc["emitters"] if e["name"] == "Tail Embers"))
reid(embers, "bossblade.embers")
embers.update(name="Blade Embers", order=order + len(flames), parentEmitterId="")
m = embers["modules"]
m["emitterTransform"]["transform"].update(position=[0.0, 0.0, 0.0], rotationEulerDegrees=[0.0, 0.0, 0.0],
                                          scale=[1.0, 1.0, 1.0])
m["shape"].update(type="Box", boxHalfExtents=P["ember_half"])
m["basic"]["lifetime"].update(minimum=P["ember_life"][0], maximum=P["ember_life"][1])
m["spawn"].update(ratePerSecond=P["ember_rate"], maximumParticles=96, capacityMode="Fixed", inheritVelocity=0.0)
m["position"]["radialSpeed"].update(minimum=P["ember_speed"][0], maximum=P["ember_speed"][1])
m["forces"].update(gravity=[0.0, P["ember_rise"], 0.0], drag=1.6)
m["forces"]["field"].update(turbulence=2.4, frequency=1.1, evolution=0.6)
for axis in "xyz":
    keys = m["scale"]["value"]["curve"][axis]["keys"]
    keys[0]["value"], keys[-1]["value"] = P["ember_size"]
stops = m["color"]["value"]["gradient"]["stops"]
stops[0]["color"] = [1.0, 0.6, 0.18, 4.0]
stops[1]["color"] = [1.0, 0.34, 0.06, 2.4]
stops[2]["color"] = [0.5, 0.08, 0.02, 0.0]
# 刀の資産は LayerUv の合成（流れ・ゆがみ）を使うので、層の後処理の扱いをそろえる（そろわないと取り込みで断られる）。
m["renderCommon"]["postProcessMode"] = fire["modules"]["renderCommon"]["postProcessMode"]
embers["inputLinks"] = [link(embers, "input.embers", "spawn", "rateMultiplier")]

# 熱のゆがみ：強さ（色 R）と幅。
haze = layers["Blade Heat Haze"]
for stop in haze["modules"]["color"]["value"]["gradient"]["stops"]:
    stop["color"][0] = P["haze_strength"]
haze["modules"]["renderShape"]["ribbonWidth"] = P["haze_width"]

doc["emitters"] += flames + [embers]
doc["inputs"].append({"defaultValue": 0.0, "id": "input.embers", "name": "embers", "type": "Float"})
doc["inputs"].append({"defaultValue": [0.0, 0.0, 0.0, 0.0], "id": "input.flame", "name": "flame", "type": "Color"})
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes((json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8"))
print("->", OUT, "layers", len(doc["emitters"]))
