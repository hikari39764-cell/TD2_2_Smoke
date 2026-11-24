"""いまの模型 Assets/Models/KasumiBushi.gltf/.bin の剣（KatanaHilt・KatanaBlade・KatanaBladeHot）だけを、make_kasumi.py の出力の
剣（bb_greatsword.py の西洋の大剣）に差し替える。ほかのノードの中身（頂点・法線・添字のバイト）は 1 つも変えない
（knight_helm/merge_knight_helm.py と同じ考え）。

使い方：
  blender --background --factory-startup --python Tools/BossBody/make_kasumi.py -- <出力フォルダ>
  python -X utf8 Tools/BossBody/merge_sword.py <出力フォルダ>
そのあと、登場の役者（BossIntroActor）は D:/GAME PJ/TD2_2/BossAnim/intro の make_actor.py → build_assets.py で作り直す
（役者は KasumiBushi.gltf の剣を読み込む）。"""
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODEL = ROOT / "Assets" / "Models" / "KasumiBushi.gltf"
REMOVE = ()
# 足す（入っていれば差し替える）ノード。
ADD = ("KatanaHilt", "KatanaBlade", "KatanaBladeHot")


class Packer:
    """使うバイトだけを新しい .bin へ詰める。"""

    def __init__(self):
        self.data = bytearray()
        self.views = []
        self.accessors = []
        self.view_map = {}

    def accessor(self, gltf, blob, index, tag):
        acc = dict(gltf["accessors"][index])
        key = (tag, acc["bufferView"])
        if key not in self.view_map:
            view = dict(gltf["bufferViews"][acc["bufferView"]])
            start = view.get("byteOffset", 0)
            while len(self.data) % 4:
                self.data.append(0)
            view["byteOffset"] = len(self.data)
            view["buffer"] = 0
            self.data.extend(blob[start:start + view["byteLength"]])
            self.views.append(view)
            self.view_map[key] = len(self.views) - 1
        acc["bufferView"] = self.view_map[key]
        self.accessors.append(acc)
        return len(self.accessors) - 1


def main():
    src_dir = pathlib.Path(sys.argv[1])
    base = json.loads(MODEL.read_text(encoding="utf-8"))
    base_bin = MODEL.with_suffix(".bin").read_bytes()
    src = json.loads((src_dir / "KasumiBushi.gltf").read_text(encoding="utf-8"))
    src_bin = (src_dir / "KasumiBushi.bin").read_bytes()
    assert len(base_bin) > 1000, "Assets/Models/KasumiBushi.bin の中身が無い（Git LFS を取り出す）"
    assert all(any(n["name"] == name for n in src["nodes"]) for name in ADD), "make_kasumi.py の出力に剣の部品が無い"

    packer = Packer()
    out = {k: base[k] for k in ("asset",)}
    out_materials, out_textures, out_images, out_samplers = [], [], [], []
    material_by_name = {}

    def texture_index(gltf, index):
        tex = dict(gltf["textures"][index])
        image = dict(gltf["images"][tex["source"]])
        image["uri"] = "KasumiBushi/" + pathlib.PurePosixPath(image["uri"]).name
        if image not in out_images:
            out_images.append(image)
        tex["source"] = out_images.index(image)
        if "sampler" in tex:
            sampler = gltf["samplers"][tex["sampler"]]
            if sampler not in out_samplers:
                out_samplers.append(sampler)
            tex["sampler"] = out_samplers.index(sampler)
        if tex not in out_textures:
            out_textures.append(tex)
        return out_textures.index(tex)

    def remap_textures(gltf, value):
        if isinstance(value, dict):
            return {k: (texture_index(gltf, v) if k == "index" and isinstance(v, int) else remap_textures(gltf, v))
                    for k, v in value.items()}
        if isinstance(value, list):
            return [remap_textures(gltf, v) for v in value]
        return value

    def material_index(gltf, index):
        material = gltf["materials"][index]
        if material["name"] not in material_by_name:
            out_materials.append(remap_textures(gltf, material))
            material_by_name[material["name"]] = len(out_materials) - 1
        return material_by_name[material["name"]]

    def copy_node(gltf, blob, node, tag):
        if "mesh" not in node:  # 印のノード（HelmVisor など。add_helm_glow.py が足す）はそのまま写す
            nodes.append(dict(node))
            return
        mesh = gltf["meshes"][node["mesh"]]
        primitives = []
        for prim in mesh["primitives"]:
            p = dict(prim)
            p["attributes"] = {k: packer.accessor(gltf, blob, v, tag) for k, v in prim["attributes"].items()}
            if "indices" in prim:
                p["indices"] = packer.accessor(gltf, blob, prim["indices"], tag)
            if "material" in prim:
                p["material"] = material_index(gltf, prim["material"])
            primitives.append(p)
        meshes.append({"name": mesh["name"], "primitives": primitives})
        nodes.append({**{k: v for k, v in node.items() if k != "mesh"}, "mesh": len(meshes) - 1})

    nodes, meshes = [], []
    for node in base["nodes"]:
        if node["name"] not in REMOVE and node["name"] not in ADD:
            copy_node(base, base_bin, node, "base")
    for name in ADD:
        copy_node(src, src_bin, next(n for n in src["nodes"] if n["name"] == name), "src")

    used = set(base.get("extensionsUsed", [])) | set(src.get("extensionsUsed", []))
    out["extensionsUsed"] = sorted(e for e in used if e in json.dumps(out_materials))
    out["scene"] = 0
    out["scenes"] = [{"name": base["scenes"][0]["name"], "nodes": list(range(len(nodes)))}]
    out["nodes"] = nodes
    out["materials"] = out_materials
    out["meshes"] = meshes
    if out_textures:
        out["textures"] = out_textures
        out["images"] = out_images
    if out_samplers:
        out["samplers"] = out_samplers
    out["accessors"] = packer.accessors
    out["bufferViews"] = packer.views
    out["buffers"] = [{"byteLength": len(packer.data), "uri": "KasumiBushi.bin"}]
    MODEL.with_suffix(".bin").write_bytes(bytes(packer.data))
    # Blender の書き出しと同じ書き方（タブの字下げ・空白なしの区切り・最後に改行）。
    text = json.dumps(out, ensure_ascii=False, indent="\t", separators=(",", ":")) + "\n"
    MODEL.write_text(text, encoding="utf-8")
    print("nodes", [n["name"] for n in nodes])
    print("images", [i["uri"] for i in out_images])
    print("bin", len(base_bin), "->", len(packer.data))


main()
