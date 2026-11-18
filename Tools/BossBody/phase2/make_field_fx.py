"""c-857ae3 1 段目：フェーズ 2 の場のエフェクト Assets/Vfx/BossPhaseTwoField.hvfx（とメタ）を作る。毎回同じ結果を書く。
1 つの資産を 2 か所で出す（コード BossPhaseTwoField）：
- ボスの足元に付けて出し続ける側：漂う火の粉（Field Embers。入力 embers＝Float で量）。
- 変わり目にボスが立っていた所に置く側：焦げた輪（Scorch Char＝黒く焦げた斑の輪、Scorch Glow＝輪の中に残る熾火の
  かすかな光。入力 scorch＝Color（A が焦げの濃さ）・scorchGlow＝Color（光の明るさ））。
焦げた輪は太くて縁の崩れた斑（energy_ring の形 × 塊の雑音）で、細く長く残る予告の輪には見せない。
使い方：python -X utf8 make_field_fx.py [名前=値 ...]"""
import copy
import hashlib
import json
import pathlib
import sys

ROOT = pathlib.Path("D:/PJ/TD2_2")
GUID = "5be8f2a1c94d4e7b8a3f61d2c7e09b54"
P = dict(
    # 漂う火の粉：出す箱（足元から m。半分の大きさ）・毎秒の数（embers 1 で）・寿命・大きさ・上へ漂う加速度・乱れ
    ember_center=[0.0, 1.8, 0.0], ember_half=[13.0, 1.6, 13.0], ember_rate=60.0, ember_life=[4.0, 7.0],
    ember_size=[0.1, 0.04], ember_rise=0.32, ember_turbulence=1.3,
    # 焦げた輪：直径 m・焦げの色と濃さ・熾火の色と明るさ・斑の細かさ
    scorch_size=9.0, char_color=[0.025, 0.02, 0.016, 0.85], glow_color=[1.6, 0.36, 0.05, 1.0], char_tiles=1.6,
    glow_tiles=2.4, glow_drift=0.025,
)
for arg in sys.argv[1:]:
    key, _, value = arg.partition("=")
    if key in P:
        P[key] = json.loads(value)


def reid(node, tag):
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


def constant_curve(curve, value):
    keys = curve["keys"]
    for key in keys:
        key["value"] = value


shock = json.loads((ROOT / "Assets/Vfx/BossShock.hvfx").read_text(encoding="utf-8"))
flame = json.loads((ROOT / "Assets/Vfx/GourdFireShotFlame.hvfx").read_text(encoding="utf-8"))
glow_template = next(e for e in shock["emitters"] if e["name"] == "Shock Glow")
haze_template = next(e for e in shock["emitters"] if e["name"] == "Shock Haze")
ember_template = next(e for e in flame["emitters"] if e["name"] == "Tail Embers")


def scorch_layer(name, tag, order, blend, texture, color, tiles, drift, input_id):
    layer = copy.deepcopy(glow_template)
    reid(layer, tag)
    layer.update(name=name, order=order, activeDuration=3600.0)
    m = layer["modules"]
    m["basic"]["lifetime"].update(minimum=3600.0, maximum=3600.0)
    m["renderCommon"].update(blendMode=blend, colorTextureAssetGuid=texture,
                             maskTextureAssetGuid="engine.vfx.texture.energy_ring", postProcessMode="Scene")
    m["emitterTransform"]["transform"]["position"] = [0.0, 0.06, 0.0]
    for axis in "xyz":
        constant_curve(m["scale"]["value"]["curve"][axis], P["scorch_size"])
    stops = m["color"]["value"]["gradient"]["stops"]
    for stop in stops:
        stop["color"] = list(color)
    # 斑の模様を貼り（UvFlow のタイル）、熾火はゆっくり流して揺らめかせる。
    flow = copy.deepcopy(haze_template["effects"][0])
    flow["id"] = layer["id"] + ".flow"
    flow["parameters"] = [drift, drift * 0.6, tiles, tiles]
    flow["space"] = "LayerUv"
    layer["effects"] = [flow]
    layer["inputLinks"] = [link(layer, input_id, "color", "valueMultiplier")]
    return layer


char = scorch_layer("Scorch Char", "phase2field.char", 0, "Alpha", "engine.vfx.texture.noise_fractal_3",
                    P["char_color"], P["char_tiles"], 0.0, "input.scorch")
glow = scorch_layer("Scorch Glow", "phase2field.glow", 1, "Additive", "engine.vfx.texture.noise_fractal_1",
                    P["glow_color"], P["glow_tiles"], P["glow_drift"], "input.scorchGlow")

embers = copy.deepcopy(ember_template)
reid(embers, "phase2field.embers")
embers.update(name="Field Embers", order=2, parentEmitterId="")
m = embers["modules"]
m["emitterTransform"]["transform"].update(position=P["ember_center"], rotationEulerDegrees=[0.0, 0.0, 0.0],
                                          scale=[1.0, 1.0, 1.0])
m["shape"].update(type="Box", boxHalfExtents=P["ember_half"])
m["basic"]["lifetime"].update(minimum=P["ember_life"][0], maximum=P["ember_life"][1])
m["spawn"].update(ratePerSecond=P["ember_rate"], maximumParticles=360, capacityMode="Fixed", inheritVelocity=0.0)
m["position"]["radialSpeed"].update(minimum=0.05, maximum=0.3)
m["forces"].update(gravity=[0.0, P["ember_rise"], 0.0], drag=0.7)
m["forces"]["field"].update(turbulence=P["ember_turbulence"], frequency=0.35, evolution=0.25)
for axis in "xyz":
    keys = m["scale"]["value"]["curve"][axis]["keys"]
    keys[0]["value"], keys[-1]["value"] = P["ember_size"]
stops = m["color"]["value"]["gradient"]["stops"]
stops[0]["color"] = [1.0, 0.5, 0.12, 0.0]
stops[1]["color"] = [1.0, 0.36, 0.07, 3.0]
stops[2]["color"] = [0.6, 0.1, 0.02, 0.0]
stops[0]["time"], stops[1]["time"], stops[2]["time"] = 0.0, 0.2, 1.0
# 大きめの粒は、火花の貼図（spark_dot）だと四つ角の星に見えたので、丸く柔らかい点にする。
m["renderCommon"]["colorTextureAssetGuid"] = "engine.vfx.texture.particle_soft"
embers["inputLinks"] = [link(embers, "input.embers", "spawn", "rateMultiplier")]

doc = {key: value for key, value in shock.items() if key not in ("emitters", "inputs", "name")}
doc.update(name="Boss Phase Two Field", loop=True, durationSeconds=3600.0)
doc["inputs"] = [{"defaultValue": 0.0, "id": "input.embers", "name": "embers", "type": "Float"},
                 {"defaultValue": [0.0, 0.0, 0.0, 0.0], "id": "input.scorch", "name": "scorch", "type": "Color"},
                 {"defaultValue": [0.0, 0.0, 0.0, 1.0], "id": "input.scorchGlow", "name": "scorchGlow",
                  "type": "Color"}]
doc["emitters"] = [char, glow, embers]
path = ROOT / "Assets/Vfx/BossPhaseTwoField.hvfx"
path.write_bytes((json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8"))
meta = json.loads((ROOT / "ProjectSettings/AssetMeta/Assets/Vfx/BossShock.hvfx.hikari.asset.json").read_text(encoding="utf-8"))
meta.update(displayName="BossPhaseTwoField", guid=GUID, sourcePath="Assets/Vfx/BossPhaseTwoField.hvfx")
meta_path = ROOT / "ProjectSettings/AssetMeta/Assets/Vfx/BossPhaseTwoField.hvfx.hikari.asset.json"
if not meta_path.exists():
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("->", path, "layers", len(doc["emitters"]))
