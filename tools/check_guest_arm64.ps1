param([string]$RecompRoot=(Join-Path (Split-Path $PSScriptRoot -Parent) '..\superman_returns_recomp'), [int]$Jobs=4, [string]$AndroidSdk)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (-not $AndroidSdk) { $AndroidSdk=$env:ANDROID_HOME }
if (-not $AndroidSdk) { $AndroidSdk=Join-Path $root '.tools\android-sdk' }
$AndroidSdk=(Resolve-Path $AndroidSdk).Path
$RecompRoot=(Resolve-Path $RecompRoot).Path
python (Join-Path $root 'tools\prepare_guest.py') --recomp $RecompRoot
if($LASTEXITCODE -ne 0) {throw 'Falha ao preparar headers privados.'}
$cmake=Join-Path $AndroidSdk 'cmake\3.22.1\bin\cmake.exe'
$build=Join-Path $root '.tools\guest-build'
& $cmake -S (Join-Path $root 'native\guest') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$AndroidSdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$AndroidSdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DCMAKE_BUILD_TYPE=Release "-DSR_RECOMP_ROOT=$RecompRoot"
if($LASTEXITCODE -ne 0) {throw 'Falha na configuração CMake.'}
& $cmake --build $build --parallel $Jobs
if($LASTEXITCODE -ne 0) {throw 'Codegen ainda não compila para ARM64; veja o diagnóstico acima.'}
Write-Host 'Codegen compilado em archive ARM64. Ainda exige runtime, renderer e testes de execução para jogar.'
