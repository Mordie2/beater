# App-local CUDA dependencies and the full accuracy model; no driver/system changes.
param([string]$Python = "$PSScriptRoot/../build-tools/stems-venv/Scripts/python.exe")
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
function Run([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE)" }
}
$runtime = "$root/build-tools/stem-runtime"
if (!(Test-Path "$runtime/python.exe")) { throw 'Run setup-stems-windows.ps1 first.' }
# Includes the Windows CUDA model-unload fix; upgrade existing CPU runtimes too.
Run $Python @('-m', 'pip', 'install', '--no-cache-dir', '--upgrade', '--no-deps',
    '--target', "$runtime/Lib/site-packages", 'ctranslate2==4.8.3')
Run $Python @('-m', 'pip', 'install', '--no-cache-dir', '--upgrade', '--target', 'build-tools/gpu-libs',
    'nvidia-cublas-cu12==12.4.5.8', 'nvidia-cudnn-cu12==9.1.0.70',
    'nvidia-cuda-runtime-cu12==12.4.127', 'nvidia-cuda-nvrtc-cu12==12.4.127')
New-Item -ItemType Directory -Force "$runtime/gpu" | Out-Null
Get-ChildItem build-tools/gpu-libs/nvidia -Recurse -Filter '*.dll' | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination "$runtime/gpu" -Force
}
# CTranslate2 preloads its package-local cuDNN loader. Keep it on the same version.
Copy-Item "$runtime/gpu/cudnn64_9.dll" "$runtime/Lib/site-packages/ctranslate2/cudnn64_9.dll" -Force
& robocopy "$root/build-tools/gpu-libs/nvidia" "$runtime/licenses/nvidia" '*.txt' /S /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -gt 7) { throw 'Could not copy NVIDIA license notices.' }
$env:HF_HOME = "$root/build-tools/hf-cache"
$env:HF_HUB_DISABLE_XET = '1'
$env:PYTHONUTF8 = '1'
Run "$runtime/python.exe" @('scripts/setup-recognition-models.py', "$runtime/models")
Run "$runtime/python.exe" @('tests/test_gpu_recognition.py', '--runtime', $runtime)
Write-Host 'Full large-v3 recognition runtime is ready and the GPU smoke test passed.'
