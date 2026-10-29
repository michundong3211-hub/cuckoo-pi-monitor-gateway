[CmdletBinding()]
param(
    [string]$Baseline = '89900f0',
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

$allowedPaths = @(
    'platformio.ini',
    'boards/waveshare_s3_tft_35.json',
    'boards/waveshare_s3_tft_35/pins_arduino.h',
    'src/waveshare_35_profile.hpp',
    'src/gateway35_storage_compat.hpp',
    'src/sdcard_sd.hpp',
    'src/gateway35_board_io.hpp', 'src/gateway35_board_io.cpp',
    'src/gateway35_touch.hpp', 'src/gateway35_touch.cpp',
    'src/gateway_touch.hpp', 'src/gateway_touch.cpp',
    'src/gateway35_display.hpp', 'src/gateway35_display.cpp',
    'src/display.hpp', 'src/display.cpp',
    'test/host/stubs/Wire.h', 'test/host/stubs/Arduino_GFX_Library.h', 'test/host/stubs/lvgl.h',
    'test/host/waveshare_35_minimal_build_contract_host_test.cpp',
    'test/host/waveshare_35_board_io_host_test.cpp', 'test/host/waveshare_35_touch_host_test.cpp',
    'test/host/gateway_touch_host_test.cpp',
    'test/host/waveshare_35_display_contract_host_test.cpp', 'test/host/waveshare_35_display_runtime_host_test.cpp',
    'test/host/layout2_ui_copy_host_test.cpp',
    'script/verify_waveshare_35_scope.ps1', 'script/run_host_tests.ps1',
    'docs/superpowers/specs/2026-08-29-waveshare-35-minimal-design.md',
    'docs/superpowers/plans/2026-08-29-waveshare-35-minimal.md',
    'docs/evidence/2026-08-29-waveshare-35-sku30733-hardware-contract.md',
    'docs/superpowers/plans/2026-08-29-waveshare-35-minimal-evidence.md'
)

$protectedHashes = [ordered]@{
    'html/app.css.gz' = '644a535f2eebcdf790136433417ef840c7e2a1af10c4fcb36bd6c2d6991bf6a3'
    'html/app.js.gz' = '886f68590f4542cd66238d7371372190f5e73732f8878d4af9b07d40ad2ab997'
    'html/favicon.ico.gz' = '2855bef22c475f8addbbe10ced7d1cd5b1ccd99df68c2824a0e8513e89f2b34a'
    'html/index.html' = '8629af47d249f9865fce34f95db4e037b3609e217d9ead8ad3decfd7e4f050c8'
    'src/config_gateway.cpp' = '1fb6c98f21fd54b1af9466799c3a2acb4771e9804d32c105b40db607d9b487cc'
    'src/config_gateway.hpp' = '28d4d4be05b3aaaec765ac32c8cc4bbe7ad4b175390a96566a328bcab0dab450'
    'src/web_gateway.cpp' = '43b8e705ba540175abc2526ae847ce0fd62497743cb3a53ca11dc27305205827'
    'src/web_gateway.hpp' = '8f83303aa129c999ffee3331f0382be562352fcb3b7bd675f3bbfed11d38548c'
    'src/ui_gravitymon_gateway.cpp' = 'd07f955e28cf9da1dc5381003a3191df27dd35755c599b217351f0510c3cc52c'
    'src/ui_gravitymon_gateway.hpp' = '4a677a527471e500b35e7698f6fcce74857884731265f91c9ebcb931971638d8'
    'bin/version.json' = 'bc6ed1895a48b3464451c27f9a9777f46f6c1dba22db3778f6a983920e5ea1c0'
    'src/idf_component.yml' = 'f45ff1aad2d08ad4774aef89271b5d4f2fd41fe86c0d06aef70142bd53e2c770'
}

function Test-TrackedRecords {
    param([string[]]$Records, [string[]]$Allowed)
    $ok = $true
    foreach ($record in $Records) {
        $parts = $record -split "`t"
        $status = $parts[0]
        $paths = if ($status -match '^R') { @($parts[1], $parts[2]) } else { @($parts[1]) }
        foreach ($path in $paths) {
            if ($path -notin $Allowed) { Write-Host "OUT-OF-SCOPE tracked $status $path"; $ok = $false }
        }
    }
    return $ok
}

function Test-UntrackedRecords {
    param([string[]]$Records, [string[]]$Allowed)
    $ok = $true
    foreach ($record in $Records) {
        $path = ($record -replace '^\?\?\s+', '').Trim('"')
        if ($path -notin $Allowed) { Write-Host "OUT-OF-SCOPE untracked $path"; $ok = $false }
    }
    return $ok
}

function Test-IgnoredHashes {
    param([hashtable]$Actual, [hashtable]$Expected)
    $ok = $true
    foreach ($path in $Expected.Keys) {
        if (-not $Actual.ContainsKey($path) -or $Actual[$path].ToLowerInvariant() -ne $Expected[$path].ToLowerInvariant()) {
            Write-Host "PROTECTED HASH MISMATCH $path expected=$($Expected[$path]) actual=$($Actual[$path])"
            $ok = $false
        }
    }
    return $ok
}

function Test-ScopeSelfTest {
    $ok = $true
    if (Test-TrackedRecords @("M`trogue.cpp") $allowedPaths) { Write-Output 'SelfTest failed: M record accepted'; $ok = $false }
    if (Test-TrackedRecords @("D`trogue.cpp") $allowedPaths) { Write-Output 'SelfTest failed: D record accepted'; $ok = $false }
    if (Test-TrackedRecords @("R100`told.cpp`tnew.cpp") $allowedPaths) { Write-Output 'SelfTest failed: R record accepted'; $ok = $false }
    if (Test-UntrackedRecords @('?? rogue/untracked.txt') $allowedPaths) { Write-Output 'SelfTest failed: untracked record accepted'; $ok = $false }
    $temp = New-TemporaryFile
    try {
        Set-Content -LiteralPath $temp.FullName -Value 'temporary ignored hash mismatch' -NoNewline
        $actual = @{ 'temporary/ignored.fixture' = (Get-FileHash -LiteralPath $temp.FullName -Algorithm SHA256).Hash }
        $expected = @{ 'temporary/ignored.fixture' = ('0' * 64) }
        if (Test-IgnoredHashes $actual $expected) { Write-Output 'SelfTest failed: ignored hash mismatch accepted'; $ok = $false }
    } finally {
        Remove-Item -LiteralPath $temp.FullName -Force
    }
    if ($ok) { Write-Host 'Scope SelfTest: 5/5 rejected as expected'; return $true }
    return $false
}

Push-Location $Root
try {
    if ($SelfTest) {
        if (-not (Test-ScopeSelfTest)) { exit 1 }
        exit 0
    }
    $tracked = @(git diff --name-status --find-renames=50% $Baseline --)
    if (-not (Test-TrackedRecords $tracked $allowedPaths)) { exit 1 }
    $untracked = @(git status --porcelain=v1 --untracked-files=all | Where-Object { $_ -match '^\?\?' })
    if (-not (Test-UntrackedRecords $untracked $allowedPaths)) { exit 1 }

    $ignored = @(git status --ignored --porcelain=v1 --untracked-files=all | Where-Object { $_ -match '^!!' })
    $ignoredAllowed = $true
    foreach ($record in $ignored) {
        $path = ($record -replace '^!!\s+', '').Trim('"')
        if ($path -notmatch '^\.superpowers/' -and $path -notmatch '^\.pio/' -and $path -notmatch '^\.vscode/' -and $path -notmatch '^\.env/' -and $path -notmatch '^src/idf_component\.yml$' -and $path -notmatch '^bin/version\.json$' -and $path -notmatch '\.map$' -and $path -notmatch '\.pyc$' -and $path -notmatch '(^|/)\.DS_Store$') {
            Write-Output "UNEXPECTED ignored path $path"
            $ignoredAllowed = $false
        }
    }
    if (-not $ignoredAllowed) { exit 1 }

    $actualHashes = @{}
    foreach ($path in $protectedHashes.Keys) {
        $full = Join-Path $Root $path
        if (-not (Test-Path -LiteralPath $full -PathType Leaf)) {
            Write-Output "PROTECTED FILE MISSING $path"
            $actualHashes[$path] = ''
        } else {
            $actualHashes[$path] = (Get-FileHash -LiteralPath $full -Algorithm SHA256).Hash.ToLowerInvariant()
        }
    }
    if (-not (Test-IgnoredHashes $actualHashes $protectedHashes)) { exit 1 }
    Write-Output "Scope audit passed: baseline=$Baseline tracked=$($tracked.Count) untracked=$($untracked.Count) ignored=$($ignored.Count)"
} finally {
    Pop-Location
}
