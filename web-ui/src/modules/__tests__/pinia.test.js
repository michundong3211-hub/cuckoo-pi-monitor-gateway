import { beforeEach, describe, expect, it, vi } from 'vitest'

describe('pinia module helpers', () => {
  const loadModule = async () => {
    vi.resetModules()
    vi.doUnmock('@/modules/pinia')

    const configState = {
      alpha: 1,
      beta: 'x',
      cuckoo_pi: { enabled: false, devices: [] }
    }
    const piniaInternals = {}
    piniaInternals.self = piniaInternals
    const configStore = {
      $state: configState,
      sendConfig: vi.fn(),
      $id: 'config',
      $subscribe: vi.fn(),
      _p: piniaInternals
    }
    const globalStore = {
      initialized: true,
      configChanged: false
    }
    const statusStore = {}
    const measurementStore = {}
    const logInfoMock = vi.fn()

    vi.doMock('pinia', () => ({
      createPinia: vi.fn(() => ({ id: 'pinia' }))
    }))

    vi.doMock('@/modules/configStore', () => ({
      useConfigStore: vi.fn(() => configStore)
    }))
    vi.doMock('@/modules/globalStore', () => ({
      useGlobalStore: vi.fn(() => globalStore)
    }))
    vi.doMock('@/modules/statusStore', () => ({
      useStatusStore: vi.fn(() => statusStore)
    }))
    vi.doMock('@/modules/measurementStore', () => ({
      useMeasurementStore: vi.fn(() => measurementStore)
    }))
    vi.doMock('@mp-se/espframework-ui-components', () => ({
      logInfo: logInfoMock
    }))

    const module = await vi.importActual('../pinia.js')
    return { module, configStore, globalStore, logInfoMock }
  }

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('saves a config snapshot and resets configChanged', async () => {
    const { module, configStore, globalStore } = await loadModule()

    expect(() => JSON.stringify(configStore._p)).toThrow(/Converting circular structure to JSON/)
    expect(() => module.saveConfigState()).not.toThrow()

    expect(globalStore.configChanged).toBe(false)
    expect(module.getConfigChanges()).toEqual({})
  })

  it('returns changed config values compared to the saved snapshot', async () => {
    const { module, configStore, globalStore } = await loadModule()
    module.saveConfigState()
    configStore.$state.alpha = 2

    expect(module.getConfigChanges()).toEqual({ alpha: 2 })

    globalStore.initialized = true
    const subscribeHandler = configStore.$subscribe.mock.calls[0][0]
    subscribeHandler()
    expect(globalStore.configChanged).toBe(true)
  })

  it('keeps configChanged false when there are no saved changes or initialization is incomplete', async () => {
    const { module, configStore, globalStore } = await loadModule()

    expect(module.getConfigChanges()).toEqual({})

    const subscribeHandler = configStore.$subscribe.mock.calls[0][0]
    globalStore.initialized = false
    subscribeHandler()
    expect(globalStore.configChanged).toBe(false)

    globalStore.initialized = true
    module.saveConfigState()
    subscribeHandler()
    expect(globalStore.configChanged).toBe(false)
  })

  it('detects nested Cuckoo Pi configuration changes', async () => {
    const { module, configStore } = await loadModule()
    module.saveConfigState()

    configStore.$state.cuckoo_pi.devices.push({ id: '001122334455' })

    expect(module.getConfigChanges()).toEqual({
      cuckoo_pi: { enabled: false, devices: [{ id: '001122334455' }] }
    })
  })

  it('does not log Cuckoo Pi credential values', async () => {
    const { module, configStore, logInfoMock } = await loadModule()
    configStore.$state.cuckoo_pi.password = 'synthetic-secret'

    module.saveConfigState()

    expect(logInfoMock.mock.calls.flat()).not.toContain('synthetic-secret')
    expect(logInfoMock).toHaveBeenCalledWith(
      'pinia.saveConfigState()',
      'Saved state keys:',
      expect.arrayContaining(['cuckoo_pi'])
    )
  })
})
