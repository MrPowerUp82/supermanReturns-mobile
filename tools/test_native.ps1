param([ValidateSet('source','contract','frontend','shaders','commands','provider','bootstrap')][string]$Suite='source', [string]$Device)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if($Suite -eq 'source') {
        python -m unittest discover -s tests -p test_prepare_native_renderer.py
        if($LASTEXITCODE -ne 0){throw 'Native source preparation tests failed.'}
    } elseif($Suite -in @('contract','frontend')) {
        if(-not $Device){throw 'Native ARM64 suites require an Android -Device serial.'}
        $sdk=Join-Path $root '.tools/android-sdk'
        $build=Join-Path $root '.tools/native-tests-arm64'
        cmake -S (Join-Path $root 'native/renderer-tests') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$sdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release "-DSR_RUNTIME_LIBRARY=$root/.references/rexglue-sdk/out/linux-arm64/librexruntime.so"
        if($LASTEXITCODE -ne 0){throw 'Native test CMake configuration failed.'}
        $targets=if($Suite -eq 'contract'){@('test_graphics_contract')}else{@('test_frontend_packets','test_guest_reads')}
        cmake --build $build --target @targets --parallel 2
        if($LASTEXITCODE -ne 0){throw 'Native test compilation failed.'}
        $adb=Join-Path $sdk 'platform-tools/adb.exe'
        $remote='/data/local/tmp/sr-native-tests'
        & $adb -s $Device shell mkdir -p $remote
        if($LASTEXITCODE -ne 0){throw 'Native test directory setup failed.'}
        $files=@((Join-Path $root '.references/rexglue-sdk/out/linux-arm64/librexruntime.so'),(Join-Path $sdk 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'))
        $files+=@($targets | ForEach-Object {Join-Path $build $_})
        & $adb -s $Device push @files "$remote/"
        if($LASTEXITCODE -ne 0){throw 'Native test upload failed.'}
        foreach($target in $targets){
            & $adb -s $Device shell chmod 700 "$remote/$target"
            if($LASTEXITCODE -ne 0){throw 'Native test permission setup failed.'}
            & $adb -s $Device shell "cd $remote && LD_LIBRARY_PATH=. ./$target"
            if($LASTEXITCODE -ne 0){throw "Native test $target failed on device."}
        }
    } else {
        throw "Native suite '$Suite' is not implemented yet; it has not passed."
    }
} finally {Pop-Location}
