param([ValidateSet('source','contract','frontend','shaders','commands','provider','bootstrap','filter','pipeline','allocations','composition','runtime')][string]$Suite='source', [string]$Device, [string]$VertexShader, [string]$PixelShader)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if($Suite -eq 'source') {
        python -m unittest discover -s tests -p test_prepare_native_renderer.py
        if($LASTEXITCODE -ne 0){throw 'Native source preparation tests failed.'}
    } elseif($Suite -eq 'runtime') {
        if(-not $Device){throw 'Native ARM64 suites require an Android -Device serial.'}
        $sdk=Join-Path $root '.tools/android-sdk'
        $build=Join-Path $root '.tools/runtime-tests-arm64'
        $runtime=Join-Path $root '.references/rexglue-sdk/out/linux-arm64/librexruntime.so'
        cmake -S (Join-Path $root 'native/runtime-tests') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$sdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release "-DSR_RUNTIME_LIBRARY=$runtime"
        if($LASTEXITCODE -ne 0){throw 'Runtime test CMake configuration failed.'}
        $targets=@('test_proc_maps','test_region_query','test_write_watch')
        cmake --build $build --target @targets
        if($LASTEXITCODE -ne 0){throw 'Runtime test compilation failed.'}
        $adb=Join-Path $sdk 'platform-tools/adb.exe'
        $remote='/data/local/tmp/sr-runtime-tests'
        & $adb -s $Device shell mkdir -p $remote
        $files=@($runtime,(Join-Path $sdk 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'))
        $files+=@($targets | ForEach-Object {Join-Path $build $_})
        & $adb -s $Device push @files "$remote/"
        if($LASTEXITCODE -ne 0){throw 'Runtime test upload failed.'}
        foreach($target in $targets){
            & $adb -s $Device shell "chmod 700 $remote/$target && cd $remote && LD_LIBRARY_PATH=. TMPDIR=/data/local/tmp ./$target"
            if($LASTEXITCODE -ne 0){throw "Runtime test $target failed on device."}
        }
    } elseif($Suite -in @('contract','frontend','shaders','commands','provider','bootstrap','filter','pipeline','allocations','composition')) {
        if(-not $Device){throw 'Native ARM64 suites require an Android -Device serial.'}
        $sdk=Join-Path $root '.tools/android-sdk'
        $build=Join-Path $root '.tools/native-tests-arm64'
        cmake -S (Join-Path $root 'native/renderer-tests') -B $build -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$sdk/ndk/27.2.12479018/build/cmake/android.toolchain.cmake" "-DCMAKE_MAKE_PROGRAM=$sdk/cmake/3.22.1/bin/ninja.exe" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33 -DANDROID_STL=c++_shared -DCMAKE_BUILD_TYPE=Release "-DSR_RUNTIME_LIBRARY=$root/.references/rexglue-sdk/out/linux-arm64/librexruntime.so"
        if($LASTEXITCODE -ne 0){throw 'Native test CMake configuration failed.'}
        $targets=@(switch($Suite){'contract' {@('test_graphics_contract')} 'frontend' {@('test_frontend_packets','test_guest_reads')} 'shaders' {@('test_android_shader_library')} 'commands' {@('test_native_commands')} 'provider' {@('test_native_provider','test_vulkan_regressions')} 'bootstrap' {@('test_native_bootstrap')} 'filter' {@('test_texture_filter','test_float_filter_gpu')} 'pipeline' {@('test_pipeline_cache_gpu')} 'allocations' {@('test_device_buffer_gpu')} 'composition' {@('test_composition_gpu')}})
        $buildTargets=@($targets)
        if($Suite -eq 'filter'){$buildTargets+=@('filter_probe_shader','filter_probe_vs','filter_probe_ps')}
        cmake --build $build --target @buildTargets --parallel 2
        if($LASTEXITCODE -ne 0){throw 'Native test compilation failed.'}
        $adb=Join-Path $sdk 'platform-tools/adb.exe'
        $remote='/data/local/tmp/sr-native-tests'
        & $adb -s $Device shell mkdir -p $remote
        if($LASTEXITCODE -ne 0){throw 'Native test directory setup failed.'}
        $files=@((Join-Path $root '.references/rexglue-sdk/out/linux-arm64/librexruntime.so'),(Join-Path $sdk 'ndk/27.2.12479018/toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'))
        $files+=@($targets | ForEach-Object {Join-Path $build $_})
        if($Suite -eq 'filter'){$files+=@('float_filter_probe.spv','float_filter_probe.vs.spv','float_filter_probe.ps.spv' | ForEach-Object {Join-Path $build $_})}
        & $adb -s $Device push @files "$remote/"
        if($LASTEXITCODE -ne 0){throw 'Native test upload failed.'}
        if($Suite -eq 'pipeline') {
            if(-not $VertexShader -or -not $PixelShader){throw 'Pipeline regression requires private -VertexShader and -PixelShader SPIR-V inputs.'}
            & $adb -s $Device push $VertexShader "$remote/pipeline.vs.spv"
            if($LASTEXITCODE -ne 0){throw 'Pipeline vertex shader upload failed.'}
            & $adb -s $Device push $PixelShader "$remote/pipeline.ps.spv"
            if($LASTEXITCODE -ne 0){throw 'Pipeline pixel shader upload failed.'}
        }
        foreach($target in $targets){
            & $adb -s $Device shell chmod 700 "$remote/$target"
            if($LASTEXITCODE -ne 0){throw 'Native test permission setup failed.'}
            $testArgs=if($target -eq "test_float_filter_gpu"){" float_filter_probe.spv"}elseif($target -eq 'test_pipeline_cache_gpu'){' pipeline.vs.spv pipeline.ps.spv'}else{""}
            & $adb -s $Device shell "cd $remote && LD_LIBRARY_PATH=. TMPDIR=/data/local/tmp ./$target$testArgs"
            if($LASTEXITCODE -ne 0){throw "Native test $target failed on device."}
        }
    } else {
        throw "Native suite '$Suite' is not implemented yet; it has not passed."
    }
} finally {Pop-Location}

