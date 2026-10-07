param([string]$JavaHome, [string]$GameRoot)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (-not $JavaHome) { $JavaHome=$env:JAVA_HOME }
if (-not $JavaHome) { $JavaHome=(Get-ChildItem (Join-Path $root '.tools\java') -Directory | Select-Object -First 1).FullName }
$output=Join-Path $root '.tools\test-classes'
New-Item -ItemType Directory -Force $output | Out-Null
$source=Join-Path $root 'android\app\src\main\java\org\supermanreturns\mobile'
& (Join-Path $JavaHome 'bin\javac.exe') -encoding UTF-8 -d $output (Join-Path $source 'GameFiles.java') (Join-Path $source 'XboxIso.java') (Join-Path $source 'InstallStore.java') (Join-Path $root 'tests\ImporterTests.java')
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes Java.' }
$testArgs=@()
if ($GameRoot) { $testArgs += (Resolve-Path $GameRoot).Path }
& (Join-Path $JavaHome 'bin\java.exe') -ea -cp $output org.supermanreturns.mobile.ImporterTests @testArgs
if ($LASTEXITCODE -ne 0) { throw 'Testes de importação falharam.' }
