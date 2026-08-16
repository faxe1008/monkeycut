# Build a portable Windows bundle (run on Windows, after cmake --build build --config Release).
# Usage: powershell -ExecutionPolicy Bypass -File package\package-windows.ps1 [build-dir]
param([string]$BuildDir = ".\build\Release")

$ErrorActionPreference = "Stop"
$app = Join-Path $BuildDir "monkeycut.exe"
if (-not (Test-Path $app)) { throw "app not found at $app (build first)" }

$out = "monkeycut-win"
$stage = Join-Path $env:TEMP $out
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory $stage | Out-Null

Copy-Item $app $stage
if (Test-Path (Join-Path $BuildDir "translations")) {
    New-Item -ItemType Directory (Join-Path $stage translations) | Out-Null
    Copy-Item (Join-Path $BuildDir "translations\*.qm") (Join-Path $stage "translations") -ErrorAction SilentlyContinue
}

# Qt runtime (platform plugins, etc.)
$windeployqt = Get-Command windeployqt -ErrorAction SilentlyContinue
if (-not $windeployqt) { throw "windeployqt not on PATH - add the Qt bin directory" }
& $windeployqt.Source --no-translations --no-system-d3d-compiler --no-opengl-sw `
    (Join-Path $stage "monkeycut.exe") | Out-Null

# FFmpeg: place the libav*-61.dll / libsw*-7.dll / libavutil-*.dll set next to the exe
# (they must match the FFmpeg the app was linked against).
# ffmpegDlls = @("avcodec-61.dll","avformat-61.dll","avutil-59.dll",
#                "swscale-8.dll","swresample-5.dll")
# foreach ($d in $ffmpegDlls) { Copy-Item (Join-Path <ffmpeg bin dir> $d) $stage }

Compress-Archive -Path (Join-Path $stage "*") -DestinationPath "$out.zip" -Force
Remove-Item $stage -Recurse -Force
Write-Host "written: $out.zip"