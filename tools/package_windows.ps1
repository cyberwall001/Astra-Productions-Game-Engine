<#
.SYNOPSIS
    Builds the downloadable Astra Studio zip for Windows x64.

.DESCRIPTION
    Expects a finished build (default: build/windows-msvc, Release). Produces
    dist/astra-studio-<version>-windows-x64.zip containing:

        AstraStudio.exe        the launcher + editor
        AstraKeyForge.exe      owner-only access code tool
        assets/                fonts, branding, demo workspace
        docs/                  the docs shipped with the build
        README.txt             how to start

.EXAMPLE
    pwsh tools/package_windows.ps1
    pwsh tools/package_windows.ps1 -BuildDir build/windows-msvc -Configuration Release
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build/windows-msvc",
    [string]$Configuration = "Release",
    [string]$OutputDir = "dist",
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $Version) {
    $cmake = Get-Content CMakeLists.txt -Raw
    if ($cmake -match "VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)") { $Version = $Matches[1] } else { $Version = "0.0.0" }
}

$binDir = Join-Path $BuildDir $Configuration
$exe = Join-Path $binDir "AstraStudio.exe"
$keyForge = Join-Path $binDir "AstraKeyForge.exe"

if (-not (Test-Path $exe)) {
    throw "AstraStudio.exe not found at $exe. Run: cmake --build --preset windows-msvc-release"
}

$staging = Join-Path $OutputDir "astra-studio-$Version-windows-x64"
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Path $staging | Out-Null
New-Item -ItemType Directory -Path (Join-Path $staging "docs") | Out-Null

Copy-Item $exe $staging
if (Test-Path $keyForge) { Copy-Item $keyForge $staging }
Copy-Item assets -Destination $staging -Recurse
Copy-Item LICENSE $staging
Copy-Item (Join-Path "docs" "*.md") -Destination (Join-Path $staging "docs") -ErrorAction SilentlyContinue

$startHere = @"
Astra Studio $Version - Windows x64
=====================================

1. Run AstraStudio.exe.
2. First launch shows the boot sequence, then the sign-in screen.
3. Create an account with an access code, or pick "Continue in demo mode" if
   this build was packaged with the demo workspace.
4. The project hub lists the GitHub repositories your account can access.
   Without a token it shows the bundled demo workspace; add a token under
   Settings > GitHub.

AstraKeyForge.exe is the owner-only tool that mints access codes. Do not ship
it to players or contractors.

Requires Windows 10 1809+ / Windows 11 and a GPU with OpenGL 3.2 support.
Settings, accounts and projects live in %LOCALAPPDATA%\Astra Productions.

Docs: docs/BUILD_WINDOWS.md, docs/SECURITY.md, docs/UI_GUIDE.md,
      docs/ARCHITECTURE.md
"@
Set-Content -Path (Join-Path $staging "README.txt") -Value $startHere -Encoding UTF8

$zip = Join-Path $OutputDir "astra-studio-$Version-windows-x64.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $staging "*") -DestinationPath $zip -CompressionLevel Optimal

Write-Host ""
Write-Host "Packaged $zip" -ForegroundColor Green
Write-Host ("Size: {0:N1} MB" -f ((Get-Item $zip).Length / 1MB))
