"""Source/Runtime のフォルダ（モジュール）どうしの #include のつながりを数え、輪（循環）を探す。

  python -I -X utf8 Tools/Verify/include_graph.py

Boss と BossBody は 1 つのモジュール（Boss）として数える。輪があれば終了コード 1。
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2] / "Source" / "Runtime"
MERGE = {"BossBody": "Boss"}


def module_of(path):
    rel = path.relative_to(ROOT)
    if len(rel.parts) < 2:
        return None
    return MERGE.get(rel.parts[0], rel.parts[0])


def main():
    edges = {}
    for path in ROOT.rglob("*"):
        if path.suffix not in (".h", ".cpp", ".inl"):
            continue
        source = module_of(path)
        if source is None:
            continue
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r'\s*#include\s+"([^"]+)"', line)
            if not m:
                continue
            target_path = (path.parent / m.group(1)).resolve()
            try:
                target = module_of(target_path)
            except ValueError:
                continue
            if target is None or target == source:
                continue
            edges.setdefault(source, {}).setdefault(target, []).append(f"{path.relative_to(ROOT)} -> {m.group(1)}")
    modules = sorted(set(edges) | {t for v in edges.values() for t in v})
    print("include の数（行 → 列）")
    print("          " + " ".join(f"{m[:7]:>7}" for m in modules))
    for a in modules:
        print(f"{a[:9]:>9} " + " ".join(f"{len(edges.get(a, {}).get(b, [])) or '':>7}" for b in modules))
    # 輪：強連結成分（2 つ以上のモジュール）
    index, low, stack, on, comps, counter = {}, {}, [], set(), [], [0]

    def visit(v):
        index[v] = low[v] = counter[0]
        counter[0] += 1
        stack.append(v)
        on.add(v)
        for w in edges.get(v, {}):
            if w not in index:
                visit(w)
                low[v] = min(low[v], low[w])
            elif w in on:
                low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            comp = []
            while True:
                w = stack.pop()
                on.discard(w)
                comp.append(w)
                if w == v:
                    break
            if len(comp) > 1:
                comps.append(sorted(comp))

    for v in modules:
        if v not in index:
            visit(v)
    if not comps:
        print("輪：なし")
        return 0
    for comp in comps:
        print("輪：", " ↔ ".join(comp))
        for a in comp:
            for b in comp:
                for site in edges.get(a, {}).get(b, [])[:5]:
                    print("   ", site)
    return 1


if __name__ == "__main__":
    sys.exit(main())
