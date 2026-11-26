"""籠手の手の甲の彫り（線刻の唐草と炎）の絵を、貼図の UV の上に描く。c-8b643c。

使い方：blender -b <build>/KnightGauntlet.blend --python engrave_pattern.py -- [out=<フォルダ>] [res=4096]
build_knight_gauntlet.py が、両手の手の甲の外の面を手の甲の向きに平らに UV へ写し、その「板の座標 → UV」の変換を
KnightGauntlet_layout.json（uv_affine）に書いてある。
その面の形（UV の三角）を塗って板の形の型を作り、型の内側に彫りの線を描く：
- 縁から内へ 2 本の線（板の縁に沿った枠）。
- 真ん中に、炎（3 枚の舌）の入った尖った楕円の枠。上下の尖りの先に小さな巻き。
- 枠の左右から四隅へ、渦を巻く大きな唐草（曲がり方が少しずつ強くなる曲線）と、その外側の葉。渦の芯に打ち点。
線は鏨で彫ったように、付け根が太く先が細い。左右の手は UV の写し方で親指の側が同じ向きになっているので、同じ絵を使う。
出す物（out。既定 Library/Handover/c-8b643c/sd）：貼図と同じ UV の 3 枚（灰色、res 四方）。
- engrave_lines.png：彫りの線（1＝溝の真ん中、縁へ柔らかく 0）。
- engrave_panel.png：枠の内側（地を細かく荒らす所）。
- engrave_plate.png：手の甲の外の面そのもの。
"""
import json
import math
import pathlib
import sys

import bpy
import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
options = dict(a.split("=", 1) for a in argv if "=" in a)
BUILD = pathlib.Path(options.get("build", ROOT / "Library" / "Handover" / "c-8b643c" / "build"))
OUT = pathlib.Path(options.get("out", ROOT / "Library" / "Handover" / "c-8b643c" / "sd"))
RES = int(options.get("res", "4096"))

# 寸法は板の大きさ（UV の四角の一辺）に対する割合。
BORDER = (0.030, 0.052)     # 枠の 2 本の線の、縁からの距離
BORDER_W = (0.0080, 0.0055) # その太さ
STROKE_W = 0.0085           # 唐草の線の付け根の太さ（先は STROKE_TIP 倍）
STROKE_TIP = 0.35
EDGE_SOFT = 1.2             # 線の縁のぼかし（画素）


# ---------------------------------------------------------------- 板の形
def plate_masks(layout):
    ob = bpy.data.objects["KnightGauntlet"]
    me = ob.data
    eng = np.zeros(len(me.polygons), bool)
    me.attributes["engrave"].data.foreach_get("value", eng)
    uv = np.empty(len(me.loops) * 2)
    me.uv_layers["UVMap"].data.foreach_get("uv", uv)
    uv = uv.reshape(-1, 2)
    mask = np.zeros((RES, RES), np.float32)
    for poly in me.polygons:
        if not eng[poly.index]:
            continue
        loops = list(poly.loop_indices)
        for k in range(1, len(loops) - 1):
            fill_triangle(mask, uv[[loops[0], loops[k], loops[k + 1]]] * RES)
    return mask


def fill_triangle(mask, t):
    x0, y0 = np.floor(t.min(0)).astype(int)
    x1, y1 = np.ceil(t.max(0)).astype(int)
    x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, RES - 1), min(y1, RES - 1)
    if x1 < x0 or y1 < y0:
        return
    ys, xs = np.mgrid[y0:y1 + 1, x0:x1 + 1] + 0.5
    (ax, ay), (bx, by), (cx, cy) = t
    den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
    if abs(den) < 1e-12:
        return
    l1 = ((by - cy) * (xs - cx) + (cx - bx) * (ys - cy)) / den
    l2 = ((cy - ay) * (xs - cx) + (ax - cx) * (ys - cy)) / den
    inside = (l1 >= -1e-6) & (l2 >= -1e-6) & (1 - l1 - l2 >= -1e-6)
    mask[y0:y1 + 1, x0:x1 + 1][inside] = 1.0


def distance_inside(mask, limit):
    """型の内側の、縁からの距離（画素。8 近傍と 4 近傍を交互に削る近似を、ぼかして滑らかに）。"""
    dist = np.zeros_like(mask)
    cur = mask > 0.5
    for i in range(limit):
        if not cur.any():
            break
        dist[cur] += 1.0
        nxt = cur.copy()
        nxt[1:, :] &= cur[:-1, :]
        nxt[:-1, :] &= cur[1:, :]
        nxt[:, 1:] &= cur[:, :-1]
        nxt[:, :-1] &= cur[:, 1:]
        if i % 2:
            nxt[1:, 1:] &= cur[:-1, :-1]
            nxt[:-1, :-1] &= cur[1:, 1:]
            nxt[1:, :-1] &= cur[:-1, 1:]
            nxt[:-1, 1:] &= cur[1:, :-1]
        cur = nxt
    return blur(dist, 2)


def blur(a, r):
    if r <= 0:
        return a
    k = np.exp(-0.5 * (np.arange(-3 * r, 3 * r + 1) / r) ** 2)
    k /= k.sum()
    a = np.apply_along_axis(lambda m: np.convolve(m, k, mode="same"), 0, a)
    return np.apply_along_axis(lambda m: np.convolve(m, k, mode="same"), 1, a)


# ---------------------------------------------------------------- 線を描く
class Canvas:
    """板の座標（x：横、y：手首 0 → 指の付け根 1。0〜1）で描く。UV へは uv_affine で写す。絵は左右対称。"""

    def __init__(self, lines, affine):
        self.lines = lines
        self.A = np.array(affine)
        self.scale = float(np.linalg.norm(self.A[:, 0])) * RES   # 板の座標の 1 → 画素

    def px(self, p):
        return (self.A @ np.array([p[0], p[1], 1.0])) * RES

    def stroke(self, pts, w0, w1=None):
        """付け根の太さ w0 から先の太さ w1 へ細る線（太さは板の一辺に対する割合）。"""
        pts = np.asarray(pts, float)
        w1 = w0 if w1 is None else w1
        seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        s = np.concatenate([[0.0], np.cumsum(seg)])
        total = s[-1]
        scale = self.scale
        n = max(2, int(total * scale / 0.35))
        ss = np.linspace(0.0, total, n)
        xy = np.column_stack([np.interp(ss, s, pts[:, 0]), np.interp(ss, s, pts[:, 1])])
        widths = (w0 + (w1 - w0) * (ss / total) ** 0.8) * scale
        for p, w in zip(xy, widths):
            c = self.px(p)
            r = w / 2 + EDGE_SOFT
            x0, x1 = int(c[0] - r - 1), int(c[0] + r + 2)
            y0, y1 = int(c[1] - r - 1), int(c[1] + r + 2)
            ys, xs = np.mgrid[y0:y1, x0:x1] + 0.5
            d = np.hypot(xs - c[0], ys - c[1])
            v = np.clip((w / 2 + EDGE_SOFT - d) / (2 * EDGE_SOFT), 0.0, 1.0)
            v = v * v * (3 - 2 * v)
            region = self.lines[y0:y1, x0:x1]
            np.maximum(region, v, out=region)


def clothoid(start, heading, length, k0, k1, n=240):
    """曲がり方が k0 → k1 へ直線的に変わる曲線（唐草の渦）。heading は度、曲がりは 1/長さ。"""
    s = np.linspace(0.0, length, n)
    th = math.radians(heading) + k0 * s + 0.5 * (k1 - k0) / length * s * s
    ds = length / (n - 1)
    x = start[0] + np.concatenate([[0.0], np.cumsum(np.cos(th[:-1]) * ds)])
    y = start[1] + np.concatenate([[0.0], np.cumsum(np.sin(th[:-1]) * ds)])
    return np.column_stack([x, y])


def mirror(pts):
    pts = np.array(pts, float)
    pts[:, 0] = 1.0 - pts[:, 0]
    return pts


def vesica(cx, cy, hw, hh, n=90):
    """尖った楕円（2 つの円弧の重なり）の輪郭。hw＝半分の幅、hh＝半分の高さ。"""
    r = (hw * hw + hh * hh) / (2.0 * hw)
    a = math.atan2(hh, -(r - hw))
    phi = np.linspace(a, 2.0 * math.pi - a, n)
    left = np.column_stack([cx + (r - hw) + r * np.cos(phi), cy + r * np.sin(phi)])
    right = left.copy()
    right[:, 0] = 2 * cx - right[:, 0]
    return left, right


def tongue(cv, base, tip, width, bend, w0, w1):
    """炎の舌（両側の輪郭を 1 本ずつ。付け根で広く、先で合う。bend だけ横へ S 字にしなる）。"""
    t = np.linspace(0.0, 1.0, 90)
    bx, by = base
    tx, ty = tip
    spine_x = bx + (tx - bx) * t + bend * np.sin(np.pi * t) * (1 - t)
    spine_y = by + (ty - by) * t
    half = width * np.sin(np.pi * np.clip(t, 0, 1) ** 0.65) * (1 - t) ** 0.7
    for side in (-1.0, 1.0):
        cv.stroke(np.column_stack([spine_x + side * half, spine_y]), w0, w1)


def design(cv, box):
    """中に炎の入った尖った楕円の枠と、その左右から四隅へ渦を巻く大きな唐草。box＝板の外接の四角（板の座標）。"""
    x0, x1, y0, y1 = box
    cx = (x0 + x1) / 2
    w, h = x1 - x0, y1 - y0

    def P(fx, fy):
        return np.array([x0 + fx * w, y0 + fy * h])

    def sym(pts, w0, w1=None):
        pts = np.asarray(pts, float)
        cv.stroke(pts, w0, w1)
        m = pts.copy()
        m[:, 0] = 2 * cx - m[:, 0]
        cv.stroke(m, w0, w1)

    def leaves(curve, at, length, curl, wk, side=-1.0):
        """唐草の外側へ、途中から短く反る葉。"""
        for t, lk in at:
            i = int(len(curve) * t)
            d = curve[i + 1] - curve[i - 1]
            ang = math.degrees(math.atan2(d[1], d[0])) + side * 38
            # 葉：枝から斜めに出て、すぐ枝の先の向きへ巻き戻る（根元が太く、先が細い）。
            sym(clothoid(curve[i], ang, length * w, -side * 4.0 / w, -side * curl / w * lk, 90), sw * 0.9 * wk,
                sw * 0.2)

    sw = STROKE_W
    mid = P(0.5, 0.47)
    hw, hh = 0.125 * w, 0.25 * h
    # 尖った楕円の枠（外と内の 2 本）。
    for off, wk in ((0.0, 1.15), (0.022 * w, 0.7)):
        for arc in vesica(mid[0], mid[1], hw - off, hh - off * 1.5):
            cv.stroke(arc, sw * wk)
    # 中の炎：真ん中の高い舌と、両側の外へしなる舌。
    base = P(0.5, 0.29)
    tongue(cv, base, P(0.5, 0.67), 0.036 * w, 0.02 * w, sw * 0.75, sw * 0.3)
    for side in (-1.0, 1.0):
        tongue(cv, base + [side * 0.052 * w, 0.025 * h], P(0.5 + side * 0.095, 0.54), 0.02 * w, side * 0.012 * w,
               sw * 0.65, sw * 0.25)
    # 上の唐草：枠の横から外へ出て上へ反り、上の隅で内へ渦を巻く。下の唐草はその逆。
    for fy, head, k0, k1, wk in ((0.53, 18.0, -2.0, 23.0, 1.0), (0.41, -22.0, 2.0, -23.0, 1.0)):
        start = np.array([mid[0] + hw * 0.92, y0 + fy * h])
        main = clothoid(start, head, 0.43 * w, k0 / w, k1 / w, 320)
        sym(main, sw * 1.05 * wk, sw * STROKE_TIP)
        sgn = 1.0 if k1 > 0 else -1.0
        leaves(main, ((0.25, 1.0), (0.5, 1.25)), 0.075, 42.0 * sgn, wk, side=-sgn)
        # 渦の芯に小さな打ち点。
        tipp = main[-1]
        ring = [tipp + 0.008 * w * np.array([math.cos(t), math.sin(t)]) for t in np.linspace(0, 2 * math.pi, 24)]
        sym(ring, sw * 0.6)
    # 上の尖りの先：短い茎と、外へ巻く小さな 2 つの巻き。
    top = mid + [0.0, hh]
    cv.stroke([top, top + [0.0, 0.045 * h]], sw * 0.9, sw * 0.6)
    sym(clothoid(top + [0.0, 0.03 * h], 150.0, 0.07 * w, -6.0 / w, -60.0 / w, 80), sw * 0.7, sw * 0.25)
    # 下の尖りの先：短い茎と、外へ巻く 2 つの巻き。
    bottom = mid - [0.0, hh]
    cv.stroke([bottom, bottom - [0.0, 0.06 * h]], sw * 0.9, sw * 0.6)
    sym(clothoid(bottom - [0.0, 0.05 * h], 200.0, 0.10 * w, 4.0 / w, 55.0 / w, 90), sw * 0.75, sw * 0.25)


def save(a, name):
    img = bpy.data.images.new(name, RES, RES, alpha=False, float_buffer=False)
    img.colorspace_settings.name = "Non-Color"
    rgba = np.repeat(np.clip(a, 0, 1)[..., None], 4, axis=2)
    rgba[..., 3] = 1.0
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = str(OUT / (name + ".png"))
    img.file_format = "PNG"
    img.save()
    print("[engrave] wrote", img.filepath_raw)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    layout = json.loads((BUILD / "KnightGauntlet_layout.json").read_text(encoding="utf-8"))
    plate = plate_masks(layout)
    lines = np.zeros_like(plate)
    panel = np.zeros_like(plate)
    for side in ("Left", "Right"):
        info = layout[side]
        motif = np.zeros_like(plate)
        cv = Canvas(motif, info["uv_affine"])
        # 板の外接の四角（板の座標）。横は右手を反して写してあるので、左右どちらも同じ並び。
        span = info["span_m"]
        lat = sorted(0.5 + info["lateral_flip"] * np.array(info["lateral_extent"]) / span)
        alo = 0.5 + np.array(info["along_extent"]) / span
        design(cv, (lat[0], lat[1], alo[0], alo[1]))
        # 枠：板の縁からの距離の 2 本の等高線。
        (bu0, bv0), (bu1, bv1) = info["uv_bounds"]
        x0, y0 = max(int(bu0 * RES) - 4, 0), max(int(bv0 * RES) - 4, 0)
        x1, y1 = min(int(bu1 * RES) + 5, RES), min(int(bv1 * RES) + 5, RES)
        sub = plate[y0:y1, x0:x1]
        side_px = cv.scale
        dist = distance_inside(sub, int(BORDER[1] * side_px * 1.6))
        for d, wd in zip(BORDER, BORDER_W):
            band = np.clip((wd * side_px / 2 + EDGE_SOFT - np.abs(dist - d * side_px)) / (2 * EDGE_SOFT), 0, 1)
            np.maximum(lines[y0:y1, x0:x1], band * (sub > 0.5), out=lines[y0:y1, x0:x1])
        inner = np.clip((dist - BORDER[1] * side_px - 2) / 3.0, 0, 1)
        panel[y0:y1, x0:x1] = np.maximum(panel[y0:y1, x0:x1], inner)
        # 唐草は枠の内側だけ（枠の線から少し離す）。枠の線と合わせる。
        keep = np.clip((dist - (BORDER[1] + BORDER_W[1] * 1.5) * side_px) / 3.0, 0, 1)
        np.maximum(lines[y0:y1, x0:x1], motif[y0:y1, x0:x1] * keep, out=lines[y0:y1, x0:x1])
    save(lines, "engrave_lines")
    save(panel, "engrave_panel")
    save(plate, "engrave_plate")


main()
