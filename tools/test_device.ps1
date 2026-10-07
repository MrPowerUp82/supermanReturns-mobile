param([Parameter(Mandatory=$true)][string]$Serial, [string]$Adb, [string]$JavaHome)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (-not $JavaHome) { $JavaHome=$env:JAVA_HOME }
if (-not $JavaHome) { $JavaHome=(Get-ChildItem (Join-Path $root '.tools\java') -Directory | Select-Object -First 1).FullName }
$env:JAVA_HOME=$JavaHome
if (-not $Adb) { $Adb=(Get-Command adb -ErrorAction Stop).Source }
& (Join-Path $root 'android\gradlew.bat') -p (Join-Path $root 'android') assembleDebug assembleDebugAndroidTest --console=plain
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o teste de dispositivo.' }
& $Adb -s $Serial install -r (Join-Path $root 'android\app\build\outputs\apk\debug\app-debug.apk')
if ($LASTEXITCODE -ne 0) { throw 'Falha ao instalar o app.' }
& $Adb -s $Serial install -r (Join-Path $root 'android\app\build\outputs\apk\androidTest\debug\app-debug-androidTest.apk')
if ($LASTEXITCODE -ne 0) { throw 'Falha ao instalar o harness de teste.' }
$result=& $Adb -s $Serial shell am instrument -w org.supermanreturns.mobile.test/org.supermanreturns.mobile.DeviceSmoke
$artifacts=Join-Path $root 'artifacts'
New-Item -ItemType Directory -Force $artifacts | Out-Null
$result | Tee-Object -FilePath (Join-Path $artifacts 'device-validation.txt')
if ($LASTEXITCODE -ne 0 -or ($result -join "`n") -notmatch '11 device checks passed' -or ($result -join "`n") -match 'FAIL|INSTRUMENTATION_FAILED') { throw 'Teste de dispositivo falhou. Consulte artifacts/device-validation.txt.' }
& $Adb -s $Serial pull /sdcard/Android/data/org.supermanreturns.mobile/files/validation (Join-Path $artifacts 'device-evidence')
if ($LASTEXITCODE -ne 0) { throw 'Não foi possível copiar as evidências do teste.' }
