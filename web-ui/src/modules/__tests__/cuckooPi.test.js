import { describe, expect, it } from 'vitest'
import {
  buildCuckooPiTopics,
  defaultCuckooPiConfig,
  defaultCuckooPiDevice,
  formatCuckooDuration,
  formatCuckooTemperature,
  normalizeCuckooPiConfig,
  normalizeCuckooPiDeviceId
} from '@/modules/cuckooPi'

describe('cuckooPi helpers', () => {
  it('normalizes a 12-digit hexadecimal device id', () => {
    expect(normalizeCuckooPiDeviceId('00:11-22 33:44:55')).toBe('001122334455')
    expect(normalizeCuckooPiDeviceId('not-an-id')).toBe('')
  })

  it('builds the five documented topics from the device id', () => {
    expect(buildCuckooPiTopics('001122334455')).toEqual({
      status: '/cuckoo/wenkong/001122334455',
      mode: '/cuckoo/mode/001122334455',
      beerSet: '/cuckoo/beer/001122334455',
      fridgeSet: '/cuckoo/ice/001122334455',
      ack: '/cuckoo/mode/ack/001122334455'
    })
  })

  it('provides enabled defaults without preconfigured devices', () => {
    expect(defaultCuckooPiConfig()).toEqual({
      enabled: true,
      broker: '',
      port: 1884,
      client_id: '',
      username: '',
      password: '',
      devices: []
    })
  })

  it('keeps per-device alarm defaults disabled', () => {
    expect(defaultCuckooPiDevice('001122334455')).toMatchObject({
      id: '001122334455',
      offline_alarm_enabled: false,
      beer_deviation_alarm_enabled: false
    })
  })

  it('migrates the previous built-in broker port to the new port', () => {
    expect(
      normalizeCuckooPiConfig({
        broker: '',
        port: 2884,
        client_id: '',
        username: '',
        password: ''
      })
    ).toMatchObject({
      broker: '',
      port: 1884,
      client_id: '',
      username: '',
      password: ''
    })
  })

  it('normalizes devices, rejects duplicates and preserves MQTT authentication', () => {
    const normalized = normalizeCuckooPiConfig({
      enabled: true,
      broker: 'mqtt.example.test',
      port: 1883,
      client_id: 'client-a',
      username: 'user-a',
      password: 'pass-a',
      devices: [
        {
          id: '00:11:22:33:44:55',
          offline_timeout_seconds: 420,
          beer_deviation_threshold_c: 1.5
        },
        { id: '001122334455' },
        { id: 'invalid' }
      ]
    })

    expect(normalized.enabled).toBe(true)
    expect(normalized.devices).toHaveLength(1)
    expect(normalized.devices[0]).toMatchObject({
      id: '001122334455',
      offline_alarm_enabled: false,
      offline_timeout_seconds: 420,
      beer_deviation_alarm_enabled: false,
      beer_deviation_threshold_c: 1.5,
      beer_deviation_hold_seconds: 600
    })
    expect(normalized).toMatchObject({
      client_id: 'client-a',
      username: 'user-a',
      password: 'pass-a'
    })
  })

  it('preserves explicitly enabled alarm values', () => {
    const normalized = normalizeCuckooPiConfig({
      devices: [
        {
          id: '001122334455',
          offline_alarm_enabled: true,
          beer_deviation_alarm_enabled: true
        }
      ]
    })

    expect(normalized.devices[0]).toMatchObject({
      id: '001122334455',
      offline_alarm_enabled: true,
      beer_deviation_alarm_enabled: true
    })
  })

  it('preserves the status time captured when an offline alert was dismissed', () => {
    const normalized = normalizeCuckooPiConfig({
      devices: [
        {
          id: '001122334455',
          offline_alarm_dismissed: true,
          offline_dismissed_status_time_seconds: 120
        }
      ]
    })

    expect(normalized.devices[0]).toMatchObject({
      offline_alarm_dismissed: true,
      offline_dismissed_status_time_seconds: 120
    })
  })

  it('defaults missing enabled and device fields without replacing saved IDs', () => {
    expect(normalizeCuckooPiConfig({}).enabled).toBe(true)
    expect(normalizeCuckooPiConfig({}).devices).toEqual([])
    expect(normalizeCuckooPiConfig({ devices: [] }).devices).toEqual([])
    expect(
      normalizeCuckooPiConfig({ devices: [{ id: '001122334455' }] }).devices.map(
        (device) => device.id
      )
    ).toEqual(['001122334455'])
    expect(normalizeCuckooPiConfig({ devices: [{ id: '001122334455' }] }).enabled).toBe(true)
  })

  it('preserves an explicitly saved disabled config and empty device list', () => {
    expect(normalizeCuckooPiConfig({ enabled: false, devices: [] })).toMatchObject({
      enabled: false,
      devices: []
    })
  })

  it('formats runtime and temperatures with missing-value placeholders', () => {
    expect(formatCuckooDuration(undefined)).toBe('--')
    expect(formatCuckooDuration(3723)).toBe('1h 2m 3s')
    expect(formatCuckooTemperature(undefined, 'C')).toBe('--')
    expect(formatCuckooTemperature(20, 'C')).toBe('20.00 °C')
    expect(formatCuckooTemperature(20, 'F')).toBe('68.00 °F')
  })
})
