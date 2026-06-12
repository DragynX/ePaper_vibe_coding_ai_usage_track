# build.ps1 — compile the reTerminal E1001 firmware with arduino-cli and archive
# the full flashable image into builds/ as E1001-AIUsageMonitor.v.<version>.bin.
#
# Usage:  powershell -ExecutionPolicy Bypass -File .\build.ps1
$ErrorActionPreference = 'Stop'

$repo = $PSScriptRoot
$fqbn = 'esp32:esp32:XIAO_ESP32S3:PSRAM=opi'
$libRoot = 'C:\esp\libraries\libraries'
$out = Join-Path $repo 'build_tmp'

# Version comes from ProjectConfig.h (#define UM_VERSION "x.y.z").
$m = Select-String -Path (Join-Path $repo 'ProjectConfig.h') `
                   -Pattern 'define\s+UM_VERSION\s+"([0-9][0-9.]*)"'
if (-not $m) { throw 'UM_VERSION not found in ProjectConfig.h' }
$ver = $m.Matches[0].Groups[1].Value
Write-Host "Building UsageMonitor v$ver for E1001..."

arduino-cli compile --fqbn $fqbn `
  --library "$libRoot\Seeed_GFX" `
  --library "$libRoot\EspAppLog" `
  --library "$libRoot\DragynESPAsyncWiFiManager" `
  --library "$libRoot\OpenFontRender" `
  --output-dir $out `
  $repo
if ($LASTEXITCODE -ne 0) { throw "compile failed ($LASTEXITCODE)" }

# The *.merged.bin is the full image (bootloader + partitions + app); flashing it
# at 0x0 FULL-ERASES the chip incl. NVS (WiFi creds + tokens + settings). Use it
# only for a deliberate full restore.
$merged = Get-ChildItem -Path $out -Filter '*.merged.bin' | Select-Object -First 1
if (-not $merged) { throw "merged .bin not found in $out" }

# The app-only image (*.ino.bin, NOT *.merged/bootloader/partitions) flashes at
# 0x10000 and leaves NVS intact — this is the default reflash (firmware only).
$app = Get-ChildItem -Path $out -Filter '*.ino.bin' |
       Where-Object { $_.Name -notmatch '\.(merged|bootloader|partitions)\.bin$' } |
       Select-Object -First 1
if (-not $app) { throw "app .ino.bin not found in $out" }

$builds = Join-Path $repo 'builds'
if (-not (Test-Path $builds)) { New-Item -ItemType Directory -Path $builds | Out-Null }
$dest    = Join-Path $builds "E1001-AIUsageMonitor.v.$ver.bin"
$destApp = Join-Path $builds "E1001-AIUsageMonitor.app.v.$ver.bin"
Copy-Item $merged.FullName $dest -Force
Copy-Item $app.FullName    $destApp -Force
Remove-Item $out -Recurse -Force
Write-Host "Archived full binary -> $dest"
Write-Host "Archived app binary  -> $destApp  (flash at 0x10000, preserves NVS)"
