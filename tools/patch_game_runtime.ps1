$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$sdk=Join-Path $root '.references\rexglue-sdk'
foreach($patch in Get-ChildItem (Join-Path $PSScriptRoot 'runtime-patches') -Filter '*.patch') {
    git -C $sdk apply --reverse --check $patch.FullName 2>$null
    if($LASTEXITCODE -eq 0){continue}
    git -C $sdk apply --check $patch.FullName
    if($LASTEXITCODE -ne 0){throw "Runtime incompatível com patch $($patch.Name)."}
    git -C $sdk apply $patch.FullName
    if($LASTEXITCODE -ne 0){throw "Falha ao aplicar $($patch.Name)."}
}
