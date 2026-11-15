"""ボスの胴の煙のエフェクトを作る（カード c-514e94）：Assets/Vfx/BossBodySmoke.hvfx と資産のメタ。
ボスの足元に付いて動く局所の流体域（場の障害物・力は受けない＝channel None）に、鎧の中と隙間へ源を置く。
- 胴の芯（鎧の中を満たす）、裾（草摺の下から重い霞が流れ落ちる）、尾（脚の無い所を地面まで埋める）、
  襟（兜の後ろへ昇る筋）、両の袖口（肩から外へ垂れる）。
- 入力：input.amount（出す量の倍率）、input.heat（源の温度の倍率。上げると熱い煙が光る＝フェーズ 2）。
霞の大剣 BossGreatswordMist.hvfx の域と源の層を写して作る（描画まわりの値を引き継ぐ）。
引数：[out=<フォルダ>]（既定は D:/PJ/TD2_2/Assets/Vfx） [d.<項目>=値] [rate=<全体の倍率>] [<源の名前>.<項目>=値]"""
import copy, hashlib, json, pathlib, sys

ROOT = pathlib.Path("D:/PJ/TD2_2")
GUID = "7c41e9d2a05b4f6e9b3d8a1f2c6e5d04"
src = json.loads((ROOT / "Assets/Vfx/BossGreatswordMist.hvfx").read_text(encoding="utf-8"))
args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a)
out_dir = pathlib.Path(args.get("out", str(ROOT / "Assets/Vfx")))
layers = {e["name"]: e for e in src["emitters"]}
SIZE = [3.0, 5.2, 3.0]
CENTER_Y = 2.1   # 域の中心（ボスの足元から）。足元の下 0.5 m から、兜の上 0.7 m まで入る。

DOMAIN = dict(
    # 分解能 72（1 格 約 7 cm）：104 と見た目はほぼ同じで、解算が約 0.4 ms 軽い。
    domainSize=SIZE, resolution=72, boundary="Open", channel="None", priority=90, combustion=False, castShadows=False,
    # 走り出し・止まりで煙が少し遅れる（1 にすると急停止で後ろの煙がまとめて前へ返る）。動いている間は後ろへたなびく。
    inertia=0.5, relativeWind=0.35,
    # 熱い煙（襟）は昇り、冷たい霞（裾）は重さで落ちる。
    buoyancy=0.9, weight=0.45, vorticity=2.2,
    densityDissipation=1.5, temperatureDissipation=1.6,
    # 場の霧と同じ、明るい霞の色（暗い灰色だと、夕方の光で汚れた煙に見えた）。
    extinction=10.0, scatteringColor=[0.78, 0.80, 0.84], ambientStrength=3.0, multipleScattering=0.8, anisotropy=0.3,
    detailScale=0.12, detailStrength=0.55,
    # 熱い煙だけが橙に光る（input.heat を上げた時）。
    emissionColor=[1.0, 0.30, 0.05], emissionHotColor=[1.0, 0.62, 0.22], emissionTemperature=1.6,
    emissionHotTemperature=4.0, emissionIntensity=90000.0, prewarmSeconds=1.5, maxSubsteps=2)

# 源：名前、形、位置（ボスの足元から）、毎秒の量、流体源の値
SOURCES = [
    # 芯は胸板の内側へ寄せて弱く：強いと、鎧の板を抜けて胸の前に煙が溜まる（域に鎧の障害物は入れていない）。
    ("Core", dict(type="Box", boxHalfExtents=[0.36, 0.40, 0.20]), (0.0, 2.50, -0.10), 8.0,
     dict(temperature=0.25, velocity=[0.0, 0.0, 0.0], emissionNoise=0.4)),
    ("Hem", dict(type="Sphere", radius=0.42), (0.0, 1.72, 0.0), 16.0,
     dict(temperature=0.0, velocity=[0.0, -0.8, 0.0], emissionNoise=0.5)),
    ("Tail", dict(type="Sphere", radius=0.34), (0.0, 1.0, 0.0), 8.0,
     dict(temperature=0.0, velocity=[0.0, -0.5, 0.0], emissionNoise=0.6)),
    ("Collar", dict(type="Sphere", radius=0.16), (0.0, 3.14, -0.14), 3.0,
     dict(temperature=1.0, velocity=[0.0, 0.35, -0.12], emissionNoise=0.8)),
    ("Arm Left", dict(type="Sphere", radius=0.15), (0.66, 2.92, 0.0), 2.5,
     dict(temperature=0.3, velocity=[0.25, -0.3, 0.0], emissionNoise=0.8)),
    ("Arm Right", dict(type="Sphere", radius=0.15), (-0.66, 2.92, 0.0), 2.5,
     dict(temperature=0.3, velocity=[-0.25, -0.3, 0.0], emissionNoise=0.8)),
]


def reid(node, tag):
    """層を写した時、層とモジュールの id を別のものにする。"""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "id" and isinstance(value, str) and value.split(".")[0] in ("module", "emitter"):
                head, _, tail = value.rpartition(".")
                node[key] = head + "." + hashlib.md5((tail + tag).encode()).hexdigest()
            else:
                reid(value, tag)
    elif isinstance(node, list):
        for item in node:
            reid(item, tag)


def pick(prefix):
    return {key[len(prefix):]: json.loads(value) for key, value in args.items() if key.startswith(prefix)}


def source(name, shape, position, rate, values, order, domain_id):
    layer = copy.deepcopy(layers["Sword Body"])
    reid(layer, "bossbody." + name)
    layer["name"] = name
    layer["order"] = order
    layer["parentEmitterId"] = domain_id
    # 域の子なので、域の中心ぶん戻して、ボスの足元をエフェクトの原点に置く。
    layer["modules"]["emitterTransform"]["transform"].update(
        position=[position[0], position[1] - CENTER_Y, position[2]], rotationEulerDegrees=[0.0, 0.0, 0.0],
        scale=[1.0, 1.0, 1.0])
    module = layer["modules"]["shape"]
    module["type"] = shape["type"]
    if "radius" in shape:
        module["radius"].update(maximum=shape["radius"], minimum=shape["radius"])
    if "boxHalfExtents" in shape:
        module["boxHalfExtents"] = shape["boxHalfExtents"]
    layer["modules"]["spawn"]["ratePerSecond"] = rate * float(args.get("rate", 1.0))
    settings = layer["modules"]["fluidSource"]
    settings.update(shapeModel="", shapeFill=1.0, shapeOffset=0.0, radialSpeed=0.0, outflow=0.0, fuelFraction=0.0,
                    emissionNoiseScale=0.35, guideStrength=0.0)
    settings.update(values)
    settings.update(pick(name.replace(" ", "") + "."))

    def link(input_id, module_id, prop):
        return {"bias": 0.0, "id": f"{layer['id']}.link.{input_id}.{prop}", "inputId": input_id, "scale": 1.0,
                "target": {"emitterId": layer["id"], "moduleId": module_id, "propertyId": prop}}
    layer["inputLinks"] = [link("input.amount", layer["modules"]["spawn"]["id"], "rateMultiplier"),
                           link("input.heat", settings["id"], "temperatureMultiplier")]
    return layer


domain = copy.deepcopy(layers["Sword Domain"])
reid(domain, "bossbody.domain")
domain["modules"]["fluidDomain"].update(DOMAIN)
domain["modules"]["fluidDomain"].update(pick("d."))
domain["modules"]["forces"]["field"]["turbulence"] = float(args.get("turb", 1.2))
domain["modules"]["forces"]["drag"] = 0.6
domain["modules"]["emitterTransform"]["transform"].update(
    position=[0.0, CENTER_Y, 0.0], rotationEulerDegrees=[0.0, 0.0, 0.0], scale=[1.0, 1.0, 1.0])
domain["inputLinks"] = []
domain["order"] = 0
domain["name"] = "Body Domain"
doc = {key: value for key, value in src.items() if key not in ("emitters", "inputs", "name")}
doc["name"] = "Boss Body Smoke"
doc["loop"] = True
doc["durationSeconds"] = 3600.0
doc["inputs"] = [{"defaultValue": 1.0, "id": "input.amount", "name": "amount", "type": "Float"},
                 {"defaultValue": 1.0, "id": "input.heat", "name": "heat", "type": "Float"}]
doc["emitters"] = [domain] + [source(name, shape, position, rate, values, i + 1, domain["id"])
                              for i, (name, shape, position, rate, values) in enumerate(SOURCES)]
path = out_dir / "BossBodySmoke.hvfx"
path.parent.mkdir(parents=True, exist_ok=True)
path.write_bytes((json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8"))
meta_src = json.loads((ROOT / "ProjectSettings/AssetMeta/Assets/Vfx/BossGreatswordMist.hvfx.hikari.asset.json")
                      .read_text(encoding="utf-8"))
meta_src.update(displayName="BossBodySmoke", guid=GUID, sourcePath="Assets/Vfx/BossBodySmoke.hvfx")
meta = pathlib.Path(args.get("meta", str(ROOT / "ProjectSettings/AssetMeta/Assets/Vfx"))) / "BossBodySmoke.hvfx.hikari.asset.json"
meta.parent.mkdir(parents=True, exist_ok=True)
meta.write_text(json.dumps(meta_src, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("->", path, "sources", len(SOURCES))
