param(
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) {
  Write-Error "g++ not found in PATH. Install MSYS2/MinGW g++ first."
}

if (-not (Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

$output = Join-Path $BuildDir "wallpaper_app.exe"
$sources = @(
  "src/main.cpp",
  "src/app.cpp",
  "src/platform_stubs.cpp",
  "src/win/wallpaper_host_win.cpp",
  "src/win/decode_pipeline_stub.cpp",
  "src/win/tray_controller_win.cpp",
  "src/config_store.cpp",
  "src/desktop_attach_policy.cpp",
  "src/frame_buffer_policy.cpp",
  "src/foreground_policy.cpp",
  "src/loop_sleep_policy.cpp",
  "src/video_path_matcher.cpp",
  "src/metrics_log_line.cpp",
  "src/metrics_log_file.cpp",
  "src/pause_transition_policy.cpp",
  "src/pause_resource_policy.cpp",
  "src/quality_governor.cpp",
  "src/probe_cadence_policy.cpp",
  "src/render_scheduler.cpp",
  "src/resource_arbiter.cpp",
  "src/metrics_sampler.cpp",
  "src/frame_bridge.cpp",
  "src/startup_policy.cpp"
)

$compileArgs = @(
  "-std=c++20",
  "-O2",
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-DUNICODE",
  "-D_UNICODE",
  "-DWIN32_LEAN_AND_MEAN",
  "-DNOMINMAX",
  "-mwindows",
  "-Iinclude"
) + $sources + @(
  "-o", $output,
  "-lole32",
  "-lmfplat",
  "-lmfreadwrite",
  "-lmfuuid",
  "-ld3d11",
  "-ldxgi",
  "-ld3dcompiler",
  "-ldwmapi",
  "-luser32",
  "-lshell32",
  "-ladvapi32",
  "-lcomdlg32",
  "-lpsapi",
  "-lwinmm"
)

Write-Host "Building wallpaper_app.exe (Windows subsystem)..."
& $gxx.Source @compileArgs
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Build complete: $output"
