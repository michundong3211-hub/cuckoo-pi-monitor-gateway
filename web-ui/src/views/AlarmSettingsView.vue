<template>
  <div class="container py-3">
    <p class="h2">Alarm settings: {{ deviceId }}</p>
    <hr />

    <BsMessage v-if="!deviceId" message="" alert="danger">
      No gravity device was selected.
    </BsMessage>

    <form v-else class="needs-validation" novalidate @submit.prevent="saveSettings">
      <div class="row gy-3">
        <div class="col-md-6">
          <label for="alarm-og" class="form-label">Original gravity (SG)</label>
          <input
            id="alarm-og"
            :value="originalGravityInput"
            class="form-control"
            type="text"
            inputmode="decimal"
            required
            :disabled="global.disabled"
            @input="updateOriginalGravity"
            @blur="formatOriginalGravityInput"
          />
          <div class="form-text">Allowed range: 1.000–1.200.</div>
        </div>

        <div class="col-md-6">
          <label for="alarm-ring-color" class="form-label">Ring color</label>
          <div class="input-group">
            <span
              class="input-group-text alarm-color-preview"
              :style="{ backgroundColor: selectedColor.hex }"
              aria-hidden="true"
            ></span>
            <select
              id="alarm-ring-color"
              v-model.number="form.ring_color_index"
              class="form-select"
              :disabled="global.disabled"
            >
              <option v-for="color in ALARM_RING_COLORS" :key="color.value" :value="color.value">
                {{ color.label }} ({{ color.hex }})
              </option>
            </select>
          </div>
        </div>

        <div class="col-12"><hr /></div>

        <div class="col-12">
          <div class="form-check form-switch">
            <input
              id="temperature-change-alarm"
              v-model="form.temperature_change_alarm_enabled"
              class="form-check-input"
              type="checkbox"
              :disabled="global.disabled"
            />
            <label class="form-check-label" for="temperature-change-alarm">
              Temperature change alarm
            </label>
          </div>
        </div>

        <div class="col-md-6">
          <label for="temperature-compare-minutes" class="form-label">Compare interval (minutes)</label>
          <input
            id="temperature-compare-minutes"
            v-model.number="form.temperature_compare_minutes"
            class="form-control"
            type="number"
            min="1"
            max="60"
            step="1"
            required
            :disabled="global.disabled || !form.temperature_change_alarm_enabled"
          />
        </div>

        <div class="col-md-6">
          <label for="temperature-threshold" class="form-label">Change threshold (°C)</label>
          <input
            id="temperature-threshold"
            v-model.number="form.temperature_change_threshold_c"
            class="form-control"
            type="number"
            min="0.1"
            max="10.0"
            step="0.1"
            required
            :disabled="global.disabled || !form.temperature_change_alarm_enabled"
          />
        </div>

        <div class="col-12"><hr /></div>

        <div class="col-12">
          <div class="form-check form-switch">
            <input
              id="offline-alarm"
              v-model="form.offline_alarm_enabled"
              class="form-check-input"
              type="checkbox"
              :disabled="global.disabled"
            />
            <label class="form-check-label" for="offline-alarm">Offline alarm</label>
          </div>
        </div>

        <div class="col-md-6">
          <label for="offline-timeout" class="form-label">Offline timeout (seconds)</label>
          <input
            id="offline-timeout"
            v-model.number="form.offline_timeout_seconds"
            class="form-control"
            type="number"
            :min="OFFLINE_TIMEOUT_MIN_SECONDS"
            :max="OFFLINE_TIMEOUT_MAX_SECONDS"
            :step="OFFLINE_TIMEOUT_STEP_SECONDS"
            required
            :disabled="global.disabled || !form.offline_alarm_enabled"
          />
          <div class="form-text">Allowed range: 60–86400 seconds, in 60-second steps.</div>
        </div>

        <div class="col-12">
          <button type="submit" class="btn btn-primary" :disabled="global.disabled">
            Save alarm settings
          </button>
          <a class="btn btn-secondary ms-2" href="/" :class="{ disabled: global.disabled }">Back</a>
        </div>
      </div>
    </form>
  </div>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { useRoute } from 'vue-router'
import { validateCurrentForm } from '@mp-se/espframework-ui-components'
import { config, global } from '@/modules/pinia'
import {
  ALARM_RING_COLORS,
  OFFLINE_TIMEOUT_MAX_SECONDS,
  OFFLINE_TIMEOUT_MIN_SECONDS,
  OFFLINE_TIMEOUT_STEP_SECONDS,
  createDefaultAlarmSettings,
  normalizeAlarmSettings,
  upsertAlarmSettings
} from '@/modules/alarmSettings'

const route = useRoute()
const deviceId = computed(() => String(route.params.deviceId || ''))
const existing = Array.isArray(config.device_settings)
  ? config.device_settings.find((entry) => entry.id === deviceId.value)
  : null
const form = reactive(
  normalizeAlarmSettings(existing || createDefaultAlarmSettings(deviceId.value))
)

const formatOriginalGravity = (value) => {
  if (value === '' || value === null || value === undefined) return ''
  const number = Number(value)
  return Number.isFinite(number) ? number.toFixed(3) : ''
}

const originalGravityInput = ref(formatOriginalGravity(form.og_sg))

const selectedColor = computed(
  () => ALARM_RING_COLORS.find((color) => color.value === form.ring_color_index) || ALARM_RING_COLORS[7]
)

const updateOriginalGravity = (event) => {
  originalGravityInput.value = event.target.value
}

const formatOriginalGravityInput = (event) => {
  const value = originalGravityInput.value
  if (isValidOriginalGravity(value)) {
    originalGravityInput.value = formatOriginalGravity(value)
  }
  event.target.value = originalGravityInput.value
}

const isValidOriginalGravity = (value) => {
  const number = Number(value)
  return (
    Number.isFinite(number) &&
    number >= 1 &&
    number <= 1.2 &&
    Math.abs(number * 1000 - Math.round(number * 1000)) < 1e-9
  )
}

const saveSettings = async () => {
  if (!isValidOriginalGravity(originalGravityInput.value)) return
  if (!validateCurrentForm()) return
  const next = normalizeAlarmSettings({
    ...form,
    id: deviceId.value,
    og_sg: Number(originalGravityInput.value)
  })
  config.device_settings = upsertAlarmSettings(config.device_settings, next)
  Object.assign(form, next)
  originalGravityInput.value = formatOriginalGravity(next.og_sg)
  await config.saveAll()
}
</script>

<style scoped>
.alarm-color-preview {
  min-width: 2.75rem;
  border-color: var(--bs-border-color);
}
</style>
