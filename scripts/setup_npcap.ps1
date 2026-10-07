param(
  [string]$Version = "1.13"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$dest = Join-Path $root "deps\npcap-sdk"
$zip = Join-Path $root "deps\npcap-sdk-$Version.zip"

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $dest) | Out-Null

$url = "https://npcap.com/dist/npcap-sdk-$Version.zip"
Write-Host "Baixando SDK do Npcap $Version de $url"
try {
  Invoke-WebRequest -Uri $url -OutFile $zip
} catch {
  Write-Warning "Falha ao baixar $url. Se a versao nao existir mais, liste as disponiveis em https://npcap.com/#download e passe -Version."
  throw
}

if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
Expand-Archive -Path $zip -DestinationPath $dest -Force

$sdkDir = Join-Path $dest "Npcap SDK"
if (Test-Path $sdkDir) { $dest = $sdkDir }

[Environment]::SetEnvironmentVariable("NPcapSDK", $dest, "User")
$env:NPcapSDK = $dest

Write-Host ""
Write-Host "SDK do Npcap instalado em: $dest"
Write-Host "Variavel de ambiente NPcapSDK definida (usuario)."
Write-Host ""
Write-Host "Lembre-se de instalar o driver de runtime do Npcap: https://npcap.com/#download"
Write-Host "O executavel precisa de wpcap.dll (C:\Windows\System32\Npcap\wpcap.dll) em tempo de execucao."