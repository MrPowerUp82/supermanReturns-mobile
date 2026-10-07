$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$sdk=Join-Path $root '.references\rexglue-sdk'
if(-not (Test-Path (Join-Path $sdk '.git'))) {
    git clone https://github.com/Buku313/rexglue-skate3-android.git $sdk
    if($LASTEXITCODE -ne 0){throw 'Falha no clone do runtime Android.'}
}
git -C $sdk checkout edd4344723ecac3ffa18c5dcd2fcc268f468ff9e
if($LASTEXITCODE -ne 0){throw 'Falha ao selecionar a revisão fixada.'}
git -C $sdk submodule update --init --depth 1 --jobs 8
if($LASTEXITCODE -ne 0){throw 'Falha ao obter dependências fixadas.'}
& (Join-Path $PSScriptRoot 'patch_game_runtime.ps1')
$java=Join-Path $sdk 'thirdparty\sdl3\android-project\app\src\main\java\org\libsdl\app'
$destination=Join-Path $root 'android\app\src\main\java\org\libsdl\app'
New-Item -ItemType Directory -Force $destination | Out-Null
Copy-Item (Join-Path $java '*.java') $destination
python (Join-Path $root 'tools\collect_runtime_notices.py')
if($LASTEXITCODE -ne 0){throw 'Falha ao reunir avisos do runtime.'}
Write-Host 'Runtime Android e fontes Java SDL preparados.'
