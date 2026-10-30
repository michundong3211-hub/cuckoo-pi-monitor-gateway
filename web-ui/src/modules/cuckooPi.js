const DEVICE_ID_PATTERN = /^[0-9a-f]{12}$/

export const MAX_CUCKOO_PI_DEVICES = 8

const DEFAULT_CUCKOO_PI_BROKER = ''
const DEFAULT_CUCKOO_PI_PORT = 0
const PREVIOUS_DEFAULT_CUCKOO_PI_PORT = 1884
const LEGACY_DEFAULT_CUCKOO_PI_PORT = 2884
const DEFAULT_CUCKOO_PI_CLIENT_ID = ''
const DEFAULT_CUCKOO_PI_USERNAME = ''
const DEFAULT_CUCKOO_PI_PASSWORD = ''
const DEFAULT_CUCKOO_PI_DEVICE_ID = ''

export function normalizeCuckooPiDeviceId(value) {
  const normalized = String(value ?? '')
    .toLowerCase()
    .replace(/[\s:-]/g, '')
  return DEVICE_ID_PATTERN.test(normalized) ? normalized : ''
}

export function buildCuckooPiTopics(deviceId) {
  const id = normalizeCuckooPiDeviceId(deviceId)
  if (!id) return null
  return {
    status: `/cuckoo/wenkong/${id}`,
    mode: `/cuckoo/mode/${id}`,
    beerSet: `/cuckoo/beer/${id}`,
    fridgeSet: `/cuckoo/ice/${id}`,
    ack: `/cuckoo/mode/ack/${id}`
  }
}

export function defaultCuckooPiDevice(deviceId) {
  return {
    id: normalizeCuckooPiDeviceId(deviceId),
    offline_alarm_enabled: false,
    offline_timeout_seconds: 300,
    beer_deviation_alarm_enabled: false,
    beer_deviation_threshold_c: 2,
    beer_deviation_hold_seconds: 600,
    offline_alarm_dismissed: false,
    offline_dismissed_status_time_seconds: 0
  }
}

export function defaultCuckooPiConfig() {
  return {
    enabled: true,
    broker: DEFAULT_CUCKOO_PI_BROKER,
    port: DEFAULT_CUCKOO_PI_PORT,
    client_id: DEFAULT_CUCKOO_PI_CLIENT_ID,
    username: DEFAULT_CUCKOO_PI_USERNAME,
    password: DEFAULT_CUCKOO_PI_PASSWORD,
    devices: []
  }
}

export function normalizeCuckooPiConfig(value) {
  const source = value && typeof value === 'object' ? value : {}
  const broker =
    typeof source.broker === 'string' && source.broker.trim()
      ? source.broker.trim()
      : DEFAULT_CUCKOO_PI_BROKER
  let port = Number.isInteger(Number(source.port)) ? Number(source.port) : DEFAULT_CUCKOO_PI_PORT
  if (
    broker === DEFAULT_CUCKOO_PI_BROKER &&
    (port === PREVIOUS_DEFAULT_CUCKOO_PI_PORT || port === LEGACY_DEFAULT_CUCKOO_PI_PORT)
  ) {
    port = DEFAULT_CUCKOO_PI_PORT
  }
  const result = {
    ...defaultCuckooPiConfig(),
    enabled: Object.prototype.hasOwnProperty.call(source, 'enabled')
      ? source.enabled === true
      : true,
    broker,
    port
  }
  result.client_id = typeof source.client_id === 'string' ? source.client_id : DEFAULT_CUCKOO_PI_CLIENT_ID
  result.username = typeof source.username === 'string' ? source.username : DEFAULT_CUCKOO_PI_USERNAME
  result.password = typeof source.password === 'string' ? source.password : DEFAULT_CUCKOO_PI_PASSWORD
  const seen = new Set()
  const devices = Object.prototype.hasOwnProperty.call(source, 'devices')
    ? Array.isArray(source.devices)
      ? source.devices
      : []
    : result.devices
  result.devices = []
  for (const candidate of devices) {
    const id = normalizeCuckooPiDeviceId(candidate?.id)
    if (!id || seen.has(id) || result.devices.length >= MAX_CUCKOO_PI_DEVICES) continue
    seen.add(id)
    result.devices.push({
      ...defaultCuckooPiDevice(id),
      ...candidate,
      id,
      offline_alarm_enabled: candidate?.offline_alarm_enabled === true,
      offline_timeout_seconds: Number(candidate?.offline_timeout_seconds ?? 300),
      beer_deviation_alarm_enabled: candidate?.beer_deviation_alarm_enabled === true,
      beer_deviation_threshold_c: Number(candidate?.beer_deviation_threshold_c ?? 2),
      beer_deviation_hold_seconds: Number(candidate?.beer_deviation_hold_seconds ?? 600),
      offline_alarm_dismissed: candidate?.offline_alarm_dismissed === true,
      offline_dismissed_status_time_seconds: Number(
        candidate?.offline_dismissed_status_time_seconds ?? 0
      )
    })
  }
  return result
}

export function formatCuckooDuration(value) {
  if (!Number.isFinite(Number(value)) || Number(value) < 0) return '--'
  let remaining = Math.floor(Number(value))
  const days = Math.floor(remaining / 86400)
  remaining %= 86400
  const hours = Math.floor(remaining / 3600)
  remaining %= 3600
  const minutes = Math.floor(remaining / 60)
  const seconds = remaining % 60
  const parts = []
  if (days) parts.push(`${days}d`)
  if (hours || days) parts.push(`${hours}h`)
  if (minutes || hours || days) parts.push(`${minutes}m`)
  parts.push(`${seconds}s`)
  return parts.join(' ')
}

export function formatCuckooTemperature(value, unit = 'C') {
  if (!Number.isFinite(Number(value))) return '--'
  const celsius = Number(value)
  const display = unit === 'F' ? (celsius * 9) / 5 + 32 : celsius
  return `${display.toFixed(2)} °${unit === 'F' ? 'F' : 'C'}`
}
