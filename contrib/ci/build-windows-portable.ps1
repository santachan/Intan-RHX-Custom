[CmdletBinding()]
param(
    [string]$BuildDirectory = "",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $repoRoot "build-windows"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "result"
}

$packageName = "IntanRHX-port-a-d-column-Windows-x64"
$packageDirectory = Join-Path $OutputDirectory $packageName
$zipPath = Join-Path $OutputDirectory "$packageName.zip"

foreach ($requiredCommand in @("qmake", "nmake", "windeployqt")) {
    if (-not (Get-Command $requiredCommand -ErrorAction SilentlyContinue)) {
        throw "Required build command was not found: $requiredCommand"
    }
}

New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

Push-Location $BuildDirectory
try {
    & qmake (Join-Path $repoRoot "IntanRHX.pro") "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) {
        throw "qmake failed with exit code $LASTEXITCODE"
    }

    & nmake
    if ($LASTEXITCODE -ne 0) {
        throw "nmake failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

$executableCandidates = @(
    (Join-Path $BuildDirectory "release/IntanRHX.exe"),
    (Join-Path $BuildDirectory "IntanRHX.exe")
)
$executablePath = $executableCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $executablePath) {
    throw "The Windows build completed without producing IntanRHX.exe"
}

if (Test-Path $packageDirectory) {
    Remove-Item -Recurse -Force $packageDirectory
}
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
New-Item -ItemType Directory -Force -Path $packageDirectory | Out-Null

Copy-Item $executablePath (Join-Path $packageDirectory "IntanRHX.exe")
Copy-Item (Join-Path $repoRoot "libraries/Windows/okFrontPanel.dll") $packageDirectory
Copy-Item (Join-Path $repoRoot "kernel.cl") $packageDirectory
Copy-Item (Join-Path $repoRoot "startup.ini") $packageDirectory
Copy-Item (Join-Path $repoRoot "FPGA-bitfiles/*.bit") $packageDirectory

& windeployqt --release --no-translations --compiler-runtime (Join-Path $packageDirectory "IntanRHX.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$readme = @"
Intan RHX Custom - Windows x64 portable build
================================================

Run IntanRHX.exe from this folder. Keep all files and subfolders together.

Included custom features:
- Spike Scope docking and safe-close fixes
- Low-latency TCP output setting from the parent branch
- Optional single-column views for Ports A-B, A-C, and A-D

Hardware requirements:
- Intan/Opal Kelly USB drivers must be installed once on each Windows PC.
- Microsoft Visual C++ x64 redistributables required by the controller drivers
  must also be installed.
- Administrator permission may be required for driver installation, but not
  for copying or launching this portable application folder afterward.

Official setup information:
https://intantech.com/downloads.html

Build commit: $env:GITHUB_SHA
"@
Set-Content -Path (Join-Path $packageDirectory "README-Windows-portable.txt") -Value $readme -Encoding UTF8

Compress-Archive -Path (Join-Path $packageDirectory "*") -DestinationPath $zipPath -CompressionLevel Optimal
Write-Host "Portable Windows package created: $zipPath"

