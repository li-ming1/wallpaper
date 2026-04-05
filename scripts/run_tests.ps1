param(
  [string]$BuildDir = "build",
  [switch]$UseCxx26,
  [switch]$UseCxx2c,
  [switch]$Portable
)

$ErrorActionPreference = "Stop"

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) {
  Write-Error "g++ not found in PATH. Install MSYS2/MinGW g++ or use CMake with MSVC toolchain."
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

$output = Join-Path $BuildDir "wallpaper_tests.exe"

$testSources = @(
  "tests/test_main.cpp",
  "tests/config_store_tests.cpp",
  "tests/cpu_frame_downscale_tests.cpp",
  "tests/cpp26_feature_support_tests.cpp",
  "tests/decode_async_read_policy_tests.cpp",
  "tests/desktop_context_policy_tests.cpp",
  "tests/desktop_attach_policy_tests.cpp",
  "tests/decode_output_policy_tests.cpp",
  "tests/async_file_writer_tests.cpp",
  "tests/frame_buffer_policy_tests.cpp",
  "tests/frame_latency_policy_tests.cpp",
  "tests/frame_bridge_tests.cpp",
  "tests/foreground_policy_tests.cpp",
  "tests/loop_sleep_policy_tests.cpp",
  "tests/long_run_load_policy_tests.cpp",
  "tests/metrics_sampler_tests.cpp",
  "tests/metrics_log_line_tests.cpp",
  "tests/metrics_log_file_tests.cpp",
  "tests/monitor_layout_policy_tests.cpp",
  "tests/nv12_layout_policy_tests.cpp",
  "tests/pause_resource_policy_tests.cpp",
  "tests/pause_suspend_policy_tests.cpp",
  "tests/pause_transition_policy_tests.cpp",
  "tests/quality_governor_tests.cpp",
  "tests/probe_cadence_policy_tests.cpp",
  "tests/render_scheduler_tests.cpp",
  "tests/resource_arbiter_tests.cpp",
  "tests/source_frame_rate_policy_tests.cpp",
  "tests/swap_chain_policy_tests.cpp",
  "tests/runtime_trim_policy_tests.cpp",
  "tests/single_instance_policy_tests.cpp",
  "tests/startup_policy_tests.cpp",
  "tests/tray_thread_stop_policy_tests.cpp",
  "tests/upload_copy_policy_tests.cpp",
  "tests/upload_scale_policy_tests.cpp",
  "tests/upload_texture_policy_tests.cpp",
  "tests/video_path_probe_policy_tests.cpp",
  "tests/video_path_matcher_tests.cpp"
)

$coreSources = @(
  "src/config_store.cpp",
  "src/cpu_frame_downscale.cpp",
  "src/decode_async_read_policy.cpp",
  "src/desktop_context_policy.cpp",
  "src/desktop_attach_policy.cpp",
  "src/decode_output_policy.cpp",
  "src/app_autostart.cpp",
  "src/frame_buffer_policy.cpp",
  "src/frame_latency_policy.cpp",
  "src/frame_bridge.cpp",
  "src/foreground_policy.cpp",
  "src/loop_sleep_policy.cpp",
  "src/long_run_load_policy.cpp",
  "src/metrics_sampler.cpp",
  "src/metrics_log_line.cpp",
  "src/metrics_log_file.cpp",
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
  "src/startup_policy.cpp",
  "src/async_file_writer.cpp",
  "src/upload_copy_policy.cpp",
  "src/upload_scale_policy.cpp",
  "src/upload_texture_policy.cpp",
  "src/video_path_probe_policy.cpp",
  "src/video_path_matcher.cpp"
)

$useExperimentalCxx = $UseCxx26 -or $UseCxx2c
$cppStdFlag = if ($useExperimentalCxx) { Resolve-Cxx26Flag $gxx.Source } else { "-std=c++23" }
$optFlags = if ($Portable) { @("-O2", "-march=x86-64-v3") } else { @("-O3", "-march=native", "-flto") }

$compileArgs = @(
  $cppStdFlag,
  $optFlags,
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-DNDEBUG",
  "-Iinclude",
  "-Wl,--gc-sections"
) + $testSources + $coreSources + @("-o", $output)

Write-Host "Compiling tests with $($gxx.Source)..."
& $gxx.Source @compileArgs
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "Running tests..."
& $output
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "All tests passed."

