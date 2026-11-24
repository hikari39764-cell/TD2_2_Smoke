<#
ボスの骨格の体のクリップの長さを、模型のファイル（glTF）から表へ書き出す（c-7b1a08）。
エンジンにはクリップの長さを前もって読む機能が無いので、技の長さをクリップに合わせるには、この表が要る。
モーションを入れ替えたら 1 回流す。

使い方（リポジトリの一番上で）:
  powershell -ExecutionPolicy Bypass -File Tools/BossAnim/export_clip_lengths.ps1
  powershell -ExecutionPolicy Bypass -File Tools/BossAnim/export_clip_lengths.ps1 -Model Assets/Models/BossIntroActor.gltf -Out Data/Boss/boss_anim_clips.csv

表の列：クリップ（glTF の animations の name）, 長さ F（60 F＝1 秒。四捨五入）, 長さ 秒
#>
param(
    [string]$Model = "Assets/Models/BossIntroActor.gltf",
    [string]$Out = "Data/Boss/boss_anim_clips.csv",
    [double]$FramesPerSecond = 60.0
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$modelPath = Join-Path $root $Model
$outPath = Join-Path $root $Out

$gltf = [System.IO.File]::ReadAllText($modelPath, [System.Text.Encoding]::UTF8) | ConvertFrom-Json
if ($null -eq $gltf.animations) {
    throw "模型 $Model にクリップ（animations）がありません"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("クリップ,長さ F,長さ 秒")
foreach ($animation in $gltf.animations) {
    # クリップの長さ＝どの軌道の時刻（sampler の input の accessor の max）よりも遅い最後の時刻。
    $seconds = 0.0
    foreach ($sampler in $animation.samplers) {
        $accessor = $gltf.accessors[[int]$sampler.input]
        if ($null -ne $accessor.max -and [double]$accessor.max[0] -gt $seconds) {
            $seconds = [double]$accessor.max[0]
        }
    }
    $frames = [int][Math]::Round($seconds * $FramesPerSecond, [MidpointRounding]::AwayFromZero)
    $name = [string]$animation.name
    if ($name.Contains(",") -or $name.Contains('"')) {
        $name = '"' + $name.Replace('"', '""') + '"'
    }
    $lines.Add(("{0},{1},{2}" -f $name, $frames, $seconds.ToString("0.###", [System.Globalization.CultureInfo]::InvariantCulture)))
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($outPath, ($lines -join "`r`n") + "`r`n", $utf8)
Write-Host ("{0} 本のクリップの長さを {1} に書き出しました" -f ($lines.Count - 1), $Out)
