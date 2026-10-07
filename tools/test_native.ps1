param([ValidateSet('source','contract','frontend','shaders','commands','provider','bootstrap')][string]$Suite='source', [string]$Device)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if($Suite -eq 'source') {
        python -m unittest discover -s tests -p test_prepare_native_renderer.py
        if($LASTEXITCODE -ne 0){throw 'Native source preparation tests failed.'}
    } elseif($Suite -eq 'contract') {
        if(-not $Device){throw 'Contract suite currently requires an Android -Device serial.'}
        $sdk=Join-Path $root '.tools/android-sdk'
        $build=Join-Path $root '.tools/native-tests-arm64'
        cmake -S (Join-Path $root 'native/renderer-tests') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$sdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
        if($LASTEXITCODE -ne 0){throw 'Native test CMake configuration failed.'}
        cmake --build $build --target test_graphics_contract
        if($LASTEXITCODE -ne 0){throw 'Native contract test compilation failed.'}
        $adb=Join-Path $sdk 'platform-tools/adb.exe'
        & $adb -s $Device push (Join-Path $build 'test_graphics_contract') /data/local/tmp/sr-test-graphics-contract
        if($LASTEXITCODE -ne 0){throw 'Native test upload failed.'}
        & $adb -s $Device shell chmod 700 /data/local/tmp/sr-test-graphics-contract
        if($LASTEXITCODE -ne 0){throw 'Native test permission setup failed.'}
        & $adb -s $Device shell /data/local/tmp/sr-test-graphics-contract
        if($LASTEXITCODE -ne 0){throw 'Native contract test failed on device.'}
    } else {
        throw "Native suite '$Suite' is not implemented yet; it has not passed."
    }
} finally {Pop-Location}
