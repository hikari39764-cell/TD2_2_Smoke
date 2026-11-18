"""c-857ae3 1 段目：胴の煙（Assets/Vfx/BossBodySmoke.hvfx）に、フェーズ 2 の「燃えている胴」を足す。
このカードの前のコミット（c9cb71a）の資産を土台に、毎回同じ結果を書く（何度流してもよい）。域の大きさ（オーナーが手で変えた [6.76, 9.39, 5.82]）・
分解能・源の値は変えない。変えるのは：
- 域の channel：None → Channel6（コードの火の舌の注入 FluidChannels::kBodyFire だけが届く。ほかは何も Channel6 へ出さない
  ので、フェーズ 1 の流れは変わらない）。
- 域の発光：起亮温度 1.0 → 1.5（フェーズ 1 の源の温度は最高 1.3 なので、フェーズ 1 は光らない。いまも鎧の中で光るだけで
  見えていない）。その上で発光を強くし（厚い発光の煙の明るさは 発光 / 消光 16）、色を火の帯（赤橙 → 橙黄）にする。
- 域に、流体の発光に付いて動く光（followFluidGlow。フェーズ 1 は発光が 0 なので灯らない）。
- 入力 burn（Float、既定 0）：火の粉（Body Embers）・熱のゆがみ（Body Heat Haze）の粒と、腰と首の隙間の熾火の源（Ember Gap）の量。フェーズ 1 は 0。
使い方：python -X utf8 edit_body_fire.py [名前=値 ...] [out=<書き出す先>]"""
import copy
import hashlib
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path("D:/PJ/TD2_2")
ASSET = "Assets/Vfx/BossBodySmoke.hvfx"
P = dict(
    # 域の発光
    ignition=1.5, hot_temperature=4.5, intensity=450000.0,
    cool=[1.0, 0.16, 0.02], hot=[1.0, 0.62, 0.22],
    # 燃焼（フェーズ 2 の火の舌の燃料だけが燃える。フェーズ 1 は燃料を入れないので何も起きない）：燃え始める温度・
    # 燃える速さ・燃えて出る熱と煤・膨らみ。
    burn_ignition=0.3, burn_rate=8.0, burn_heat=4.0, burn_soot=0.3, burn_expansion=0.0,
    # 発光に付いて動く光（光束の上限 lm・届く m）
    light_flux=90000.0, light_range=6.5,
    # 火の粉：毎秒の数（burn 1 で）・寿命・大きさ・明るさ・出す箱（足元から m、半分の大きさ）
    ember_rate=36.0, ember_life=[1.0, 2.0], ember_size=[0.09, 0.03], ember_bright=1.0,
    ember_center=[0.0, 5.0, -0.35], ember_half=[1.15, 0.45, 0.5],
    # 隙間の熾火（フェーズ 2 だけの源。量は入力 burn）：腰の前（胴と草摺の間、帯の上下）と首の前の両脇の、鎧のすぐ外に、暗い橙に
    # 光るぬるい煙を少し入れる（芯の火が隙間から透けて見える）。鎧の内側や裾の中に置くと、鎧に隠れるうえ、裾の冷たい源
    # （Hem・Tail）に冷やされて光らなかった。燃料は入れない（stageB：胴の中を昇った燃料が襟でまとめて燃えて、火が急に
    # 膨らんだ）。位置は足元から m（左右は反転して 2 つ）、半径 m。
    # （stageB：腰の両脇・前、首の両脇・前に置いて試したが、少ない量では見分けられず、見えるまで増やすと鎧の前を光る煙が覆った。
    # 既定は置かない。芯の透けは 2 段目の鎧の熾火（版の継ぎ目の発光）で見せる。試す時の例：
    # gaps=[[0.36,3.45,0.42],[0.3,5.0,0.22]] gap_radius=[0.2,0.18] gap_rate=[8,6]）
    gaps=[], gap_radius=[], gap_rate=[], gap_temperature=2.4,
    # 熱のゆがみ：毎秒の数・寿命・大きさ・強さ（色 R）・出す箱
    haze_rate=9.0, haze_life=[0.9, 1.4], haze_size=[0.9, 1.9], haze_strength=0.06,
    haze_center=[0.0, 5.55, -0.1], haze_half=[0.7, 0.25, 0.35],
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


def curve_keys(curve, values):
    keys = curve["keys"]
    for key, value in zip(keys, values):
        key["value"] = value


doc = json.loads(head(ASSET).decode("utf-8"))
flame = json.loads((ROOT / "Assets/Vfx/GourdFireShotFlame.hvfx").read_text(encoding="utf-8"))
blade = json.loads((ROOT / "Assets/Vfx/BossBlade.hvfx").read_text(encoding="utf-8"))
templates = {e["name"]: e for e in flame["emitters"]}
blade_light = next(e for e in blade["emitters"] if e["name"] == "Blade Light")

domain = doc["emitters"][0]
assert domain["name"] == "Body Domain"
fluid = domain["modules"]["fluidDomain"]
assert [round(v, 2) for v in fluid["domainSize"]] == [6.76, 9.39, 5.82], fluid["domainSize"]
fluid.update(channel="Channel6", emissionTemperature=P["ignition"], emissionHotTemperature=P["hot_temperature"],
             emissionIntensity=P["intensity"], emissionColor=P["cool"], emissionHotColor=P["hot"],
             combustion=True, ignitionTemperature=P["burn_ignition"], burnRate=P["burn_rate"],
             heatPerBurn=P["burn_heat"], smokePerBurn=P["burn_soot"], expansionPerBurn=P["burn_expansion"])
light = copy.deepcopy(blade_light["specialOutputs"][0])
light["id"] = domain["id"] + ".light"
light["light"].update(followFluidGlow=True, luminousFlux=P["light_flux"], range=P["light_range"],
                      color=[1.0, 0.52, 0.18, 1.0], flickerAmplitude=0.12, flickerFrequency=6.0, priority=2,
                      sourceRadius=0.4)
domain["specialOutputs"] = [light]

order = max(e["order"] for e in doc["emitters"]) + 1

# 火の粉：胸から肩の箱から、上へ漂って消える小さな光の点。
embers = copy.deepcopy(templates["Tail Embers"])
reid(embers, "bossbody.embers")
embers.update(name="Body Embers", order=order, parentEmitterId="")
m = embers["modules"]
m["emitterTransform"]["transform"].update(position=P["ember_center"], rotationEulerDegrees=[0.0, 0.0, 0.0],
                                          scale=[1.0, 1.0, 1.0])
m["shape"].update(type="Box", boxHalfExtents=P["ember_half"])
m["basic"]["lifetime"].update(minimum=P["ember_life"][0], maximum=P["ember_life"][1])
m["spawn"].update(ratePerSecond=P["ember_rate"], maximumParticles=96, capacityMode="Fixed", inheritVelocity=0.0)
m["position"]["radialSpeed"].update(minimum=0.2, maximum=0.8)
m["forces"].update(gravity=[0.0, 1.6, 0.0], drag=1.4)
m["forces"]["field"].update(turbulence=2.2, frequency=0.9, evolution=0.5)
for axis in "xyz":
    curve_keys(m["scale"]["value"]["curve"][axis], P["ember_size"])
b = P["ember_bright"]
stops = m["color"]["value"]["gradient"]["stops"]
stops[0]["color"] = [1.0, 0.55, 0.16, 4.0 * b]
stops[1]["color"] = [1.0, 0.32, 0.06, 2.4 * b]
stops[2]["color"] = [0.5, 0.08, 0.02, 0.0]
embers["inputLinks"] = [link(embers, "input.burn", "spawn", "rateMultiplier")]

# 熱のゆがみ：襟と肩の上から昇る、ゆがみの粒（色は付けない）。
haze = copy.deepcopy(templates["Heat Wake"])
reid(haze, "bossbody.haze")
haze.update(name="Body Heat Haze", order=order + 1, parentEmitterId="")
m = haze["modules"]
m["emitterTransform"]["transform"].update(position=P["haze_center"], rotationEulerDegrees=[0.0, 0.0, 0.0],
                                          scale=[1.0, 1.0, 1.0])
m["shape"].update(type="Box", boxHalfExtents=P["haze_half"])
m["basic"]["lifetime"].update(minimum=P["haze_life"][0], maximum=P["haze_life"][1])
m["spawn"].update(ratePerSecond=P["haze_rate"], maximumParticles=24, capacityMode="Fixed")
m["position"]["radialSpeed"].update(minimum=0.0, maximum=0.2)
m["forces"].update(gravity=[0.0, 1.4, 0.0], drag=0.8)
for axis in "xyz":
    curve_keys(m["scale"]["value"]["curve"][axis], P["haze_size"])
for stop in m["color"]["value"]["gradient"]["stops"]:
    stop["color"][0] = P["haze_strength"]
haze["inputLinks"] = [link(haze, "input.burn", "spawn", "rateMultiplier")]

# 隙間の熾火：裾の源（Hem）を写して、温度を持つ小さな源にする。入力 burn（フェーズ 1 は 0）で量を掛けるだけ：Bool の入力で
# 層の「有効」を切り替える作りは headless で有効にならなかった。量 0 の源は温度も入れないので、押す速さ（velocity）と
# 源の力場を 0 にしておけば、フェーズ 1 の流れには何もしない。昇るのは温度の浮力に任せる。
hem_src = next(e for e in doc["emitters"] if e["name"] == "Hem")
center_y = domain["modules"]["emitterTransform"]["transform"]["position"][1]
ember_gaps = []
for k, (gap, radius, rate) in enumerate(zip(P["gaps"], P["gap_radius"], P["gap_rate"])):
    for side in (1.0, -1.0):
        layer = copy.deepcopy(hem_src)
        reid(layer, f"bossbody.embergap{k}{side}")
        layer.update(name=f"Ember Gap {k + 1}{'L' if side > 0 else 'R'}", order=order + 2 + len(ember_gaps),
                     parentEmitterId=domain["id"])
        m = layer["modules"]
        m["emitterTransform"]["transform"]["position"] = [gap[0] * side, gap[1] - center_y, gap[2]]
        m["shape"]["radius"].update(minimum=radius, maximum=radius)
        m["spawn"]["ratePerSecond"] = rate
        m["fluidSource"].update(temperature=P["gap_temperature"], velocity=[0.0, 0.0, 0.0], emissionNoise=0.7,
                                fuelFraction=0.0)
        layer["inputLinks"] = [link(layer, "input.burn", "spawn", "rateMultiplier")]
        ember_gaps.append(layer)

doc["emitters"] += [embers, haze] + ember_gaps
doc["inputs"].append({"defaultValue": 0.0, "id": "input.burn", "name": "burn", "type": "Float"})
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes((json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8"))
print("->", OUT, {k: P[k] for k in ("intensity", "ignition", "ember_rate", "haze_rate")})
