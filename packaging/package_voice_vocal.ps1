param(
    [Parameter(Mandatory = $true)]
    [string]$Binary,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$Commit
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $Binary -PathType Leaf)) {
    throw "Voice/Vocal binary not found: $Binary"
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$versionHeader = Join-Path $repositoryRoot 'include\monkeys_ear\vocal_product_version.h'
$changelog = Join-Path $repositoryRoot 'docs\VOICE_VOCAL_CHANGELOG.md'
if (-not (Test-Path -LiteralPath $versionHeader -PathType Leaf)) {
    throw "Voice/Vocal version owner not found: $versionHeader"
}
if (-not (Test-Path -LiteralPath $changelog -PathType Leaf)) {
    throw "Voice/Vocal changelog not found: $changelog"
}
$versionMatch = [regex]::Match(
    (Get-Content -LiteralPath $versionHeader -Raw),
    '#define\s+MONKEYS_EAR_VOCAL_PRODUCT_VERSION\s+"([0-9]+\.[0-9]+\.[0-9]+-[0-9A-Za-z.-]+)"'
)
if (-not $versionMatch.Success) {
    throw 'Voice/Vocal product version is missing or is not an explicit pre-release version'
}
$productVersion = $versionMatch.Groups[1].Value

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$bundleName = "monkeys-ear-voice-vocal-$productVersion-windows-x64-validation-$Commit"
$stage = Join-Path $OutputDirectory $bundleName
$zipPath = Join-Path $OutputDirectory "$bundleName.zip"
$zipHashPath = "$zipPath.sha256"

if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
Remove-Item -LiteralPath $zipPath, $zipHashPath -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stage | Out-Null

$pluginName = 'monkeys_ear_vocal.vst3'
$pluginPath = Join-Path $stage $pluginName
Copy-Item -LiteralPath $Binary -Destination $pluginPath

@"
MONKEY'S EAR VOICE/VOCAL — VALIDATION CANDIDATE

Commit: $Commit
Voice/Vocal version: $productVersion
Platform: Windows x64
Format: VST3 effect
Purpose: bounded REAPER and musician evaluation

This bundle is not a public release. Its binary passed the repository's
automated DSP, tracker, host-load, and automation probes before packaging.
Those probes do not prove REAPER compatibility, project recall, listening
quality, musical usefulness, or broad host support.
"@ | Set-Content -LiteralPath (Join-Path $stage 'BUILD.txt') -Encoding ascii

$productVersion | Set-Content -LiteralPath (Join-Path $stage 'VERSION.txt') -Encoding ascii
Copy-Item -LiteralPath $changelog -Destination (Join-Path $stage 'CHANGELOG.md')

@'
INSTALL (WINDOWS x64)

1. Close REAPER and other audio hosts.
2. Copy monkeys_ear_vocal.vst3 to:
   C:\Program Files\Common Files\VST3\
3. Start REAPER.
4. Open Options > Preferences > Plug-ins > VST.
5. Confirm the VST3 path above is listed, then choose Re-scan.

REMOVE

1. Close REAPER and other audio hosts.
2. Delete monkeys_ear_vocal.vst3 from:
   C:\Program Files\Common Files\VST3\
3. Re-scan VST plug-ins in REAPER.

Windows may require administrator permission for the copy or delete.
Keep the ZIP and its SHA-256 file together so the exact candidate can be
identified when reporting a failure.
'@ | Set-Content -LiteralPath (Join-Path $stage 'INSTALL_REMOVE.txt') -Encoding ascii

@'
KNOWN LIMITS / TEST BOUNDARY

CONFIRMED BY AUTOMATION FOR THIS COMMIT
- Windows x64 build completed.
- The binary loaded in the repository's minimal VST3 host probe.
- Parameter enumeration, processing, and Mix automation passed that probe.
- The DSP core and vocal tracker characterization passed.

NOT YET CONFIRMED
- REAPER discovery or loading.
- Save/reopen project recall.
- Offline render behavior.
- CPU behavior in a real session.
- Audible quality, preference, usefulness, or safe monitoring level.
- Operation in hosts other than the repository's probe.
- Installation by a second person.

REPORT
Include the commit from BUILD.txt, REAPER version, Windows version, sample
rate, buffer size, exact step, expected result, actual result, and whether
audio became silent, distorted, unstable, or unexpectedly loud.

Do not treat this candidate as hearing protection. Begin at a low monitor
level and bypass immediately if output is unexpected.
'@ | Set-Content -LiteralPath (Join-Path $stage 'KNOWN_LIMITS.txt') -Encoding ascii

$pluginHash = (Get-FileHash -LiteralPath $pluginPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$pluginHash  $pluginName" | Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Encoding ascii

Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -CompressionLevel Optimal

$verify = Join-Path $OutputDirectory '_verify_voice_vocal'
Remove-Item -LiteralPath $verify -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $zipPath -DestinationPath $verify

$expected = @('BUILD.txt', 'CHANGELOG.md', 'INSTALL_REMOVE.txt', 'KNOWN_LIMITS.txt', 'SHA256SUMS.txt', 'VERSION.txt', $pluginName)
$actual = @(Get-ChildItem -LiteralPath $verify -File | Select-Object -ExpandProperty Name | Sort-Object)
$delta = Compare-Object ($expected | Sort-Object) $actual
if ($delta) {
    throw "Bundle contents differ from the five-file contract: $($delta | Out-String)"
}

$verifiedPlugin = Join-Path $verify $pluginName
$recordedHash = ((Get-Content -LiteralPath (Join-Path $verify 'SHA256SUMS.txt') -Raw).Trim() -split '\s+')[0]
$verifiedHash = (Get-FileHash -LiteralPath $verifiedPlugin -Algorithm SHA256).Hash.ToLowerInvariant()
if ($recordedHash -ne $verifiedHash) {
    throw 'Packaged plug-in checksum does not match SHA256SUMS.txt'
}
$verifiedVersion = (Get-Content -LiteralPath (Join-Path $verify 'VERSION.txt') -Raw).Trim()
if ($verifiedVersion -ne $productVersion) {
    throw 'Packaged Voice/Vocal version does not match the canonical product version'
}

$zipHash = (Get-FileHash -LiteralPath $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$zipHash  $([System.IO.Path]::GetFileName($zipPath))" | Set-Content -LiteralPath $zipHashPath -Encoding ascii

Remove-Item -LiteralPath $verify -Recurse -Force
Write-Host "Verified $zipPath"
Write-Host "ZIP SHA-256: $zipHash"
