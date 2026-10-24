$ErrorActionPreference = 'Stop'
$destination = Join-Path $env:LOCALAPPDATA 'Programs\E4 Companion'
$plugins = Join-Path $env:LOCALAPPDATA 'Programs\Common\VST3'
New-Item -ItemType Directory -Force $destination, $plugins | Out-Null
$plugin = Join-Path $plugins 'E-4 Companion.vst3'
if (Test-Path $plugin) { Move-Item $plugin ($plugin + '.backup-' + (Get-Date -Format yyyyMMdd-HHmmss)) }
Copy-Item (Join-Path $PSScriptRoot 'VST3\E-4 Companion.vst3') $plugins -Recurse
foreach ($folder in 'Standalone','Licences','Sources','Studio-test-kit') {
    Copy-Item (Join-Path $PSScriptRoot $folder) $destination -Recurse -Force
}
Write-Host "Installed. Launch $destination\Standalone\E-4 Companion.exe"
