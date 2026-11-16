"""筋斗雲の飛行の飾りを作る（カード c-6eb449）：Assets/Vfx/NimbusTrail.hvfx・NimbusBoost.hvfx と資産のメタ。
どちらも流体の域を持たない（シェーダーの層だけ。域の枠を使わない）。
- NimbusTrail（雲の根＝足元に付けて、本体の雲と同じ変換で毎フレーム送る。ずっと出し、入力で出し入れする）：
  - 長い尾の帯（Streak *）：雲の左右の端から後ろへ伸びる細い白い筋。軌跡の履歴（history=Trail）で、長さは時間で決まる
    （duration 秒 × 速さ。巡航で約 15 m、ブーストで 30 m 以上）。帯の断面は層の局所 X（水平。後ろ上のカメラから見える）。
    入力 input.streak（Color）＝明るさの倍率。
  - 空気のゆがみ（Haze）：雲の後ろに沿う屈折の帯（Distortion。distortion_noise を時間の向きに流さずタイル 1：k-b51982）。
    入力 input.haze（Color）＝強さの倍率（資産の強さ HAZE_STRENGTH に掛かる。0.1〜0.3 が使える範囲：k-8288aa）。
  - 高速の気流の錐（Cone *）：体の前の円の上の点から後ろへ引く、短い加算の軌跡の筋（長さ＝秒 × 速さ）。体の真上・
    真下には置かない（体と進む先を隠さない）。入力 input.cone（Color）＝明るさの倍率。
- NimbusBoost（ブーストを押した瞬間に、雲の芯に一度だけ出す。世界に置くので、前へ飛ぶ体から見て後ろへ抜ける）：
  カメラへ向けた太く柔らかいゆがみの輪（glow_halo の Distortion。細い輪の貼図はぎざぎざになる：k-8288aa）と、薄い白い輪。
  入力 input.ring（Color）＝輪の強さの倍率。0.5 秒以内に消える。
地面に寝かせた輪・色つきの殻は作らない（攻撃の予告・ガラスの覆いに見える：k-46d3cb、g-4c7e19）。
資産に無い入力を送ると再生を断られる（k-a2812a）ので、名前は CloudMountVisuals.cpp と合わせる。
層は BossDash（帯）・BossBlade（ゆがみの帯・錐の筋）・GourdCatch（輪）から写す。エディタで資産が直されていたら（最後に
このスクリプトが書いた内容と違えば）書かない。上書きするなら force=1。引数：[out=<フォルダ>] [meta=<フォルダ>] [force=1]"""
import copy, hashlib, json, math, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
STAMP = pathlib.Path(__file__).with_name("nimbus_trims.last.json")
args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a)
out_dir = pathlib.Path(args.get("out", str(ROOT / "Assets/Vfx")))
meta_dir = pathlib.Path(args.get("meta", str(ROOT / "ProjectSettings/AssetMeta/Assets/Vfx")))


def load(name):
    return json.loads((ROOT / f"Assets/Vfx/{name}.hvfx").read_text(encoding="utf-8"))


def emitter(doc, name):
    return next(e for e in doc["emitters"] if e["name"] == name)


DASH, BLADE, CATCH = load("BossDash"), load("BossBlade"), load("GourdCatch")
STREAK_INPUT, HAZE_INPUT, CONE_INPUT, RING_INPUT = "input.streak", "input.haze", "input.cone", "input.ring"


def num(key, default):
    return float(args.get(key, default))


# ---- 値（引数で上書きして試せる：streakWidth=0.4 など） ----
STREAK_X = num("streakX", 1.1)            # 帯を出す雲の端（足元からの左右の距離 m）
STREAK_Y = num("streakY", -0.3)           # 帯の高さ（足元から。雲の芯）
STREAK_Z = num("streakZ", -0.5)           # 帯の頭の前後（足元から。後ろが負）
STREAK_WIDTH = num("streakWidth", 1.4)    # 帯の幅 (m。貼図 soft_line の明るい芯はこの 3 割ほど)
STREAK_SECONDS = num("streakSeconds", 0.75)  # 帯の長さ（秒。× 速さ ＝ m）
STREAK_ALPHA = num("streakAlpha", 0.26)   # 白い帯の不透明度（頭）
STREAK_FADE = num("streakFade", 3.0)      # 尾へ薄れる速さ（大きいほど頭の近くで消える。カメラの横を抜ける所を薄く）
STREAK_SPREAD = num("streakSpread", 2.2)  # 尾の幅（頭に対する倍率。尾ほど広がって霞む）
GOLD_WIDTH = num("goldWidth", 0.6)        # 帯の芯の金の霞の幅 (m)
GOLD_ALPHA = num("goldAlpha", 0.3)
HAZE_STRENGTH = num("hazeStrength", 0.2)  # ゆがみの強さ（画面の 6% × これ）
HAZE_WIDTH = num("hazeWidth", 1.8)
HAZE_SECONDS = num("hazeSeconds", 0.45)
CONE_RADIUS = num("coneRadius", 1.6)      # 錐の筋の、体の中心からの距離 (m)
CONE_HEIGHT = num("coneHeight", 1.0)      # 体の中心の足元からの高さ (m)
CONE_FRONT = num("coneFront", 0.9)        # 筋の頭（体の前）の前後 (m)
CONE_WIDTH = num("coneWidth", 0.3)
CONE_SECONDS = num("coneSeconds", 0.14)   # 筋の長さ（秒。× 速さ。ブーストで約 4.5 m）
CONE_GAIN = num("coneGain", 1.0)          # 筋の明るさ（色の倍率）
# 筋の置き場所（体の中心を回る角度、度。+X＝右が 0、上が 90）。真上・真下は空ける。
CONE_ANGLES = [18.0, 52.0, 128.0, 162.0, 205.0, 335.0]
RING_STRENGTH = num("ringStrength", 0.28)


def reid(node, tag):
    """層を写した時、層とモジュールと鍵の id を別のものにする。"""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "id" and isinstance(value, str) and value.split(".")[0] in ("module", "emitter", "key"):
                head, _, tail = value.rpartition(".")
                node[key] = head + "." + hashlib.md5((tail + tag).encode()).hexdigest()
            else:
                reid(value, tag)
    elif isinstance(node, list):
        for item in node:
            reid(item, tag)


def copy_layer(base, name, order, position, rotation=(0.0, 0.0, 0.0)):
    out = copy.deepcopy(base)
    reid(out, "nimbustrims." + name)
    out["name"] = name
    out["order"] = order
    out["parentEmitterId"] = ""
    out["inputLinks"] = []
    out["modules"]["emitterTransform"]["transform"].update(position=list(position),
                                                           rotationEulerDegrees=list(rotation), scale=[1.0, 1.0, 1.0])
    return out


def solid(layer, color):
    """層の色を寿命の間ずっと同じにする（帯の頭の色。尾は history の tailColor）。"""
    value = layer["modules"]["color"]["value"]
    value["mode"] = "Gradient"
    value["gradient"]["stops"] = [{"color": list(color), "id": f"{layer['id']}.c{i}", "time": float(i)}
                                  for i in range(2)]


def color_link(layer, input_id):
    layer["inputLinks"] = [{"bias": 0.0, "id": f"{layer['id']}.link.{input_id}.valueMultiplier", "inputId": input_id,
                            "scale": 1.0, "target": {"emitterId": layer["id"],
                                                     "moduleId": layer["modules"]["color"]["id"],
                                                     "propertyId": "valueMultiplier"}}]


def streak(name, order, position, width, blend, color, tail, tail_width, seconds, fade, input_id, samples=16):
    layer = copy_layer(emitter(DASH, "Dash Band L"), name, order, position)
    layer["modules"]["renderShape"]["ribbonWidth"] = width
    layer["modules"]["renderCommon"].update(blendMode=blend, colorTextureAssetGuid="engine.vfx.texture.soft_line")
    solid(layer, color)
    # 急に折れる動き（場面の作り直し・召喚の飛来）で長い筋を引かないよう、根が breakDistance より飛んだら切る。
    layer["history"].update(mode="Trail", duration=seconds, samples=samples, breakDistance=8.0,
                            tailWidth=tail_width, fadePower=fade, tailColor=list(tail))
    color_link(layer, input_id)
    return layer


def haze(order):
    layer = copy_layer(emitter(BLADE, "Trail Haze"), "Haze", order, (0.0, STREAK_Y, -1.4))
    layer["modules"]["renderShape"]["ribbonWidth"] = HAZE_WIDTH
    # 色 R＝強さ、B 1＝貼図の RG をゆがみの向きに使う（k-8288aa）。
    solid(layer, [HAZE_STRENGTH, 0.0, 1.0, 1.0])
    layer["history"].update(mode="Trail", duration=HAZE_SECONDS, samples=12, breakDistance=8.0, tailWidth=1.4,
                            fadePower=1.0, tailColor=[1.0, 1.0, 1.0, 0.0])
    color_link(layer, HAZE_INPUT)
    return layer


def cone(index, angle, order):
    """体のまわりの短い気流の筋：体の前の円の上の点から後ろへ引く軌跡（長さ＝秒 × 速さ。速いほど長い）。帯の断面は
    水平（後ろ上のカメラから見える）。最初は Ribbon＋CylindricalBillboard の筋を UvFlow で流す作りにしたが、headless の
    画に出なかったので、帯と同じ軌跡の作りにした。"""
    rad = math.radians(angle)
    position = (CONE_RADIUS * math.cos(rad), CONE_HEIGHT + CONE_RADIUS * math.sin(rad), CONE_FRONT)
    gain = CONE_GAIN
    return streak(f"Cone {index}", order, position, CONE_WIDTH, "Additive", [0.95 * gain, 0.9 * gain, 0.8 * gain, 0.8],
                  [0.9, 0.8, 0.6, 0.0], 0.5, CONE_SECONDS, 1.0, CONE_INPUT, samples=6)


def gradient(stops, tag):
    return {"stops": [{"color": list(c), "id": f"emitter.{tag}.c{i}", "time": t} for i, (t, c) in enumerate(stops)]}


def curve(keys, tag):
    def axis(name):
        return {"interpolation": "Smooth", "randomOffset": {"maximum": 0.0, "minimum": 0.0, "mode": "Constant",
                                                            "randomGroup": 0},
                "keys": [{"id": f"key.{tag}.{name}.{i}", "inTangent": 0.0, "outTangent": 0.0, "time": t, "value": v}
                         for i, (t, v) in enumerate(keys)]}
    return {"x": axis("x"), "y": axis("y"), "z": axis("z")}


def ring(name, order, life, size, color, blend=None):
    layer = copy_layer(emitter(CATCH, "Catch Ring"), name, order, (0.0, 0.0, 0.0))
    modules = layer["modules"]
    modules["basic"]["lifetime"].update(minimum=life, maximum=life)
    modules["scale"]["value"]["mode"] = "Curve"
    modules["scale"]["value"]["curve"] = curve(size, name.replace(" ", ""))
    modules["color"]["value"]["mode"] = "Gradient"
    modules["color"]["value"]["gradient"] = gradient(color, name.replace(" ", ""))
    if blend:
        modules["renderCommon"]["blendMode"] = blend
    layer["activeDuration"] = life
    color_link(layer, RING_INPUT)
    return layer


# 長い尾の帯：左右の白い帯（Alpha）と、その芯の細い金の線（加算。白里透金）。
WHITE, WHITE_TAIL = [1.0, 0.98, 0.95, STREAK_ALPHA], [1.0, 0.96, 0.9, 0.0]
GOLD, GOLD_TAIL = [0.9, 0.66, 0.32, GOLD_ALPHA], [0.8, 0.55, 0.22, 0.0]
TRAIL_LAYERS = [
    streak("Streak Left", 0, (STREAK_X, STREAK_Y, STREAK_Z), STREAK_WIDTH, "Alpha", WHITE, WHITE_TAIL, STREAK_SPREAD,
           STREAK_SECONDS, STREAK_FADE, STREAK_INPUT),
    streak("Streak Right", 1, (-STREAK_X, STREAK_Y, STREAK_Z), STREAK_WIDTH, "Alpha", WHITE, WHITE_TAIL,
           STREAK_SPREAD, STREAK_SECONDS, STREAK_FADE, STREAK_INPUT),
    streak("Streak Gold Left", 2, (STREAK_X, STREAK_Y, STREAK_Z), GOLD_WIDTH, "Additive", GOLD, GOLD_TAIL, 1.5,
           STREAK_SECONDS * 0.6, STREAK_FADE, STREAK_INPUT),
    streak("Streak Gold Right", 3, (-STREAK_X, STREAK_Y, STREAK_Z), GOLD_WIDTH, "Additive", GOLD, GOLD_TAIL, 1.5,
           STREAK_SECONDS * 0.6, STREAK_FADE, STREAK_INPUT),
    haze(4),
] + [cone(i, angle, 5 + i) for i, angle in enumerate(CONE_ANGLES)]
# 層の UV の効果（錐の UvFlow など）を持つ資産は、全部の層の後処理の扱いをそろえる（そろわないと取り込みで
# Vfx.MixedCompositionPostProcessMode）。BossBlade と同じ PreserveColor にする。
for _layer in TRAIL_LAYERS:
    _layer["modules"]["renderCommon"]["postProcessMode"] = "PreserveColor"

TRAIL_DOC = copy.deepcopy({k: v for k, v in DASH.items() if k not in ("emitters", "inputs", "name")})
TRAIL_DOC.update(name="Nimbus Trail", loop=True, durationSeconds=3600.0, emitters=TRAIL_LAYERS, inputs=[
    {"defaultValue": [0.0, 0.0, 0.0, 1.0], "id": STREAK_INPUT, "name": "streak", "type": "Color"},
    {"defaultValue": [0.0, 1.0, 1.0, 1.0], "id": HAZE_INPUT, "name": "haze", "type": "Color"},
    {"defaultValue": [0.0, 0.0, 0.0, 1.0], "id": CONE_INPUT, "name": "cone", "type": "Color"}])

# ブーストの衝撃の輪：0.45 秒で 1.5 m → 7 m に広がりながら薄れる（カメラへ向ける＝後ろのカメラから見て進む向きに垂直）。
BOOST_LAYERS = [
    ring("Boost Ring", 0, 0.45, [(0.0, 1.5), (0.4, 5.0), (1.0, 7.0)],
         [(0.0, [RING_STRENGTH, 0.0, 0.0, 0.0]), (0.1, [RING_STRENGTH, 0.0, 0.0, 1.0]),
          (0.5, [RING_STRENGTH, 0.0, 0.0, 0.6]), (1.0, [RING_STRENGTH, 0.0, 0.0, 0.0])]),
    ring("Boost Ring White", 1, 0.35, [(0.0, 1.2), (0.4, 3.8), (1.0, 5.2)],
         [(0.0, [1.0, 0.97, 0.9, 0.0]), (0.12, [1.0, 0.97, 0.9, 0.22]), (1.0, [1.0, 0.9, 0.7, 0.0])], "Additive"),
]
BOOST_DOC = copy.deepcopy({k: v for k, v in CATCH.items() if k not in ("emitters", "inputs", "name")})
BOOST_DOC.update(name="Nimbus Boost", loop=False, durationSeconds=0.5, emitters=BOOST_LAYERS, inputs=[
    {"defaultValue": [1.0, 1.0, 1.0, 1.0], "id": RING_INPUT, "name": "ring", "type": "Color"}])

ASSETS = [("NimbusTrail", "3e8b1c94d27f4a05b6c0e9a7f15d2b48", TRAIL_DOC, "BossDash"),
          ("NimbusBoost", "c72f05a1e94b4d3a8e6b1f0d2a7c9e53", BOOST_DOC, "GourdCatch")]

default_out = out_dir == ROOT / "Assets/Vfx"
stamps = json.loads(STAMP.read_text(encoding="utf-8")) if STAMP.exists() else {}
payloads = {name: (json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8")
            for name, _, doc, _ in ASSETS}
if default_out and args.get("force") != "1":
    for name, *_ in ASSETS:
        path = out_dir / f"{name}.hvfx"
        if path.exists() and hashlib.sha1(path.read_bytes()).hexdigest() != stamps.get(name):
            sys.exit(f"{path} はエディタで直されている（最後に書いた内容と違う）。上書きするなら force=1")
for name, guid, _, meta_from in ASSETS:
    path = out_dir / f"{name}.hvfx"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payloads[name])
    stamps[name] = hashlib.sha1(payloads[name]).hexdigest()
    meta_src = json.loads((ROOT / f"ProjectSettings/AssetMeta/Assets/Vfx/{meta_from}.hvfx.hikari.asset.json")
                          .read_text(encoding="utf-8"))
    meta = dict(meta_src, displayName=name, guid=guid, sourcePath=f"Assets/Vfx/{name}.hvfx")
    meta_dir.mkdir(parents=True, exist_ok=True)
    (meta_dir / f"{name}.hvfx.hikari.asset.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
                                                            encoding="utf-8")
    print("->", path, len(doc["emitters"]) if (doc := (TRAIL_DOC if name == "NimbusTrail" else BOOST_DOC)) else 0)
if default_out:
    STAMP.write_text(json.dumps(stamps, sort_keys=True) + "\n", encoding="utf-8")
