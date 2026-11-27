"""二人がそれぞれ役者の模型 BossIntroActor にクリップを足した時に、片方の版へもう片方のクリップを写して 1 つにする（c-60e87b）。
使い方（リポジトリの直下で）：
  python -I -X utf8 Tools/BossBody/merge_actor_clips.py <土台の版> <写す元の版> <クリップ> [<クリップ> ...]
  例：python -I -X utf8 Tools/BossBody/merge_actor_clips.py HEAD origin/develop-kujata Atk_Evade2
<土台の版> の BossIntroActor.gltf/.bin（メッシュ・材質・兜の光・今あるクリップ）をそのまま使い、<写す元の版> の指定のクリップの
アクセサ・バッファビュー・中身を後ろへ足して、作業ツリーの Assets/Models/ に書く。土台の .bin は 1 バイトも動かさない。
マージで .gltf/.bin が衝突した時に、衝突を解く代わりに流す（流した後で git add する）。
書いた後で、全部のクリップが元の版と、全部のメッシュが土台とバイト単位で同じかを確かめ、違えば終了コード 1。
骨格（節点・スキン）が二つの版で違う時は止まる（クリップの行き先の骨がずれるので、写せない）。
.bin は Git LFS なので、版の中身は .git/lfs/objects から読む。無ければ先に git lfs fetch origin <ブランチ> --include=<.bin の場所>。
"""
import hashlib
import json
import os
import subprocess
import sys

GLTF = "Assets/Models/BossIntroActor.gltf"
BIN = "Assets/Models/BossIntroActor.bin"
# glTF の部品の型（componentType）→ 1 つのバイト数、型（type）→ 部品の数。
COMPONENT_BYTES = {5120: 1, 5121: 1, 5122: 2, 5123: 2, 5125: 4, 5126: 4}
COMPONENT_COUNT = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}
# バッファビューの始まりをそろえる幅（glTF の決まり：4 バイト）。
ALIGN = 4


def git_show(rev, path):
    raw = subprocess.run(["git", "show", f"{rev}:{path}"], capture_output=True, check=True).stdout
    if not raw.startswith(b"version https://git-lfs"):
        return raw
    oid = next(l for l in raw.decode().splitlines() if l.startswith("oid sha256:")).split(":")[1]
    common = subprocess.run(["git", "rev-parse", "--git-common-dir"], capture_output=True, check=True,
                            text=True).stdout.strip()
    path = os.path.join(common, "lfs", "objects", oid[:2], oid[2:4], oid)
    if not os.path.exists(path):
        sys.exit(f"LFS の中身がありません：{rev}:{BIN}（git lfs fetch origin <ブランチ> --include={BIN}）")
    with open(path, "rb") as f:
        return f.read()


def load(rev):
    doc = json.loads(git_show(rev, GLTF))
    data = git_show(rev, BIN)
    if len(data) != doc["buffers"][0]["byteLength"]:
        sys.exit(f"{rev}：.bin の長さが .gltf の buffers[0] と合いません")
    return doc, data


def accessor_bytes(doc, data, index):
    acc = doc["accessors"][index]
    view = doc["bufferViews"][acc["bufferView"]]
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    size = COMPONENT_BYTES[acc["componentType"]] * COMPONENT_COUNT[acc["type"]]
    stride = view.get("byteStride", 0)
    if stride and stride != size:
        return b"".join(data[start + k * stride:start + k * stride + size] for k in range(acc["count"]))
    return bytes(data[start:start + size * acc["count"]])


def accessor_sig(doc, data, index, h):
    acc = {k: v for k, v in doc["accessors"][index].items() if k not in ("bufferView", "byteOffset")}
    h.update(json.dumps(acc, sort_keys=True).encode())
    h.update(accessor_bytes(doc, data, index))


def clip_sig(doc, data, clip):
    h = hashlib.sha256()
    for channel in clip["channels"]:
        sampler = clip["samplers"][channel["sampler"]]
        h.update(json.dumps(channel["target"], sort_keys=True).encode())
        h.update(sampler.get("interpolation", "LINEAR").encode())
        accessor_sig(doc, data, sampler["input"], h)
        accessor_sig(doc, data, sampler["output"], h)
    return h.hexdigest()


def mesh_sig(doc, data, mesh):
    h = hashlib.sha256()
    h.update(json.dumps({k: v for k, v in mesh.items() if k != "primitives"}, sort_keys=True).encode())
    for prim in mesh["primitives"]:
        for name in sorted(prim["attributes"]):
            h.update(name.encode())
            accessor_sig(doc, data, prim["attributes"][name], h)
        if "indices" in prim:
            accessor_sig(doc, data, prim["indices"], h)
        h.update(json.dumps({k: v for k, v in prim.items() if k not in ("attributes", "indices")}, sort_keys=True).encode())
    return h.hexdigest()


def transplant(base, base_bin, source, source_bin, names):
    out_bin = bytearray(base_bin)
    have = {c["name"] for c in base["animations"]}
    for name in names:
        if name in have:
            sys.exit(f"{name} は土台にもうあります")
        clip = next((c for c in source["animations"] if c["name"] == name), None)
        if clip is None:
            sys.exit(f"{name} は写す元にありません")
        remap = {}
        for old in sorted({s["input"] for s in clip["samplers"]} | {s["output"] for s in clip["samplers"]}):
            acc = dict(source["accessors"][old])
            view = dict(source["bufferViews"][acc["bufferView"]])
            begin = view.get("byteOffset", 0)
            chunk = source_bin[begin:begin + view["byteLength"]]
            out_bin += b"\0" * (-len(out_bin) % ALIGN)
            view["buffer"] = 0
            view["byteOffset"] = len(out_bin)
            out_bin += chunk
            base["bufferViews"].append(view)
            acc["bufferView"] = len(base["bufferViews"]) - 1
            base["accessors"].append(acc)
            remap[old] = len(base["accessors"]) - 1
        copied = json.loads(json.dumps(clip))
        for sampler in copied["samplers"]:
            sampler["input"] = remap[sampler["input"]]
            sampler["output"] = remap[sampler["output"]]
        base["animations"].append(copied)
        print(f"写した：{name}（アクセサ {len(remap)} 個 → {min(remap.values())}〜{max(remap.values())}）")
    base["buffers"][0]["byteLength"] = len(out_bin)
    return base, bytes(out_bin)


def verify(result, result_bin, base, base_bin, source, source_bin, names):
    ok = True

    def check(cond, text):
        nonlocal ok
        ok = ok and cond
        if not cond:
            print("違う：" + text)

    check(result_bin[:len(base_bin)] == base_bin, "土台の .bin の中身が動いた")
    for key in set(base) | set(result):
        if key not in ("accessors", "bufferViews", "animations", "buffers"):
            check(base.get(key) == result.get(key), f"最上位の {key} が土台と違う")
    for clip in result["animations"]:
        doc, data = (source, source_bin) if clip["name"] in names else (base, base_bin)
        original = next(c for c in doc["animations"] if c["name"] == clip["name"])
        check(clip_sig(result, result_bin, clip) == clip_sig(doc, data, original), f"クリップ {clip['name']}")
    for k, mesh in enumerate(result["meshes"]):
        check(mesh_sig(result, result_bin, mesh) == mesh_sig(base, base_bin, base["meshes"][k]), f"メッシュ {mesh['name']}")
    return ok


def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    base_rev, source_rev, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    base, base_bin = load(base_rev)
    source, source_bin = load(source_rev)
    if base["nodes"] != source["nodes"] or [s.get("joints") for s in base["skins"]] != [s.get("joints") for s in source["skins"]]:
        sys.exit("骨格（節点・スキン）が二つの版で違うので写せません")
    result, result_bin = transplant(json.loads(json.dumps(base)), base_bin, source, source_bin, names)
    with open(GLTF, "wb") as f:
        f.write(json.dumps(result, separators=(",", ":"), ensure_ascii=False).encode("utf-8"))
    with open(BIN, "wb") as f:
        f.write(result_bin)
    print(f"書いた：クリップ {len(result['animations'])} 本、.bin {len(result_bin)} バイト")
    if not verify(result, result_bin, base, base_bin, source, source_bin, names):
        sys.exit(1)
    print("確かめた：全部のクリップが元の版と、全部のメッシュ・材質・節点が土台とバイト単位で同じ")


if __name__ == "__main__":
    main()
