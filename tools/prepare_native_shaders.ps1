param([Parameter(Mandatory=$true)][string]$RecompRoot,[switch]$Install,[string]$Device,[switch]$AllowIncomplete)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$RecompRoot=(Resolve-Path -LiteralPath $RecompRoot).Path
$out=Join-Path $root '.tools/native-shaders'
$corpus=Join-Path $root '.tools/native-shader-corpus'
$cache=Join-Path $root '.tools/native-shader-cache'
$library=Join-Path $out 'superman_returns_vulkan.srvk'
$extract=Join-Path $RecompRoot 'tools/shaders/extract_shaders.py'
$emitter=Join-Path $RecompRoot 'build/vulkan-m2/emitter-build/XenosRecompCorpus.exe'
$common=Join-Path $RecompRoot 'build/vulkan-m2/emitter-tree/src/XenosRecomp/shader_common.h'
$dxc=Join-Path $RecompRoot '.tools/dxc/bin/x64/dxc.exe'
foreach($path in @($extract,$emitter,$common,$dxc)){if(-not(Test-Path -LiteralPath $path -PathType Leaf)){throw "Required existing PC tool missing: $path"}}
New-Item -ItemType Directory -Force $out | Out-Null
$dumpDirs=@(Join-Path $RecompRoot 'artifacts/shaders/raw')
$logs=Join-Path $RecompRoot 'logs'
if(Test-Path -LiteralPath $logs){
    $dumpDirs+=@(Get-ChildItem -LiteralPath $logs -Directory -Recurse | Where-Object {$_.FullName -match 'native_shaders|rt_corpus3|rt_shaders' -and (Get-ChildItem -LiteralPath $_.FullName -Filter '*.bin' -File | Select-Object -First 1)} | Select-Object -ExpandProperty FullName)
}
$extractArgs=@($extract,'--game',(Join-Path $RecompRoot 'game'),'--out',$corpus)
foreach($directory in $dumpDirs){if(Test-Path -LiteralPath $directory){$extractArgs+=@('--dump-dir',$directory)}}
python @extractArgs
if($LASTEXITCODE -ne 0){throw 'Private shader corpus extraction failed.'}
python (Join-Path $RecompRoot 'tools/shaders/make_vulkan_preshaders.py') --corpus (Join-Path $corpus 'raw') --cache $cache --out $library --emitter $emitter --common $common --dxc $dxc --jobs 4
$compilerExit=$LASTEXITCODE
if($compilerExit -notin @(0,1)){throw "Shader compiler failed: $compilerExit"}
if(-not(Test-Path -LiteralPath $library)){throw 'Shader compiler produced no library.'}
python (Join-Path $RecompRoot 'tools/shaders/verify_vulkan_preshaders.py') $library
if($LASTEXITCODE -ne 0){throw 'Generated Vulkan library failed independent verification.'}
$report=Get-Content ([IO.Path]::ChangeExtension($library,'.report.json')) -Raw | ConvertFrom-Json
$metadata=@{schema=1;sha256=(Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash;pc_revision=(git -C $RecompRoot rev-parse HEAD);translator_sha256=(Get-FileHash -LiteralPath $emitter -Algorithm SHA256).Hash;common_sha256=(Get-FileHash -LiteralPath $common -Algorithm SHA256).Hash;dxc_sha256=(Get-FileHash -LiteralPath $dxc -Algorithm SHA256).Hash;total=$report.total;ready=$report.ready;failed=$report.failed.Count;runtime_dump_directories=$dumpDirs}
$metadata | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $out 'provenance.json')
if($compilerExit -ne 0 -and -not $AllowIncomplete){throw "Incomplete library: $($report.failed.Count) failed shaders. Inspect report; -AllowIncomplete permits diagnostic integration only."}
if($compilerExit -ne 0){Write-Warning "Diagnostic library only: $($report.ready)/$($report.total) shaders. Any used missing shader must stop native gameplay."}
if($Install){
    if(-not $Device){throw '-Install requires -Device.'}
    $adb=Join-Path $root '.tools/android-sdk/platform-tools/adb.exe'
    & $adb -s $Device push $library /data/local/tmp/sr-native-library.pending
    if($LASTEXITCODE -ne 0){throw 'Shader upload failed.'}
    & $adb -s $Device shell run-as org.supermanreturns.mobile sh -c '"mkdir -p files/shaders && cp /data/local/tmp/sr-native-library.pending files/shaders/superman_returns_vulkan.srvk.pending && mv files/shaders/superman_returns_vulkan.srvk.pending files/shaders/superman_returns_vulkan.srvk"'
    if($LASTEXITCODE -ne 0){throw 'Private shader library installation failed.'}
    & $adb -s $Device shell rm /data/local/tmp/sr-native-library.pending
    if($LASTEXITCODE -ne 0){throw 'Shader transport cleanup failed.'}
}
$metadata | ConvertTo-Json -Depth 4
