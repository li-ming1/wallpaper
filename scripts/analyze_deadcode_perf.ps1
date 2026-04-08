param(
  [string]$BuildDir = "build_tmp/analysis",
  [string]$CompilerPath = "D:\\msys64\\ucrt64\\bin\\g++.exe",
  [string]$WindresPath = "D:\\msys64\\ucrt64\\bin\\windres.exe",
  [string[]]$HotFiles = @(
    "src/cpu_frame_downscale.cpp",
    "src/win/wallpaper_host_win.cpp",
    "src/win/decode_pipeline_mf.cpp"
  ),
  [switch]$SkipAnalyzer,
  [switch]$SkipGcSections,
  [switch]$SkipOptInfo
)

$ErrorActionPreference = "Stop"

function Resolve-ToolPath {
  param(
    [string]$PreferredPath,
    [string[]]$FallbackCommands
  )

  if (![string]::IsNullOrWhiteSpace($PreferredPath) -and (Test-Path $PreferredPath)) {
    return (Resolve-Path $PreferredPath).Path
  }

  foreach ($cmd in $FallbackCommands) {
    $found = Get-Command $cmd -ErrorAction SilentlyContinue
    if ($found) {
      return $found.Source
    }
  }

  throw "Tool not found. Preferred: '$PreferredPath', fallback: '$($FallbackCommands -join ", ")'."
}

function Get-QuotedItemsFromArrayBlock {
  param(
    [string[]]$Lines,
    [string]$VariableName
  )

  $items = @()
  $inBlock = $false
  $startToken = '$' + $VariableName
  foreach ($line in $Lines) {
    $trimmed = $line.Trim()
    if (-not $inBlock) {
      if ($trimmed.StartsWith($startToken) -and
          ($trimmed.Contains("=@(") -or $trimmed.Contains("= @("))) {
        $inBlock = $true
      }
      continue
    }
    if ($trimmed -eq ")") {
      break
    }
    if ($line -match '"([^"]+)"') {
      $items += $Matches[1]
    }
  }
  return $items
}

function Invoke-CaptureOutput {
  param(
    [string]$Exe,
    [string[]]$CommandArgs
  )

  $raw = & $Exe @CommandArgs 2>&1
  $exitCode = $LASTEXITCODE
  $text = @($raw | ForEach-Object { "$_" })
  return @{
    ExitCode = $exitCode
    Output = $text
  }
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $repoRoot

$compiler = Resolve-ToolPath -PreferredPath $CompilerPath -FallbackCommands @("g++")
$windres = Resolve-ToolPath -PreferredPath $WindresPath -FallbackCommands @("windres", "x86_64-w64-mingw32-windres")

if (-not (Test-Path $BuildDir)) {
  New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
}
$reportDir = Join-Path $BuildDir "reports"
if (-not (Test-Path $reportDir)) {
  New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}

$buildScriptPath = Join-Path $PSScriptRoot "build_app.ps1"
if (-not (Test-Path $buildScriptPath)) {
  throw "Missing build script: $buildScriptPath"
}
$buildScriptLines = Get-Content $buildScriptPath
$coreSources = Get-QuotedItemsFromArrayBlock -Lines $buildScriptLines -VariableName "coreSources"
$appSources = Get-QuotedItemsFromArrayBlock -Lines $buildScriptLines -VariableName "appSources"
$allSources = @($coreSources + $appSources | Select-Object -Unique)

if ($allSources.Count -eq 0) {
  throw "No source files parsed from scripts/build_app.ps1."
}

$summaryPath = Join-Path $reportDir "summary.txt"
$summary = @()
$summary += "compiler: $compiler"
$summary += "windres: $windres"
$summary += "source_count: $($allSources.Count)"
$summary += "generated_at: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

$commonWinFlags = @(
  "-DUNICODE",
  "-D_UNICODE",
  "-DWIN32_LEAN_AND_MEAN",
  "-DNOMINMAX",
  "-Iinclude"
)

$hadError = $false

if (-not $SkipAnalyzer) {
  $analyzerReport = Join-Path $reportDir "analyzer_unused.log"
  Set-Content -Path $analyzerReport -Value "== g++ static analyzer and unused checks =="

  $analyzerFlags = @(
    "-std=c++23",
    "-O3",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wunused",
    "-Wunreachable-code",
    "-fanalyzer",
    "-fsyntax-only"
  ) + $commonWinFlags

  $diagCount = 0
  $errorCount = 0
  foreach ($src in $allSources) {
    if (-not (Test-Path $src)) {
      Add-Content -Path $analyzerReport -Value ("[missing] " + $src)
      continue
    }
    $result = Invoke-CaptureOutput -Exe $compiler -CommandArgs @($analyzerFlags + @($src))
    if ($result.Output.Count -gt 0) {
      Add-Content -Path $analyzerReport -Value ("`n---- " + $src + " ----")
      $result.Output | Add-Content -Path $analyzerReport
    }

    $diagCount += ($result.Output | Select-String -Pattern "warning:|error:" | Measure-Object).Count
    if ($result.ExitCode -ne 0) {
      $errorCount += 1
    }
  }

  $summary += "analyzer_report: $analyzerReport"
  $summary += "analyzer_diagnostics: $diagCount"
  $summary += "analyzer_failed_units: $errorCount"
  if ($errorCount -gt 0) {
    $hadError = $true
  }
}

if (-not $SkipGcSections) {
  $gcLogPath = Join-Path $reportDir "gc_sections.log"
  $gcMapPath = Join-Path $reportDir "gc_sections.map"
  $gcExePath = Join-Path $BuildDir "wallpaper_app_gc.exe"
  $resourceObj = Join-Path $BuildDir "app_icon_res_gc.o"

  $resResult = Invoke-CaptureOutput -Exe $windres -CommandArgs @("resources/app_icon.rc", "-O", "coff", "-o", $resourceObj)
  if ($resResult.Output.Count -gt 0) {
    $resResult.Output | Set-Content -Path $gcLogPath
  } else {
    Set-Content -Path $gcLogPath -Value @()
  }
  if ($resResult.ExitCode -ne 0) {
    $summary += "gc_sections_report: $gcLogPath"
    $summary += "gc_sections_removed: 0"
    $summary += "gc_sections_status: windres_failed"
    $hadError = $true
  } else {
    $linkArgs = @(
      "-std=c++23",
      "-O3",
      "-march=native",
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
      "-mwindows"
    ) + $commonWinFlags + @(
      "-Wl,--gc-sections",
      "-Wl,--print-gc-sections",
      "-Wl,-Map,$gcMapPath"
    ) + $coreSources + $appSources + @(
      $resourceObj,
      "-o", $gcExePath,
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

    $linkResult = Invoke-CaptureOutput -Exe $compiler -CommandArgs $linkArgs
    if ($linkResult.Output.Count -gt 0) {
      $linkResult.Output | Add-Content -Path $gcLogPath
    }
    $removedCount =
      ($linkResult.Output | Select-String -Pattern "removing unused section" | Measure-Object).Count
    $summary += "gc_sections_report: $gcLogPath"
    $summary += "gc_sections_map: $gcMapPath"
    $summary += "gc_sections_removed: $removedCount"
    if ($linkResult.ExitCode -ne 0) {
      $summary += "gc_sections_status: link_failed"
      $hadError = $true
    } else {
      $summary += "gc_sections_status: ok"
    }
  }
}

if (-not $SkipOptInfo) {
  $optDir = Join-Path $reportDir "opt-info"
  if (-not (Test-Path $optDir)) {
    New-Item -ItemType Directory -Force -Path $optDir | Out-Null
  }

  $optSummary = Join-Path $reportDir "opt_info_summary.txt"
  Set-Content -Path $optSummary -Value "file,optimized_count,missed_count,exit_code"

  foreach ($file in $HotFiles) {
    $leaf = [System.IO.Path]::GetFileNameWithoutExtension($file)
    if ([string]::IsNullOrWhiteSpace($leaf)) {
      $leaf = "unknown"
    }
    if (-not (Test-Path $file)) {
      Add-Content -Path $optSummary -Value "$file,0,0,missing"
      continue
    }

    $objPath = Join-Path $BuildDir ("opt_" + $leaf + ".o")
    $logPath = Join-Path $optDir ($leaf + ".log")
    $optFlags = @(
      "-std=c++23",
      "-O3",
      "-march=native",
      "-fopt-info-vec-optimized",
      "-fopt-info-vec-missed",
      "-c",
      $file,
      "-o",
      $objPath
    ) + $commonWinFlags
    $result = Invoke-CaptureOutput -Exe $compiler -CommandArgs $optFlags
    $result.Output | Set-Content -Path $logPath
    $optimizedCount = ($result.Output | Select-String -Pattern "optimized:" | Measure-Object).Count
    $missedCount = ($result.Output | Select-String -Pattern "missed:" | Measure-Object).Count
    Add-Content -Path $optSummary -Value "$file,$optimizedCount,$missedCount,$($result.ExitCode)"
    if ($result.ExitCode -ne 0) {
      $hadError = $true
    }
  }

  $summary += "opt_info_summary: $optSummary"
}

$summary | Set-Content -Path $summaryPath
Write-Host "Report summary: $summaryPath"
Write-Host "Details dir: $reportDir"

if ($hadError) {
  exit 1
}
