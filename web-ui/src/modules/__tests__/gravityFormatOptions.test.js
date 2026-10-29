import { describe, it, expect } from 'vitest'
import {
  gravityHttpPostFormatOptions,
  gravityHttpGetFormatOptions,
  gravityInfluxdb2FormatOptions,
  gravityMqttFormatOptions
} from '@/modules/gravityFormatOptions'

describe('gravityFormatOptions', () => {
  it('exports gravityHttpPostFormatOptions', () => {
    expect(gravityHttpPostFormatOptions).toBeTruthy()
    expect(Array.isArray(gravityHttpPostFormatOptions.value)).toBe(true)
    expect(gravityHttpPostFormatOptions.value.length).toBeGreaterThan(0)
  })

  it('gravityHttpPostFormatOptions contains GravityMon entries', () => {
    const labels = gravityHttpPostFormatOptions.value.map((opt) => opt.label)
    expect(labels).toContain('GravityMon')
    expect(labels).toContain('GravityMon v2')
  })

  it('provides the complete CuckooTilt HTTP Post template', () => {
    const option = gravityHttpPostFormatOptions.value.find((opt) => opt.label === 'CuckooTilt')

    expect(option).toBeTruthy()

    const template = decodeURIComponent(option.value)
    const requiredFields = [
      '"name":"${mdns}"',
      '"ID":"${id}"',
      '"token":"${token}"',
      '"interval":${sleep-interval}',
      '"http-post-sleep-interval":${http-post-sleep-interval}',
      '"ble-sleep-interval":${ble-sleep-interval}',
      '"temperature":${temp}',
      '"temp_units":"${temp-unit}"',
      '"gravity":${gravity}',
      '"velocity":${velocity}',
      '"angle":${angle}',
      '"battery":${battery}',
      '"RSSI":${rssi}',
      '"corr-gravity":${corr-gravity}',
      '"gravity-unit":"${gravity-unit}"',
      '"ip-address":"${ip-address}"',
      '"app-ver":"${app-ver}"',
      '"run-mode":"${run-mode}"',
      '"run-time":${run-time}'
    ]

    for (const field of requiredFields) expect(template).toContain(field)
  })

  it('exports gravityHttpGetFormatOptions', () => {
    expect(gravityHttpGetFormatOptions).toBeTruthy()
    expect(Array.isArray(gravityHttpGetFormatOptions.value)).toBe(true)
    expect(gravityHttpGetFormatOptions.value.length).toBeGreaterThan(0)
  })

  it('gravityHttpGetFormatOptions contains GravityMon entries', () => {
    const labels = gravityHttpGetFormatOptions.value.map((opt) => opt.label)
    expect(labels).toContain('GravityMon')
    expect(labels).toContain('GravityMon v2')
  })

  it('exports gravityInfluxdb2FormatOptions', () => {
    expect(gravityInfluxdb2FormatOptions).toBeTruthy()
    expect(Array.isArray(gravityInfluxdb2FormatOptions.value)).toBe(true)
    expect(gravityInfluxdb2FormatOptions.value.length).toBeGreaterThan(0)
  })

  it('exports gravityMqttFormatOptions', () => {
    expect(gravityMqttFormatOptions).toBeTruthy()
    expect(Array.isArray(gravityMqttFormatOptions.value)).toBe(true)
    expect(gravityMqttFormatOptions.value.length).toBeGreaterThan(0)
  })

  it('gravityMqttFormatOptions contains expected entries', () => {
    const labels = gravityMqttFormatOptions.value.map((opt) => opt.label)
    expect(labels.some((l) => l.includes('Gravitymon'))).toBe(true)
    expect(labels.some((l) => l.includes('HomeAssistant'))).toBe(true)
  })

  it('all options have label and value properties', () => {
    const allOptions = [
      ...gravityHttpPostFormatOptions.value,
      ...gravityHttpGetFormatOptions.value,
      ...gravityInfluxdb2FormatOptions.value,
      ...gravityMqttFormatOptions.value
    ]
    for (const opt of allOptions) {
      expect(opt).toHaveProperty('label')
      expect(opt).toHaveProperty('value')
      expect(typeof opt.label).toBe('string')
      expect(typeof opt.value).toBe('string')
    }
  })
})
