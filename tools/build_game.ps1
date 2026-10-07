param([string]$RecompRoot=(Join-Path (Split-Path $PSScriptRoot -Parent) '..\superman_returns_recomp'),[int]$Jobs=4,[string]$AndroidSdk)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $AndroidSdk) {$AndroidSdk=Join-Path $root '.tools\android-sdk'}
$AndroidSdk=(Resolve-Path $AndroidSdk).Path
$RecompRoot=(Resolve-Path $RecompRoot).Path
& (Join-Path $PSScriptRoot 'patch_game_runtime.ps1')
python (Join-Path $root 'tools\collect_runtime_notices.py')
if($LASTEXITCODE -ne 0){throw 'Falha ao reunir avisos do runtime.'}
python (Join-Path $root 'tools\prepare_runtime_guest.py') --recomp $RecompRoot
if($LASTEXITCODE -ne 0){throw 'Falha ao preparar sources privados do jogo.'}
$build=Join-Path $root '.tools\game-build'
cmake -S (Join-Path $root 'native\game') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$AndroidSdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$AndroidSdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON
if($LASTEXITCODE -ne 0){throw 'Configuração do runtime Android falhou.'}
cmake --build $build --target superman_game --parallel $Jobs
if($LASTEXITCODE -ne 0){throw 'Compilação/linkagem do jogo falhou.'}
$libs=Join-Path $root 'android\app\libs\arm64-v8a'
New-Item -ItemType Directory -Force $libs | Out-Null
Copy-Item (Join-Path $build 'libsuperman_game.so') $libs
Copy-Item (Join-Path $root '.references\rexglue-sdk\out\linux-arm64\librexruntime.so') $libs
Copy-Item (Join-Path $AndroidSdk 'ndk\27.2.12479018\toolchains\llvm\prebuilt\windows-x86_64\sysroot\usr\lib\aarch64-linux-android\libc++_shared.so') $libs
