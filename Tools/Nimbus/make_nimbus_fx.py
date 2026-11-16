"""筋斗雲の召喚と解除の一度きりのエフェクトを作る（カード c-87d7cd）：Assets/Vfx/NimbusArrive.hvfx・NimbusDisperse.hvfx と
資産のメタ。どちらも流体の域を持たない（域の枠を使わず、出したフレームに詰まらない）。雲そのものの飛来・沈み・散りは
本体の雲（NimbusCloud.hvfx、make_nimbus_cloud.py）を CloudMountVisuals が動かして出す。ここは上に重ねる飾りだけ。
- NimbusArrive：雲が足元に着いた瞬間。空気のゆがみの輪（Distortion。太く柔らかい glow_halo をカメラへ向けて広げる。
  細い輪の貼図はぎざぎざになる：k-8288aa）、やわらかい光、金の光の粒。入力 input.ring（Color）＝輪の強さの倍率、
  input.glow（Color）＝光と粒の明るさの倍率。どちらも 0 にすると何も見えない（場面の始めの下準備に使う）。
- NimbusDisperse：解除で雲が散る瞬間。外へ散って消える金と白の光の粒。
地面に寝かせた輪は作らない（攻撃の予告に見える：k-46d3cb）。
GourdCatch.hvfx の層（輪・光・粒）を写して値を変える。エディタで資産が直されていたら（最後にこのスクリプトが書いた内容と
違えば）書かない。上書きするなら force=1。引数：[out=<フォルダ>] [meta=<フォルダ>] [force=1]"""
import copy, hashlib, json, pathlib, sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
STAMP = pathlib.Path(__file__).with_name("nimbus_fx.last.json")
args = dict(a.split("=", 1) for a in sys.argv[1:] if "=" in a)
out_dir = pathlib.Path(args.get("out", str(ROOT / "Assets/Vfx")))
meta_dir = pathlib.Path(args.get("meta", str(ROOT / "ProjectSettings/AssetMeta/Assets/Vfx")))
src = json.loads((ROOT / "Assets/Vfx/GourdCatch.hvfx").read_text(encoding="utf-8"))
layers = {e["name"]: e for e in src["emitters"]}
RING_INPUT = "input.ring"
GLOW_INPUT = "input.glow"


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


def curve(keys, tag):
    """[(時間, 値), …] の滑らかな曲線（3 軸とも同じ）。"""
    def axis(name):
        return {"interpolation": "Smooth", "randomOffset": {"maximum": 0.0, "minimum": 0.0, "mode": "Constant",
                                                            "randomGroup": 0},
                "keys": [{"id": f"key.{tag}.{name}.{i}", "inTangent": 0.0, "outTangent": 0.0, "time": t, "value": v}
                         for i, (t, v) in enumerate(keys)]}
    return {"x": axis("x"), "y": axis("y"), "z": axis("z")}


def gradient(stops, tag):
    """[(時間, [r, g, b, a]), …] の色の変わり方（寿命に沿う）。"""
    return {"stops": [{"color": list(c), "id": f"emitter.{tag}.c{i}", "time": t} for i, (t, c) in enumerate(stops)]}


def layer(base, name, order, life, size, color, position=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    out = copy.deepcopy(layers[base])
    reid(out, "nimbusfx." + name)
    out["name"] = name
    out["order"] = order
    out["parentEmitterId"] = ""
    out["inputLinks"] = []
    modules = out["modules"]
    modules["emitterTransform"]["transform"].update(position=list(position), rotationEulerDegrees=[0.0, 0.0, 0.0],
                                                    scale=list(scale))
    modules["basic"]["lifetime"].update(life)
    modules["scale"]["value"]["mode"] = "Curve"
    modules["scale"]["value"]["curve"] = curve(size, name.replace(" ", ""))
    modules["color"]["value"]["mode"] = "Gradient"
    modules["color"]["value"]["gradient"] = gradient(color, name.replace(" ", ""))
    return out


def sparkles(name, order, count, radius, speed, life, size, color, gravity, drag, position, scale):
    """光の粒（カメラへ向けた丸い点。向きのある粒は使わない：k-5a5fc0）。外へ散って消える。"""
    out = layer("Catch Sparks", name, order, life, size, color, position, scale)
    modules = out["modules"]
    modules["spawn"]["burstTimeline"][0]["count"] = count
    modules["spawn"]["maximumParticles"] = max(32, count * 2)
    modules["shape"]["radius"].update(minimum=radius[0], maximum=radius[1], mode="RandomRange")
    modules["position"]["radialSpeed"].update(minimum=speed[0], maximum=speed[1], mode="RandomRange")
    modules["forces"].update(gravity=[0.0, gravity, 0.0], drag=drag)
    modules["renderShape"]["billboardMode"] = "RotatedBillboard"
    modules["renderCommon"]["colorTextureAssetGuid"] = "engine.vfx.texture.particle_soft"
    return out


def shot(name, duration, emitters, inputs):
    doc = {key: value for key, value in src.items() if key not in ("emitters", "inputs", "name")}
    doc.update(name=name, loop=False, durationSeconds=duration, inputs=inputs, emitters=emitters)
    return doc


# 着いた瞬間（根＝雲の芯）。輪は 0.4 秒で広がりながら薄れる。色 R＝ゆがみの強さ（画面の 6% × R。0.1〜0.3 が使える範囲）、
# B 0＝輪の縁が内へ屈折する。
RING_STRENGTH = 0.3
ring = layer("Catch Ring", "Arrive Ring", 0, dict(minimum=0.4, maximum=0.4), [(0.0, 1.5), (0.45, 4.5), (1.0, 6.0)],
             [(0.0, [RING_STRENGTH, 0.0, 0.0, 0.0]), (0.12, [RING_STRENGTH, 0.0, 0.0, 1.0]),
              (0.55, [RING_STRENGTH, 0.0, 0.0, 0.6]), (1.0, [RING_STRENGTH, 0.0, 0.0, 0.0])])
ring["activeDuration"] = 0.4


def color_link(target, input_id):
    """入力（Color）を層の色の倍率へつなぐ。"""
    target["inputLinks"] = [{"bias": 0.0, "id": f"{target['id']}.link.{input_id}.valueMultiplier", "inputId": input_id,
                             "scale": 1.0, "target": {"emitterId": target["id"],
                                                      "moduleId": target["modules"]["color"]["id"],
                                                      "propertyId": "valueMultiplier"}}]


color_link(ring, RING_INPUT)
glow = layer("Catch Flash", "Arrive Glow", 1, dict(minimum=0.28, maximum=0.28), [(0.0, 1.0), (0.3, 2.2), (1.0, 2.6)],
             [(0.0, [1.5, 1.3, 1.0, 0.0]), (0.2, [1.5, 1.3, 1.0, 0.45]), (1.0, [1.2, 0.9, 0.5, 0.0])])
glow["activeDuration"] = 0.28
arrive_sparkles = sparkles("Arrive Sparkles", 2, 14, (0.5, 0.9), (2.0, 4.0), dict(minimum=0.35, maximum=0.6),
                           [(0.0, 0.07), (1.0, 0.0)],
                           [(0.0, [3.0, 2.3, 1.2, 1.0]), (0.5, [2.4, 1.6, 0.6, 0.9]), (1.0, [1.2, 0.7, 0.25, 0.0])],
                           -1.5, 2.0, (0.0, 0.0, 0.0), (1.4, 0.6, 1.4))
color_link(glow, GLOW_INPUT)
color_link(arrive_sparkles, GLOW_INPUT)
ARRIVE = shot("Nimbus Arrive", 0.7, [ring, glow, arrive_sparkles],
              [{"defaultValue": [1.0, 1.0, 1.0, 1.0], "id": RING_INPUT, "name": "ring", "type": "Color"},
               {"defaultValue": [1.0, 1.0, 1.0, 1.0], "id": GLOW_INPUT, "name": "glow", "type": "Color"}])

# 散る瞬間（根＝雲の芯）。雲の平たい形に沿って外へ、少し浮きながら散る。
DISPERSE = shot("Nimbus Disperse", 0.9, [
    sparkles("Disperse Sparkles", 0, 18, (0.6, 1.1), (1.2, 2.8), dict(minimum=0.4, maximum=0.75),
             [(0.0, 0.06), (0.6, 0.045), (1.0, 0.0)],
             [(0.0, [2.6, 2.4, 2.0, 1.0]), (0.4, [2.4, 1.8, 0.9, 0.9]), (1.0, [1.2, 0.8, 0.3, 0.0])],
             0.4, 1.6, (0.0, 0.0, 0.0), (1.6, 0.5, 1.8)),
], [])

ASSETS = [("NimbusArrive", "5c1e7a3f2b8d4e60a9f4d2c7b1e08a36", ARRIVE),
          ("NimbusDisperse", "a7d93e15c4f2480bb6e1f0a2d95c3b74", DISPERSE)]

default_out = out_dir == ROOT / "Assets/Vfx"
stamps = json.loads(STAMP.read_text(encoding="utf-8")) if STAMP.exists() else {}
payloads = {name: (json.dumps(doc, ensure_ascii=False, indent=2) + "\n").replace("\n", "\r\n").encode("utf-8")
            for name, _, doc in ASSETS}
if default_out and args.get("force") != "1":
    for name, _, _ in ASSETS:
        path = out_dir / f"{name}.hvfx"
        if path.exists() and hashlib.sha1(path.read_bytes()).hexdigest() != stamps.get(name):
            sys.exit(f"{path} はエディタで直されている（最後に書いた内容と違う）。上書きするなら force=1")
meta_src = json.loads((ROOT / "ProjectSettings/AssetMeta/Assets/Vfx/GourdCatch.hvfx.hikari.asset.json")
                      .read_text(encoding="utf-8"))
for name, guid, _ in ASSETS:
    path = out_dir / f"{name}.hvfx"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payloads[name])
    stamps[name] = hashlib.sha1(payloads[name]).hexdigest()
    meta = dict(meta_src, displayName=name, guid=guid, sourcePath=f"Assets/Vfx/{name}.hvfx")
    meta_dir.mkdir(parents=True, exist_ok=True)
    (meta_dir / f"{name}.hvfx.hikari.asset.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n",
                                                            encoding="utf-8")
    print("->", path)
if default_out:
    STAMP.write_text(json.dumps(stamps, sort_keys=True) + "\n", encoding="utf-8")
