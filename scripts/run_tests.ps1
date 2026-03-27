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
  "tests/render_scheduler_tests.cpp",
  "tests/resource_arbiter_tests.cpp",
  "src/config_store.cpp",
  "src/render_scheduler.cpp",
  "src/resource_arbiter.cpp"
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

Write-Host "Running tests..."
& $output
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

Write-Host "All tests passed."
