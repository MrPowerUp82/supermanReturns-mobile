param([ValidateSet('source','contract','frontend','shaders','commands','provider','bootstrap')][string]$Suite='source', [string]$Device)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if($Suite -eq 'source') {
        python -m unittest discover -s tests -p test_prepare_native_renderer.py
        if($LASTEXITCODE -ne 0){throw 'Native source preparation tests failed.'}
    } else {
        throw "Native suite '$Suite' is not implemented yet; it has not passed."
    }
} finally {Pop-Location}
