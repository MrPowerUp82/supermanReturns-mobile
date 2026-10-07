$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$sdk=Join-Path $root '.references\rexglue-sdk'
$normalized=Join-Path $root '.tools/runtime-patches-lf'
New-Item -ItemType Directory -Force $normalized | Out-Null
foreach($patch in Get-ChildItem (Join-Path $PSScriptRoot 'runtime-patches') -Filter '*.patch') {
    $patchPath=Join-Path $normalized $patch.Name
    $patchText=[System.IO.File]::ReadAllText($patch.FullName).Replace("`r`n","`n")
    [System.IO.File]::WriteAllText($patchPath,$patchText,[System.Text.UTF8Encoding]::new($false))
    git -C $sdk -c core.autocrlf=false apply --reverse --check $patchPath 2>$null
    if($LASTEXITCODE -eq 0){continue}
    git -C $sdk -c core.autocrlf=false apply --check $patchPath
    if($LASTEXITCODE -ne 0){throw "Runtime incompatível com patch $($patch.Name)."}
    git -C $sdk -c core.autocrlf=false apply $patchPath
    if($LASTEXITCODE -ne 0){throw "Falha ao aplicar $($patch.Name)."}
}
