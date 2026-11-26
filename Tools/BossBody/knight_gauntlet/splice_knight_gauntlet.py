"""build_knight_gauntlet.py の籠手の網を、ボスの模型（glTF）へ入れる。草摺（下半身）も外す。c-8b643c。

使い方（numpy の要る Python。Blender に付いている物でよい）：
  python -I -X utf8 Tools/BossBody/knight_gauntlet/splice_knight_gauntlet.py [build=<フォルダ>] [models=<フォルダ>] [--metas]

入れる先（Assets/Models/）：
- BossIntroActor.gltf（骨格のボス。登場の役者 960 と戦いの体 970 が使う）：ノード HandL・HandR・CuffLMesh・CuffRMesh の
  網を籠手に替える（ノードの名前はそのまま。コードが名前で隠す・出す）。Kusazuri0〜6 のノードの網を外す（ノードは残す）。
- BossIntroGauntletL/R.gltf（登場で落ちている籠手）：IntroGauntlet<L|R>_Hand<L|R>・_Cuff<L|R> の網を替える。
  この模型の結び姿の空間＝役者の手の骨の空間なので、役者の手の骨の逆の結び行列を掛け、Cuff の骨の重みは手の骨へ寄せる。
材質 KnightGauntlet（KasumiBushi/KnightGauntlet_BaseColor・_ORM・_Normal）を材質の最後に足す（コードが材質の槽を番号で
名指ししているので、前の材質の順番は変えない）。何度流しても同じ結果になる（前に足した材質・貼図を使い直す）。
網の外れたアクセサーは書き出しの時に捨て、.bin を詰める（extras.helmGlow の印も詰めた後の番号へ直す）。
--metas：まだメタの無い貼図に資産のメタ（ProjectSettings/AssetMeta/...）を書く。
"""
import hashlib
import json
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
args = sys.argv[1:]
options = dict(a.split("=", 1) for a in args if "=" in a)
# models=<フォルダ>：写しの模型へ入れて確かめる時（既定はプロジェクトの Assets/Models）。
MODELS = pathlib.Path(options.get("models", ROOT / "Assets" / "Models"))
BUILD = pathlib.Path(options.get("build", ROOT / "Library" / "Handover" / "c-8b643c" / "build"))
MATERIAL = "KnightGauntlet"
TEXTURES = {"BaseColor": "KasumiBushi/KnightGauntlet_BaseColor.png", "ORM": "KasumiBushi/KnightGauntlet_ORM.png",
            "Normal": "KasumiBushi/KnightGauntlet_Normal.png"}
TASSETS = [f"Kusazuri{k}" for k in range(7)]
# build の npz のノード番号 → 役者のノード名・登場の籠手のノード名
PARTS = {0: ("HandL", "Left"), 1: ("CuffLMesh", "Left"), 2: ("HandR", "Right"), 3: ("CuffRMesh", "Right")}
INTRO_NODE = {"HandL": "IntroGauntletL_HandL", "CuffLMesh": "IntroGauntletL_CuffL",
              "HandR": "IntroGauntletR_HandR", "CuffRMesh": "IntroGauntletR_CuffR"}
CT = {5121: np.uint8, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class Gltf:
    def __init__(self, path):
        self.path = path
        self.doc = json.loads(path.read_text(encoding="utf-8"))
        self.blob = (path.parent / self.doc["buffers"][0]["uri"]).read_bytes()
        self.extra = []   # 足すデータ（(bufferView の番号, bytes)）

    def read(self, index):
        a = self.doc["accessors"][index]
        v = self.doc["bufferViews"][a["bufferView"]]
        dt, n = CT[a["componentType"]], NC[a["type"]]
        off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        assert v.get("byteStride", np.dtype(dt).itemsize * n) == np.dtype(dt).itemsize * n
        return np.frombuffer(self.blob, dt, a["count"] * n, off).reshape(a["count"], n)

    def node(self, name):
        return next(n for n in self.doc["nodes"] if n.get("name") == name)

    def add(self, array, kind, target=None, bounds=False):
        array = np.ascontiguousarray(array)
        code = {np.dtype(np.float32): 5126, np.dtype(np.uint8): 5121, np.dtype(np.uint16): 5123,
                np.dtype(np.uint32): 5125}[array.dtype]
        view = {"buffer": 0, "byteLength": array.nbytes}
        if target is not None:
            view["target"] = target
        self.doc["bufferViews"].append(view)
        self.extra.append((len(self.doc["bufferViews"]) - 1, array.tobytes()))
        acc = {"bufferView": len(self.doc["bufferViews"]) - 1, "componentType": code,
               "count": int(array.shape[0]), "type": kind}
        if bounds:
            acc["min"] = array.min(0).astype(float).tolist()
            acc["max"] = array.max(0).astype(float).tolist()
        self.doc["accessors"].append(acc)
        return len(self.doc["accessors"]) - 1

    def material(self):
        """KnightGauntlet の材質の番号（無ければ最後に足す。貼図・画像も使い回す）。"""
        doc = self.doc
        for i, m in enumerate(doc.get("materials", [])):
            if m.get("name") == MATERIAL:
                return i
        doc.setdefault("images", [])
        doc.setdefault("textures", [])
        if not doc.get("samplers"):
            doc["samplers"] = [{"magFilter": 9729, "minFilter": 9987}]

        def texture(uri):
            for ti, t in enumerate(doc["textures"]):
                if doc["images"][t["source"]].get("uri") == uri:
                    return ti
            doc["images"].append({"name": pathlib.Path(uri).stem, "uri": uri})
            doc["textures"].append({"sampler": 0, "source": len(doc["images"]) - 1})
            return len(doc["textures"]) - 1

        orm = texture(TEXTURES["ORM"])
        doc.setdefault("materials", []).append({
            "name": MATERIAL, "doubleSided": False,
            "pbrMetallicRoughness": {"baseColorTexture": {"index": texture(TEXTURES["BaseColor"])},
                                     "metallicRoughnessTexture": {"index": orm},
                                     "metallicFactor": 1.0, "roughnessFactor": 1.0},
            "normalTexture": {"index": texture(TEXTURES["Normal"])},
            "occlusionTexture": {"index": orm}})
        return len(doc["materials"]) - 1

    def save(self):
        """使われているアクセサーだけを残して詰め直し、.gltf と .bin を書く。"""
        doc = self.doc
        used = set()
        for m in doc.get("meshes", []):
            for p in m["primitives"]:
                used.update(p["attributes"].values())
                if "indices" in p:
                    used.add(p["indices"])
                for t in p.get("targets", []):
                    used.update(t.values())
        for s in doc.get("skins", []):
            if "inverseBindMatrices" in s:
                used.add(s["inverseBindMatrices"])
        for a in doc.get("animations", []):
            for s in a["samplers"]:
                used.update((s["input"], s["output"]))
        meshes_used = sorted({n["mesh"] for n in doc["nodes"] if "mesh" in n})
        mesh_map = {old: new for new, old in enumerate(meshes_used)}
        for n in doc["nodes"]:
            if "mesh" in n:
                n["mesh"] = mesh_map[n["mesh"]]
        doc["meshes"] = [doc["meshes"][i] for i in meshes_used]
        old_acc = doc["accessors"]
        keep = sorted(used)
        acc_map = {old: new for new, old in enumerate(keep)}
        extra = dict(self.extra)
        chunks, accessors, views, offset, view_start = [], [], [], 0, {}
        for old in keep:
            a = dict(old_acc[old])
            v = doc["bufferViews"][a["bufferView"]]
            if a["bufferView"] in extra:
                data = extra[a["bufferView"]]
            else:
                start = v.get("byteOffset", 0) + a.get("byteOffset", 0)
                size = np.dtype(CT[a["componentType"]]).itemsize * NC[a["type"]] * a["count"]
                data = self.blob[start:start + size]
            pad = (-offset) % 4
            chunks.append(b"\0" * pad)
            offset += pad
            view = {"buffer": 0, "byteOffset": offset, "byteLength": len(data)}
            if "target" in v:
                view["target"] = v["target"]
            view_start[old] = offset
            views.append(view)
            chunks.append(data)
            offset += len(data)
            a["bufferView"] = len(views) - 1
            a.pop("byteOffset", None)
            accessors.append(a)
        for m in doc["meshes"]:
            for p in m["primitives"]:
                p["attributes"] = {k: acc_map[v] for k, v in p["attributes"].items()}
                if "indices" in p:
                    p["indices"] = acc_map[p["indices"]]
                if "targets" in p:
                    p["targets"] = [{k: acc_map[v] for k, v in t.items()} for t in p["targets"]]
        for s in doc.get("skins", []):
            if "inverseBindMatrices" in s:
                s["inverseBindMatrices"] = acc_map[s["inverseBindMatrices"]]
        for an in doc.get("animations", []):
            for s in an["samplers"]:
                s["input"], s["output"] = acc_map[s["input"]], acc_map[s["output"]]
        # add_helm_glow.py の印（足す前の数）：詰めた後の番号へ（印より前で残った物の数）。
        mark = doc.get("extras", {}).get("helmGlow")
        if mark:
            first_acc = min((o for o in keep if o >= mark["accessors"]), default=None)
            mark["meshes"] = sum(1 for o in meshes_used if o < mark["meshes"])
            mark["accessors"] = sum(1 for o in keep if o < mark["accessors"])
            mark["bufferViews"] = mark["accessors"]
            mark["bytes"] = view_start[first_acc] if first_acc is not None else offset
        doc["accessors"], doc["bufferViews"] = accessors, views
        blob = b"".join(chunks)
        doc["buffers"][0]["byteLength"] = len(blob)
        (self.path.parent / doc["buffers"][0]["uri"]).write_bytes(blob)
        self.path.write_text(json.dumps(doc, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")
        print("[splice] wrote %s (%d accessors, %.1f MB)" % (self.path.name, len(accessors), len(blob) / 1e6))


def to_gltf(p):
    """Blender の座標（Z 上）→ glTF（Y 上）。"""
    return np.column_stack([p[:, 0], p[:, 2], -p[:, 1]]).astype(np.float32)


def primitive(g, part, joint_index, matrix=None, joint_remap=None):
    pos, nor = to_gltf(part["pos"]), to_gltf(part["nor"])
    if matrix is not None:
        pos = (np.c_[pos, np.ones(len(pos))] @ matrix.T)[:, :3].astype(np.float32)
        nor = (nor @ matrix[:3, :3].T).astype(np.float32)
    nor /= np.linalg.norm(nor, axis=1, keepdims=True)
    uv = part["uv"].copy()
    uv[:, 1] = 1.0 - uv[:, 1]
    names = part["jn"]
    if joint_remap:
        names = np.vectorize(lambda n: joint_remap.get(n, n))(names)
    joints = np.vectorize(joint_index.__getitem__)(names).astype(np.uint8)
    weights = part["jw"].astype(np.float32)
    # 同じ骨が 2 か所に来た（寄せた）時は 1 つにまとめる。
    for k in range(1, 4):
        for j in range(k):
            same = (joints[:, k] == joints[:, j]) & (weights[:, k] > 0)
            weights[same, j] += weights[same, k]
            weights[same, k] = 0.0
    weights /= weights.sum(1, keepdims=True)
    idx = part["idx"].ravel()
    idx = idx.astype(np.uint16 if len(pos) < 65536 else np.uint32)
    return {"attributes": {"POSITION": g.add(pos, "VEC3", 34962, bounds=True),
                           "NORMAL": g.add(nor, "VEC3", 34962),
                           "TEXCOORD_0": g.add(uv.astype(np.float32), "VEC2", 34962),
                           "JOINTS_0": g.add(joints, "VEC4", 34962),
                           "WEIGHTS_0": g.add(weights, "VEC4", 34962)},
            "indices": g.add(idx[:, None], "SCALAR", 34963), "material": g.material()}


def load_parts():
    data = np.load(BUILD / "KnightGauntlet_mesh.npz")
    return {nid: {k: data[f"n{nid}_{k}"] for k in ("pos", "nor", "uv", "jn", "jw", "idx")} for nid in PARTS}


def joints_of(g):
    skin = g.doc["skins"][0]
    return {g.doc["nodes"][j]["name"]: i for i, j in enumerate(skin["joints"])}


def set_mesh(g, node_name, prim):
    node = g.node(node_name)
    g.doc["meshes"].append({"name": node_name, "primitives": [prim]})
    node["mesh"] = len(g.doc["meshes"]) - 1
    node["skin"] = 0


def splice_actor(parts):
    g = Gltf(MODELS / "BossIntroActor.gltf")
    ji = joints_of(g)
    for nid, (node, _) in PARTS.items():
        set_mesh(g, node, primitive(g, parts[nid], ji))
    for name in TASSETS:
        n = g.node(name)
        n.pop("mesh", None)
        n.pop("skin", None)
    ibm = g.read(g.doc["skins"][0]["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
    g.save()
    return {name: ibm[i].astype(float) for name, i in ji.items()}


def splice_intro(parts, actor_ibm):
    for side, s in (("Left", "L"), ("Right", "R")):
        g = Gltf(MODELS / f"BossIntroGauntlet{s}.gltf")
        ji = joints_of(g)
        hand = f"{side}Hand"
        to_hand = actor_ibm[hand]     # 役者の結び姿 → 手の骨の空間（＝この模型の結び姿）
        # 確かめ：この模型の指の骨の逆の結び行列が、役者のもの × 役者の手の骨の結び行列と同じ。
        ibm = g.read(g.doc["skins"][0]["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
        for name, i in ji.items():
            expect = actor_ibm[name] @ np.linalg.inv(to_hand)
            err = np.abs(ibm[i] - expect).max()
            assert err < 2e-3, (s, name, err)
        remap = {f"Cuff{s}": hand}
        for nid, (node, part_side) in PARTS.items():
            if part_side == side:
                set_mesh(g, INTRO_NODE[node], primitive(g, parts[nid], ji, to_hand, remap))
        g.save()


def write_metas():
    src = ROOT / "ProjectSettings/AssetMeta/Assets/Models/KasumiBushi"
    for key, rel in TEXTURES.items():
        rel_path = f"Assets/Models/{rel}"
        meta = ROOT / "ProjectSettings/AssetMeta" / (rel_path + ".hikari.asset.json")
        if meta.exists():
            continue
        template = json.loads((src / f"KnightBody_{key}.png.hikari.asset.json").read_text(encoding="utf-8"))
        template.update(displayName=pathlib.Path(rel).stem, guid=hashlib.md5(rel_path.encode("utf-8")).hexdigest(),
                        sourcePath=rel_path)
        meta.write_text(json.dumps(template, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print("[splice] meta", meta.name)


def main():
    parts = load_parts()
    actor_ibm = splice_actor(parts)
    splice_intro(parts, actor_ibm)
    if "--metas" in args:
        write_metas()


main()
