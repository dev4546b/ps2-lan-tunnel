param(
  [string]$Configuration = "Release",
  [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
  $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
} else {
  $vsPath = $null
}

if (-not $vsPath) {
  Write-Error "Visual Studio com ferramentas C++ (MSVC) nao encontrado via vswhere. Instale o 'Desktop development with C++' workload."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
  Write-Error "vcvars64.bat nao encontrado em: $vcvars"
}

$genArgs = ""
if ($Generator) { $genArgs = "-G `"$Generator`"" }

$buildDir = Join-Path $root "build"

$cmd = "`"$vcvars`" >nul && cmake -S `"$root`" -B `"$buildDir`" $genArgs && cmake --build `"$buildDir`" --config $Configuration"
Write-Host "==> $cmd"
cmd /c $cmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$exe = Join-Path $buildDir "$Configuration\ps2lan.exe"
if (Test-Path $exe) {
  Write-Host ""
  Write-Host "Build OK: $exe"
  Write-Host "Para executar: `"$exe`""
}