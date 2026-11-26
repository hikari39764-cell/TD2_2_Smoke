"""場面のコンポーネントの項目を、同じ物体の別のコンポーネントへ移す（c-eb3b08）。

部品を分けた時（例：ボスの部品から登場の部品を分けた）に、場面に保存してある項目を新しい部品へ移す。移す元の部品が
付いている物体ごとに、移す先の部品が無ければ作り（元の部品のすぐ後ろ。id はその物体の中の最大 + 1）、項目を移す。
移す先にもう同じ項目があれば止める。ほかの所は 1 文字も変えない（数の書き方も保存されたまま）。

使い方（Python は D:/RGBMoCap/runtime/python.exe）：
  python Tools/Verify/migrate_scene_components.py <場面.json>... --from <元の型> --to <先の型>
         (--keys a,b,c | --describe <Describe を書いた .cpp>) [--always] [--dry-run]

  --keys      移す項目の名前（カンマ区切り）
  --describe  その .cpp の fields.Xxx("名前", ...) の名前を全部移す（新しい部品の検査器の項目と同じ）
  --always    移す項目が無い物体にも、先の部品を作る（既定は、移す項目がある物体だけ）
  --dry-run   書かずに、何をするかだけ出す

例（ボスの登場の部品）：
  python Tools/Verify/migrate_scene_components.py Assets/Scenes/SmokeArena.scene.json \\
      --from Project_TD2_2.Boss --to Project_TD2_2.BossIntro --describe Source/Runtime/Boss/BossIntroSettings.cpp
"""
import argparse
import json
import re
import sys
from pathlib import Path


class Number(str):
    """読んだ数の書き方をそのまま持つ（書き戻す時に変えない）。"""


def load(text):
    return json.loads(text, parse_float=Number, parse_int=Number)


def dump(value, indent=0):
    """エンジンの場面の書き方（字下げ 2・": "・空の入れ物は [] {}）で書く。"""
    pad = "  " * (indent + 1)
    end = "  " * indent
    if isinstance(value, Number):
        return str(value)
    if value is True:
        return "true"
    if value is False:
        return "false"
    if value is None:
        return "null"
    if isinstance(value, str):
        return json.dumps(value, ensure_ascii=False)
    if isinstance(value, list):
        if not value:
            return "[]"
        return "[\n" + ",\n".join(pad + dump(v, indent + 1) for v in value) + "\n" + end + "]"
    if isinstance(value, dict):
        if not value:
            return "{}"
        items = (pad + json.dumps(k, ensure_ascii=False) + ": " + dump(v, indent + 1) for k, v in value.items())
        return "{\n" + ",\n".join(items) + "\n" + end + "}"
    raise TypeError(type(value))


def keys_from_describe(path):
    text = Path(path).read_text(encoding="utf-8")
    return re.findall(r'fields\.\w+\(\s*"(\w+)"', text)


def migrate(scene, source, target, keys, always):
    """場面（読んだ形）を書き換え、したことの行を返す。"""
    notes = []
    for obj in scene.get("objects") or []:
        comps = obj.get("components") or []
        src = next((c for c in comps if c.get("type") == source), None)
        if src is None:
            continue
        props = src.get("properties") or {}
        moving = [k for k in props if k in keys]
        if not moving and not always:
            continue
        dst = next((c for c in comps if c.get("type") == target), None)
        if dst is None:
            used = [int(c.get("id", 0)) for c in comps]
            dst = {"id": Number(str(max(used, default=0) + 1)), "properties": {}, "type": target}
            comps.insert(comps.index(src) + 1, dst)
            notes.append(f"  {obj.get('name')}: {target} を作った（id {dst['id']}）")
        dprops = dst.setdefault("properties", {})
        clash = [k for k in moving if k in dprops]
        if clash:
            raise SystemExit(f"{obj.get('name')}: 移す先にもう同じ項目がある：{clash}")
        for k in moving:
            dprops[k] = props.pop(k)
        notes.append(f"  {obj.get('name')}: {len(moving)} 項目を移した {moving}")
    return notes


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("scenes", nargs="+")
    parser.add_argument("--from", dest="source", required=True)
    parser.add_argument("--to", dest="target", required=True)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--keys")
    group.add_argument("--describe")
    parser.add_argument("--always", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    keys = set(args.keys.split(",")) if args.keys else set(keys_from_describe(args.describe))
    if not keys:
        raise SystemExit("移す項目がありません")
    for path in map(Path, args.scenes):
        raw = path.read_bytes().decode("utf-8")
        newline = "\r\n" if "\r\n" in raw else "\n"
        tail = newline if raw.endswith(newline) else ""
        scene = load(raw)
        # 書き方を変えないことを、読んだそのままを書き戻して確かめる（違えば、触らずに止める）。
        if dump(scene).replace("\n", newline) + tail != raw:
            raise SystemExit(f"{path}: 書き戻すと元の場面と同じになりません（書き方が違う）。触りません")
        notes = migrate(scene, args.source, args.target, keys, args.always)
        print(f"{path}:", "変えるところなし" if not notes else "")
        for line in notes:
            print(line)
        if notes and not args.dry_run:
            path.write_bytes((dump(scene).replace("\n", newline) + tail).encode("utf-8"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
