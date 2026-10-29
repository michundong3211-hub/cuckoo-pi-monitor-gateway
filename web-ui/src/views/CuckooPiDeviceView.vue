<template>
  <div class="container py-3">
    <template v-if="!configuredDevice">
      <h2>Cuckoo Pi device not found</h2>
      <p>The requested Cuckoo Pi device is not configured.</p>
      <RouterLink :to="{ name: 'cuckoo-pi' }">Back to Cuckoo Pi devices</RouterLink>
    </template>

    <template v-else>
      <div class="d-flex justify-content-between align-items-center mb-3">
        <h2>Cuckoo Pi: {{ configuredDevice.id }}</h2>
        <RouterLink :to="{ name: 'cuckoo-pi' }">Back to Cuckoo Pi devices</RouterLink>
      </div>

      <div class="mb-4">
        <label class="form-label" for="cuckoo-device-id">Device ID</label>
        <input
          id="cuckoo-device-id"
          class="form-control"
          data-testid="cuckoo-device-id"
          :value="configuredDevice.id"
          readonly
        />
      </div>

      <div class="alert alert-secondary">
        <span :class="['badge', statusFor.online ? 'bg-success' : 'bg-danger']">
          {{ statusFor.online ? 'Online' : 'Offline' }}
        </span>
        <span v-if="!statusFor.mqtt_connected" class="badge bg-warning text-dark ms-2">
          Broker disconnected
        </span>
        <span class="ms-2">Last message: {{ lastMessage }}</span>
      </div>

      <form class="needs-validation" novalidate @submit.prevent="save">
        <div class="cuckoo-alarm-grid">
          <div data-testid="offline-alarm-row" class="row g-3 align-items-end">
            <div class="col-md-4 form-check form-switch px-3 px-md-4 d-flex align-items-center">
              <input
                id="offline-alarm"
                v-model="draft.offline_alarm_enabled"
                class="form-check-input"
                data-testid="offline-alarm"
                type="checkbox"
                :disabled="global.disabled"
              />
              <label class="form-check-label ms-2" for="offline-alarm">Offline alarm</label>
            </div>
            <div class="col-md-8">
              <label class="form-label" for="offline-timeout">Offline timeout (seconds)</label>
              <input
                id="offline-timeout"
                v-model.number="draft.offline_timeout_seconds"
                class="form-control"
                data-testid="offline-timeout"
                type="number"
                min="60"
                max="86400"
                required
                :disabled="global.disabled"
              />
            </div>
          </div>
          <div data-testid="deviation-alarm-row" class="row g-3 align-items-end mt-1">
            <div class="col-md-4 form-check form-switch px-3 px-md-4 d-flex align-items-center">
              <input
                id="deviation-alarm"
                v-model="draft.beer_deviation_alarm_enabled"
                class="form-check-input"
                data-testid="deviation-alarm"
                type="checkbox"
                :disabled="global.disabled"
              />
              <label class="form-check-label ms-2" for="deviation-alarm">
                Beer deviation alarm
              </label>
            </div>
            <div class="col-md-4">
              <label class="form-label" for="deviation-threshold">Threshold (°C)</label>
              <input
                id="deviation-threshold"
                v-model.number="draft.beer_deviation_threshold_c"
                class="form-control"
                data-testid="deviation-threshold"
                type="number"
                min="0.1"
                max="20"
                step="0.1"
                required
                :disabled="global.disabled"
              />
            </div>
            <div class="col-md-4">
              <label class="form-label" for="deviation-hold">Hold time (seconds)</label>
              <input
                id="deviation-hold"
                v-model.number="draft.beer_deviation_hold_seconds"
                class="form-control"
                data-testid="deviation-hold"
                type="number"
                min="10"
                max="86400"
                required
                :disabled="global.disabled"
              />
            </div>
          </div>
        </div>

        <div class="mt-4 d-flex gap-2">
          <button
            class="btn btn-primary"
            data-testid="save-cuckoo-device"
            type="submit"
            :disabled="global.disabled"
          >
            Save
          </button>
          <button
            class="btn btn-secondary"
            data-testid="cancel-cuckoo-device"
            type="button"
            :disabled="global.disabled"
            @click="cancel"
          >
            Cancel
          </button>
        </div>
      </form>
    </template>
  </div>
</template>

<script setup>
import { computed, reactive, watch } from 'vue'
import { useRouter } from 'vue-router'
import { validateCurrentForm } from '@mp-se/espframework-ui-components'
import { config, global, status } from '@/modules/pinia'
import {
  defaultCuckooPiDevice,
  formatCuckooDuration,
  normalizeCuckooPiDeviceId
} from '@/modules/cuckooPi'

const props = defineProps({
  deviceId: {
    type: String,
    required: true
  }
})

const router = useRouter()
const draft = reactive({})

const normalizedDeviceId = computed(() => normalizeCuckooPiDeviceId(props.deviceId))
const configuredDevice = computed(() =>
  config.cuckoo_pi.devices.find((device) => device.id === normalizedDeviceId.value)
)
const statusFor = computed(
  () =>
    status.cuckoo_pi_device.find((device) => device.id === normalizedDeviceId.value) ?? {
      online: false,
      mqtt_connected: false,
      update_time: 0
    }
)
const lastMessage = computed(() => {
  const runtime = statusFor.value
  return runtime.online || Number(runtime.update_time) > 0
    ? `${formatCuckooDuration(runtime.update_time)} ago`
    : '--'
})
watch(
  configuredDevice,
  (device) => {
    for (const key of Object.keys(draft)) delete draft[key]
    if (device) {
      Object.assign(draft, {
        ...defaultCuckooPiDevice(normalizedDeviceId.value),
        ...device
      })
    }
  },
  { immediate: true }
)

function validateDraft() {
  const ranges = [
    ['offline_timeout_seconds', 60, 86400, 'Offline timeout must be between 60 and 86400 seconds.'],
    [
      'beer_deviation_threshold_c',
      0.1,
      20,
      'Beer deviation threshold must be between 0.1 and 20 °C.'
    ],
    [
      'beer_deviation_hold_seconds',
      10,
      86400,
      'Beer deviation hold time must be between 10 and 86400 seconds.'
    ]
  ]
  for (const [field, minimum, maximum, message] of ranges) {
    const value = Number(draft[field])
    if (!Number.isFinite(value) || value < minimum || value > maximum) {
      global.messageError = message
      return false
    }
  }
  return true
}

async function save() {
  if (!configuredDevice.value || !validateCurrentForm() || !validateDraft()) return
  const target = config.cuckoo_pi.devices.find((device) => device.id === normalizedDeviceId.value)
  if (!target) return
  const original = { ...target }

  Object.assign(target, {
    offline_alarm_enabled: draft.offline_alarm_enabled,
    offline_timeout_seconds: Number(draft.offline_timeout_seconds),
    beer_deviation_alarm_enabled: draft.beer_deviation_alarm_enabled,
    beer_deviation_threshold_c: Number(draft.beer_deviation_threshold_c),
    beer_deviation_hold_seconds: Number(draft.beer_deviation_hold_seconds)
  })

  try {
    const saved = await config.saveAll()
    if (saved !== true) {
      Object.assign(target, original)
      return
    }
  } catch {
    Object.assign(target, original)
    return
  }

  await router.push({ name: 'cuckoo-pi' })
}

async function cancel() {
  await router.push({ name: 'cuckoo-pi' })
}
</script>
