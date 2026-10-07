param([string]$AndroidSdk, [string]$JavaHome, [switch]$NativeSideBySide)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (-not $JavaHome) { $JavaHome=$env:JAVA_HOME }
if (-not $JavaHome) {
    $bundled=Get-ChildItem (Join-Path $root '.tools\java') -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($bundled) { $JavaHome=$bundled.FullName }
}
if (-not $JavaHome -or -not (Test-Path (Join-Path $JavaHome 'bin\java.exe'))) { throw 'Defina JAVA_HOME para um JDK 17.' }
if (-not $AndroidSdk) { $AndroidSdk=$env:ANDROID_HOME }
if (-not $AndroidSdk) { $AndroidSdk=Join-Path $root '.tools\android-sdk' }
if (-not (Test-Path (Join-Path $AndroidSdk 'platforms\android-35\android.jar'))) { throw 'Instale Android SDK 35, Build Tools 35.0.0, NDK 27.2.12479018 e CMake 3.22.1.' }
$env:JAVA_HOME=$JavaHome
$env:ANDROID_HOME=$AndroidSdk
$sdkPath=(Resolve-Path $AndroidSdk).Path.Replace('\','/')
Set-Content -LiteralPath (Join-Path $root 'android\local.properties') -Value "sdk.dir=$sdkPath" -Encoding ascii
$gradleArgs=@('assembleDebug','lintDebug','--console=plain')
if($NativeSideBySide){$gradleArgs+='-PnativeSideBySide=true'}
& (Join-Path $root 'android\gradlew.bat') -p (Join-Path $root 'android') @gradleArgs
if ($LASTEXITCODE -ne 0) { throw "Gradle falhou: $LASTEXITCODE" }
$artifacts=Join-Path $root 'artifacts'
New-Item -ItemType Directory -Force $artifacts | Out-Null
$apkName=if($NativeSideBySide){'superman-returns-native-vulkan-0.1.0-dev.apk'}else{'superman-returns-mobile-0.1.0-dev.apk'}
$apk=Join-Path $artifacts $apkName
Copy-Item -LiteralPath (Join-Path $root 'android\app\build\outputs\apk\debug\app-debug.apk') -Destination $apk
$verifyArgs=@($apk)
if(Test-Path (Join-Path $root 'android\app\libs\arm64-v8a\libsuperman_game.so')) {$verifyArgs+='--with-game'}
python (Join-Path $root 'tools\verify_apk.py') @verifyArgs
if ($LASTEXITCODE -ne 0) { throw 'Verificação do APK falhou.' }
& (Join-Path $AndroidSdk 'build-tools\35.0.0\apksigner.bat') verify $apk
if ($LASTEXITCODE -ne 0) { throw 'Assinatura do APK inválida.' }
& (Join-Path $AndroidSdk 'build-tools\35.0.0\zipalign.exe') -c -P 16 4 $apk
if ($LASTEXITCODE -ne 0) { throw 'Alinhamento do APK inválido.' }
Get-FileHash -LiteralPath $apk -Algorithm SHA256 | Format-List
Write-Host "APK de desenvolvimento: $apk"
