import { describe, expect, it } from 'vitest'
import {
  ALARM_RING_COLORS,
  createDefaultAlarmSettings,
  normalizeAlarmSettings,
  upsertAlarmSettings
} from '@/modules/alarmSettings'

describe('alarm settings helpers', () => {
  it('defines the seven requested colors followed by Default', () => {
    expect(ALARM_RING_COLORS).toEqual([
      { label: 'Yellow', value: 0, hex: '#FFD700' },
      { label: 'Purple', value: 1, hex: '#61026F' },
      { label: 'Red', value: 2, hex: '#FF0000' },
      { label: 'Green', value: 3, hex: '#07A73E' },
      { label: 'Blue', value: 4, hex: '#1E90FF' },
      { label: 'White', value: 5, hex: '#D5D1CF' },
      { label: 'Black', value: 6, hex: '#000000' },
      { label: 'Default', value: 7, hex: '#45D9B2' }
    ])
  })

  it('creates the agreed alarm defaults for a new gravity device', () => {
    expect(createDefaultAlarmSettings('tilt-1')).toMatchObject({
      id: 'tilt-1',
      og_sg: 1.05,
      ring_color_index: 7,
      temperature_change_alarm_enabled: false,
      temperature_compare_minutes: 5,
      temperature_change_threshold_c: 1,
      offline_alarm_enabled: false,
      offline_timeout_seconds: 3660,
      offline_alarm_dismissed: false
    })
  })

  it('uses the legacy 900-second fallback only when offline timeout is missing', () => {
    expect(normalizeAlarmSettings({ id: 'legacy' }).offline_timeout_seconds).toBe(900)
    expect(
      normalizeAlarmSettings({ id: 'legacy', offline_timeout_seconds: undefined })
        .offline_timeout_seconds
    ).toBe(900)
    expect(
      normalizeAlarmSettings({ id: 'legacy', offline_timeout_seconds: 900 })
        .offline_timeout_seconds
    ).toBe(900)
  })

  it('normalizes offline timeout to 60-second steps within the supported bounds', () => {
    expect(normalizeAlarmSettings({ offline_timeout_seconds: 60 }).offline_timeout_seconds).toBe(60)
    expect(normalizeAlarmSettings({ offline_timeout_seconds: 3661 }).offline_timeout_seconds).toBe(3660)
    expect(normalizeAlarmSettings({ offline_timeout_seconds: 86400 }).offline_timeout_seconds).toBe(86400)
  })

  it('normalizes numeric bounds while preserving compatibility fields', () => {
    const normalized = normalizeAlarmSettings({
      id: 'tilt-2',
      og_sg: 9,
      ring_color_index: 99,
      temperature_compare_minutes: 0,
      temperature_change_threshold_c: 99,
      offline_timeout_seconds: 5,
      buzzer_enabled: true,
      temp_min_c: 16,
      offline_alarm_dismissed: true
    })

    expect(normalized).toMatchObject({
      id: 'tilt-2',
      og_sg: 1.2,
      ring_color_index: 7,
      temperature_compare_minutes: 1,
      temperature_change_threshold_c: 10,
      offline_timeout_seconds: 60,
      buzzer_enabled: true,
      temp_min_c: 16,
      offline_alarm_dismissed: true
    })
  })

  it('replaces one device without dropping other per-device settings', () => {
    const current = [
      { id: 'tilt-1', og_sg: 1.04, offline_alarm_dismissed: true },
      { id: 'tilt-2', og_sg: 1.06 }
    ]
    const updated = upsertAlarmSettings(current, {
      ...current[0],
      og_sg: 1.055
    })

    expect(updated).toHaveLength(2)
    expect(updated[0].og_sg).toBe(1.055)
    expect(updated[0].offline_alarm_dismissed).toBe(true)
    expect(updated[1].id).toBe('tilt-2')
  })
})
