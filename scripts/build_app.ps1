param(
  [string]$BuildDir = "build",
  [switch]$UseCxx26,
  [switch]$UseCxx2c,
  [switch]$Portable
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
$coreSources = @(
  "src/config_store.cpp",
  "src/cpu_frame_buffer_pool.cpp",
  "src/cpu_frame_downscale.cpp",
  "src/desktop_context_policy.cpp",
  "src/desktop_attach_policy.cpp",
  "src/decode_output_policy.cpp",
  "src/decode_async_read_policy.cpp",
  "src/frame_buffer_policy.cpp",
  "src/foreground_policy.cpp",
  "src/loop_sleep_policy.cpp",
  "src/long_run_load_policy.cpp",
  "src/video_path_matcher.cpp",
  "src/metrics_log_line.cpp",
  "src/metrics_log_file.cpp",
  "src/monitor_rect_cache.cpp",
  "src/monitor_layout_policy.cpp",
  "src/nv12_layout_policy.cpp",
  "src/pause_suspend_policy.cpp",
  "src/pause_transition_policy.cpp",
  "src/pause_resource_policy.cpp",
  "src/quality_governor.cpp",
  "src/probe_cadence_policy.cpp",
  "src/render_scheduler.cpp",
  "src/resource_arbiter.cpp",
  "src/source_frame_rate_policy.cpp",
  "src/swap_chain_policy.cpp",
  "src/runtime_trim_policy.cpp",
  "src/frame_bridge.cpp",
  "src/startup_policy.cpp",
  "src/video_path_probe_policy.cpp",
  "src/async_file_writer.cpp",
  "src/upload_copy_policy.cpp",
  "src/upload_scale_policy.cpp",
  "src/upload_texture_policy.cpp"
)

$appSources = @(
  "src/main.cpp",
  "src/app.cpp",
  "src/app_metrics.cpp",
  "src/app_decode_control.cpp",
  "src/app_tray.cpp",
  "src/app_autostart.cpp",
  "src/win/wallpaper_host_win.cpp",
  "src/win/decode_pipeline_core.cpp",
  "src/win/decode_pipeline_mf.cpp",
  "src/win/tray_controller_win.cpp"
)

Write-Host "Compiling Windows resources with $($windres.Source)..."
& $windres.Source "resources/app_icon.rc" "-O" "coff" "-o" $resourceObj
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$useExperimentalCxx = $UseCxx26 -or $UseCxx2c
$cppStdFlag = if ($useExperimentalCxx) { Resolve-Cxx26Flag $gxx.Source } else { "-std=c++23" }

$optFlags = if ($Portable) { @("-O2", "-march=x86-64-v3") } else { @("-O3", "-march=native", "-flto") }

$compileArgs = @(
  $cppStdFlag,
  $optFlags,
  "-ffunction-sections",
  "-fdata-sections",
  "-fno-exceptions",
  "-fno-rtti",
  "-fno-asynchronous-unwind-tables",
  "-fno-unwind-tables",
  "-fomit-frame-pointer",
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-DNDEBUG",
  "-DUNICODE",
  "-D_UNICODE",
  "-DWIN32_LEAN_AND_MEAN",
  "-DNOMINMAX",
  "-mwindows",
  "-Iinclude",
  "-Wl,--gc-sections"
) + $coreSources + $appSources + @(
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

$strip = Get-Command strip -ErrorAction SilentlyContinue
if ($strip) {
  Write-Host "Stripping symbols with $($strip.Source)..."
  & $strip.Source "--strip-all" $output
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

Write-Host "Build complete: $output"
