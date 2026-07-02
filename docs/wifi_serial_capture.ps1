# Passive ESP32-S3 serial capture for the AI-Usage-Monitor (COM4) with reopen-on-drop.
# No DTR/RTS assert => opening the port will NOT reset the board (lets us watch a
# reboot the USER triggers). Survives native-USB-CDC re-enumeration across resets.
# Adapted from C:\esp\DragynWeather\docs\fw_serial_capture.ps1.
# Stops when the STOP file appears or MaxSeconds elapses.
param(
  [string]$PreferPort = "COM4",
  [string]$LogPath  = "C:\esp\ePaper_vibe_coding_ai_usage_track\docs\wifi_capture.txt",
  [string]$StopFile = "C:\esp\ePaper_vibe_coding_ai_usage_track\docs\wifi_capture.STOP",
  [int]$MaxSeconds  = 600
)

if (Test-Path $StopFile) { Remove-Item $StopFile -Force }
"=== capture start $(Get-Date -Format 'dd/MM/yyyy HH:mm:ss')  prefer=$PreferPort ===" | Out-File -FilePath $LogPath -Encoding utf8

function Get-EspPort {
  param([string]$Prefer)
  $ports = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
  if (-not $ports) { return $null }
  # 1) if the preferred port is present, use it
  if ($ports -contains $Prefer) { return $Prefer }
  # 2) else the highest-numbered port that isn't a known non-ESP bridge (COM5/COM8)
  $pref = @($ports | Where-Object { $_ -notin @('COM5','COM8') })
  if ($pref.Count -gt 0) { return $pref[-1] }
  return @($ports)[-1]
}

$deadline = (Get-Date).AddSeconds($MaxSeconds)
$curPort = $null
$sp = $null

while ((Get-Date) -lt $deadline) {
  if (Test-Path $StopFile) { break }
  if ($null -eq $sp -or -not $sp.IsOpen) {
    $p = Get-EspPort -Prefer $PreferPort
    if ($null -eq $p) { Start-Sleep -Milliseconds 500; continue }
    try {
      $sp = New-Object System.IO.Ports.SerialPort($p, 115200, 'None', 8, 'One')
      $sp.DtrEnable = $false
      $sp.RtsEnable = $false
      $sp.ReadTimeout = 800
      $sp.NewLine = "`n"
      $sp.Open()
      $curPort = $p
      $stamp = Get-Date -Format 'HH:mm:ss'
      "[$stamp] <<< opened $p >>>" | Out-File -FilePath $LogPath -Append -Encoding utf8
    } catch {
      if ($sp) { try { $sp.Dispose() } catch {} ; $sp = $null }
      Start-Sleep -Milliseconds 600
      continue
    }
  }
  try {
    $line = $sp.ReadLine()
    $stamp = Get-Date -Format 'HH:mm:ss.fff'
    "[$stamp] $line" | Out-File -FilePath $LogPath -Append -Encoding utf8
  } catch [System.TimeoutException] {
    # no data this window; loop
  } catch {
    # port dropped (reboot / re-enumerate) -> close and re-detect
    $stamp = Get-Date -Format 'HH:mm:ss'
    "[$stamp] <<< port $curPort dropped: $($_.Exception.GetType().Name) >>>" | Out-File -FilePath $LogPath -Append -Encoding utf8
    try { $sp.Close() } catch {}
    try { $sp.Dispose() } catch {}
    $sp = $null
    Start-Sleep -Milliseconds 600
  }
}

if ($sp) { try { $sp.Close() } catch {} ; try { $sp.Dispose() } catch {} }
"=== capture end $(Get-Date -Format 'dd/MM/yyyy HH:mm:ss') ===" | Out-File -FilePath $LogPath -Append -Encoding utf8
