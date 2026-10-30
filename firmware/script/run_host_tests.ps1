[CmdletBinding()]
param(
    [string]$Root = (Split-Path -Parent $PSScriptRoot),
    [string]$OutputDirectory = (Join-Path ([System.IO.Path]::GetTempPath()) 'gravitymon-gateway-35-host-tests'),
    [switch]$NDebug
)

$ErrorActionPreference = 'Stop'

function New-HostTest {
    param(
        [string]$Name,
        [string[]]$Sources = @(),
        [string[]]$IncludePaths = @(),
        [string[]]$Defines = @()
    )
    [pscustomobject]@{
        Name = "test/host/$Name"
        Sources = $Sources
        IncludePaths = $IncludePaths
        Defines = $Defines
    }
}

# Keep this manifest explicit: changing or adding a production dependency must
# be visible in the review diff rather than being hidden by a broad glob.
$manifest = @(
    (New-HostTest 'gmt154_offline_frame_host_test.cpp')
    (New-HostTest 'gmt154_lvgl_budget_host_test.cpp' @() @('test/host/lvglbudget_stubs'))
    (New-HostTest 'gmt154_ap_diagnostics_host_test.cpp')
    (New-HostTest 'gmt154_ap_diagnostics_runtime_host_test.cpp' @() @('test/host/apdiag_stubs'))
    (New-HostTest 'cuckoo_pi_alarm_host_test.cpp' @('src/cuckoo_pi_alarm.cpp', 'src/cuckoo_pi_config.cpp', 'src/cuckoo_pi_model.cpp'))
    (New-HostTest 'cuckoo_pi_alarm_bridge_host_test.cpp' @('src/gateway_alarm.cpp'))
    (New-HostTest 'cuckoo_pi_config_host_test.cpp' @('src/cuckoo_pi_config.cpp', 'src/cuckoo_pi_model.cpp'))
    (New-HostTest 'cuckoo_pi_model_host_test.cpp' @('src/cuckoo_pi_model.cpp'))
    (New-HostTest 'cuckoo_pi_mqtt_policy_host_test.cpp' @('src/cuckoo_pi_mqtt_policy.cpp', 'src/cuckoo_pi_config.cpp', 'src/cuckoo_pi_model.cpp'))
    (New-HostTest 'cuckoo_pi_screen_host_test.cpp' @('src/cuckoo_pi_screen.cpp'))
    (New-HostTest 'cuckoo_pi_settings_state_host_test.cpp' @('src/cuckoo_pi_settings_state.cpp', 'src/cuckoo_pi_config.cpp', 'src/cuckoo_pi_model.cpp'))
    (New-HostTest 'config_gateway_device_id_lifetime_host_test.cpp')
    (New-HostTest 'config_gateway_serialization_log_host_test.cpp')
    (New-HostTest 'cst3530_touch_host_test.cpp' @('lib/cst3530/cst3530_touch.cpp'))
    (New-HostTest 'gateway_alarm_close_persistence_host_test.cpp' @('src/gateway_alarm.cpp', 'src/gateway_alert_state.cpp', 'src/gateway_alarm_lifecycle.cpp', 'src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_alarm_close_pipeline_host_test.cpp' @('src/gateway_alarm.cpp', 'src/gateway_alert_state.cpp', 'src/gateway_alarm_lifecycle.cpp', 'src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_alarm_close_retry_backoff_host_test.cpp' @('src/gateway_alarm.cpp', 'src/gateway_alert_state.cpp', 'src/gateway_alarm_lifecycle.cpp', 'src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_alarm_event_window_host_test.cpp' @('src/gateway_alarm.cpp'))
    (New-HostTest 'gateway_alarm_host_test.cpp' @('src/gateway_alarm.cpp'))
    (New-HostTest 'gateway_alarm_lifecycle_host_test.cpp' @('src/gateway_alarm.cpp', 'src/gateway_alert_state.cpp', 'src/gateway_alarm_lifecycle.cpp', 'src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_alarm_stale_close_host_test.cpp' @('src/gateway_alarm.cpp', 'src/gateway_alert_state.cpp', 'src/gateway_alarm_lifecycle.cpp', 'src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_alarm_temperature_boundary_host_test.cpp' @('src/gateway_alarm.cpp'))
    (New-HostTest 'gateway_alert_state_host_test.cpp' @('src/gateway_alert_state.cpp'))
    (New-HostTest 'gateway_audio_alarm_host_test.cpp' @('src/gateway_audio.cpp', 'src/gravity_display_units.cpp'))
    (New-HostTest 'gateway_config_alarm_persistence_host_test.cpp' @('src/gateway_device_settings.cpp'))
    (New-HostTest 'gateway_config_boot_state_host_test.cpp')
    (New-HostTest 'gateway_config_integration_contract_host_test.cpp')
    (New-HostTest 'gateway_config_storage_host_test.cpp' @('src/gateway_config_storage.cpp'))
    (New-HostTest 'gateway_display_cycle_host_test.cpp' @('src/gateway_display_cycle.cpp', 'src/cuckoo_pi_config.cpp', 'src/cuckoo_pi_model.cpp'))
    (New-HostTest 'gateway_default_templates_host_test.cpp')
    (New-HostTest 'gateway_device_metadata_host_test.cpp')
    (New-HostTest 'gateway_device_settings_host_test.cpp' @('src/gateway_device_settings.cpp'))
    (New-HostTest 'gateway_http_template_host_test.cpp')
    (New-HostTest 'factory_route_contract_host_test.cpp')
    (New-HostTest 'gateway_measurement_integration_host_test.cpp')
    (New-HostTest 'gateway_orientation_host_test.cpp' @('src/gateway_orientation.cpp'))
    (New-HostTest 'gateway_push_test_policy_host_test.cpp')
    (New-HostTest 'gateway_rotation_teardown_host_test.cpp' @('src/gateway_rotation_teardown.cpp'))
    (New-HostTest 'gateway_runtime_policy_host_test.cpp')
    (New-HostTest 'gateway_settings_device_id_integration_host_test.cpp' @('src/gateway_settings_state.cpp', 'src/gateway_device_settings.cpp'))
    (New-HostTest 'gateway_settings_entry_host_test.cpp')
    (New-HostTest 'gateway_settings_layout_host_test.cpp' @('src/gateway_settings_layout.cpp'))
    (New-HostTest 'waveshare_35_ui_resolution_host_test.cpp')
    (New-HostTest 'gateway_settings_state_host_test.cpp' @('src/gateway_settings_state.cpp', 'src/gateway_device_settings.cpp'))
    (New-HostTest 'gateway_settings_thread_state_host_test.cpp')
    (New-HostTest 'gmt154_backlight_host_test.cpp')
    (New-HostTest 'gmt154_c3_build_contract_host_test.cpp')
    (New-HostTest 'gmt154_c3_display_memory_host_test.cpp' @() @() @('GMT154_06=1', 'GMT154_06_C3=1'))
    (New-HostTest 'gmt154_build_version_host_test.cpp')
    (New-HostTest 'gmt154_display_adapter_host_test.cpp' @() @() @('GMT154_06=1'))
    (New-HostTest 'gmt154_audio_contract_host_test.cpp')
    (New-HostTest 'gmt154_ui_policy_host_test.cpp' @('src/gateway_orientation.cpp') @() @('GMT154_06=1'))
    (New-HostTest 'gateway_touch_host_test.cpp' @('src/gateway_touch.cpp'))
    (New-HostTest 'gateway_ui_style_host_test.cpp' @('src/gateway_ui_style.cpp'))
    (New-HostTest 'layout2_ui_alignment_host_test.cpp')
    (New-HostTest 'layout2_alarm_ui_host_test.cpp')
    (New-HostTest 'layout2_ui_copy_host_test.cpp')
    (New-HostTest 'platformio_build_contract_host_test.cpp')
    (New-HostTest 'qmi8658_host_test.cpp' @('lib/qmi8658/Qmi8658.cpp'))
    (New-HostTest 'ring_color_host_test.cpp')
    (New-HostTest 'waveshare_35_board_io_host_test.cpp' @('src/gateway35_board_io.cpp'))
    (New-HostTest 'waveshare_35_audio_contract_host_test.cpp')
    (New-HostTest 'waveshare_35_display_contract_host_test.cpp')
    (New-HostTest 'waveshare_35_display_runtime_host_test.cpp' @('src/gateway35_display.cpp', 'src/gateway35_board_io.cpp', 'src/gateway35_touch.cpp') @() @('GATEWAY_35'))
    (New-HostTest 'waveshare_35_minimal_build_contract_host_test.cpp')
    (New-HostTest 'waveshare_35_touch_host_test.cpp' @('src/gateway35_touch.cpp'))
)
$pythonManifest = @(
    'test/host/gmt154_c3_ble_memory_profile_test.py'
    'test/host/gmt154_c3_lvgl24_config_test.py'
    'test/host/gmt154_lvgl_budget_integration_test.py'
    'test/host/gmt154_c3_single_app_patch_test.py'
    'test/host/gmt154_c3_wifi_recovery_host_test.py'
    'test/host/gmt154_c3_mdns_off_patch_test.py'
    'test/host/gmt154_c3_diag_dependency_hooks_test.py'
    'test/host/gmt154_c3_connection_budget_test.py'
)

Push-Location $Root
try {
    $compiler = (Get-Command g++ -ErrorAction Stop).Source
    $python = (Get-Command python -ErrorAction Stop).Source
    $testDirectory = Join-Path $Root 'test/host'
    $discovered = @(Get-ChildItem -LiteralPath $testDirectory -Filter '*_host_test.cpp' -File | Sort-Object Name | ForEach-Object {
        $_.FullName.Substring($Root.Length + 1).Replace('\', '/')
    })
    $manifestNames = @($manifest | ForEach-Object Name | Sort-Object)
    $missing = @($discovered | Where-Object { $_ -notin $manifestNames })
    $stale = @($manifestNames | Where-Object { $_ -notin $discovered })
    if ($missing.Count -gt 0 -or $stale.Count -gt 0) {
        if ($missing.Count -gt 0) { Write-Error "unregistered host test: $($missing -join ', ')" }
        if ($stale.Count -gt 0) { Write-Error "manifest entry has no source: $($stale -join ', ')" }
        exit 2
    }

    $jsonCandidates = @(
        (Join-Path $Root '.pio/libdeps/gateway32s3wave-tft-sd/ArduinoJson/src')
        (Join-Path $Root '.pio/libdeps/gateway35s3wave-tft-sd/ArduinoJson/src')
    )
    if ($env:GRAVITYMON_ARDUINOJSON_INCLUDE) { $jsonCandidates += $env:GRAVITYMON_ARDUINOJSON_INCLUDE }
    $jsonInclude = $jsonCandidates | Where-Object { Test-Path (Join-Path $_ 'ArduinoJson.h') } | Select-Object -First 1
    if (-not $jsonInclude) {
        $jsonInclude = Get-ChildItem -LiteralPath ([System.IO.Path]::GetTempPath()) -Filter 'ArduinoJson.h' -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '[\\/]ArduinoJson[\\/](src[\\/])?ArduinoJson\.h$' } |
            Select-Object -First 1 -ExpandProperty DirectoryName
    }
    if (-not $jsonInclude) { Write-Error 'ArduinoJson.h was not found; set GRAVITYMON_ARDUINOJSON_INCLUDE to the dependency include directory'; exit 3 }

    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
    $commonArgs = @('-std=c++17', '-Wall', '-Wextra', '-pedantic', '-Itest/host/stubs', '-Isrc', '-Ilib', "-I$jsonInclude")
    if ($NDebug) { $commonArgs += '-DNDEBUG' }
    $passed = 0
    $total = $manifest.Count + $pythonManifest.Count
    foreach ($entry in $manifest) {
        $safeName = ($entry.Name -replace '[^A-Za-z0-9_.-]', '_') -replace '\.cpp$', ''
        $exe = Join-Path $OutputDirectory ($safeName + $(if ($NDebug) { '-ndebug' } else { '' }) + '.exe')
        $log = Join-Path $OutputDirectory ($safeName + $(if ($NDebug) { '-ndebug' } else { '' }) + '.log')
        if (Test-Path $exe) { Remove-Item -LiteralPath $exe -Force }
        $includeArgs = @($entry.IncludePaths | ForEach-Object { "-I$(Join-Path $Root $_)" })
        $defines = @($entry.Defines | ForEach-Object { "-D$_" })
        $args = @($commonArgs + $includeArgs + $defines + (Join-Path $Root $entry.Name) + @($entry.Sources | ForEach-Object { Join-Path $Root $_ }) + @('-o', $exe))
        & $compiler @args *> $log
        $compileCode = $LASTEXITCODE
        if ($compileCode -ne 0) {
            Write-Output "FAIL compile $($entry.Name) exit=$compileCode log=$log"
            continue
        }
        & $exe >> $log 2>&1
        $runCode = $LASTEXITCODE
        if ($runCode -eq 0) {
            $passed++
            Write-Output "PASS $($entry.Name)"
        } else {
            Write-Output "FAIL run $($entry.Name) exit=$runCode log=$log"
        }
    }
    foreach ($testName in $pythonManifest) {
        $testPath = Join-Path $Root $testName
        if (-not (Test-Path -LiteralPath $testPath -PathType Leaf)) {
            Write-Output "FAIL missing $testName"
            continue
        }
        $safeName = ($testName -replace '[^A-Za-z0-9_.-]', '_')
        $log = Join-Path $OutputDirectory ($safeName + '.log')
        & $python $testPath *> $log
        $runCode = $LASTEXITCODE
        if ($runCode -eq 0) {
            $passed++
            Write-Output "PASS $testName"
        } else {
            Write-Output "FAIL run $testName exit=$runCode log=$log"
        }
    }
    Write-Output "Host tests: $passed/$total passed$(if ($NDebug) { ' (NDEBUG)' } else { '' })"
    if ($passed -ne $total) { exit 1 }
} finally {
    Pop-Location
}
