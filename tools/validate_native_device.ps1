param([Parameter(Mandatory=$true)][string]$Device,[int]$DurationSeconds=600,[string]$OutputDir,[string]$Adb,[ValidateSet('org.supermanreturns.mobile','org.supermanreturns.mobile.native')][string]$Package='org.supermanreturns.mobile')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(-not $Adb){$Adb=Join-Path $root '.tools/android-sdk/platform-tools/adb.exe'}
if(-not $OutputDir){$OutputDir=Join-Path $root ('.tools/native-device/'+(Get-Date -Format 'yyyyMMdd-HHmmss'))}
python (Join-Path $PSScriptRoot 'validate_native_device.py') --adb $Adb --device $Device --package $Package --duration $DurationSeconds --output $OutputDir
if($LASTEXITCODE -ne 0){throw "Native measurement failed or was interrupted. Inspect $OutputDir/summary.json"}
