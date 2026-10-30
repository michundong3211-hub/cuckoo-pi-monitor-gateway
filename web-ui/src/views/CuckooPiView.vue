<template>
  <div class="container py-3">
    <h2>Cuckoo Pi</h2>
    <div class="alert alert-info" role="note">
      Plain MQTT, read-only monitoring. The gateway subscribes only to the Status topic and never
      publishes mode, setpoint, or ACK messages.
    </div>

    <form class="needs-validation" novalidate @submit.prevent="save">
      <div class="form-check form-switch mb-3">
        <input
          id="cuckoo-enabled"
          v-model="draft.enabled"
          class="form-check-input"
          type="checkbox"
          :disabled="global.disabled"
        />
        <label class="form-check-label" for="cuckoo-enabled">Enable Cuckoo Pi monitoring</label>
      </div>

      <div class="row g-3 mb-3">
        <div class="col-md-8"><label for="mqtt-broker" class="form-label">MQTT broker</label><input id="mqtt-broker" v-model.trim="draft.broker" class="form-control" :disabled="global.disabled" /></div>
        <div class="col-md-4"><label for="mqtt-port" class="form-label">Port</label><input id="mqtt-port" :value="draft.port || ''" class="form-control" type="number" min="1" max="65535" placeholder="e.g. 1883" :disabled="global.disabled" @input="updatePort" /></div>
        <div class="col-md-6"><label for="mqtt-username" class="form-label">Username</label><input id="mqtt-username" v-model="draft.username" class="form-control" autocomplete="off" :disabled="global.disabled" /></div>
        <div class="col-md-6"><label for="mqtt-password" class="form-label">Password</label><input id="mqtt-password" v-model="draft.password" class="form-control" type="password" autocomplete="new-password" :disabled="global.disabled" /></div>
      </div>
      <div class="input-group mb-4">
        <input
          v-model="newDeviceId"
          class="form-control"
          data-testid="new-cuckoo-id"
          placeholder="12-digit device ID, e.g. 001122334455"
          :disabled="global.disabled"
          @keyup.enter.prevent="addDevice"
        />
        <button
          class="btn btn-outline-primary"
          data-testid="add-cuckoo-device"
          type="button"
          :disabled="global.disabled"
          @click="addDevice"
        >
          Add device
        </button>
      </div>

      <div data-testid="cuckoo-device-list">
        <div
          v-for="device in draft.devices"
          :key="device.id"
          class="card mb-3"
          :data-testid="'cuckoo-device-row-' + device.id"
        >
          <div class="card-body d-flex flex-wrap align-items-center gap-2">
            <strong class="me-auto">Cuckoo Pi: {{ device.id }}</strong>
            <span :class="['badge', statusFor(device.id).online ? 'bg-success' : 'bg-danger']">
              {{ statusFor(device.id).online ? 'Online' : 'Offline' }}
            </span>
            <span v-if="!statusFor(device.id).mqtt_connected" class="badge bg-warning text-dark">
              Broker disconnected
            </span>
            <span>Last message: {{ lastMessage(device.id) }}</span>
            <RouterLink
              class="btn btn-sm btn-outline-primary"
              :to="{ name: 'cuckoo-pi-device', params: { deviceId: device.id } }"
              :data-testid="'edit-cuckoo-' + device.id"
            >
              Edit
            </RouterLink>
            <button
              class="btn btn-sm btn-outline-danger"
              :data-testid="'remove-cuckoo-' + device.id"
              type="button"
              :disabled="global.disabled"
              @click="removeDevice(device.id)"
            >
              Remove
            </button>
          </div>
        </div>
      </div>

      <button
        type="submit"
        class="btn btn-primary"
        :disabled="!canSave"
      >
        Save
      </button>
    </form>
  </div>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { validateCurrentForm } from '@mp-se/espframework-ui-components'
import { config, global, status } from '@/modules/pinia'
import {
  MAX_CUCKOO_PI_DEVICES,
  defaultCuckooPiDevice,
  formatCuckooDuration,
  normalizeCuckooPiConfig,
  normalizeCuckooPiDeviceId
} from '@/modules/cuckooPi'

const newDeviceId = ref('')
const cloneValue = (value) => JSON.parse(JSON.stringify(value))
const initialConfig = normalizeCuckooPiConfig(config.cuckoo_pi)
const draft = reactive({
  enabled: initialConfig.enabled,
  broker: initialConfig.broker,
  port: initialConfig.port,
  client_id: initialConfig.client_id,
  username: initialConfig.username,
  password: initialConfig.password,
  devices: initialConfig.devices.map((device) => cloneValue(device))
})
const serializeDraft = (value) => JSON.stringify(value)
const savedDraftSnapshot = ref(serializeDraft(cloneValue(draft)))
const draftChanged = computed(() => serializeDraft(draft) !== savedDraftSnapshot.value)
const saving = ref(false)
const canSave = computed(
  () => !global.disabled && !saving.value && (draftChanged.value || global.configChanged)
)

function addDevice() {
  global.messageError = ''
  const id = normalizeCuckooPiDeviceId(newDeviceId.value)
  if (!id) {
    global.messageError = 'Device ID must contain exactly 12 hexadecimal digits.'
    return
  }
  if (draft.devices.some((device) => device.id === id)) {
    global.messageError = `Cuckoo Pi ${id} is already configured.`
    return
  }
  if (draft.devices.length >= MAX_CUCKOO_PI_DEVICES) {
    global.messageError = `A maximum of ${MAX_CUCKOO_PI_DEVICES} Cuckoo Pi devices is supported.`
    return
  }
  draft.devices.push(defaultCuckooPiDevice(id))
  newDeviceId.value = ''
}

function removeDevice(id) {
  if (!window.confirm(`Remove Cuckoo Pi ${id}?`)) return
  draft.devices = draft.devices.filter((device) => device.id !== id)
}

function statusFor(id) {
  return (
    status.cuckoo_pi_device.find((device) => device.id === id) ?? {
      online: false,
      mqtt_connected: false
    }
  )
}

function lastMessage(id) {
  const runtime = statusFor(id)
  return runtime.online || Number(runtime.update_time) > 0
    ? `${formatCuckooDuration(runtime.update_time)} ago`
    : '--'
}

function updatePort(event) {
  const value = event.target.value
  draft.port = value === '' ? 0 : Number(value)
}

async function save() {
  if (!canSave.value) return
  if (!validateCurrentForm()) return
  if (
    !draft.broker ||
    !Number.isInteger(draft.port) ||
    draft.port < 1 ||
    draft.port > 65535
  ) {
    global.messageError = 'Enter a valid MQTT broker and port.'
    return
  }
  const credentialLimits = [
    ['Client ID', draft.client_id, 39],
    ['Username', draft.username, 63],
    ['Password', draft.password, 63]
  ]
  const invalidCredential = credentialLimits.find(
    ([, value, maxBytes]) =>
      typeof value !== 'string' || new TextEncoder().encode(value).length > maxBytes
  )
  if (invalidCredential) {
    global.messageError = `${invalidCredential[0]} is too long.`
    return
  }

  const submittedSnapshot = cloneValue(draft)
  const submittedSerialized = serializeDraft(submittedSnapshot)
  const original = cloneValue(config.cuckoo_pi)
  saving.value = true
  config.cuckoo_pi = submittedSnapshot

  try {
    const saved = await config.saveAll()
    if (saved === true) {
      savedDraftSnapshot.value = submittedSerialized
      return
    }
    config.cuckoo_pi = original
  } catch {
    // Keep the draft and existing error message when saving fails unexpectedly.
    config.cuckoo_pi = original
  } finally {
    saving.value = false
  }
}
</script>
