#!/usr/bin/env bash
# Designer の図（KnightGauntlet.sbs）を .sbsar にして、出力を 16 bit の PNG に書き出す（Designer を開かずに）。c-8b643c。
# 使い方：bash render_sd.sh <書き出し先>   （.sbs を作り直す時は Designer で sd_gauntlet.py を流す）
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd -W 2>/dev/null || pwd)"
SDD="${SDD:-D:/软件/SD/Adobe Substance 3D Designer}"
OUT="${1:?書き出し先}"
mkdir -p "$OUT/sbsar"
"$SDD/sbscooker.exe" --inputs "$HERE/KnightGauntlet.sbs" --output-path "$OUT/sbsar" 2>&1 | grep -iE "error" || true
"$SDD/sbsrender.exe" render --inputs "$OUT/sbsar/KnightGauntlet.sbsar" --output-path "$OUT" \
    --output-name '{outputNodeName}' --output-format png --output-bit-depth 16 > "$OUT/render.log" 2>&1
ls "$OUT"/*.png
