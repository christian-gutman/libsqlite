# Downloads the JNA jar used by this binding (not committed: see README).
# Usage: powershell -ExecutionPolicy Bypass -File get_jna.ps1 [version]

param([string]$Version = "5.14.0")

$ErrorActionPreference = "Stop"
$dir = Join-Path $PSScriptRoot "lib"
New-Item -ItemType Directory -Force -Path $dir | Out-Null
$url = "https://repo1.maven.org/maven2/net/java/dev/jna/jna/$Version/jna-$Version.jar"
$out = Join-Path $dir "jna-$Version.jar"
Write-Host "downloading $url"
Invoke-WebRequest -Uri $url -OutFile $out -TimeoutSec 120
Write-Host "jna $Version ready at $out ($((Get-Item $out).Length) bytes)"