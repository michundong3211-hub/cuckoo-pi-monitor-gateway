import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createMemoryHistory, createRouter } from 'vue-router'
import { config, global, status } from '@/modules/pinia'
import CuckooPiDeviceView from '@/views/CuckooPiDeviceView.vue'

function createTestRouter() {
  return createRouter({
    history: createMemoryHistory(),
    routes: [
      { path: '/push/cuckoo-pi', name: 'cuckoo-pi', component: { template: '<div />' } },
      {
        path: '/push/cuckoo-pi/:deviceId',
        name: 'cuckoo-pi-device',
        component: { template: '<div />' }
      }
    ]
  })
}

describe('CuckooPiDeviceView', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    config.cuckoo_pi = {
      enabled: true,
      broker: '',
      port: 2883,
      devices: []
    }
    config.saveAll = vi.fn(async () => true)
    global.disabled = false
    global.configChanged = true
    global.messageError = ''
    status.cuckoo_pi_device = []
  })

  it('shows one device detail with read-only id and aligned alarm fields', () => {
    config.cuckoo_pi.devices = [
      {
        id: '001122334455',
        offline_alarm_enabled: true,
        offline_timeout_seconds: 300,
        beer_deviation_alarm_enabled: true,
        beer_deviation_threshold_c: 2,
        beer_deviation_hold_seconds: 600
      }
    ]
    status.cuckoo_pi_device = [
      {
        id: '001122334455',
        online: true,
        mqtt_connected: true,
        update_time: 25,
        mode: 'Beer Constant',
        beer_set_c: 18,
        fridge_set_c: 8
      }
    ]

    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [createTestRouter()] },
      props: { deviceId: '001122334455' }
    })

    expect(wrapper.text()).toContain('Cuckoo Pi: 001122334455')
    expect(wrapper.get('[data-testid="cuckoo-device-id"]').attributes('readonly')).toBeDefined()
    expect(wrapper.text()).toContain('Online')
    expect(wrapper.find('table').exists()).toBe(false)
    expect(wrapper.get('[data-testid="offline-alarm-row"]').classes()).toContain('row')
    expect(wrapper.get('[data-testid="deviation-alarm-row"]').classes()).toContain('row')
    expect(wrapper.get('[data-testid="offline-timeout"]').element.value).toBe('300')
    expect(wrapper.get('[data-testid="deviation-threshold"]').element.value).toBe('2')
  })

  it('does not mutate config when detail draft is cancelled', async () => {
    const original = { id: '001122334455', offline_timeout_seconds: 300 }
    config.cuckoo_pi.devices = [original]
    const router = createTestRouter()
    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [router] },
      props: { deviceId: original.id }
    })

    await wrapper.get('[data-testid="offline-timeout"]').setValue(900)
    await wrapper.get('[data-testid="cancel-cuckoo-device"]').trigger('click')
    await flushPromises()

    expect(config.cuckoo_pi.devices[0].offline_timeout_seconds).toBe(300)
    expect(router.currentRoute.value.name).toBe('cuckoo-pi')
  })

  it('saves the detail draft into the matching device and navigates to the list', async () => {
    config.cuckoo_pi.devices = [
      { id: '001122334455', offline_timeout_seconds: 300 },
      { id: '112233445566', offline_timeout_seconds: 600 }
    ]
    const router = createTestRouter()
    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [router] },
      props: { deviceId: '001122334455' }
    })

    await wrapper.get('[data-testid="offline-timeout"]').setValue(900)
    expect(wrapper.get('form').element.checkValidity()).toBe(true)
    await wrapper.get('form').trigger('submit')
    await flushPromises()

    expect(config.cuckoo_pi.devices[0].offline_timeout_seconds).toBe(900)
    expect(config.cuckoo_pi.devices[1].offline_timeout_seconds).toBe(600)
    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(router.currentRoute.value.name).toBe('cuckoo-pi')
  })

  it('keeps the detail draft and error visible unless Save explicitly succeeds', async () => {
    config.cuckoo_pi.devices = [{ id: '001122334455', offline_timeout_seconds: 300 }]
    config.saveAll = vi.fn(async () => {
      global.messageError = 'Failed to store configuration to device'
      return undefined
    })
    const router = createTestRouter()
    await router.push('/push/cuckoo-pi/001122334455')
    await router.isReady()
    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [router] },
      props: { deviceId: '001122334455' }
    })

    await wrapper.get('[data-testid="offline-timeout"]').setValue(900)
    await wrapper.get('form').trigger('submit')
    await flushPromises()

    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(router.currentRoute.value.name).toBe('cuckoo-pi-device')
    expect(config.cuckoo_pi.devices[0].offline_timeout_seconds).toBe(300)
    expect(wrapper.get('[data-testid="offline-timeout"]').element.value).toBe('900')
    expect(global.messageError).toBe('Failed to store configuration to device')
  })

  it('keeps the detail page and draft when Save throws', async () => {
    config.cuckoo_pi.devices = [{ id: '001122334455', offline_timeout_seconds: 300 }]
    config.saveAll = vi.fn(async () => {
      global.messageError = 'Failed to save configuration'
      throw new Error('save failed')
    })
    const router = createTestRouter()
    await router.push('/push/cuckoo-pi/001122334455')
    await router.isReady()
    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [router] },
      props: { deviceId: '001122334455' }
    })

    await wrapper.get('[data-testid="offline-timeout"]').setValue(900)
    await wrapper.get('form').trigger('submit')
    await flushPromises()

    expect(router.currentRoute.value.name).toBe('cuckoo-pi-device')
    expect(config.cuckoo_pi.devices[0].offline_timeout_seconds).toBe(300)
    expect(wrapper.get('[data-testid="offline-timeout"]').element.value).toBe('900')
    expect(global.messageError).toBe('Failed to save configuration')
  })

  it('shows an error and list link without creating a missing device', () => {
    const router = createTestRouter()
    const wrapper = mount(CuckooPiDeviceView, {
      global: { plugins: [router] },
      props: { deviceId: 'deadbeefcafe' }
    })

    expect(wrapper.text()).toContain('Cuckoo Pi device not found')
    expect(wrapper.get('a').attributes('href')).toBe('/push/cuckoo-pi')
    expect(config.cuckoo_pi.devices).toEqual([])
    expect(config.saveAll).not.toHaveBeenCalled()
  })
})
