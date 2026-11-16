"""筋斗雲の本体のエフェクトを作る（カード c-b58d83）：Assets/Vfx/NimbusCloud.hvfx と資産のメタ。
プレイヤーの足元に付いて動く局所の流体域（専用のチャンネル。場の霧と混ざらない）に、足元の平たい雲の塊と、速さで
伸びる尾の源を置く。白い霞に、低い日の光と少しの発光で金色の縁を足す（白里透金）。
- 入力：input.appear（体の雲の量。0〜1 の出し入れ × CloudMount の量）、input.speed（尾の量。速さで 0〜）、
  input.boost（ブーストの吹き出しの量と、金の縁の明るさ）。資産に無い入力は再生を断られるので、コードと名前を合わせる。
- 尾の長さは域の relativeWind（速いほど空気が後ろへ流れる）と消散で決まり、尾の源の量は input.speed で変わる。
胴の煙 BossBodySmoke.hvfx の域と源の層を写して作る（描画まわりの値を引き継ぐ）。
エディタで資産が直されていたら（最後にこのスクリプトが書いた内容と違えば）書かない。上書きするなら force=1。
引数：[out=<フォルダ>] [meta=<フォルダ>] [force=1] [rate=<全体の倍率>] [d.<域の項目>=値] [<源の名前>.<項目>=値]
（名前の空白は詰める：Puff1.temperature=0.2 など。値は JSON）"""
import copy, hashlib, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUID = "467fbd610a584de599d6946884fcdb67"
NAME = "NimbusCloud"
STAMP = pathlib.Path(__file__).with_name("nimbus_cloud.last.json")
src = json.loads((ROOT / "Assets/Vfx/BossBodySmoke.hvfx").read_text(encoding="utf-8"))
args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a)
out_dir = pathlib.Path(args.get("out", str(ROOT / "Assets/Vfx")))
layers = {e["name"]: e for e in src["emitters"]}

# 域：エフェクトの原点＝足元、+Z＝体の前。後ろ（尾）を長く取る。1 格 5 cm（8 m ÷ 160）。
SIZE = [5.0, 2.5, 8.0]
CENTER = [0.0, -0.55, -1.5]     # 足元の下 1.8 m 〜 上 0.7 m、前 2.5 m 〜 後ろ 5.5 m
DOMAIN = dict(
    domainSize=SIZE, resolution=160, boundary="Open", channel="Channel2", priority=98, combustion=False,
    castShadows=False, cellColor=False,
    # 加速の慣性は使わない（急に止まっても雲が前へ返らない）。相対風：速いほど空気が後ろへ流れて尾が伸びる。
    inertia=0.0, relativeWind=0.15,
    # 浮かない・沈まない雲。いつも湧いて入れ替わるよう、消散は速め。
    buoyancy=0.0, weight=0.0, vorticity=1.6,
    densityDissipation=2.6, temperatureDissipation=1.2,
    # 明るい白。前方散乱を強めにして、逆光で縁が光る（夕方の積雲の銀の縁）。
    extinction=14.0, scatteringColor=[1.0, 0.97, 0.92], ambientStrength=2.2, multipleScattering=0.9, anisotropy=0.45,
    detailScale=0.14, detailStrength=0.6,
    # 金の縁：温めた煙（縁の源とブースト）だけが金色に光る。冷めた尾は白。
    emissionColor=[1.0, 0.70, 0.28], emissionHotColor=[1.0, 0.86, 0.55], emissionTemperature=0.05,
    emissionHotTemperature=1.6, emissionIntensity=42000.0, prewarmSeconds=0.0, maxSubsteps=2, substepRate=60.0)
# 域全体の力：綿の塊に巻く乱流と、流れを落ち着かせる減衰。
TURBULENCE = dict(turbulence=1.4, frequency=2.6, evolution=1.0)
DRAG = 0.5

APPEAR, SPEED, BOOST = "input.appear", "input.speed", "input.boost"
# ブーストで金の縁の温度を上げる倍率（温度の倍率 = 1 + これ × boost）。
GLOW_BOOST = 1.5

# 源：名前、形、位置（足元から）、拡縮、毎秒の量、流体源の値、量の入力、温度の入力（無ければ None）。15 個まで
SOURCES = [
    # 芯：足首まで包む平たい楕円の塊。
    ("Core", dict(type="Sphere", radius=0.5), (0.0, -0.32, 0.0), (2.0, 0.7, 2.2), 14.0,
     dict(temperature=0.0, velocity=[0.0, 0.12, 0.0], emissionNoise=0.5, emissionNoiseScale=0.3,
          guideStrength=2.0, guideThickness=0.3), APPEAR, None),
    # 上の綿の盛り上がり（縁に沿って 4 つ。高さと大きさを少しずつ変えて、平らな板に見せない）。
    ("Puff1", dict(type="Sphere", radius=0.36), (0.62, -0.18, 0.35), (1.0, 0.8, 1.0), 7.0,
     dict(temperature=0.0, velocity=[0.2, 0.25, 0.1], emissionNoise=0.7, emissionNoiseScale=0.22), APPEAR, None),
    ("Puff2", dict(type="Sphere", radius=0.32), (-0.6, -0.2, 0.42), (1.0, 0.8, 1.0), 7.0,
     dict(temperature=0.0, velocity=[-0.2, 0.25, 0.1], emissionNoise=0.7, emissionNoiseScale=0.22), APPEAR, None),
    ("Puff3", dict(type="Sphere", radius=0.38), (0.7, -0.24, -0.45), (1.0, 0.8, 1.0), 7.0,
     dict(temperature=0.0, velocity=[0.2, 0.2, -0.1], emissionNoise=0.7, emissionNoiseScale=0.22), APPEAR, None),
    ("Puff4", dict(type="Sphere", radius=0.36), (-0.68, -0.22, -0.4), (1.0, 0.8, 1.0), 7.0,
     dict(temperature=0.0, velocity=[-0.2, 0.2, -0.1], emissionNoise=0.7, emissionNoiseScale=0.22), APPEAR, None),
    # 腹：下を丸く。
    ("Belly", dict(type="Sphere", radius=0.45), (0.0, -0.62, -0.1), (1.6, 0.55, 1.8), 6.0,
     dict(temperature=0.0, velocity=[0.0, -0.15, 0.0], emissionNoise=0.6, emissionNoiseScale=0.3), APPEAR, None),
    # 金の縁：平たい球殻に温めた薄い煙を出す。ブーストで温度を上げて明るくする。
    ("Rim", dict(type="Sphere", radius=0.5, inner=0.4), (0.0, -0.3, 0.0), (2.5, 0.95, 2.7), 4.0,
     dict(temperature=0.9, velocity=[0.0, 0.05, 0.0], emissionNoise=0.8, emissionNoiseScale=0.25), APPEAR, BOOST),
    # 尾：体の後ろから後ろへ吹き出す（量は速さで増える。長さは相対風と消散）。
    ("Tail", dict(type="Box", box=[0.5, 0.16, 0.4]), (0.0, -0.32, -1.05), (1.0, 1.0, 1.0), 10.0,
     dict(temperature=0.15, velocity=[0.0, 0.0, -2.5], emissionNoise=0.7, emissionNoiseScale=0.3), SPEED, None),
    ("Wisp Left", dict(type="Sphere", radius=0.2), (0.75, -0.28, -0.8), (1.0, 1.0, 1.0), 5.0,
     dict(temperature=0.15, velocity=[0.35, 0.0, -3.0], emissionNoise=0.8, emissionNoiseScale=0.2), SPEED, None),
    ("Wisp Right", dict(type="Sphere", radius=0.2), (-0.75, -0.28, -0.8), (1.0, 1.0, 1.0), 5.0,
     dict(temperature=0.15, velocity=[-0.35, 0.0, -3.0], emissionNoise=0.8, emissionNoiseScale=0.2), SPEED, None),
    # ブースト：後ろへ強く吹き出す、温めた（金に光る）煙。
    ("Boost", dict(type="Sphere", radius=0.5), (0.0, -0.3, -0.6), (1.4, 0.6, 1.0), 14.0,
     dict(temperature=1.2, velocity=[0.0, 0.0, -4.0], emissionNoise=0.6, emissionNoiseScale=0.3), BOOST, None),
    # 尾の消え際：域の後ろの端の手前で煙を吸って薄くする（速い時に尾が域の後ろの面で切れ、箱の形が見えないように）。
    # 2 つ重ねて、後ろへ行くほど強く消す。煙は出さない。
    ("Fade Near", dict(type="Box", box=[2.6, 1.3, 1.25]), (0.0, CENTER[1], -4.25), (1.0, 1.0, 1.0), 0.0,
     dict(absorption=5.0, emissionNoise=0.0), None, None),
    # 上の消え際：足首より上へ昇った煙を薄くする（下降で相対風が雲を吹き上げても、体を包まず、域の上の面で切れない）。
    ("Fade Top", dict(type="Box", box=[2.6, 0.3, 4.1]), (0.0, 0.45, CENTER[2]), (1.0, 1.0, 1.0), 0.0,
     dict(absorption=6.0, emissionNoise=0.0), None, None),
    ("Fade Far", dict(type="Box", box=[2.6, 1.3, 0.6]), (0.0, CENTER[1], -4.9), (1.0, 1.0, 1.0), 0.0,
     dict(absorption=12.0, emissionNoise=0.0), None, None),
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


def link(layer, input_id, module_id, prop, scale=1.0, bias=0.0):
    return {"bias": bias, "id": f"{layer['id']}.link.{input_id}.{prop}", "inputId": input_id, "scale": scale,
            "target": {"emitterId": layer["id"], "moduleId": module_id, "propertyId": prop}}


def source(name, shape, position, scale, rate, values, rate_input, heat_input, order, domain_id):
    layer = copy.deepcopy(layers["Core"])
    reid(layer, "nimbus." + name)
    layer["name"] = name
    layer["order"] = order
    layer["parentEmitterId"] = domain_id
    # 域の子なので、域の中心ぶん戻して、足元をエフェクトの原点に置く。
    layer["modules"]["emitterTransform"]["transform"].update(
        position=[position[i] - CENTER[i] for i in range(3)], rotationEulerDegrees=[0.0, 0.0, 0.0], scale=list(scale))
    module = layer["modules"]["shape"]
    module["type"] = shape["type"]
    if "radius" in shape:
        module["radius"].update(maximum=shape["radius"], minimum=shape.get("inner", shape["radius"]),
                                mode="RandomRange" if "inner" in shape else "Constant")
    if "box" in shape:
        module["boxHalfExtents"] = list(shape["box"])
    layer["modules"]["spawn"]["ratePerSecond"] = rate * float(args.get("rate", 1.0))
    settings = layer["modules"]["fluidSource"]
    settings.update(shapeModel="", shapeFill=1.0, shapeOffset=0.0, radialSpeed=0.0, outflow=0.0, fuelFraction=0.0,
                    emissionNoiseScale=0.3, guideStrength=0.0, guideThickness=0.25, absorption=0.0)
    settings.update(values)
    settings.update(pick(name.replace(" ", "") + "."))
    layer["inputLinks"] = []
    if rate_input:
        layer["inputLinks"].append(link(layer, rate_input, layer["modules"]["spawn"]["id"], "rateMultiplier"))
    if heat_input:
        layer["inputLinks"].append(link(layer, heat_input, settings["id"], "temperatureMultiplier", GLOW_BOOST, 1.0))
    return layer


domain = copy.deepcopy(layers["Body Domain"])
reid(domain, "nimbus.domain")
domain["modules"]["fluidDomain"].update(DOMAIN)
domain["modules"]["fluidDomain"].update(pick("d."))
domain["modules"]["forces"]["field"].update(TURBULENCE)
domain["modules"]["forces"]["drag"] = DRAG
domain["modules"]["forces"]["gravity"] = [0.0, 0.0, 0.0]
domain["modules"]["emitterTransform"]["transform"].update(
    position=list(CENTER), rotationEulerDegrees=[0.0, 0.0, 0.0], scale=[1.0, 1.0, 1.0])
domain["inputLinks"] = []
domain["order"] = 0
domain["name"] = "Cloud Domain"
doc = {key: value for key, value in src.items() if key not in ("emitters", "inputs", "name")}
doc["name"] = "Nimbus Cloud"
doc["loop"] = True
doc["durationSeconds"] = 3600.0
doc["inputs"] = [{"defaultValue": 1.0, "id": APPEAR, "name": "appear", "type": "Float"},
                 {"defaultValue": 0.0, "id": SPEED, "name": "speed", "type": "Float"},
                 {"defaultValue": 0.0, "id": BOOST, "name": "boost", "type": "Float"}]
doc["emitters"] = [domain] + [source(*entry, order=i + 1, domain_id=domain["id"]) for i, entry in enumerate(SOURCES)]
assert len(SOURCES) <= 15, "一つの域の源は 15 個まで"
payload = (json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8")

path = out_dir / f"{NAME}.hvfx"
default_out = out_dir == ROOT / "Assets/Vfx"
if default_out and path.exists() and args.get("force") != "1":
    last = json.loads(STAMP.read_text(encoding="utf-8"))["sha1"] if STAMP.exists() else None
    if hashlib.sha1(path.read_bytes()).hexdigest() != last:
        sys.exit(f"{path} はエディタで直されている（最後に書いた内容と違う）。上書きするなら force=1")
path.parent.mkdir(parents=True, exist_ok=True)
path.write_bytes(payload)
if default_out:
    STAMP.write_text(json.dumps({"sha1": hashlib.sha1(payload).hexdigest()}) + "\n", encoding="utf-8")
meta_src = json.loads((ROOT / "ProjectSettings/AssetMeta/Assets/Vfx/BossBodySmoke.hvfx.hikari.asset.json")
                      .read_text(encoding="utf-8"))
meta_src.update(displayName=NAME, guid=GUID, sourcePath=f"Assets/Vfx/{NAME}.hvfx")
meta = pathlib.Path(args.get("meta", str(ROOT / "ProjectSettings/AssetMeta/Assets/Vfx"))) / f"{NAME}.hvfx.hikari.asset.json"
meta.parent.mkdir(parents=True, exist_ok=True)
meta.write_text(json.dumps(meta_src, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("->", path, "sources", len(SOURCES))
