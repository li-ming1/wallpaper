param(
  [string]$BuildDir = "build"
)

$ErrorActionPreference = "Stop"

$gxx = Get-Command g++ -ErrorAction SilentlyContinue
if (-not $gxx) {
  Write-Error "g++ not found in PATH. Install MSYS2/MinGW g++ or use CMake with MSVC toolchain."
}

if (-not (Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}

$output = Join-Path $BuildDir "wallpaper_tests.exe"
$sources = @(
  "tests/test_main.cpp",
  "tests/config_store_tests.cpp",
  "tests/desktop_attach_policy_tests.cpp",
  "tests/frame_buffer_policy_tests.cpp",
  "tests/frame_bridge_tests.cpp",
  "tests/foreground_policy_tests.cpp",
  "tests/loop_sleep_policy_tests.cpp",
  "tests/metrics_log_line_tests.cpp",
  "tests/metrics_log_file_tests.cpp",
  "tests/pause_resource_policy_tests.cpp",
  "tests/pause_transition_policy_tests.cpp",
  "tests/quality_governor_tests.cpp",
  "tests/probe_cadence_policy_tests.cpp",
  "tests/render_scheduler_tests.cpp",
  "tests/resource_arbiter_tests.cpp",
  "tests/startup_policy_tests.cpp",
  "tests/video_path_matcher_tests.cpp",
  "src/config_store.cpp",
  "src/desktop_attach_policy.cpp",
  "src/frame_buffer_policy.cpp",
  "src/frame_bridge.cpp",
  "src/foreground_policy.cpp",
  "src/loop_sleep_policy.cpp",
  "src/metrics_log_line.cpp",
  "src/metrics_log_file.cpp",
  "src/pause_transition_policy.cpp",
  "src/pause_resource_policy.cpp",
  "src/quality_governor.cpp",
  "src/probe_cadence_policy.cpp",
  "src/render_scheduler.cpp",
  "src/resource_arbiter.cpp",
  "src/startup_policy.cpp",
  "src/video_path_matcher.cpp"
)

$compileArgs = @(
  "-std=c++20",
  "-Wall",
  "-Wextra",
  "-Wpedantic",
  "-Iinclude"
) + $sources + @("-o", $output)

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
