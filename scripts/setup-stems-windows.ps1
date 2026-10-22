# Downloads the CPU separator once. The finished runtime is portable.
param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path $PSScriptRoot -Parent)
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}
Run $Python @('-c', 'import sys; assert sys.version_info[:2] == (3, 11), "Use Python 3.11 x64"')
Run $Python @('-m', 'venv', 'build-tools/stems-venv')
$venv = "$PWD/build-tools/stems-venv/Scripts/python.exe"
Run $venv @('-m', 'pip', 'install', 'torch==2.5.1', 'torchaudio==2.5.1', '--index-url', 'https://download.pytorch.org/whl/cpu')
Run $venv @('-m', 'pip', 'install', '-r', 'scripts/stem-requirements.txt')
$runtime = "$PWD/build-tools/stem-runtime"
Invoke-WebRequest https://www.python.org/ftp/python/3.11.9/python-3.11.9-embed-amd64.zip -OutFile build-tools/python-embed.zip
Expand-Archive build-tools/python-embed.zip -DestinationPath $runtime -Force
Set-Content "$runtime/python311._pth" "python311.zip`n.`nLib/site-packages`nimport site"
New-Item -ItemType Directory -Force "$runtime/Lib" | Out-Null
& robocopy "$PWD/build-tools/stems-venv/Lib/site-packages" "$runtime/Lib/site-packages" /E /MT:8 /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -gt 7) { throw 'Could not copy Python dependencies.' }
$env:TORCH_HOME = "$runtime/models"
Run "$runtime/python.exe" @('-c', 'from demucs.pretrained import get_model; get_model("htdemucs")')
& "$PSScriptRoot/setup-recognition-windows.ps1" -Python $venv
Run "$runtime/python.exe" @('tests/test_stem_cache.py')
Write-Host "Vocal separator ready: $runtime"
