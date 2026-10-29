<!--
 * GravityMon Gateway UI
 * Copyright (c) 2021-2026 Magnus
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 -->
<template>
  <div class="container">
    <p></p>

    <div v-if="status" class="container overflow-hidden text-center">
      <div class="row gy-4">
        <template v-for="g in status.gravity_device" :key="g.device">
          <div class="col-md-4">
            <BsCard
              :header="'Gravity Device: ' + g.device"
              color="info"
              :title="
                g.name + ' (' + formatTime(g.update_time) + ' / ' + formatTime(g.push_time) + ')'
              "
            >
              <p class="text-center">
                Gravity: {{ formatGravity(g.gravity) }}
                {{ config.gravity_unit === 'G' ? ' SG' : ' P' }} Temperature:
                {{ formatTemp(g.temp) }} {{ config.temp_unit }}
              </p>

              <span class="badge bg-primary">{{ g.source }}</span
              >&nbsp;
              <span class="badge bg-primary">{{ displayGravityDeviceType(g.type) }}</span>
              <template v-if="offlineAlarmDismissed(g.device)">
                &nbsp;
                <span
                  class="badge bg-danger"
                  :data-testid="'alarm-dismissed-' + g.device"
                  title="Offline alarm dismissed until this device reports again"
                >
                  Offline alarm dismissed
                </span>
              </template>
              <div class="mt-3">
                <RouterLink
                  class="btn btn-sm btn-outline-primary"
                  :to="{ name: 'alarm-settings', params: { deviceId: g.device } }"
                  :data-testid="'alarm-settings-' + g.device"
                >
                  Alarm settings
                </RouterLink>
              </div>
            </BsCard>
          </div>
        </template>

        <template v-for="p in status.pressure_device" :key="p.device">
          <div class="col-md-4">
            <BsCard
              :header="'Pressure Device: ' + p.device"
              color="info"
              :title="
                p.name + ' (' + formatTime(p.update_time) + ' / ' + formatTime(p.push_time) + ')'
              "
            >
              <p class="text-center">
                Pressure: {{ formatPressure(p.pressure) }}
                {{
                  config.pressure_unit === 'PSI'
                    ? ' psi'
                    : config.pressure_unit === 'kPa'
                      ? ' kPa'
                      : ' Bar'
                }}
                <template v-if="p.pressure1 !== undefined">
                  , Pressure1: {{ formatPressure(p.pressure1) }}
                  {{
                    config.pressure_unit === 'PSI'
                      ? ' psi'
                      : config.pressure_unit === 'kPa'
                        ? ' kPa'
                        : ' Bar'
                  }}
                </template>
                <template v-if="p.temp !== undefined">
                  , Temperature: {{ formatTemp(p.temp) }} {{ config.temp_unit }}
                </template>
              </p>

              <span class="badge bg-primary">{{ p.source }}</span
              >&nbsp;
              <span class="badge bg-primary">{{ p.type }}</span>
            </BsCard>
          </div>
        </template>

        <template v-for="t in status.temperature_device" :key="t.device">
          <div class="col-md-4">
            <BsCard
              header="Temperature Device"
              color="secondary"
              :title="t.name === '' ? t.device : t.name + ' (' + formatTime(t.update_time) + ')'"
            >
              <p class="text-center">
                <template v-if="t.chamber_temp !== undefined">
                  Chamber: {{ formatTemp(t.chamber_temp) }} {{ config.temp_unit }}
                </template>
                <template v-if="t.beer_temp !== undefined">
                  , Beer: {{ formatTemp(t.beer_temp) }} {{ config.temp_unit }}
                </template>
              </p>

              <span class="badge bg-primary">{{ t.source }}</span
              >&nbsp;
              <span class="badge bg-primary">{{ t.type }}</span>
            </BsCard>
          </div>
        </template>

        <template v-for="device in status.cuckoo_pi_device" :key="device.id">
          <div class="col-md-4" :data-testid="'cuckoo-card-' + device.id">
            <div class="cuckoo-card">
              <div class="cuckoo-card-header">Cuckoo Pi: {{ device.id }}</div>
              <div class="cuckoo-card-body">
                <div v-if="!device.online || !device.mqtt_connected" class="cuckoo-card-alerts">
                  <span v-if="!device.online" class="badge bg-danger">Offline</span>
                  <span v-if="!device.mqtt_connected" class="badge bg-warning text-dark">
                    Broker disconnected
                  </span>
                </div>
                <div class="cuckoo-card-row">
                  <strong>Mode</strong>
                  <span>{{ device.mode || '--' }}</span>
                </div>
                <div class="cuckoo-card-row">
                  <strong>Beer</strong>
                  <span>{{ formatCuckooTemperature(device.beer_temp_c, config.temp_unit) }}&nbsp;&nbsp;&nbsp;&nbsp;{{ formatCuckooTemperature(device.beer_set_c, config.temp_unit) }}</span>
                </div>
                <div class="cuckoo-card-row">
                  <strong>Fridge</strong>
                  <span>{{ formatCuckooTemperature(device.fridge_temp_c, config.temp_unit) }}&nbsp;&nbsp;&nbsp;&nbsp;{{ formatCuckooTemperature(device.fridge_set_c, config.temp_unit) }}</span>
                </div>
                <div class="cuckoo-card-row">
                  <strong>State</strong>
                  <span>{{ device.state || '--' }}&nbsp;&nbsp;&nbsp;&nbsp;{{ device.status_time_seconds == null ? '--' : formatCuckooDuration(device.status_time_seconds) }}</span>
                </div>
                <div class="cuckoo-card-actions">
                  <RouterLink
                    class="btn btn-sm btn-outline-primary"
                    :to="{ name: 'cuckoo-pi-device', params: { deviceId: device.id } }"
                    :data-testid="'cuckoo-alarm-settings-' + device.id"
                  >
                    Alarm settings
                  </RouterLink>
                </div>
              </div>
            </div>
          </div>
        </template>

        <div class="col-md-4">
          <BsCard header="Device" title="WIFI">
            <p class="text-center">{{ status.rssi }} dBm - {{ status.wifi_ssid }}</p>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="IP Address">
            <p class="text-center">
              {{ status.ip }}
            </p>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="Memory">
            <p class="text-center">
              Free: {{ status.free_heap }} kb, Total: {{ status.total_heap }} kb
            </p>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="Software version">
            <p class="text-center">
              Firmware: {{ global.app_ver }}（{{ global.build_datetime || '--' }}）
            </p>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="Platform">
            Platform: <span class="badge bg-secondary">{{ global.platform }}</span> Board:
            <span class="badge bg-secondary">{{ global.boardDisplayName }}</span>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="Device ID">
            <p class="text-center">{{ status.id }}</p>
          </BsCard>
        </div>

        <div class="col-md-4">
          <BsCard header="Device" title="Uptime">
            <p class="text-center">
              {{ status.uptime_days }} days {{ status.uptime_hours }} hours
              {{ status.uptime_minutes }} minutes {{ status.uptime_seconds }} seconds
            </p>
          </BsCard>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { global, status, config } from '@/modules/pinia'
import { computed, ref, onBeforeMount, onBeforeUnmount } from 'vue'
import { psiToBar, psiToKPa, tempToF, gravityToPlato } from '@mp-se/espframework-ui-components'
import { formatCuckooDuration, formatCuckooTemperature } from '@/modules/cuckooPi'

const polling = ref(null)

function formatTime(t) {
  if (t < 60)
    // less than 1 min
    return new Number(t).toFixed(0) + 's'

  if (t < 60 * 60)
    // less than 1 hour
    return new Number(t / 60).toFixed(0) + 'm'

  if (t < 60 * 60 * 24)
    // less than 1 day
    return new Number(t / (60 * 60)).toFixed(0) + 'h'

  return new Number(t / (60 * 60 * 24)).toFixed(0) + 'd'
}

function formatGravity(g) {
  return config.gravity_unit === 'G'
    ? new Number(g).toFixed(3)
    : Number(gravityToPlato(Number(g))).toFixed(3)
}

function displayGravityDeviceType(type) {
  return type === 'Gravitymon' ? 'Cuckoo Tilt' : type
}

function formatPressure(p) {
  return config.pressure_unit === 'PSI'
    ? new Number(p).toFixed(3)
    : config.pressure_unit === 'kPa'
      ? new Number(psiToKPa(p)).toFixed(2)
      : new Number(psiToBar(p)).toFixed(2)
}

function formatTemp(t) {
  const value = Number(t)
  return Number(config.temp_unit === 'C' ? value : tempToF(value)).toFixed(2)
}

function offlineAlarmDismissed(deviceId) {
  return (
    Array.isArray(config.device_settings) &&
    config.device_settings.some(
      (settings) => settings.id === deviceId && settings.offline_alarm_dismissed === true
    )
  )
}

async function refresh() {
  await status.load()
}

onBeforeMount(() => {
  refresh()
  polling.value = setInterval(refresh, 10000)
})

onBeforeUnmount(() => {
  clearInterval(polling.value)
})
</script>

<style>
.cuckoo-card {
  min-width: 0;
  height: 100%;
  overflow: hidden;
  border: 1px solid #cde5d4;
  border-radius: 0.75rem;
  background: #fff;
}

.cuckoo-card-header {
  min-width: 0;
  padding: 0.65rem 0.75rem;
  overflow-wrap: anywhere;
  background: #e3f3e7;
  color: #286343;
  font-weight: 700;
  text-align: center;
}

.cuckoo-card-body {
  min-width: 0;
  padding: 0.5rem 0.75rem 0.75rem;
  text-align: center;
}

.cuckoo-card-alerts {
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  gap: 0.35rem;
  padding-bottom: 0.35rem;
}

.cuckoo-card-row {
  display: flex;
  min-width: 0;
  align-items: baseline;
  justify-content: center;
  gap: 0.5rem;
  padding: 0.35rem 0.25rem;
  border-bottom: 1px solid #e8f0ea;
}

.cuckoo-card-row > strong {
  flex: 0 0 auto;
}

.cuckoo-card-row > span {
  min-width: 0;
  overflow-wrap: anywhere;
}

.cuckoo-card-actions {
  padding-top: 0.6rem;
}
</style>
