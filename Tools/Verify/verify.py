"""無頭（headless）の確かめ：リポジトリの写し（Library/Verify/Probe）でビルドし、場面を流して記録を数える。

整理（リファクタリング）の前後で、ボスの技・当たり・警告の数が変わっていないかを比べるための道具（c-fa97d7）。
エンジンのビルドの場所は環境変数 HIKARI_ENGINE_BIN（無ければ D:/PJ/HIKARI_Engine/out/build/editor-release/bin）。

  python -I -X utf8 Tools/Verify/verify.py sync build                 写しを作ってビルドする（警告 0 を見る）
  python -I -X utf8 Tools/Verify/verify.py run <技の名前> [ticks=1400] [caps=a,b,...]
                                                                     SmokeArena で技を 1 つだけ繰り返させる
  python -I -X utf8 Tools/Verify/verify.py scene <main|kujata> [ticks=3000] [shoot=1] [caps=...]
                                                                     ふだんの戦い（行動ツリー）
  python -I -X utf8 Tools/Verify/verify.py baseline <名前> [only=moves,shift,fight_main,fight_kujata]
                                                                     基準の場面を流し、Library/Verify/baselines/<名前>/ に記録。
                                                                     既定は短い場面（quick_moves・quick_fight。数分で終わる）だけ。
                                                                     長い場面は only= に名前を書いた時だけ流す
  python -I -X utf8 Tools/Verify/verify.py compare <名前A> <名前B>     2 つの基準の記録を比べる（違えば終了コード 1）
  python -I -X utf8 Tools/Verify/verify.py reach [close=2.5] [reachProbeDistanceStep=0.25] [reachProbeAngleStep=10]
                                                                     剣の技が当たる間合い（距離 × 角度）を測り、
                                                                     Library/Verify/runs/reach/reach.md に表を書く（c-c010ef）
  python -I -X utf8 Tools/Verify/verify.py sync build aim            狙いのずれの表 Data/Boss/boss_aim_offsets.csv を作り直す
                                                                     （c-884095。モーション・区間の表を変えたら流す。写しにも入れる
                                                                     ので、続けて run・reach・aimcheck を流せる）
  python -I -X utf8 Tools/Verify/verify.py aimcheck [moves=a,b] [distances=2,3,4]
                                                                     技 × 距離 × ねじりの有無で当たるかを数え、
                                                                     Library/Verify/runs/aimcheck.md に表を書く（c-884095）

写しを作る時、作業中でない他人の書き換え（コミットしていない変更）は HEAD の中身に戻して入れる（比べる記録に
混ぜない）。自分の作業のパスは own=<頭,頭,...>、無ければ .hikari-hub/active.json の paths を使う。
プレイヤーは最初の 2 秒だけ前へ歩き、tick 160 に登場を飛ばす入力（場面の introSkipAction。既定は Gameplay.Menu）を押す。
"""
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
WORK = ROOT / "Library" / "Verify"
PROBE = WORK / "Probe"
BASELINES = WORK / "baselines"
BIN = os.environ.get("HIKARI_ENGINE_BIN", "D:/PJ/HIKARI_Engine/out/build/editor-release/bin")
ENGINE = BIN + "/hikari.exe"
SDK = BIN + "/Runtime/Sdk"
SCENES = {"main": "Assets/Scenes/SmokeArena.scene.json", "kujata": "Assets/Scenes/SmokeArena_Kujata.scene.json"}
BOSS_TYPE = "Project_TD2_2.Boss"
PLAYER_TYPE = "TD2_2.Player"  # 新しいゲーム層の部品（c-df5bfd）
COPIED = ("Source", "Data", "Assets", "ProjectSettings", "Tools")
# エンジンの側の、整理と関係なく毎回出る警告（数えない）。
IGNORED_WARNINGS = (r"SubmitActionCinematicGrade", r"GPU wait exceeded", r"RetiredSpecialOutput")


def guid(scene):
    meta = ROOT / "ProjectSettings/AssetMeta" / (scene + ".hikari.asset.json")
    return json.loads(meta.read_text(encoding="utf-8"))["guid"]


def own_prefixes(opts):
    if "own" in opts:
        return [p for p in opts["own"].split(",") if p]
    active = ROOT / ".hikari-hub" / "active.json"
    if active.exists():
        return [p.rstrip("*") for p in json.loads(active.read_text(encoding="utf-8")).get("paths", [])]
    return None


def put(target, data):
    """中身が違う時だけ書く（今の時刻になる）。同じなら触らない（ビルドが作り直さない）。"""
    if target.is_file() and target.read_bytes() == data:
        return
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


def mirror(src, dst):
    """src を dst へ写す。ファイルの時刻は写さない：写すと、前の写し（別の中身）のビルドより古く見えて、変わった
    ファイルが作り直されないことがある（c-eb3b08）。src に無いファイルは消す。"""
    seen = set()
    if src.exists():
        for path in src.rglob("*"):
            if "__pycache__" in path.parts or not path.is_file():
                continue
            rel = path.relative_to(src)
            seen.add(rel)
            put(dst / rel, path.read_bytes())
    if dst.exists():
        for path in dst.rglob("*"):
            if path.is_file() and path.relative_to(dst) not in seen and "__pycache__" not in path.parts:
                path.unlink()


def sync(opts):
    for name in COPIED:
        mirror(ROOT / name, PROBE / name)
    for name in ("CMakeLists.txt", "Project.hikari"):
        put(PROBE / name, (ROOT / name).read_bytes())
    # 自分の作業でない、コミットしていない書き換えは HEAD の中身に戻す（消えたファイルは消したまま比べない）。
    own = own_prefixes(opts)
    restored = []
    if own is not None:
        status = subprocess.run(["git", "status", "--porcelain", "--", *COPIED], cwd=ROOT, capture_output=True, text=True,
                                encoding="utf-8").stdout.splitlines()
        for line in status:
            code, path = line[:2], line[3:].strip().strip('"')
            if any(path.startswith(p) for p in own) or not path.split("/")[0] in COPIED:
                continue
            target = PROBE / path
            if code.strip() == "??":
                if target.is_file():
                    target.unlink()
                restored.append(path + " (未追跡を外した)")
                continue
            blob = subprocess.run(["git", "show", f"HEAD:{path}"], cwd=ROOT, capture_output=True).stdout
            put(target, blob)
            restored.append(path)
    print("SYNC done", "restored", len(restored), restored[:6], flush=True)


def build():
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", SDK + "/Tools/Build-HikariProject.ps1",
           "-Project", str(PROBE / "Project.hikari"), "-SdkRoot", SDK, "-Preset", "editor-release"]
    start = time.perf_counter()
    proc = subprocess.run(cmd, cwd=PROBE, capture_output=True)
    text = (proc.stdout + proc.stderr).decode("utf-8", "replace")
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "build.log").write_text(text, encoding="utf-8")
    bad = [line for line in text.splitlines() if re.search(r"warning C|warning LNK|error C|error LNK|: error|: warning", line)]
    print("BUILD rc", proc.returncode, "warnings/errors", len(bad), "sec", round(time.perf_counter() - start), flush=True)
    for line in bad[:30]:
        print("   ", line[:260])
    return 1 if proc.returncode != 0 or bad else 0


def component(scene_json, type_name, object_name=None):
    for obj in scene_json["objects"]:
        for comp in obj.get("components") or []:
            if comp.get("type") == type_name:
                return comp.setdefault("properties", {})
    raise SystemExit(f"{type_name} が場面に無い")


def skip_action(scene_json):
    for obj in scene_json["objects"]:
        for comp in obj.get("components") or []:
            if comp.get("type") == "Project_TD2_2.BossIntro":
                return (comp.get("properties") or {}).get("introSkipAction", "Gameplay.Menu")
    return "Gameplay.Menu"


def headless(key, ticks, caps, folder, boss_patch=None, player_patch=None, extra=None, skip_tick=160):
    """場面を ticks だけ流す。記録の行（engine.log）を返す。skip_tick の tick に登場を飛ばす入力（場面の introSkipAction）を押す
    （None なら飛ばさない）。"""
    scene = SCENES[key]
    settings = json.loads((ROOT / "ProjectSettings/project_settings.json").read_text(encoding="utf-8"))
    settings["startupSceneGuid"] = guid(scene)
    (PROBE / "ProjectSettings/project_settings.json").write_text(json.dumps(settings, ensure_ascii=False, indent=2) + "\n",
                                                               encoding="utf-8")
    src = json.loads((PROBE / scene).read_text(encoding="utf-8")) if (PROBE / scene).exists() else None
    src = json.loads((ROOT / scene).read_text(encoding="utf-8")) if src is None else src
    original = (PROBE / scene).read_bytes() if (PROBE / scene).exists() else None
    if boss_patch:
        component(src, BOSS_TYPE).update(boss_patch)
    if player_patch:
        component(src, PLAYER_TYPE).update(player_patch)
    (PROBE / scene).write_text(json.dumps(src, ensure_ascii=False, indent=2), encoding="utf-8")
    if folder.exists():
        shutil.rmtree(folder)
    folder.mkdir(parents=True)
    events = [dict(tick=30, action="Gameplay.Move", value=[0, 1]), dict(tick=150, action="Gameplay.Move", value=[0, 0])]
    if skip_tick is not None:
        # 登場を飛ばす入力は、場面のボスの登場の部品の introSkipAction（無ければ既定の Gameplay.Menu）。
        skip = skip_action(src)
        events += [dict(tick=skip_tick, action=skip, value=1), dict(tick=skip_tick + 1, action=skip, value=0)]
    events += extra or []
    track = dict(assetType="InputTrack", formatVersion=1, length=max(ticks, 200), events=events)
    (folder / "track.json").write_text(json.dumps(track, indent=2), encoding="utf-8")
    logs = PROBE / "Library/Logs"
    before = set(logs.glob("*.log")) if logs.exists() else set()
    cmd = [ENGINE, "headless", str(max(ticks, 200)), "--track", str(folder / "track.json"), "--pretty", "--quality", "project"]
    caps = [c for c in (caps or []) if c < ticks]
    if caps:
        cmd += ["--capture", ",".join(map(str, caps[:8]))]
    start = time.perf_counter()
    try:
        proc = subprocess.run(cmd, cwd=PROBE, capture_output=True, timeout=3600)
    finally:
        if original is not None:
            (PROBE / scene).write_bytes(original)
    (folder / "stdout.json").write_bytes(proc.stdout)
    try:
        data = json.loads(proc.stdout.decode("utf-8-sig")).get("data", {})
    except ValueError:
        data = {}
    lines = []
    for p in set(logs.glob("*.log")) - before:
        shutil.copy2(p, folder / "engine.log")
        lines = p.read_text(encoding="utf-8", errors="replace").splitlines()
    for c in data.get("captures") or []:
        if c.get("ok") and pathlib.Path(c["path"]).exists():
            shutil.copyfile(c["path"], folder / f"t{c['requestedTick']:04d}.png")
    print(key, folder.name, "rc", proc.returncode, "ticks", data.get("completedTicks"), "wall", round(time.perf_counter() - start, 1),
          flush=True)
    return lines


def metrics(lines):
    """記録の行から数える：ボスの出来事ごとの数、技ごとの出た数・当たり、警告。"""
    events, per_move, warnings, table = {}, {}, {}, {}
    for line in lines:
        m = re.search(r"\[(INFO|WARN|ERROR)\] \[([A-Za-z]+)\] (\[[A-Za-z]+\] )?([A-Za-z]+)", line)
        if m and m.group(1) == "INFO" and m.group(2) == "Boss":
            name = m.group(4)
            events[name] = events.get(name, 0) + 1
            mv = re.search(r"move=(\S+)", line)
            if mv and name in ("MoveStart", "PlayerHit", "MoveHit", "PlayerDodge", "Combo", "RingRelease", "MoveBroken"):
                row = per_move.setdefault(mv.group(1), {})
                row[name] = row.get(name, 0) + 1
            if name == "TableLoaded":
                table = dict(re.findall(r"(\w+)=(\d+)", line))
        if re.search(r"\[(WARN|ERROR)\]", line) and not any(re.search(p, line) for p in IGNORED_WARNINGS):
            key = re.sub(r"\d+(\.\d+)?", "#", line.split("] ", 2)[-1])[:160]
            warnings[key] = warnings.get(key, 0) + 1
    return dict(events=dict(sorted(events.items())), moves=dict(sorted(per_move.items())), warnings=warnings, table=table)


def all_move_names():
    props = component(json.loads((ROOT / SCENES["main"]).read_text(encoding="utf-8")), BOSS_TYPE)
    table = ROOT / "Data" / props.get("movesPath", "Boss/kasumi_bushi_moves_kujata.csv")
    rows = table.read_text(encoding="utf-8-sig").splitlines()[1:]
    return [r.split(",")[0] for r in rows if r.strip()]


# 基準の場面：名前 → (場面, tick, ボスの差し替え, プレイヤーの差し替え, 撃つか, 比べ方)
# 比べ方 exact：行動ツリーを使わない場面（確かめ用の頭）。同じコードなら記録の数が全部同じになるので、全部比べる。
# 比べ方 loose：ふだんの戦い。行動ツリーの乱数と、GPU から読み戻す値（吸った煙の量など）で毎回少し変わるので、
# 新しい警告・出なくなった出来事だけを見る（1 分の戦いでは出ない技もあるので、技ごとの数は比べない）。
STURDY = {"maxHealth": 10000.0}
# 表の技を順番に出す（プレイヤーは立ったまま、HP を大きくして倒れない）
SEQUENCE = dict(testSequence=True, testLoop=False, testHoldStill=True, testStartFrames=60, testGapFrames=60, testChain=False)
SCENARIOS = {
    # 短い場面（baseline の既定）：表の最初の数行（斬り・連撃・突進横・溜め横…）を順に出す 20 秒と、ふだんの戦いの 15 秒。
    "quick_moves": ("main", 1200, SEQUENCE, STURDY, False, "exact"),
    "quick_fight": ("main", 900, None, STURDY, True, "loose"),
    # 長い場面（only= に名前を書いた時だけ）。表の技を全部出す
    "moves": ("main", 7200, SEQUENCE, STURDY, False, "exact"),
    # フェーズの変わり目（確かめのダメージで 1 本目を空にする。技は出さない）
    "shift": ("main", 900, dict(testDamageAtFrame=240, testDamageAmount=100000.0, testHoldStill=True, testMove="斜め切り下ろし",
                                testStartFrames=5000), STURDY, False, "exact"),
    # ふだんの戦い（行動ツリー。鎖で狙って一定の間隔で撃つ）
    "fight_main": ("main", 3600, None, STURDY, True, "loose"),
    "fight_kujata": ("kujata", 3600, None, STURDY, True, "loose"),
    # 登場（飛ばさずに 25 秒）と、登場の途中（tick 600）で飛ばして戦いに入るまで（確かめ用の頭は登場を切るので
    # 使わない。飛ばした後は行動ツリーの乱数で技が変わるので、登場の出来事だけを比べる：EVENT_PREFIX）
    "intro": ("main", 1500, None, STURDY, False, "exact", None),
    "intro_skip": ("main", 1100, None, STURDY, False, "exact", 600),
}
# exact の場面で、同じコードでも数が変わるので比べない出来事（場面ごと）。立ち位置のログ（SeeStance。30 F ごと）は、
# 流し終わった後もエンジンが少し進む間に 1 つ多く出ることがある（終わりの tick がちょうど区切り）。
IGNORED_EVENTS = {"intro": ("SeeStance",)}
# この頭で始まる出来事だけを比べる場面（技ごとの数も比べない。警告と表は比べる）。
EVENT_PREFIX = {"intro_skip": "Intro"}
# baseline で only= が無い時に流す場面。
QUICK = ("quick_moves", "quick_fight")
# loose の場面で「出なくなったら違い」とみなす出来事。
LOOSE_EVENTS = ("MoveStart", "PlayerHit", "Pick", "Reaction", "KasumiStep", "WaveRelease")


def shoot_events(ticks):
    extra = [dict(tick=200, action="Gameplay.LockOn", value=1), dict(tick=202, action="Gameplay.LockOn", value=0)]
    for k in range(300, ticks - 100, 240):
        extra += [dict(tick=k, action="Gameplay.Attack", value=1), dict(tick=k + 25, action="Gameplay.Attack", value=0)]
    return extra


def baseline(name, opts):
    out = BASELINES / name
    out.mkdir(parents=True, exist_ok=True)
    only = opts.get("only", ",".join(QUICK)).split(",")
    head = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()
    for key in only:
        scene, ticks, boss, player, shoot, mode, *rest = SCENARIOS[key]
        skip_tick = rest[0] if rest else 160
        lines = headless(scene, ticks, [], WORK / "runs" / f"{name}_{key}", boss, player, shoot_events(ticks) if shoot else None,
                         skip_tick)
        result = metrics(lines)
        result.update(scenario=key, head=head, ticks=ticks, mode=mode)
        (out / f"{key}.json").write_text(json.dumps(result, ensure_ascii=False, indent=1), encoding="utf-8")
        starts = sum(v.get("MoveStart", 0) for v in result["moves"].values())
        hits = sum(v.get("PlayerHit", 0) for v in result["moves"].values())
        print(f"  {key}: events {sum(result['events'].values())} moveStarts {starts} playerHits {hits} "
              f"warnings {sum(result['warnings'].values())} table {result['table']}", flush=True)
    print("BASELINE", out)


def compare(a, b):
    differences = 0
    for path in sorted((BASELINES / a).glob("*.json")):
        other = BASELINES / b / path.name
        if not other.exists():
            print(f"[{path.stem}] {b} に無い")
            differences += 1
            continue
        x = json.loads(path.read_text(encoding="utf-8"))
        y = json.loads(other.read_text(encoding="utf-8"))
        lines = []
        exact = x.get("mode", "exact") == "exact"
        prefix = EVENT_PREFIX.get(path.stem)
        for move in sorted(set(x["moves"]) | set(y["moves"])):
            if exact and prefix is None and x["moves"].get(move, {}) != y["moves"].get(move, {}):
                lines.append(f"  技 {move}: {x['moves'].get(move, {})} -> {y['moves'].get(move, {})}")
        ignored = IGNORED_EVENTS.get(path.stem, ())
        for event in sorted(set(x["events"]) | set(y["events"])):
            if event in ignored or (prefix is not None and not event.startswith(prefix)):
                continue
            a_n, b_n = x["events"].get(event, 0), y["events"].get(event, 0)
            if (exact and a_n != b_n) or (not exact and event in LOOSE_EVENTS and a_n > 0 and b_n == 0):
                lines.append(f"  出来事 {event}: {a_n} -> {b_n}")
        for warning in sorted(set(y["warnings"]) - set(x["warnings"])):
            lines.append(f"  新しい警告 x{y['warnings'][warning]}: {warning}")
        if x["table"] != y["table"]:
            lines.append(f"  表: {x['table']} -> {y['table']}")
        print(f"[{path.stem}]（{'全部' if exact else '大まかに'}比べる）{'同じ' if not lines else str(len(lines)) + ' 件の違い'}")
        for line in lines[:40]:
            print(line)
        differences += len(lines)
    return 1 if differences else 0


def summarize(lines):
    m = metrics(lines)
    print("warnings", sum(m["warnings"].values()))
    for k, n in sorted(m["warnings"].items(), key=lambda kv: -kv[1])[:15]:
        print(f"  x{n} {k}")
    for line in lines:
        if re.search(r"TableLoaded|MoveStart|PlayerHit|RingRelease|Reaction|PhaseChange", line):
            print("  |", line.split("] ", 1)[-1][:200])


def parse_angles(text):
    """「-40..30,90..120」→ 角度の集合（刻みは step）。"""
    if text == "-":
        return []
    out = []
    for part in text.split(","):
        a, b = part.split("..")
        out.append((int(a), int(b)))
    return out


def covers(intervals, angle):
    return any(a <= angle <= b for a, b in intervals)


AIM_CSV = ROOT / "Data" / "Boss" / "boss_aim_offsets.csv"
AIM_HEADER = "技,斬り,距離 m,狙いのずれ °,当たりの始め F,当たりの終わり F,クリップ,クリップの長さ F,指紋,メモ"


def probe(opts, folder, defaults):
    """ボスの reachProbe で測り、斬りごとの記録を返す：({(技, 斬り): {window, low, high, center, clip, clipFrames,
    fingerprint, hitStart, hitEnd, rows}}, 斬りの順)。rows は (距離, 当たる角度の区間, 高さを無視した区間, 文字, 文字)。"""
    patch = dict(reachProbe=True, testMove="斜め切り下ろし", testHoldStill=True, testStartFrames=100000)
    patch.update(defaults)
    for key in ("reachProbeDistanceMin", "reachProbeDistanceMax", "reachProbeDistanceStep"):
        if key in opts:
            patch[key] = float(opts[key])
    if "reachProbeAngleStep" in opts:
        patch["reachProbeAngleStep"] = int(opts["reachProbeAngleStep"])
    lines = headless("main", int(opts.get("ticks", 300)), [], folder, patch, STURDY)
    strikes, order = {}, []
    for line in lines:
        text = line.split("] ", 1)[-1]
        m = re.search(r"ReachStrike move=(\S+) strike=(\d+) window=(\S+) frames=(\d+) bladeLow=(\S+) bladeHigh=(\S+) "
                      r"radius=(\S+) playerCenter=(\S+) clip=(\S*) clipFrames=(\d+) fingerprint=(\S+) "
                      r"hitStart=(\d+) hitEnd=(\d+)", text)
        if m:
            key = (m.group(1), int(m.group(2)))
            strikes[key] = dict(window=m.group(3), low=float(m.group(5)), high=float(m.group(6)),
                                center=float(m.group(8)), clip=m.group(9), clipFrames=int(m.group(10)),
                                fingerprint=m.group(11), hitStart=int(m.group(12)), hitEnd=int(m.group(13)), rows=[])
            order.append(key)
            continue
        m = re.search(r"ReachProbe move=(\S+) strike=(\d+) d=(\S+) hit=(\S+) tall=(\S+)", text)
        if m:
            strikes[(m.group(1), int(m.group(2)))]["rows"].append(
                (float(m.group(3)), parse_angles(m.group(4)), parse_angles(m.group(5)), m.group(4), m.group(5)))
    return strikes, order


def best_angle(intervals, step):
    """当たる角度の区間のうち一番広いものの真ん中（度。正がボスの右）。±180 をまたぐ区間はつないで見る。無ければ None。"""
    spans = list(intervals)
    if len(spans) >= 2 and spans[0][0] == -180 + step and spans[-1][1] == 180:
        spans = [(spans[-1][0], spans[0][1] + 360)] + spans[1:-1]
    if not spans:
        return None
    a, b = max(spans, key=lambda s: s[1] - s[0])
    center = (a + b) / 2.0
    return center - 360.0 if center > 180.0 else center


def read_aim():
    """狙いのずれの表 → {(技, 斬り 0 起): [(距離, 角度)]}（無ければ空）。"""
    table = {}
    if not AIM_CSV.exists():
        return table
    for row in AIM_CSV.read_text(encoding="utf-8").splitlines()[1:]:
        cells = row.split(",")
        if len(cells) >= 4 and cells[0]:
            table.setdefault((cells[0], int(cells[1]) - 1), []).append((float(cells[2]), float(cells[3])))
    return table


def aim_at(samples, d):
    """距離 d の狙いのずれ（ゲームの BossMoveTable::AimDegrees と同じ：間は補間、端の外は端の値）。"""
    if not samples:
        return 0.0
    samples = sorted(samples)
    if d <= samples[0][0]:
        return samples[0][1]
    for (d0, a0), (d1, a1) in zip(samples, samples[1:]):
        if d <= d1:
            return a0 + (a1 - a0) * ((d - d0) / (d1 - d0) if d1 > d0 else 1.0)
    return samples[-1][1]


def aim(opts):
    """狙いのずれの表（c-884095）を作る：クリップの刀を細かく測り、斬りごと・距離ごとに一番当たる向きを
    Data/Boss/boss_aim_offsets.csv に書く。モーション（クリップ・区間の表）を変えたら流し直す（ゲームは起動時に表を読む
    だけで計算しない。古いと警告が出る）。"""
    folder = WORK / "runs" / "aim"
    step = int(opts.get("reachProbeAngleStep", 2))
    strikes, order = probe(opts, folder, dict(reachProbeAngleStep=step, reachProbeDistanceMin=1.0,
                                              reachProbeDistanceMax=8.0, reachProbeDistanceStep=0.5))
    if not order:
        print("AIM 記録が無い（ReachStrike が出ていない）。engine.log を見る:", folder)
        return 1
    out = [AIM_HEADER]
    for key in order:
        s = strikes[key]
        found = [(d, best_angle(hit, step)) for d, hit, _, _, _ in s["rows"]]
        hits = [(d, a) for d, a in found if a is not None]
        for d, a in found:
            note = ""
            if a is None:
                # その距離では当たらない：近い距離（同じなら遠い方）の角度を使う。どの距離でも当たらなければ 0。
                if hits:
                    a = min(hits, key=lambda h: (abs(h[0] - d), -h[0]))[1]
                    note = "この距離では当たらない（近い距離の角度）"
                else:
                    a = 0.0
                    note = "どの距離でも当たらない"
            out.append(f"{key[0]},{key[1] + 1},{d:g},{a:g},{s['hitStart']},{s['hitEnd']},{s['clip']},{s['clipFrames']},"
                       f"{s['fingerprint']},{note}")
        print(f"  {key[0]} {key[1] + 1}: " + " ".join(f"{d:g}m:{a if a is not None else '-'}" for d, a in found))
    AIM_CSV.write_text("\n".join(out) + "\n", encoding="utf-8")
    # 写し（Probe）にも入れる：続けて run・reach・aimcheck を流した時に、作り直した表で動くように。
    put(PROBE / AIM_CSV.relative_to(ROOT), AIM_CSV.read_bytes())
    print("AIM ->", AIM_CSV, "rows", len(out) - 1)
    return 0


# aimcheck で流す技（滑って・突進して寄る剣の技。止まる距離の項目で、プレイヤーとの距離を決められるもの）。
AIMCHECK_MOVES = ("斜め切り下ろし", "横二連撃", "突進横", "溜め横", "三連撃")
AIMCHECK_DISTANCES = (2.0, 2.5, 3.0, 3.5, 4.0, 5.0, 6.0)


def aimcheck(opts):
    """狙いのずれが効いているか（c-884095）：技 × プレイヤーとの距離 × ねじりの有無で、技を本当に出させて当たるかを数え、
    Library/Verify/runs/aimcheck.md に表を書く。距離はボスがプレイヤーの手前で止まる項目（滑って入る・連撃の歩き寄り・
    突進）を d にして決める。斬りごとの距離は、その斬りを狙い始めた時の記録（AimTwist）の値。"""
    moves = opts["moves"].split(",") if "moves" in opts else AIMCHECK_MOVES
    distances = [float(x) for x in opts["distances"].split(",")] if "distances" in opts else AIMCHECK_DISTANCES
    folder = WORK / "runs" / "aimcheck_run"
    results = {}
    for move in moves:
        for d in distances:
            for twist in (False, True):
                patch = dict(testMove=move, testHoldStill=True, testStartFrames=60, testGapFrames=90, testChain=False,
                             slideInStopDistance=d, stringApproachStop=d, dashStopDistance=d)
                if not twist:
                    patch["aimOffsetsPath"] = ""
                lines = headless("main", int(opts.get("ticks", 900)), [], folder, patch, STURDY)
                seen, hits = {}, {}
                for line in lines:
                    # 角度を止めた時（振り始め）の距離と角度。同じ斬りを何度も止めたら最初の 1 回。
                    m = re.search(r"AimHold .*strike=(\d+) used=(\S+) distance=(\S+)", line)
                    if m and int(m.group(1)) not in seen:
                        seen[int(m.group(1))] = (float(m.group(2)), float(m.group(3)))
                    m = re.search(r"PlayerHit .*strike=(\d+) source=(\S+)", line)
                    if m:
                        hits[int(m.group(1))] = m.group(2)
                for strike, (used, at) in seen.items():
                    results[(move, strike, d, twist)] = (hits.get(strike), used, at)
                print(f"  {move} d={d:g} {'ねじる' if twist else 'なし'}: " +
                      " ".join(f"{k + 1}:{hits.get(k, '-')}" for k in sorted(seen)), flush=True)
    strikes = sorted({(m, k) for (m, k, _, _) in results}, key=lambda x: (moves.index(x[0]), x[1]))
    md = ["# 狙いのずれの効き目（c-884095）", "",
          "技を本当に出させ、正面に立ったプレイヤーに当たるかを数えた。各マスは「ねじりなし → ねじる」。",
          "○ = 刀で当たった、◇ = 飛ぶ斬撃で当たった、× = 当たらない、空 = その斬りまで行かなかった。"
          "距離は止まる距離の項目の値（括弧は、ねじる時にその斬りを振り始めた時の実際の距離 m と、ねじった角度）。", "",
          "| 技 | 斬り | " + " | ".join(f"{d:g} m" for d in distances) + " |",
          "|---|---|" + "---|" * len(distances)]
    mark = lambda r: "" if r is None else ("○" if r[0] == "sword" else "◇" if r[0] else "×")
    for move, k in strikes:
        cells = []
        for d in distances:
            off, on = results.get((move, k, d, False)), results.get((move, k, d, True))
            detail = f"（{on[2]:.1f} m・{on[1]:+.0f}°）" if on else ""
            cells.append(f"{mark(off)} → {mark(on)}{detail}")
        md.append(f"| {move} | {k + 1} | " + " | ".join(cells) + " |")
    out = WORK / "runs" / "aimcheck.md"
    out.write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(md))
    print("AIMCHECK ->", out)
    return 0


def reach(opts):
    """剣の技が当たる間合い（c-c010ef）：ボスの reachProbe で測り、技・斬りごとの表を Library/Verify/runs/reach/reach.md に書く。
    hit はプレイヤーの当たりの筒（ゲームと同じ）、tall は高さを無視した筒。tall に入るのに hit に入らない所は、
    刀が頭の上か足の下を通っている（向きは合っている）。狙いのずれの表（c-884095）があれば、そのねじりを入れた時に
    正面で当たる距離も出す。"""
    folder = WORK / "runs" / "reach"
    strikes, order = probe(opts, folder, {})
    if not order:
        print("REACH 記録が無い（ReachStrike が出ていない）。engine.log を見る:", folder)
        return 1
    aim_table = read_aim()
    aim_max = float(opts.get("aimTwistMax", 80))
    step = int(opts.get("reachProbeAngleStep", 10))
    close = float(opts.get("close", 2.5))
    md = ["# ボスの剣の技が当たる間合い（c-c010ef）", "",
          "ボスを原点・正面固定に置き、プレイヤーの体を距離 × 角度に置いて、ゲームと同じ当たりの式で測った（踏み込み・左右移動・"
          "振りながらの向き直りは入れない）。角度は正がボスの右。",
          "「正面で当たる距離」は角度 0 で当たる距離の範囲。「高さで外れる」は、高さを無視すれば当たるのに実際は当たらない所"
          "（刀が頭の上か足の下を通る）。", "",
          "「一番当たる向き」は、当たる距離の幅が一番広い角度（ボスをその分だけ回して向けば、正面のプレイヤーに当たる）。", "",
          "「ねじった時」は狙いのずれの表（Data/Boss/boss_aim_offsets.csv）の角度だけ体を回した時に、正面のプレイヤーに"
          f"当たる距離（角度が {aim_max:g} 度を超える距離はねじらない。角度は測った刻みに丸めて見る）。", "",
          f"| 技 | 斬り | 窓 (F) | 刀の高さ (m) | 正面で当たる距離 (m) | ねじった時に正面で当たる距離 (m) | "
          f"一番当たる向き：角度・距離 (m) | {close:g} m 以内で当たる角度 | 正面の高さで外れる距離 (m) |",
          "|---|---|---|---|---|---|---|---|---|"]
    detail = []
    for key in order:
        s = strikes[key]
        front = [d for d, hit, _, _, _ in s["rows"] if covers(hit, 0)]
        over = [d for d, hit, tall, _, _ in s["rows"] if covers(tall, 0) and not covers(hit, 0)]
        span = lambda xs: f"{min(xs):g}–{max(xs):g}" if xs else "なし"
        near_text = "なし"
        rows_close = [r for r in s["rows"] if r[0] <= close + 1e-6]
        if rows_close:
            near_text = " / ".join(f"{d:g}m:{h}" for d, _, _, h, _ in rows_close if h != "-") or "なし"
        # 角度ごとに当たる距離を数え、一番多い角度（同じなら 0 に近い方）。
        widest_angle, best = 0, []
        for angle in range(-180, 181, step):
            ds = [d for d, hit, _, _, _ in s["rows"] if covers(hit, angle)]
            if len(ds) > len(best) or (len(ds) == len(best) and ds and abs(angle) < abs(widest_angle)):
                widest_angle, best = angle, ds
        best_text = f"{widest_angle:+d}°・{span(best)}" if best else "なし"
        twisted = []
        for d, hit, _, _, _ in s["rows"]:
            a = aim_at(aim_table.get(key, []), d)
            a = 0.0 if abs(a) > aim_max else a
            if covers(hit, int(round(a / step)) * step):
                twisted.append(d)
        twisted_text = span(twisted) if key in aim_table else "表なし"
        md.append(f"| {key[0]} | {key[1] + 1} | {s['window']} | {s['low']:.2f}〜{s['high']:.2f} | {span(front)} | "
                  f"{twisted_text} | {best_text} | {near_text} | {span(over)} |")
        detail += ["", f"## {key[0]} の {key[1] + 1} 刀目（窓 {s['window']} F）", "", "| 距離 m | 当たる角度 | 高さを無視すれば当たる角度 |",
                   "|---|---|---|"]
        detail += [f"| {d:g} | {h} | {t} |" for d, _, _, h, t in s["rows"]]
    center = strikes[order[0]]["center"]
    md += ["", f"プレイヤーの体の中心は足元から {center:.2f} m（測った時の立ち位置）。"] + detail
    out = folder / "reach.md"
    out.write_text("\n".join(md) + "\n", encoding="utf-8")
    print("\n".join(md[:12 + len(order)]))
    print("REACH ->", out)
    return 0


def main():
    args = sys.argv[1:]
    opts = dict(a.split("=", 1) for a in args if "=" in a)
    words = [a for a in args if "=" not in a]
    if "sync" in words:
        sync(opts)
    if "build" in words and build() != 0:
        sys.exit(1)
    if "run" in words:
        name = words[words.index("run") + 1]
        caps = [int(x) for x in opts["caps"].split(",")] if "caps" in opts else []
        patch = dict(testMove=name, testHoldStill=True, testStartFrames=60, testGapFrames=90, testChain=False)
        folder = WORK / "runs" / ("move_" + re.sub(r"[^A-Za-z0-9]", "", name.encode("unicode_escape").decode())[:40])
        summarize(headless("main", int(opts.get("ticks", 1400)), caps, folder, patch, STURDY))
        print("captures ->", folder)
    if "scene" in words:
        key = words[words.index("scene") + 1]
        ticks = int(opts.get("ticks", 3000))
        caps = [int(x) for x in opts["caps"].split(",")] if "caps" in opts else []
        extra = shoot_events(ticks) if opts.get("shoot") == "1" else None
        summarize(headless(key, ticks, caps, WORK / "runs" / f"scene_{key}", None, STURDY, extra))
    if "aim" in words and aim(opts) != 0:
        sys.exit(1)
    if "aimcheck" in words and aimcheck(opts) != 0:
        sys.exit(1)
    if "reach" in words and reach(opts) != 0:
        sys.exit(1)
    if "baseline" in words:
        baseline(words[words.index("baseline") + 1], opts)
    if "compare" in words:
        i = words.index("compare")
        sys.exit(compare(words[i + 1], words[i + 2]))


if __name__ == "__main__":
    main()
