# Compiles and runs the canonical example. Usage:
#   powershell -ExecutionPolicy Bypass -File run.ps1 [path-to-libsqlite3]
# The JNA jar is fetched on demand (see get_jna.ps1).

param([string]$LibPath = "")

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $here

$jar = Get-ChildItem "lib\jna-*.jar" | Select-Object -First 1
if (-not $jar) {
    & (Join-Path $here "get_jna.ps1")
    $jar = Get-ChildItem "lib\jna-*.jar" | Select-Object -First 1
}
if (-not $jar) { throw "JNA jar not available" }

Write-Host "javac with $($jar.Name)"
& javac -encoding UTF-8 -cp $jar.FullName -d classes LibSqlite.java ExampleFull.java
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($LibPath -eq "") {
    $LibPath = "..\..\bin\x64-Windows\shared\Release\libsqlite3.dll"
}
& java -cp "classes;$($jar.FullName)" ExampleFull $LibPath
exit $LASTEXITCODE