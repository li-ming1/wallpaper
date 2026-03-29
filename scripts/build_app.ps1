param(
  [string]$BuildDir = "build",
  [switch]$UseCxx26,
  [switch]$UseCxx2c
)

$ErrorActionPreference = "Stop"

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) {
  Write-Error "g++ not found in PATH. Install MSYS2/MinGW g++ first."
}
$windres = Get-Command windres -ErrorAction SilentlyContinue
if (-not $windres) {
  $windres = Get-Command x86_64-w64-mingw32-windres -ErrorAction SilentlyContinue
}
if (-not $windres) {
  Write-Error "windres not found in PATH. Install MinGW windres first."
}

if (-not (Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

function Resolve-Cxx26Flag {
  param([string]$CompilerPath)

  $probe = Join-Path $env:TEMP "wallpaper_cxx26_probe.cpp"
  Set-Content -Path $probe -Value "int main(){return 0;}" -NoNewline
  try {
    & $CompilerPath "-std=c++26" "-x" "c++" $probe "-fsyntax-only" *> $null
    if ($LASTEXITCODE -eq 0) {
      return "-std=c++26"
    }

    & $CompilerPath "-std=c++2c" "-x" "c++" $probe "-fsyntax-only" *> $null
    if ($LASTEXITCODE -eq 0) {
      return "-std=c++2c"
    }
  } finally {
    Remove-Item -Path $probe -ErrorAction SilentlyContinue
  }

  Write-Error "Current g++ does not support -std=c++26 or -std=c++2c."
}

$output = Join-Path $BuildDir "wallpaper_app.exe"
$resourceObj = Join-Path $BuildDir "app_icon_res.o"
$sources = @(
  "src/main.cpp",
  "src/app.cpp",
  "src/platform_stubs.cpp",
  "src/win/wallpaper_host_win.cpp",
  "src/win/decode_pipeline_stub.cpp",
  "src/win/tray_controller_win.cpp",
  "src/config_store.cpp",
  "src/decode_async_read_policy.cpp",
  "src/desktop_context_policy.cpp",
  "src/desktop_attach_policy.cpp",
  "src/decode_output_policy.cpp",
  "src/frame_buffer_policy.cpp",
  "src/frame_latency_policy.cpp",
  "src/foreground_policy.cpp",
  "src/loop_sleep_policy.cpp",
  "src/long_run_load_policy.cpp",
  "src/video_path_matcher.cpp",
  "src/metrics_log_line.cpp",
  "src/metrics_log_file.cpp",
  "src/nv12_layout_policy.cpp",
  "src/pause_suspend_policy.cpp",
  "src/pause_transition_policy.cpp",
  "src/pause_resource_policy.cpp",
  "src/quality_governor.cpp",
  "src/probe_cadence_policy.cpp",
  "src/render_scheduler.cpp",
  "src/resource_arbiter.cpp",
  "src/source_frame_rate_policy.cpp",
  "src/runtime_trim_policy.cpp",
  "src/metrics_sampler.cpp",
  "src/frame_bridge.cpp",
  "src/startup_policy.cpp",
  "src/video_path_probe_policy.cpp"
)

Write-Host "Compiling Windows resources with $($windres.Source)..."
& $windres.Source "resources/app_icon.rc" "-O" "coff" "-o" $resourceObj
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$useExperimentalCxx = $UseCxx26 -or $UseCxx2c
$cppStdFlag = if ($useExperimentalCxx) { Resolve-Cxx26Flag $gxx.Source } else { "-std=c++23" }

$compileArgs = @(
  $cppStdFlag,
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
  $resourceObj,
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
