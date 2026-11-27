"""兜の面頬の 5 本の隙間の中に、自発光の面と、エフェクトを結ぶ場所の印を、模型へ足す（c-e0c500）。
使い方：python -X utf8 Tools/BossBody/add_helm_glow.py     （何度流しても同じ結果になる）
光の面がもうある模型では、面の網目だけを同じ場所へ書き直す：ほかの道具が後から足したもの（役者の動画の片段など）は動かさない。

兜（KnightHelm）は閉じた殻で、面頬の隙間は底のある溝になっている（中は外から見えない）。光は模型の側で持つ：
- 溝の底の少し前に、溝 1 本につき 1 枚の平らな四角（HelmGlow）を張る。四角は溝の口より外（兜の殻の中）まで広げて
  あるので、隙間からだけ見え、見える縁は兜の溝の縁そのものになる（格子の升を並べた網目だと、斜めの溝の縁が階段になり、
  寄りの画でモザイクに見えた）。溝の場所と深さは、兜の網目を正面から調べて決める（数字を手で置かない）。
- エフェクトや灯を結ぶ場所の印（HelmVisor：隙間の帯の中心・面の表、HelmCore：兜の原点）をノードとして足す。
足す先は 2 つ：
- Assets/Models/KasumiBushi.gltf（戦いの体）：ノード HelmGlow・HelmGlowHot（フェーズ 2 の色）・HelmVisor・HelmCore。
- Assets/Models/BossIntroActor.gltf（登場の役者）：頭の骨に付いた HelmGlowMesh、頭の骨の子の HelmVisor・HelmCore。
  役者の兜が頭の骨のどこにあるかは、役者の兜の網目を戦いの兜に重ねて確かめる（合わなければ止まる）。
コードが使う数字は Source/Runtime/BossBody/BossHelmGlowData.h に書き出す（手で直さない）。
役者の模型を作り直した後（BossAnim/intro/build_assets.py）と、戦いの体を作り直した後（make_kasumi.py）に流し直す。
"""
import json
import math
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODELS = ROOT / "Assets/Models"
HEADER = ROOT / "Source/Runtime/BossBody/BossHelmGlowData.h"
# 調べる範囲（兜の模型の寸法。+Y 上・+Z 前）と刻み。溝＝周りの面より GROOVE_DEPTH 以上へこんだ所。
X_RANGE, Y_RANGE = (-0.34, 0.34), (-0.40, -0.04)
STEP_X, STEP_Y = 0.006, 0.008
GROOVE_DEPTH = 0.014
ENVELOPE = 0.036          # 周りの面を取る幅（溝より広く）
LIFT = 0.35               # 溝の底から、溝の深さのこの割合だけ前へ出した所に面を張る
MARGIN = 0.008            # 四角を、溝の口より外へ広げる幅（兜の殻の中へ入る分）
MIN_CELLS = 12            # 溝 1 本と数える格子の点の数の下限（小さなへこみを拾わない）
# 光の色と強さ（材質の物理量。昼の空の下で、隙間がはっきり光る強さ）。登場の間はコードが明るさを上書きする。
GLOW_COLOR, GLOW_STRENGTH = (0.42, 0.66, 1.0), 14000.0
HOT_COLOR, HOT_STRENGTH = (1.0, 0.42, 0.12), 14000.0
# 役者の兜の、頭の骨からの置き場所（BossIntroActor.cpp の kHelmOffset と同じ値）。向きの取り方の違いは重ねて確かめる。
HELM_OFFSET, HELM_PITCH = (0.0, 0.45625, 0.05063), 2.0 * math.asin(0.104528)
COMPONENT = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


class Model:
    def __init__(self, name):
        self.path = MODELS / (name + ".gltf")
        text = self.path.read_text(encoding="utf-8")
        # 元の書き方（タブで字下げした形か、1 行に詰めた形か）のまま書き戻す。
        self.pretty = "\n\t" in text
        self.doc = json.loads(text)
        self.bin_path = MODELS / self.doc["buffers"][0]["uri"]
        self.blob = bytearray(self.bin_path.read_bytes())

    def replace(self, index, rows, code, kind, bounds=False):
        """アクセサ index の中身を、同じ場所へ書き直す（前より小さい時だけ）。配列の番号も、後ろに足されたほかのデータ
        （動画の片段など）も動かさない。余った分は、使わない隙間としてバッファに残る。"""
        accessor = self.doc["accessors"][index]
        view = self.doc["bufferViews"][accessor["bufferView"]]
        data = bytearray()
        for row in rows:
            data.extend(struct.pack("<%d%s" % (WIDTH[kind], code), *row) if WIDTH[kind] > 1 else struct.pack("<" + code, row))
        if accessor.get("byteOffset", 0) != 0 or COMPONENT[accessor["componentType"]] != code or len(data) > view["byteLength"]:
            raise SystemExit("%s: 面の網目を同じ場所へ書き直せません（アクセサ %d）" % (self.path.name, index))
        start = view.get("byteOffset", 0)
        self.blob[start:start + len(data)] = data
        view["byteLength"] = len(data)
        accessor["count"] = len(rows)
        if bounds:
            accessor["min"] = [min(r[i] for r in rows) for i in range(3)]
            accessor["max"] = [max(r[i] for r in rows) for i in range(3)]

    def glow_primitives(self):
        """前に足した光の面（網目の名前 HelmGlow・HelmGlowHot）の primitive（アクセサを分け合うものは 1 つだけ）。"""
        seen, out = set(), []
        for mesh in self.doc["meshes"]:
            if mesh.get("name") in ("HelmGlow", "HelmGlowHot"):
                primitive = mesh["primitives"][0]
                if primitive["attributes"]["POSITION"] not in seen:
                    seen.add(primitive["attributes"]["POSITION"])
                    out.append(primitive)
        return out

    def unmark(self):
        """前の版の道具が付けた印を外す（前の版は、印から後ろを全部切り落として足し直した）。"""
        self.doc.get("extras", {}).pop("helmGlow", None)
        if "extras" in self.doc and not self.doc["extras"]:
            del self.doc["extras"]

    def read(self, index):
        a = self.doc["accessors"][index]
        v = self.doc["bufferViews"][a["bufferView"]]
        code, n = COMPONENT[a["componentType"]], WIDTH[a["type"]]
        size = struct.calcsize(code) * n
        stride = v.get("byteStride", size)
        start = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return [struct.unpack_from("<%d%s" % (n, code), self.blob, start + i * stride) for i in range(a["count"])]

    def add(self, rows, code, kind, target=None, bounds=False):
        while len(self.blob) % 4:
            self.blob.append(0)
        start = len(self.blob)
        for row in rows:
            self.blob.extend(struct.pack("<%d%s" % (WIDTH[kind], code), *row) if WIDTH[kind] > 1 else struct.pack("<" + code, row))
        view = {"buffer": 0, "byteOffset": start, "byteLength": len(self.blob) - start}
        if target:
            view["target"] = target
        self.doc["bufferViews"].append(view)
        component = next(k for k, c in COMPONENT.items() if c == code)
        accessor = {"bufferView": len(self.doc["bufferViews"]) - 1, "componentType": component, "count": len(rows), "type": kind}
        if bounds:
            accessor["min"] = [min(r[i] for r in rows) for i in range(3)]
            accessor["max"] = [max(r[i] for r in rows) for i in range(3)]
        self.doc["accessors"].append(accessor)
        return len(self.doc["accessors"]) - 1

    def node(self, name):
        return next(i for i, n in enumerate(self.doc["nodes"]) if n.get("name") == name)

    def triangles(self, node_name):
        points, faces = [], []
        for primitive in self.doc["meshes"][self.doc["nodes"][self.node(node_name)]["mesh"]]["primitives"]:
            base = len(points)
            points += self.read(primitive["attributes"]["POSITION"])
            flat = [row[0] for row in self.read(primitive["indices"])]
            faces += [(flat[i] + base, flat[i + 1] + base, flat[i + 2] + base) for i in range(0, len(flat), 3)]
        return points, faces

    def save(self):
        self.doc["buffers"][0]["byteLength"] = len(self.blob)
        used = set(self.doc.get("extensionsUsed", [])) | {"KHR_materials_emissive_strength"}
        self.doc["extensionsUsed"] = sorted(used)
        self.bin_path.write_bytes(bytes(self.blob))
        text = json.dumps(self.doc, ensure_ascii=False, separators=(",", ":"), indent="\t" if self.pretty else None)
        self.path.write_text(text + ("\n" if self.pretty else ""), encoding="utf-8", newline="\n")


def material(name, color, strength):
    return {"name": name, "doubleSided": True, "emissiveFactor": list(color),
            "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": strength}},
            "pbrMetallicRoughness": {"baseColorFactor": [0.0, 0.0, 0.0, 1.0], "metallicFactor": 0.0, "roughnessFactor": 1.0}}


def depth_map(points, faces):
    """正面（+Z）から見た、格子の点ごとの一番前の面の z（面が無ければ None）。"""
    nx = int(round((X_RANGE[1] - X_RANGE[0]) / STEP_X)) + 1
    ny = int(round((Y_RANGE[1] - Y_RANGE[0]) / STEP_Y)) + 1
    grid = [[None] * nx for _ in range(ny)]
    for a, b, c in faces:
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = points[a], points[b], points[c]
        det = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(det) < 1e-12:
            continue
        i0 = max(0, int(math.ceil((min(ax, bx, cx) - X_RANGE[0]) / STEP_X)))
        i1 = min(nx - 1, int(math.floor((max(ax, bx, cx) - X_RANGE[0]) / STEP_X)))
        j0 = max(0, int(math.ceil((min(ay, by, cy) - Y_RANGE[0]) / STEP_Y)))
        j1 = min(ny - 1, int(math.floor((max(ay, by, cy) - Y_RANGE[0]) / STEP_Y)))
        for j in range(j0, j1 + 1):
            y = Y_RANGE[0] + j * STEP_Y
            for i in range(i0, i1 + 1):
                x = X_RANGE[0] + i * STEP_X
                u = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / det
                v = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / det
                w = 1.0 - u - v
                if u < -1e-6 or v < -1e-6 or w < -1e-6:
                    continue
                z = u * az + v * bz + w * cz
                if grid[j][i] is None or z > grid[j][i]:
                    grid[j][i] = z
    return grid


def solve3(m, v):
    """3x3 の連立方程式（掃き出し）。"""
    a = [list(m[r]) + [v[r]] for r in range(3)]
    for c in range(3):
        pivot = max(range(c, 3), key=lambda r: abs(a[r][c]))
        a[c], a[pivot] = a[pivot], a[c]
        if abs(a[c][c]) < 1e-12:
            return None
        a[c] = [x / a[c][c] for x in a[c]]
        for r in range(3):
            if r != c:
                a[r] = [x - a[r][c] * y for x, y in zip(a[r], a[c])]
    return [a[r][3] for r in range(3)]


def fit_line(ys, xs):
    """x = p*y + q（最小二乗。点が 1 行だけなら傾き 0）。"""
    n = len(ys)
    my, mx = sum(ys) / n, sum(xs) / n
    den = sum((y - my) ** 2 for y in ys)
    slope = sum((y - my) * (x - mx) for y, x in zip(ys, xs)) / den if den > 1e-12 else 0.0
    return slope, mx - slope * my


def glow_mesh(points, faces):
    """溝ごとに 1 枚の平らな四角：溝の底から少し前へ出した高さの平面を当てはめ、溝の左右の縁に沿う平行四辺形を、溝の口
    より外（兜の殻の中）まで広げて張る。"""
    grid = depth_map(points, faces)
    ny, nx = len(grid), len(grid[0])
    reach = max(1, int(round(ENVELOPE / STEP_X)))
    lifted = [[None] * nx for _ in range(ny)]
    for j in range(ny):
        row = grid[j]
        for i in range(nx):
            if row[i] is None:
                continue
            # 周りの面：左右それぞれの一番前の高さのうち、低い方（溝の両側の縁の高さ）。
            left = [row[k] for k in range(max(0, i - reach), i) if row[k] is not None]
            right = [row[k] for k in range(i + 1, min(nx, i + reach + 1)) if row[k] is not None]
            if not left or not right:
                continue
            surface = min(max(left), max(right))
            if surface - row[i] >= GROOVE_DEPTH:
                lifted[j][i] = row[i] + (surface - row[i]) * LIFT
    # 溝ごとに分ける（上下左右につながった格子の点）。
    seen = [[False] * nx for _ in range(ny)]
    regions = []
    for j in range(ny):
        for i in range(nx):
            if lifted[j][i] is None or seen[j][i]:
                continue
            stack, cells = [(j, i)], []
            seen[j][i] = True
            while stack:
                cj, ci = stack.pop()
                cells.append((cj, ci))
                for dj, di in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nj, ni = cj + dj, ci + di
                    if 0 <= nj < ny and 0 <= ni < nx and lifted[nj][ni] is not None and not seen[nj][ni]:
                        seen[nj][ni] = True
                        stack.append((nj, ni))
            if len(cells) >= MIN_CELLS:
                regions.append(cells)
    vertices, triangles, all_x, all_y = [], [], [], []
    for cells in regions:
        xs = [X_RANGE[0] + i * STEP_X for _, i in cells]
        ys = [Y_RANGE[0] + j * STEP_Y for j, _ in cells]
        zs = [lifted[j][i] for j, i in cells]
        all_x += xs
        all_y += ys
        # 平面 z = a*x + b*y + c（最小二乗）。
        n = float(len(cells))
        sx, sy, sz = sum(xs), sum(ys), sum(zs)
        plane = solve3([[sum(x * x for x in xs), sum(x * y for x, y in zip(xs, ys)), sx],
                        [sum(x * y for x, y in zip(xs, ys)), sum(y * y for y in ys), sy],
                        [sx, sy, n]],
                       [sum(x * z for x, z in zip(xs, zs)), sum(y * z for y, z in zip(ys, zs)), sz]) or [0.0, 0.0, sz / n]
        # 溝の左右の縁：行ごとの左端・右端に、直線を当てはめる（溝は斜めの平行四辺形）。
        rows = sorted({j for j, _ in cells})
        row_y = [Y_RANGE[0] + j * STEP_Y for j in rows]
        left_line = fit_line(row_y, [X_RANGE[0] + min(i for cj, i in cells if cj == j) * STEP_X for j in rows])
        right_line = fit_line(row_y, [X_RANGE[0] + max(i for cj, i in cells if cj == j) * STEP_X for j in rows])
        y0, y1 = min(row_y) - MARGIN, max(row_y) + MARGIN
        corners = [(left_line[0] * y0 + left_line[1] - MARGIN, y0), (right_line[0] * y0 + right_line[1] + MARGIN, y0),
                   (right_line[0] * y1 + right_line[1] + MARGIN, y1), (left_line[0] * y1 + left_line[1] - MARGIN, y1)]
        base = len(vertices)
        vertices += [(x, y, plane[0] * x + plane[1] * y + plane[2]) for x, y in corners]
        triangles += [(base, base + 1, base + 2), (base, base + 2, base + 3)]
        print("GROOVE cells %3d  x %.3f..%.3f  y %.3f..%.3f  z %.3f..%.3f" % (len(cells), min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))
    # 広げた四角が、溝の口の外で、兜の表より前へ出ていないか（出ていれば、そこが光って見える）。
    exposed = 0
    for a, b, c in triangles:
        (ax, ay, az), (bx, by, bz), (cx, cy, cz) = vertices[a], vertices[b], vertices[c]
        det = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        for j in range(ny):
            y = Y_RANGE[0] + j * STEP_Y
            for i in range(nx):
                x = X_RANGE[0] + i * STEP_X
                u = ((by - cy) * (x - cx) + (cx - bx) * (y - cy)) / det
                v = ((cy - ay) * (x - cx) + (ax - cx) * (y - cy)) / det
                if u < 0.0 or v < 0.0 or u + v > 1.0 or lifted[j][i] is not None:
                    continue
                # 溝の中（壁や、幅の広い溝の端の列）：周り（左右へ溝の幅 2 つ分）の一番前の面より 4 mm 以上へこんだ所。
                # そこで面が見えるのは正しい。
                if grid[j][i] is not None:
                    near = [grid[j][k] for k in range(max(0, i - 2 * reach), min(nx, i + 2 * reach + 1)) if grid[j][k] is not None]
                    if max(near) - grid[j][i] >= 0.004:
                        continue
                z = u * az + v * bz + (1.0 - u - v) * cz
                if grid[j][i] is None or grid[j][i] < z - 0.002:
                    exposed += 1
    print("GLOW grooves %d, grid points where a quad shows outside a groove: %d" % (len(regions), exposed))
    if exposed:
        raise SystemExit("光の面が、溝の外で兜の表より前に出ています（MARGIN を小さくしてください）")
    centre_x, centre_y = 0.5 * (min(all_x) + max(all_x)), 0.5 * (min(all_y) + max(all_y))
    column = int(round((centre_x - X_RANGE[0]) / STEP_X))
    row = int(round((centre_y - Y_RANGE[0]) / STEP_Y))
    front = max(z for z in grid[row][max(0, column - reach):column + reach + 1] if z is not None)
    info = {"centre": (centre_x, centre_y, front),
            "half": (0.5 * (max(all_x) - min(all_x)), 0.5 * (max(all_y) - min(all_y)))}
    return vertices, triangles, info


# ---- 行列（列ベクトル。4x4 は行の並びのリスト）
def mul(a, b):
    return [[sum(a[r][k] * b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]


def apply(m, p, w=1.0):
    return tuple(m[r][0] * p[0] + m[r][1] * p[1] + m[r][2] * p[2] + m[r][3] * w for r in range(3))


def inverse(m):
    n = [list(m[r]) + [1.0 if r == c else 0.0 for c in range(4)] for r in range(4)]
    for c in range(4):
        pivot = max(range(c, 4), key=lambda r: abs(n[r][c]))
        n[c], n[pivot] = n[pivot], n[c]
        scale = n[c][c]
        n[c] = [v / scale for v in n[c]]
        for r in range(4):
            if r != c and n[r][c] != 0.0:
                factor = n[r][c]
                n[r] = [v - factor * w for v, w in zip(n[r], n[c])]
    return [row[4:] for row in n]


def helm_frame(head, offset_z, pitch, yaw, scale):
    """役者の兜の置き場所（兜の模型の寸法 → 役者の綁定の姿の空間）。"""
    c, s = math.cos(pitch), math.sin(pitch)
    cy, sy = math.cos(yaw), math.sin(yaw)
    place = [[1, 0, 0, HELM_OFFSET[0]], [0, c, -s, HELM_OFFSET[1]], [0, s, c, offset_z], [0, 0, 0, 1]]
    turn = [[cy, 0, sy, 0], [0, 1, 0, 0], [-sy, 0, cy, 0], [0, 0, 0, 1]]
    size = [[scale, 0, 0, 0], [0, scale, 0, 0], [0, 0, scale, 0], [0, 0, 0, 1]]
    return mul(mul(mul(head, place), turn), size)


def fit_actor(actor, fight_points, helm_node="KnightHelm"):
    """役者の兜が、頭の骨のどこに・どの向きで・どの大きさであるか。戦いの兜の点を写して、役者の兜の点に重なる置き方を選ぶ。
    落ちている兜の模型（BossIntroHelm）も同じ作り（頭の骨に付いた網目）なので、網目のノードの名前を変えて同じに調べる。"""
    skin = actor.doc["skins"][0]
    joint = [actor.doc["nodes"][j].get("name") for j in skin["joints"]].index("Head")
    flat = actor.read(skin["inverseBindMatrices"])[joint]
    head = inverse([[flat[c * 4 + r] for c in range(4)] for r in range(4)])
    actor_points, _ = actor.triangles(helm_node)
    width = max(p[0] for p in fight_points) - min(p[0] for p in fight_points)
    sample = fight_points[::max(1, len(fight_points) // 160)]
    best = None
    for axis in range(3):
        extent = max(p[axis] for p in actor_points) - min(p[axis] for p in actor_points)
        scale = extent / width
        for offset_z in (HELM_OFFSET[2], -HELM_OFFSET[2]):
            for pitch in (HELM_PITCH, -HELM_PITCH):
                for yaw in (0.0, math.pi):
                    frame = helm_frame(head, offset_z, pitch, yaw, scale)
                    error = 0.0
                    for p in sample:
                        q = apply(frame, p)
                        error = max(error, min((q[0] - a[0]) ** 2 + (q[1] - a[1]) ** 2 + (q[2] - a[2]) ** 2 for a in actor_points))
                    if best is None or error < best[0]:
                        best = (error, frame, scale, joint, head)
    error = math.sqrt(best[0])
    print("ACTOR helm fit: worst distance %.4f m, scale %.4f" % (error, best[2]))
    if error > 0.03:
        raise SystemExit("役者の兜が、戦いの兜の置き方に重なりません（%.3f m）。kHelmOffset と役者の模型を確かめてください" % error)
    return best[1], best[2], best[3], best[4]


def main():
    fight = Model("KasumiBushi")
    points, faces = fight.triangles("KnightHelm")
    vertices, triangles, info = glow_mesh(points, faces)
    centre, half = info["centre"], info["half"]
    print("GLOW vertices %d triangles %d centre (%.3f, %.3f, %.3f) half (%.3f, %.3f)" % (len(vertices), len(triangles), *centre, *half))
    if len(triangles) < 6:
        raise SystemExit("溝が見つかりません（兜の網目か、調べる範囲を確かめてください）")
    flat = [i for t in triangles for i in t]

    # ---- 戦いの体
    fight_done = False
    for primitive in fight.glow_primitives():
        fight.replace(primitive["attributes"]["POSITION"], vertices, "f", "VEC3", True)
        fight.replace(primitive["attributes"]["NORMAL"], [(0.0, 0.0, 1.0)] * len(vertices), "f", "VEC3")
        fight.replace(primitive["indices"], flat, "H", "SCALAR")
        fight_done = True
    fight.unmark()
    if not fight_done:
        fight_add(fight, vertices, flat, centre)
    fight.save()

    # ---- 登場の役者
    actor = Model("BossIntroActor")
    frame, scale, joint, head = fit_actor(actor, points)
    rows = [apply(frame, v) for v in vertices]
    facing = apply(frame, (0.0, 0.0, 1.0), 0.0)
    length = math.sqrt(sum(c * c for c in facing))
    facing = tuple(c / length for c in facing)
    actor_slot = skinned(actor, rows, facing, joint, flat, "KnightHelm", GLOW_COLOR, GLOW_STRENGTH)
    if actor.fresh:
        head_node = actor.doc["nodes"][actor.doc["skins"][0]["joints"][joint]]
        to_head = inverse(head)
        for name, at in (("HelmVisor", centre), ("HelmCore", (0.0, 0.0, 0.0))):
            actor.doc["nodes"].append({"name": name, "translation": list(apply(to_head, apply(frame, at)))})
            head_node.setdefault("children", []).append(len(actor.doc["nodes"]) - 1)
    blade_slot = next(i for i, m in enumerate(actor.doc["materials"]) if m.get("name") == "BladeEdge")
    actor.save()

    # ---- 落ちている兜（体に付く前、地面に置いてある部品）：序幕で、隙間が一瞬だけ灯る。ふだんは発光 0 の黒い面で、
    # コードが瞬きの間だけ材質の発光を上書きする（編集中のシーンで光って見えない。溝の中なので、黒い面は見分けられない）。
    part = Model("BossIntroHelm")
    part_frame, _, part_joint, _ = fit_actor(part, points, "IntroHelm_KnightHelm")
    part_facing = apply(part_frame, (0.0, 0.0, 1.0), 0.0)
    length = math.sqrt(sum(c * c for c in part_facing))
    part_slot = skinned(part, [apply(part_frame, v) for v in vertices], tuple(c / length for c in part_facing), part_joint,
                        flat, "IntroHelm_KnightHelm", (0.0, 0.0, 0.0), 0.0)
    part.save()
    write_header(actor_slot, blade_slot, part_slot, centre, half, scale)
    print("WROTE", fight.path.name, actor.path.name, part.path.name, HEADER.name, "actor material slot", actor_slot, "part", part_slot)


def skinned(model, rows, facing, joint, flat, helm_name, color, strength):
    """頭の骨に付いた光の面（役者・落ちている兜）。もうあれば、網目だけを同じ場所へ書き直す。材質の槽の番号を返す。"""
    primitives = model.glow_primitives()
    model.fresh = not primitives
    model.unmark()
    count = len(rows)
    if primitives:
        for primitive in primitives:
            attributes = primitive["attributes"]
            model.replace(attributes["POSITION"], rows, "f", "VEC3", True)
            model.replace(attributes["NORMAL"], [facing] * count, "f", "VEC3")
            model.replace(attributes["JOINTS_0"], [(joint, 0, 0, 0)] * count, "H", "VEC4")
            model.replace(attributes["WEIGHTS_0"], [(1.0, 0.0, 0.0, 0.0)] * count, "f", "VEC4")
            model.replace(primitive["indices"], flat, "H", "SCALAR")
        return primitives[0]["material"]
    position = model.add(rows, "f", "VEC3", 34962, True)
    normal = model.add([facing] * count, "f", "VEC3", 34962)
    joints = model.add([(joint, 0, 0, 0)] * count, "H", "VEC4", 34962)
    weights = model.add([(1.0, 0.0, 0.0, 0.0)] * count, "f", "VEC4", 34962)
    indices = model.add(flat, "H", "SCALAR", 34963)
    slot = len(model.doc["materials"])
    model.doc["materials"].append(material("HelmGlow", color, strength))
    model.doc["meshes"].append({"name": "HelmGlow", "primitives": [{
        "attributes": {"POSITION": position, "NORMAL": normal, "JOINTS_0": joints, "WEIGHTS_0": weights},
        "indices": indices, "material": slot}]})
    helm_node = model.node(helm_name)
    # 網目のノードは骨と同じ名前にしない（名前でノードを隠す時、先に見つかった骨の方が隠れる）。
    model.doc["nodes"].append({"name": "HelmGlowMesh", "mesh": len(model.doc["meshes"]) - 1, "skin": 0})
    glow_node = len(model.doc["nodes"]) - 1
    parent = next((n for n in model.doc["nodes"] if helm_node in n.get("children", [])), None)
    (parent["children"] if parent else model.doc["scenes"][0]["nodes"]).append(glow_node)
    return slot


def fight_add(fight, vertices, flat, centre):
    """戦いの体へ、光の面と場所の印を初めて足す。"""
    position = fight.add(vertices, "f", "VEC3", 34962, True)
    normal = fight.add([(0.0, 0.0, 1.0)] * len(vertices), "f", "VEC3", 34962)
    indices = fight.add(flat, "H", "SCALAR", 34963)
    base = len(fight.doc["materials"])
    fight.doc["materials"] += [material("HelmGlow", GLOW_COLOR, GLOW_STRENGTH), material("HelmGlowHot", HOT_COLOR, HOT_STRENGTH)]
    helm = fight.doc["nodes"][fight.node("KnightHelm")]
    placed = {k: helm[k] for k in ("translation", "rotation", "scale") if k in helm}
    for name, slot in (("HelmGlow", base), ("HelmGlowHot", base + 1)):
        fight.doc["meshes"].append({"name": name, "primitives": [{"attributes": {"POSITION": position, "NORMAL": normal},
                                                                  "indices": indices, "material": slot}]})
        fight.doc["nodes"].append(dict(placed, name=name, mesh=len(fight.doc["meshes"]) - 1))
        fight.doc["scenes"][0]["nodes"].append(len(fight.doc["nodes"]) - 1)
    for name, at in (("HelmVisor", centre), ("HelmCore", (0.0, 0.0, 0.0))):
        fight.doc["nodes"].append({"name": name, "translation": list(at)})
        fight.doc["scenes"][0]["nodes"].append(len(fight.doc["nodes"]) - 1)


def write_header(actor_slot, blade_slot, part_slot, centre, half, scale):
    HEADER.write_bytes(("""#pragma once

#include <cstdint>
#include <string_view>

#include "HIKARI/Project/ValueTypes.h"

// このファイルは Tools/BossBody/add_helm_glow.py が書き出す（手で直さない。模型を作り直したら流し直す）。
// 兜の面頬の隙間の中の光（模型に足した自発光の面）と、エフェクトを結ぶ場所。寸法は兜の模型のもの（兜の原点から、
// +Y 上・+Z 前。兜の大きさが掛かる）。
namespace Project_TD2_2::BossHelmGlowData {

// 戦いの体（KasumiBushi）のノード：ふだんの光と、フェーズ 2 の光。登場の役者（BossIntroActor）のノードと材質槽。
inline constexpr std::string_view kNode = "HelmGlow";
inline constexpr std::string_view kNodeHot = "HelmGlowHot";
inline constexpr std::string_view kActorNode = "HelmGlowMesh";
inline constexpr uint32_t kActorMaterial = %du;
// 役者の刀の刃口の材質（BladeEdge）の槽。登場の間、ここを光らせる。
inline constexpr uint32_t kActorBladeEdgeMaterial = %du;
// 落ちている兜（BossIntroHelm）の、隙間の中の面の材質槽（ふだんは黒。序幕の瞬きの間だけ、コードが光らせる）。
inline constexpr uint32_t kPartMaterial = %du;
// 役者の、場所の印のノード（頭の骨の子）：隙間の帯の中心（面の表）と、兜の原点。
inline constexpr std::string_view kVisorSocket = "HelmVisor";
inline constexpr std::string_view kCoreSocket = "HelmCore";
// 隙間の帯の中心（面の表）と、帯の半分の幅・高さ。
inline constexpr HIKARI::GameProjectFloat3 kVisorCentre{%.5ff, %.5ff, %.5ff};
inline constexpr float kVisorHalfWidth = %.5ff;
inline constexpr float kVisorHalfHeight = %.5ff;
// 役者の兜の大きさ（兜の模型の寸法に対する倍率）。
inline constexpr float kActorHelmScale = %.5ff;

} // namespace Project_TD2_2::BossHelmGlowData
""" % (actor_slot, blade_slot, part_slot, centre[0], centre[1], centre[2], half[0], half[1], scale)).replace("\n", "\r\n").encode("utf-8"))


main()
