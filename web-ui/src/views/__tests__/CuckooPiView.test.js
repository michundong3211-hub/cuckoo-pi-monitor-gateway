import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createMemoryHistory, createRouter } from 'vue-router'
import { nextTick } from 'vue'

const testPinia = vi.hoisted(() => ({
  config: {
    cuckoo_pi: {
      enabled: false,
      broker: 'broker.example',
      port: 1884,
      client_id: '',
      username: '',
      password: 'server-password',
      devices: []
    },
    saveAll: vi.fn(async () => true)
  },
  global: {
    disabled: false,
    configChanged: false,
    messageError: ''
  },
  status: {
    cuckoo_pi_device: []
  }
}))

vi.mock('@/modules/pinia', async () => {
  const { reactive } = await vi.importActual('vue')
  return {
    config: reactive(testPinia.config),
    global: reactive(testPinia.global),
    status: reactive(testPinia.status)
  }
})

import { config, global, status } from '@/modules/pinia'
import CuckooPiView from '@/views/CuckooPiView.vue'

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

function mountView() {
  return mount(CuckooPiView, { global: { plugins: [createTestRouter()] } })
}

describe('CuckooPiView', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    vi.unstubAllGlobals()
    config.cuckoo_pi = {
      enabled: false,
      broker: 'broker.example',
      port: 1884,
      client_id: '',
      username: '',
      password: 'synthetic-secret',
      devices: []
    }
    config.saveAll = vi.fn(async () => true)
    global.disabled = false
    global.configChanged = false
    global.messageError = ''
    status.cuckoo_pi_device = []
  })

  it('disables Save when neither the draft nor global config is dirty', async () => {
    const wrapper = mountView()

    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).not.toHaveBeenCalled()
  })

  it('lists configured devices with status and edit links', () => {
    config.cuckoo_pi.devices = [{ id: '001122334455' }, { id: '112233445566' }]
    status.cuckoo_pi_device = [
      { id: '001122334455', online: true, mqtt_connected: true, update_time: 25 },
      { id: '112233445566', online: false, mqtt_connected: false, update_time: 0 }
    ]

    const wrapper = mountView()

    expect(
      wrapper
        .get('[data-testid="cuckoo-device-list"]')
        .findAll('[data-testid^="cuckoo-device-row-"]')
    ).toHaveLength(2)
    expect(wrapper.get('[data-testid="cuckoo-device-row-001122334455"]').text()).toContain('Online')
    expect(wrapper.get('[data-testid="cuckoo-device-row-112233445566"]').text()).toContain(
      'Offline'
    )
    expect(wrapper.get('[data-testid="cuckoo-device-row-112233445566"]').text()).toContain(
      'Broker disconnected'
    )
    expect(wrapper.get('[data-testid="edit-cuckoo-001122334455"]').attributes('href')).toContain(
      '/push/cuckoo-pi/001122334455'
    )
  })

  it('normalizes an id and adds a device with default alarm settings', async () => {
    const wrapper = mountView()

    await wrapper.get('[data-testid="new-cuckoo-id"]').setValue('00:11:22:33:44:55')
    await wrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')

    expect(config.cuckoo_pi.devices).toHaveLength(0)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()

    await wrapper.get('form').trigger('submit')

    expect(config.cuckoo_pi.devices).toHaveLength(1)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
    expect(config.cuckoo_pi.devices[0]).toMatchObject({
      id: '001122334455',
      offline_alarm_enabled: false,
      offline_timeout_seconds: 300,
      beer_deviation_alarm_enabled: false,
      beer_deviation_threshold_c: 2,
      beer_deviation_hold_seconds: 600
    })
  })

  it('rejects invalid, duplicate, and ninth device ids without changing the list', async () => {
    config.cuckoo_pi.devices = [{ id: '001122334455' }]
    const wrapper = mountView()

    await wrapper.get('[data-testid="new-cuckoo-id"]').setValue('001122334455')
    await wrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')
    expect(global.messageError).toContain('already configured')
    expect(config.cuckoo_pi.devices).toHaveLength(1)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()

    await wrapper.get('[data-testid="new-cuckoo-id"]').setValue('invalid')
    await wrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')
    expect(global.messageError).toContain('12 hexadecimal')
    expect(config.cuckoo_pi.devices).toHaveLength(1)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()

    wrapper.unmount()
    config.cuckoo_pi.devices = Array.from({ length: 8 }, (_, index) => ({
      id: `00000000000${index + 1}`
    }))
    const fullWrapper = mountView()
    await fullWrapper.get('[data-testid="new-cuckoo-id"]').setValue('aabbccddeeff')
    await fullWrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')
    expect(global.messageError).toContain('maximum of 8')
    expect(config.cuckoo_pi.devices).toHaveLength(8)
  })

  it('confirms removal and only removes the selected device', async () => {
    config.cuckoo_pi.devices = [{ id: '001122334455' }, { id: '112233445566' }]
    const wrapper = mountView()
    const confirmMock = vi.fn(() => false)
    vi.stubGlobal('confirm', confirmMock)

    await wrapper.get('[data-testid="remove-cuckoo-001122334455"]').trigger('click')
    expect(confirmMock).toHaveBeenCalledWith('Remove Cuckoo Pi 001122334455?')
    expect(config.cuckoo_pi.devices.map((device) => device.id)).toEqual([
      '001122334455',
      '112233445566'
    ])
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()

    confirmMock.mockReturnValue(true)
    await wrapper.get('[data-testid="remove-cuckoo-001122334455"]').trigger('click')
    expect(config.cuckoo_pi.devices.map((device) => device.id)).toEqual([
      '001122334455',
      '112233445566'
    ])
    expect(config.saveAll).not.toHaveBeenCalled()
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()

    await wrapper.get('form').trigger('submit')

    expect(config.cuckoo_pi.devices.map((device) => device.id)).toEqual(['112233445566'])
    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })

  it('activates Save for field edits and clears it when every edit is reverted', async () => {
    const wrapper = mountView()

    await wrapper.get('#cuckoo-enabled').setValue(true)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()

    await wrapper.get('#cuckoo-enabled').setValue(false)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
  })

  it('edits MQTT settings while preserving an existing password', async () => {
    const wrapper = mountView()
    expect(wrapper.get('#mqtt-password').attributes('type')).toBe('password')
    await wrapper.get('#mqtt-broker').setValue('other.example')
    await wrapper.get('#mqtt-port').setValue(1883)
    await wrapper.get('#mqtt-username').setValue('test-user')
    await wrapper.get('form').trigger('submit')
    expect(config.cuckoo_pi).toMatchObject({ broker: 'other.example', port: 1883, username: 'test-user', password: 'synthetic-secret' })
    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })
  it('shows a blank default port and requires a manually supplied valid endpoint', async () => {
    config.cuckoo_pi = {
      enabled: true,
      broker: '',
      port: 0,
      client_id: '',
      username: '',
      password: '',
      devices: []
    }
    const wrapper = mountView()

    expect(wrapper.get('#mqtt-port').element.value).toBe('')
    await wrapper.get('#mqtt-port').setValue('')
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).not.toHaveBeenCalled()

    await wrapper.get('#mqtt-broker').setValue('broker.example')
    await wrapper.get('#mqtt-port').setValue('1884')
    await wrapper.get('form').trigger('submit')
    expect(config.cuckoo_pi).toMatchObject({ broker: 'broker.example', port: 1884 })
    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })
  it('keeps global and device-list edits out of Pinia until Save', async () => {
    const original = {
      enabled: true,
      broker: 'old-broker',
      port: 1883,
      devices: [{ id: '001122334455' }]
    }
    config.cuckoo_pi = structuredClone(original)
    const wrapper = mountView()

    await wrapper.get('#cuckoo-enabled').setValue(false)
    await wrapper.get('[data-testid="new-cuckoo-id"]').setValue('112233445566')
    await wrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')
    vi.stubGlobal(
      'confirm',
      vi.fn(() => true)
    )
    await wrapper.get('[data-testid="remove-cuckoo-001122334455"]').trigger('click')

    expect(config.cuckoo_pi).toEqual(original)
    expect(config.saveAll).not.toHaveBeenCalled()

    await wrapper.get('form').trigger('submit')

    expect(config.cuckoo_pi).toMatchObject({
      enabled: false,
      broker: 'old-broker',
      port: 1883,
      devices: [{ id: '112233445566' }]
    })
    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })

  it('restores the original list configuration when Save fails', async () => {
    const original = {
      enabled: true,
      broker: 'old-broker',
      port: 1883,
      devices: [{ id: '001122334455' }]
    }
    config.cuckoo_pi = structuredClone(original)
    config.saveAll = vi.fn(async () => {
      global.messageError = 'Failed to store configuration to device'
      return false
    })
    const wrapper = mountView()

    await wrapper.get('#cuckoo-enabled').setValue(false)
    await wrapper.get('form').trigger('submit')

    expect(config.cuckoo_pi).toEqual(original)
    expect(global.messageError).toBe('Failed to store configuration to device')
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()

    config.saveAll = vi.fn(async () => true)
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(config.cuckoo_pi.enabled).toBe(false)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
  })

  it('restores the original list configuration when Save throws', async () => {
    const original = {
      enabled: true,
      broker: 'old-broker',
      port: 1883,
      devices: [{ id: '001122334455' }]
    }
    config.cuckoo_pi = structuredClone(original)
    config.saveAll = vi.fn(async () => {
      global.messageError = 'Failed to save configuration'
      throw new Error('save failed')
    })
    const wrapper = mountView()

    await wrapper.get('#cuckoo-enabled').setValue(false)
    await wrapper.get('form').trigger('submit')

    expect(config.cuckoo_pi).toEqual(original)
    expect(global.messageError).toBe('Failed to save configuration')
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()

    config.saveAll = vi.fn(async () => true)
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(config.cuckoo_pi.enabled).toBe(false)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
  })

  it('clears Save after a successful global-dirty save clears the global marker', async () => {
    global.configChanged = true
    config.saveAll = vi.fn(async () => {
      global.configChanged = false
      return true
    })
    const wrapper = mountView()

    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()
    await wrapper.get('form').trigger('submit')

    expect(config.saveAll).toHaveBeenCalledTimes(1)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
  })

  it('does not submit while globally disabled, including a programmatic form submit', async () => {
    global.disabled = true
    global.configChanged = true
    const wrapper = mountView()

    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
    await wrapper.get('form').trigger('submit')

    expect(config.saveAll).not.toHaveBeenCalled()
  })

  it('does not submit twice while a save is pending', async () => {
    let resolveSave
    config.saveAll = vi.fn(
      () =>
        new Promise((resolve) => {
          resolveSave = resolve
        })
    )
    const wrapper = mountView()
    await wrapper.get('#cuckoo-enabled').setValue(true)

    const firstSubmit = wrapper.get('form').trigger('submit')
    await vi.waitFor(() => expect(config.saveAll).toHaveBeenCalledTimes(1))
    global.disabled = false
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).toHaveBeenCalledTimes(1)

    resolveSave(true)
    await firstSubmit
    await flushPromises()
    await nextTick()
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeDefined()
    await wrapper.get('form').trigger('submit')
    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })

  it('keeps a draft edit made during Save pending dirty after the submitted snapshot succeeds', async () => {
    let resolveSave
    config.saveAll = vi.fn(
      () =>
        new Promise((resolve) => {
          resolveSave = resolve
        })
    )
    const wrapper = mountView()
    await wrapper.get('#cuckoo-enabled').setValue(true)

    const pendingSubmit = wrapper.get('form').trigger('submit')
    await vi.waitFor(() => expect(config.saveAll).toHaveBeenCalledTimes(1))
    await wrapper.get('[data-testid="new-cuckoo-id"]').setValue('112233445566')
    await wrapper.get('[data-testid="add-cuckoo-device"]').trigger('click')

    resolveSave(true)
    await pendingSubmit
    await flushPromises()
    await nextTick()

    expect(config.cuckoo_pi.enabled).toBe(true)
    expect(config.cuckoo_pi.devices).toEqual([])
    expect(wrapper.get('[data-testid="cuckoo-device-row-112233445566"]').exists()).toBe(true)
    expect(wrapper.get('button[type="submit"]').attributes('disabled')).toBeUndefined()
  })

  it('saves the global and device-list configuration from the list page', async () => {
    global.configChanged = true
    const wrapper = mountView()

    await wrapper.get('form').trigger('submit')

    expect(config.saveAll).toHaveBeenCalledTimes(1)
  })
})
