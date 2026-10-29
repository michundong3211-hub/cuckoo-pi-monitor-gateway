export const ALARM_RING_COLORS = Object.freeze([
  { label: 'Yellow', value: 0, hex: '#FFD700' },
  { label: 'Purple', value: 1, hex: '#61026F' },
  { label: 'Red', value: 2, hex: '#FF4500' },
  { label: 'Green', value: 3, hex: '#07A73E' },
  { label: 'Blue', value: 4, hex: '#1E90FF' },
  { label: 'White', value: 5, hex: '#D5D1CF' },
  { label: 'Black', value: 6, hex: '#000000' },
  { label: 'Default', value: 7, hex: '#45D9B2' }
])

export const OFFLINE_TIMEOUT_MIN_SECONDS = 60
export const OFFLINE_TIMEOUT_MAX_SECONDS = 86400
export const OFFLINE_TIMEOUT_STEP_SECONDS = 60
export const OFFLINE_TIMEOUT_DEFAULT_SECONDS = 3660
export const OFFLINE_TIMEOUT_LEGACY_FALLBACK_SECONDS = 900

const clamp = (value, minimum, maximum, fallback) => {
  const number = Number(value)
  if (!Number.isFinite(number)) return fallback
  return Math.min(maximum, Math.max(minimum, number))
}

export const createDefaultAlarmSettings = (deviceId) => ({
  id: String(deviceId || ''),
  og_sg: 1.05,
  temp_min_c: 18,
  temp_max_c: 28,
  low_battery_percent: 20,
  buzzer_enabled: false,
  ring_color_index: 7,
  temperature_change_alarm_enabled: false,
  temperature_compare_minutes: 5,
  temperature_change_threshold_c: 1,
  offline_alarm_enabled: false,
  offline_timeout_seconds: OFFLINE_TIMEOUT_DEFAULT_SECONDS,
  offline_alarm_dismissed: false
})

export const normalizeAlarmSettings = (settings = {}) => {
  const normalized = {
    ...createDefaultAlarmSettings(settings.id),
    ...settings
  }

  normalized.id = String(settings.id || '')
  normalized.og_sg = clamp(settings.og_sg, 1, 1.2, 1.05)
  normalized.ring_color_index = ALARM_RING_COLORS.some(
    (color) => color.value === Number(settings.ring_color_index)
  )
    ? Number(settings.ring_color_index)
    : 7
  normalized.temperature_change_alarm_enabled = Boolean(
    settings.temperature_change_alarm_enabled
  )
  normalized.temperature_compare_minutes = Math.round(
    clamp(settings.temperature_compare_minutes, 1, 60, 5)
  )
  normalized.temperature_change_threshold_c = clamp(
    settings.temperature_change_threshold_c,
    0.1,
    10,
    1
  )
  normalized.offline_alarm_enabled = Boolean(settings.offline_alarm_enabled)
  normalized.offline_timeout_seconds = Math.round(
    clamp(
      settings.offline_timeout_seconds,
      OFFLINE_TIMEOUT_MIN_SECONDS,
      OFFLINE_TIMEOUT_MAX_SECONDS,
      OFFLINE_TIMEOUT_LEGACY_FALLBACK_SECONDS
    ) / OFFLINE_TIMEOUT_STEP_SECONDS
  ) * OFFLINE_TIMEOUT_STEP_SECONDS
  normalized.offline_alarm_dismissed = Boolean(settings.offline_alarm_dismissed)
  return normalized
}

export const normalizeAlarmSettingsList = (settings) =>
  Array.isArray(settings)
    ? settings.filter((entry) => entry && entry.id).map(normalizeAlarmSettings)
    : []

export const upsertAlarmSettings = (settings, replacement) => {
  const current = Array.isArray(settings) ? settings : []
  const normalized = normalizeAlarmSettings(replacement)
  let replaced = false
  const updated = current.map((entry) => {
    if (entry.id !== normalized.id) return { ...entry }
    replaced = true
    return normalized
  })
  if (!replaced) updated.push(normalized)
  return updated
}
