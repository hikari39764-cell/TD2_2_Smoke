"""c-857ae3 2 段目：灼けた鎧のノード（DoHot・KabutoHot・SodeHot・KusazuriPanelHot）を、いまの模型
Assets/Models/KasumiBushi.gltf/.bin の後ろへ足す（既にあるノード・バイトは 1 つも変えない）。
Blender で Tools/BossBody/make_kasumi.py を流し直すと、手の法線などが最後の桁で揺れて .bin の既存の部分が変わるので、
新しく書き出した模型から足すノードだけを写す。
使い方：python -X utf8 merge_hot_nodes.py <make_kasumi.py の書き出し先フォルダ>"""
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path("D:/PJ/TD2_2")
BASE = "c9cb71a"
NAMES = ("DoHot", "KabutoHot", "SodeHot", "KusazuriPanelHot")


def git_show(path):
    return subprocess.run(["git", "show", f"{BASE}:{path}"], cwd=ROOT, capture_output=True, check=True).stdout


base = json.loads(git_show("Assets/Models/KasumiBushi.gltf").decode("utf-8"))
# .bin は Git LFS に入っている（git show では指し示しの文だけが出る）。中身は git lfs smudge で取り出す。
base_bin = bytearray(subprocess.run(["git", "lfs", "smudge", "Assets/Models/KasumiBushi.bin"], cwd=ROOT,
                                    input=git_show("Assets/Models/KasumiBushi.bin"), capture_output=True,
                                    check=True).stdout)
assert len(base_bin) > 1000, "LFS の中身が取り出せない"
src_dir = pathlib.Path(sys.argv[1])
src = json.loads((src_dir / "KasumiBushi.gltf").read_text(encoding="utf-8"))
src_bin = (src_dir / "KasumiBushi.bin").read_bytes()
assert not any(n["name"] in NAMES for n in base["nodes"])


def copy_accessor(index):
    acc = dict(src["accessors"][index])
    view = dict(src["bufferViews"][acc["bufferView"]])
    data = src_bin[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]]
    while len(base_bin) % 4:
        base_bin.append(0)
    view["byteOffset"] = len(base_bin)
    view["buffer"] = 0
    base_bin.extend(data)
    base["bufferViews"].append(view)
    acc["bufferView"] = len(base["bufferViews"]) - 1
    base["accessors"].append(acc)
    return len(base["accessors"]) - 1


def material_index(index):
    material = src["materials"][index]
    for k, m in enumerate(base["materials"]):
        if m["name"] == material["name"]:
            return k
    base["materials"].append(material)
    return len(base["materials"]) - 1


for node in src["nodes"]:
    if node["name"] not in NAMES:
        continue
    mesh = src["meshes"][node["mesh"]]
    primitives = []
    for prim in mesh["primitives"]:
        p = dict(prim)
        p["attributes"] = {k: copy_accessor(v) for k, v in prim["attributes"].items()}
        if "indices" in prim:
            p["indices"] = copy_accessor(prim["indices"])
        if "material" in prim:
            p["material"] = material_index(prim["material"])
        primitives.append(p)
    base["meshes"].append({"name": mesh["name"], "primitives": primitives})
    new_node = {k: v for k, v in node.items() if k != "mesh"}
    new_node["mesh"] = len(base["meshes"]) - 1
    base["nodes"].append(new_node)
    base["scenes"][0]["nodes"].append(len(base["nodes"]) - 1)
base["buffers"][0]["byteLength"] = len(base_bin)
(ROOT / "Assets/Models/KasumiBushi.bin").write_bytes(bytes(base_bin))
# Blender の書き出しと同じ書き方（タブの字下げ・空白なしの区切り・最後に改行）。
text = json.dumps(base, ensure_ascii=False, indent="\t", separators=(",", ":")) + "\n"
(ROOT / "Assets/Models/KasumiBushi.gltf").write_text(text, encoding="utf-8")
print("nodes", [n["name"] for n in base["nodes"]], "bin", len(base_bin))
