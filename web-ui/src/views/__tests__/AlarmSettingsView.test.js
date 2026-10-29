import { beforeEach, describe, expect, it, vi } from 'vitest'
import { mount } from '@vue/test-utils'
import { config, global } from '@/modules/pinia'
import { validateCurrentForm } from '@mp-se/espframework-ui-components'
import AlarmSettingsView from '@/views/AlarmSettingsView.vue'

vi.mock('vue-router', () => ({
  useRoute: () => ({ params: { deviceId: 'tilt-1' } })
}))

describe('AlarmSettingsView', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    global.disabled = false
    global.messageError = ''
    config.device_settings = [
      {
        id: 'tilt-1',
        og_sg: 1.048,
        temp_min_c: 17,
        buzzer_enabled: true,
        ring_color_index: 2,
        temperature_change_alarm_enabled: true,
        temperature_compare_minutes: 8,
        temperature_change_threshold_c: 1.5,
        offline_alarm_enabled: true,
        offline_timeout_seconds: 1200,
        offline_alarm_dismissed: true
      },
      { id: 'tilt-2', og_sg: 1.06 }
    ]
  })

  it('renders settings only and does not expose CLOSE', () => {
    const wrapper = mount(AlarmSettingsView)

    expect(wrapper.text()).toContain('Alarm settings: tilt-1')
    expect(wrapper.text()).toContain('Original gravity (SG)')
    expect(wrapper.text()).toContain('Temperature change alarm')
    expect(wrapper.text()).toContain('Offline alarm')
    expect(wrapper.text()).not.toMatch(/\bCLOSE\b/i)
  })

  it('loads the selected device values including its persisted dismissal state', () => {
    const wrapper = mount(AlarmSettingsView)

    expect(wrapper.vm.form.og_sg).toBe(1.048)
    expect(wrapper.get('#alarm-og').element.value).toBe('1.048')
    expect(wrapper.vm.form.ring_color_index).toBe(2)
    expect(wrapper.vm.form.offline_alarm_dismissed).toBe(true)
  })

  it('formats the default original gravity with three decimal places', () => {
    config.device_settings = [{ id: 'tilt-2', og_sg: 1.06 }]

    const wrapper = mount(AlarmSettingsView)

    expect(wrapper.vm.form.og_sg).toBe(1.05)
    expect(wrapper.get('#alarm-og').element.value).toBe('1.050')
  })

  it('uses the new timeout default and keeps the 60-second input step', () => {
    config.device_settings = []

    const wrapper = mount(AlarmSettingsView)

    expect(wrapper.vm.form.offline_timeout_seconds).toBe(3660)
    expect(wrapper.get('#offline-timeout').element.value).toBe('3660')
    expect(wrapper.get('#offline-timeout').element.step).toBe('60')
  })

  it('keeps original gravity numeric while displaying three decimal places after save', async () => {
    validateCurrentForm.mockReturnValue(true)
    const wrapper = mount(AlarmSettingsView)
    const input = wrapper.get('#alarm-og')

    await input.setValue('1.05')
    await input.trigger('blur')
    await wrapper.vm.saveSettings()

    expect(config.device_settings[0].og_sg).toBe(1.05)
    expect(typeof config.device_settings[0].og_sg).toBe('number')
    expect(input.element.value).toBe('1.050')
  })

  it.each(['', 'not-a-number', 0.999, 1.201])(
    'does not save invalid original gravity value %s',
    async (ogSg) => {
      validateCurrentForm.mockReturnValue(true)
      const wrapper = mount(AlarmSettingsView)
      const before = config.device_settings
      await wrapper.get('#alarm-og').setValue(String(ogSg))

      await wrapper.vm.saveSettings()

      expect(config.saveAll).not.toHaveBeenCalled()
      expect(config.device_settings).toBe(before)
    }
  )

  it('saves by replacing the full device_settings array and preserving hidden fields', async () => {
    validateCurrentForm.mockReturnValue(true)
    const wrapper = mount(AlarmSettingsView)
    await wrapper.get('#alarm-og').setValue('1.055')
    wrapper.vm.form.temperature_compare_minutes = 10

    await wrapper.vm.saveSettings()

    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(config.device_settings).toHaveLength(2)
    expect(config.device_settings[0]).toMatchObject({
      id: 'tilt-1',
      og_sg: 1.055,
      temperature_compare_minutes: 10,
      buzzer_enabled: true,
      temp_min_c: 17,
      offline_alarm_dismissed: true
    })
      expect(config.device_settings[1].id).toBe('tilt-2')
  })

  it('does not modify configuration when validation fails', async () => {
    validateCurrentForm.mockReturnValue(false)
    const before = config.device_settings
    const wrapper = mount(AlarmSettingsView)

    await wrapper.vm.saveSettings()

    expect(config.saveAll).not.toHaveBeenCalled()
    expect(config.device_settings).toBe(before)
  })
})
