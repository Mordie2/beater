param(
    [string]$QtRoot,
    [string]$CMake = 'cmake',
    [string]$BuildDir = 'build-windows',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (!$QtRoot) { $QtRoot = Join-Path $root 'build-tools/Qt/6.9.3/msvc2022_64' }
if (!(Test-Path "$QtRoot/bin/windeployqt.exe")) { throw 'Pass -QtRoot with a Qt 6.9+ MSVC 2022 x64 installation.' }
if ($CMake -eq 'cmake' -and (Test-Path 'build-tools/python/cmake/data/bin/cmake.exe')) {
    $CMake = Join-Path $root 'build-tools/python/cmake/data/bin/cmake.exe'
}
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
if (!$SkipBuild) {
    Invoke-Checked $CMake @('-S', '.', '-B', $BuildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_PREFIX_PATH=$QtRoot")
    Invoke-Checked $CMake @('--build', $BuildDir, '--config', 'Release', '--parallel', '8')
}
Invoke-Checked "$BuildDir/Release/engine_test.exe" @()
foreach ($required in @('models/whisper-large-v3/model.bin', 'models/alignment-en/wav2vec2-base-960h-alignment.pt', 'gpu/cublas64_12.dll', 'gpu/cudnn64_9.dll')) {
    if (!(Test-Path "build-tools/stem-runtime/$required")) {
        throw 'GPU recognition is not installed. Run scripts/setup-recognition-windows.ps1 before packaging.'
    }
}
Invoke-Checked 'build-tools/stem-runtime/python.exe' @('-c', 'import ctranslate2; assert ctranslate2.__version__ == "4.8.3", "Run setup-recognition-windows.ps1 to install the tested Windows GPU runtime"')
$package = Join-Path $BuildDir 'beater-windows-x64'
New-Item -ItemType Directory -Force $package | Out-Null
Copy-Item "$BuildDir/Release/beater.exe" $package
Copy-Item "$BuildDir/Release/beater-transcript.exe" $package
if (!(Test-Path 'build-tools/stem-runtime/models/hub/checkpoints')) {
    throw 'Run scripts/setup-stems-windows.ps1 with Python 3.11 before packaging vocal separation.'
}
Copy-Item scripts/stem-worker.py $package
Copy-Item scripts/transcript-worker.py $package
Copy-Item scripts/transcript-alignment.py $package
& robocopy "$root/build-tools/stem-runtime" "$package/stem-runtime" /E /MT:8 /NFL /NDL /NJH /NJS /NP
if ($LASTEXITCODE -gt 7) { throw 'Could not copy the vocal separator runtime.' }
Invoke-Checked "$QtRoot/bin/windeployqt.exe" @('--release', '--qmldir', "$root/qml", '--qmldir', "$root/external/highlighter-ui/HighlighterUI", '--no-translations', '--compiler-runtime', "$package/beater.exe")
# App-local runtime DLLs allow launching without a separate VC++ installer.
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$crt = Get-ChildItem "$vs/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT" -Directory | Sort-Object FullName -Descending | Select-Object -First 1
if (!$crt) { throw 'Cannot locate the Visual C++ redistributable DLLs.' }
Copy-Item "$($crt.FullName)/*.dll" $package
$licenses = Join-Path $package 'licenses'
New-Item -ItemType Directory -Force $licenses | Out-Null
Copy-Item "$BuildDir/_deps/rubberband-src/COPYING" "$licenses/RubberBand.txt"
Copy-Item "$BuildDir/_deps/miniaudio-src/LICENSE" "$licenses/miniaudio.txt"
if (Test-Path "$QtRoot/licenses") { Copy-Item "$QtRoot/licenses" "$licenses/Qt" -Recurse -Force }
Set-Content "$package/README.txt" 'Run beater.exe. Keep all files and folders together. Windows x64. Supported audio: WAV, MP3, FLAC. Loading a deck opens its separate Vocal Text Test window, caches the original and queues local vocal extraction. When vocals are ready, full Whisper large-v3 transcribes them locally on NVIDIA GPU when available, with CPU fallback using the same model. Recognition is queued between decks. The Language selector can correct automatic guesses. Confident English word times are refined locally; other timings stay with Whisper. The current word lights up as the deck plays. Pauses, seeks, loops and tempo changes follow the deck playhead. Follow words scrolls the current word into view. Each deck has a Lyrics button to reopen or restore its window. Cached vocals load without waiting behind another separation job. The full vocal timeline is transcribed without a speech filter, and older transcripts are regenerated once. Word timing is estimated and can be imperfect, especially for singing. The full track stays on the deck. Copy text and Open files give access to the saved transcript, including timestamps in JSON. Separation and transcription can take several minutes; repeated loads reuse completed results. Reloading a deck updates its test window. Closing beater closes its test windows and cancels pending work. No Resolume connection is included yet.'
Invoke-Checked "$package/stem-runtime/python.exe" @('scripts/package-windows.py', $package, "$BuildDir/beater-windows-x64.zip")
Write-Host "Ready: $package/beater.exe"
