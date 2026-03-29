param(
  [string]$ExePath = "build_tmp/wallpaper_app.exe",
  [ValidateSet("startup", "desktop", "fullscreen")]
  [string]$Scenario = "startup",
  [int]$DurationSec = 30,
  [int]$SampleMs = 500,
  [int]$WarmupSec = 10,
  [string]$OutputDir = "build_tmp/bench",
  [string]$Tag = ""
)

$ErrorActionPreference = "Stop"

if ($DurationSec -le 0) {
  Write-Error "DurationSec must be > 0."
}
if ($SampleMs -le 0) {
  Write-Error "SampleMs must be > 0."
}

$resolvedExe = Resolve-Path -Path $ExePath -ErrorAction SilentlyContinue
if (-not $resolvedExe) {
  Write-Error "Executable not found: $ExePath"
}
$exe = $resolvedExe.Path

if (-not (Test-Path $OutputDir)) {
  New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
}

$cpuCount = [Environment]::ProcessorCount
if ($cpuCount -le 0) {
  $cpuCount = 1
}

$scenarioWarmupSec = if ($Scenario -eq "startup") { 0 } else { [Math]::Max(0, $WarmupSec) }
$totalDurationSec = $scenarioWarmupSec + $DurationSec
$startStamp = Get-Date -Format "yyyyMMdd_HHmmss"
$tagSuffix = if ([string]::IsNullOrWhiteSpace($Tag)) { "" } else { "_$Tag" }
$filePrefix = "$($Scenario)_$startStamp$tagSuffix"
$csvPath = Join-Path $OutputDir "$filePrefix.csv"
$jsonPath = Join-Path $OutputDir "$filePrefix.json"

Write-Host "Launching: $exe"
$proc = Start-Process -FilePath $exe -PassThru

if ($Scenario -eq "fullscreen") {
  Write-Host "Warmup window: switch to fullscreen app now, then keep it stable."
} elseif ($Scenario -eq "desktop") {
  Write-Host "Warmup window: keep only desktop visible (no fullscreen app)."
}

$samples = New-Object System.Collections.Generic.List[object]
$loopStart = Get-Date

$prevSampleAt = Get-Date
$prevCpuTime = [TimeSpan]::Zero

Start-Sleep -Milliseconds $SampleMs
try {
  $warmupDone = $false
  while ($true) {
    Start-Sleep -Milliseconds $SampleMs
    $now = Get-Date

    $elapsedSec = ($now - $loopStart).TotalSeconds
    if ($elapsedSec -ge $totalDurationSec) {
      break
    }

    $current = Get-Process -Id $proc.Id -ErrorAction SilentlyContinue
    if (-not $current) {
      break
    }

    $cpuDeltaMs = ($current.TotalProcessorTime - $prevCpuTime).TotalMilliseconds
    $wallDeltaMs = ($now - $prevSampleAt).TotalMilliseconds
    $cpuPercent = 0.0
    if ($wallDeltaMs -gt 0) {
      $cpuPercent = ($cpuDeltaMs / $wallDeltaMs) * 100.0 / $cpuCount
      if ($cpuPercent -lt 0) { $cpuPercent = 0.0 }
    }

    $prevSampleAt = $now
    $prevCpuTime = $current.TotalProcessorTime

    if (-not $warmupDone -and $elapsedSec -ge $scenarioWarmupSec) {
      $warmupDone = $true
    }
    if (-not $warmupDone) {
      continue
    }

    $samples.Add([PSCustomObject]@{
      timestamp_utc = $now.ToUniversalTime().ToString("o")
      elapsed_ms = [Math]::Round(($elapsedSec - $scenarioWarmupSec) * 1000.0)
      cpu_percent = [Math]::Round($cpuPercent, 4)
      private_bytes = [int64]$current.PrivateMemorySize64
      working_set_bytes = [int64]$current.WorkingSet64
    }) | Out-Null
  }
} finally {
  Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}

$samples | Export-Csv -Path $csvPath -NoTypeInformation -Encoding UTF8

if ($samples.Count -eq 0) {
  $summary = [PSCustomObject]@{
    scenario = $Scenario
    sample_count = 0
    message = "No samples collected."
    raw_csv = $csvPath
  }
  $summary | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath -Encoding UTF8
  Write-Host ($summary | ConvertTo-Json -Depth 5)
  exit 0
}

$cpuValues = @($samples | ForEach-Object { [double]$_.cpu_percent })
$privateValues = @($samples | ForEach-Object { [double]$_.private_bytes })
$wsValues = @($samples | ForEach-Object { [double]$_.working_set_bytes })
$sortedCpu = @($cpuValues | Sort-Object)
$p95Index = [Math]::Floor(($sortedCpu.Count - 1) * 0.95)

$summary = [PSCustomObject]@{
  scenario = $Scenario
  sample_count = $samples.Count
  sample_interval_ms = $SampleMs
  duration_sec = $DurationSec
  warmup_sec = $scenarioWarmupSec
  cpu_avg_percent = [Math]::Round((($cpuValues | Measure-Object -Average).Average), 4)
  cpu_p95_percent = [Math]::Round($sortedCpu[$p95Index], 4)
  private_bytes_min = [int64](($privateValues | Measure-Object -Minimum).Minimum)
  private_bytes_max = [int64](($privateValues | Measure-Object -Maximum).Maximum)
  private_bytes_delta = [int64]($privateValues[-1] - $privateValues[0])
  working_set_bytes_min = [int64](($wsValues | Measure-Object -Minimum).Minimum)
  working_set_bytes_max = [int64](($wsValues | Measure-Object -Maximum).Maximum)
  working_set_bytes_delta = [int64]($wsValues[-1] - $wsValues[0])
  raw_csv = $csvPath
}

$summary | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath -Encoding UTF8
Write-Host "Summary JSON: $jsonPath"
Write-Host ($summary | ConvertTo-Json -Depth 5)
