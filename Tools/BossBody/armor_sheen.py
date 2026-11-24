"""騎士の鎧（兜・胸当て・肩当て）の材質に、少し照りを足す。貼図（ORM）の粗さに ROUGHNESS_SCALE を掛ける（glTF の
roughnessFactor）。貼図はそのまま。戦いの模型と登場の役者・落ちている兜の、同じ名前の材質を全部そろえる。

模型を作り直した後（make_kasumi.py → merge_*.py、登場の役者の build_assets.py）に、もう一度流す。何度流しても同じになる。
使い方：python -X utf8 Tools/BossBody/armor_sheen.py
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
MODELS = ("KasumiBushi.gltf", "BossIntroActor.gltf", "BossIntroHelm.gltf")
# 照りを足す材質（灼けた版の *Hot も同じ値にする）。
ARMOR = ("KnightHelm", "KnightBody", "KnightPauldron")
# 粗さに掛ける割合（1 で貼図のまま。小さいほど、映り込みと照りが強く・鋭くなる）。
ROUGHNESS_SCALE = 0.72
# 金属らしさに掛ける割合（1 で貼図のまま）。
METALLIC_SCALE = 1.0


def main():
    for name in MODELS:
        path = ROOT / "Assets" / "Models" / name
        raw = path.read_bytes()
        text = raw.decode("utf-8")
        doc = json.loads(text)
        changed = []
        for material in doc.get("materials", []):
            base = material.get("name", "")
            if base.endswith("Hot"):
                base = base[:-3]
            if base not in ARMOR:
                continue
            pbr = material.setdefault("pbrMetallicRoughness", {})
            pbr["roughnessFactor"] = ROUGHNESS_SCALE
            pbr["metallicFactor"] = METALLIC_SCALE
            changed.append(material["name"])
        if not changed:
            continue
        # 書き方は元のファイルに合わせる（1 行に詰めた物と、タブで字下げした物がある）。
        if "\n\t" in text:
            out = json.dumps(doc, ensure_ascii=False, indent="\t", separators=(",", ":")) + "\n"
        else:
            out = json.dumps(doc, ensure_ascii=False, separators=(",", ":"))
        if "\r\n" in text:
            out = out.replace("\n", "\r\n")
        path.write_bytes(out.encode("utf-8"))
        print(name, changed)


main()
