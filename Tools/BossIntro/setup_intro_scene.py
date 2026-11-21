"""ボスの登場の役者と、付く前の部品（兜・手甲）の物体をシーンに置く（c-e0c500）。何度流しても同じ結果になる。
使い方：python -X utf8 Tools/BossIntro/setup_intro_scene.py [シーン] [--keep]
  シーンの既定は Assets/Scenes/SmokeArena.scene.json。--keep を付けると、もうある物体の位置と向きは変えない
  （エディタで置き直した所を残す）。付けなければ、intro_layout.json の既定の位置へ戻す。

- 役者（IntroActor）：ボスの物体の足元（体の中心から体の高さの半分だけ下）に、同じ向きで置く。模型は
  Assets/Models/BossIntroActor、動画の部品は片段 Rise の最初の姿で止めておく（演技は演出の資産が流す）。
- 兜・右の手甲・左の手甲：役者の子。位置と向きは役者に対する値（intro_layout.json。エディタで自由に動かしてよい）。
- ボスの部品（Project_TD2_2.Boss）の項目 introActorObject などに、物体の番号を入れる。
物体の番号は 960〜966（ほかと重ならない番号に固定。演出の資産の結びがこの番号を指す。964 は演出の灯、965・966 は
兜の中の灯）。
"""
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
args = [a for a in sys.argv[1:] if not a.startswith("--")]
KEEP = "--keep" in sys.argv
scene_path = ROOT / (args[0] if args else "Assets/Scenes/SmokeArena.scene.json")
layout = json.loads((Path(__file__).resolve().parent / "intro_layout.json").read_text(encoding="utf-8"))
BOSS_ID, ACTOR_ID, PLAYER_ID = 3, 960, 2
PARTS = (("Helm", 961, "IntroHelm", "BossIntroHelm", "", "introHelmObject"),
         ("HandR", 962, "IntroGauntletR", "BossIntroGauntletR", "Twitch", "introGauntletRightObject"),
         ("HandL", 963, "IntroGauntletL", "BossIntroGauntletL", "Twitch", "introGauntletLeftObject"))
# ボスの体の中心の高さ（Boss.bodyHalfHeight の既定。シーンで変えていれば、その値を使う）。
HALF_HEIGHT = 3.375

raw = scene_path.read_bytes()
crlf = b"\r\n" in raw
scene = json.loads(raw.decode("utf-8"))
objects = {o["id"]: o for o in scene["objects"]}
boss = objects[BOSS_ID]
boss_props = next(c for c in boss["components"] if c["type"] == "Project_TD2_2.Boss").setdefault("properties", {})
half = float(boss_props.get("bodyHalfHeight", HALF_HEIGHT))


def model(guid):
    return {"id": 1, "type": "ModelComponent",
            "properties": {"assetId": guid, "castShadow": True, "receiveShadow": True, "renderStatic": False, "visible": True}}


def animator(clip, guid):
    return {"id": 2, "type": "AnimatorComponent",
            "properties": {"autoPlay": False, "clip": clip, "clipModelAssetId": guid if clip else "", "finished": False,
                           "loop": False, "playing": False, "rootMotionEnabled": False, "speed": 1.0, "timeSec": 0.0}}


def ensure(oid, name, parent, transform, components):
    old = objects.get(oid)
    obj = {"components": components, "editorLocked": False, "editorVisible": True, "enabled": True, "id": oid, "name": name,
           "parent": parent, "sourcePrefabId": "",
           "transform": old["transform"] if (old is not None and KEEP) else transform}
    if old is None:
        scene["objects"].append(obj)
    else:
        scene["objects"][scene["objects"].index(old)] = obj
    objects[oid] = obj


bp = boss["transform"]["position"]
guid = layout["models"]["BossIntroActor"]
# 役者の向き：プレイヤーの物体の方を向ける（Play では、演技が始まる時のプレイヤーの方へコードが向け直す。編集器の
# プレビューでも同じ向きの背景でカメラを見られるように、シーンの値も合わせておく）。
player = objects.get(PLAYER_ID)
yaw = boss["transform"]["rotationEulerDeg"][1]
if player is not None:
    pp = player["transform"]["position"]
    if abs(pp[0] - bp[0]) + abs(pp[2] - bp[2]) > 0.5:
        yaw = round(math.degrees(math.atan2(pp[0] - bp[0], pp[2] - bp[2])), 3)
ensure(ACTOR_ID, "IntroActor", None,
       {"position": [bp[0], bp[1] - half, bp[2]], "rotationEulerDeg": [0.0, yaw, 0.0], "scale": [1.0, 1.0, 1.0]},
       [model(guid), animator(layout["clips"][0]["name"], guid)])
boss_props["introActorObject"] = {"$hikariRef": "SceneObject", "id": ACTOR_ID}
for key, oid, name, asset, clip, prop in PARTS:
    part = layout["parts"][key]
    guid = layout["models"][asset]
    ensure(oid, name, ACTOR_ID,
           {"position": part["position"], "rotationEulerDeg": part["rotationEulerDeg"], "scale": [1.0, 1.0, 1.0]},
           [model(guid), animator(clip, guid)])
    boss_props[prop] = {"$hikariRef": "SceneObject", "id": oid}
# 演出の灯：役者の子のスポットライト。ふだんは消してあり、演出の間だけコードが点ける。置き場所・向き・明るさは、演出の
# 資産の物体の轨がカットごとに決める（明るさは、ここに書く値への倍率）。明るさの単位は「場面の光に対する倍率」
# （1 m 先の照度が、太陽と空の照度の何倍か）なので、昼でも夕でも同じ見え方になる。
LIGHT_ID = 964
ensure(LIGHT_ID, "IntroKeyLight", ACTOR_ID,
       {"position": [-6.0, 5.5, 9.0], "rotationEulerDeg": [5.0, 146.0, 0.0], "scale": [1.0, 1.0, 1.0]},
       [{"id": 1, "type": "LightComponent",
         "properties": {"enabled": False, "type": "Spot", "color": [1.0, 0.93, 0.84], "intensityUnit": "SceneRelative",
                        "sceneRelativeIntensity": 40.0, "intensity": 800.0, "range": 60.0, "castsShadows": False,
                        "sourceRadius": 0.3, "innerConeAngleDegrees": 24.0, "outerConeAngleDegrees": 40.0,
                        "angularFalloff": 1.0, "priority": 50}}])
boss_props["introKeyLightObject"] = {"$hikariRef": "SceneObject", "id": LIGHT_ID}
# 兜の中の灯（登場でも戦いでも使う）：前を向く影付きのスポットライトと、兜の中を満たす弱い点光。親は無し（置き場所と
# 明るさ・色は、コードが毎フレーム兜に合わせて入れる。ふだんは消してある）。面頬の隙間から本物の光が漏れる。
HELM_SPOT_ID, HELM_FILL_ID = 965, 966
head = [bp[0], bp[1] + 2.5, bp[2]]


def helm_light(kind, intensity, light_range, shadows, extra):
    props = {"enabled": False, "type": kind, "color": [0.37, 0.58, 1.0], "intensityUnit": "SceneRelative",
             "sceneRelativeIntensity": intensity, "intensity": 800.0, "range": light_range, "castsShadows": shadows,
             "sourceRadius": 0.05, "priority": 80}
    props.update(extra)
    return [{"id": 1, "type": "LightComponent", "properties": props}]


ensure(HELM_SPOT_ID, "BossHelmSpot", None,
       {"position": head, "rotationEulerDeg": [4.0, yaw, 0.0], "scale": [1.0, 1.0, 1.0]},
       helm_light("Spot", 3.0, 3.0, True, {"innerConeAngleDegrees": 34.0, "outerConeAngleDegrees": 62.0, "angularFalloff": 1.0}))
ensure(HELM_FILL_ID, "BossHelmFill", None,
       {"position": head, "rotationEulerDeg": [0.0, yaw, 0.0], "scale": [1.0, 1.0, 1.0]},
       helm_light("Point", 0.25, 1.0, False, {}))
boss_props["bodyHelmSpotObject"] = {"$hikariRef": "SceneObject", "id": HELM_SPOT_ID}
boss_props["bodyHelmFillObject"] = {"$hikariRef": "SceneObject", "id": HELM_FILL_ID}
# 前の版が入れた「兜の中の目」の物体（967）と項目は取り除く（光は、兜の模型の中の面で出す：Tools/BossBody/add_helm_glow.py）。
scene["objects"] = [o for o in scene["objects"] if o["id"] != 967]
boss_props.pop("bodyHelmEyesObject", None)
text = json.dumps(scene, ensure_ascii=False, indent=2) + "\n"
scene_path.write_bytes(text.replace("\n", "\r\n").encode("utf-8") if crlf else text.encode("utf-8"))
print("WROTE", scene_path, "objects", [(o["id"], o["name"]) for o in scene["objects"] if o["id"] >= ACTOR_ID])
