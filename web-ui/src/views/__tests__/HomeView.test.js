import { beforeEach, describe, expect, it, vi } from 'vitest'
import { flushPromises, mount } from '@vue/test-utils'
import { createMemoryHistory, createRouter } from 'vue-router'
import { config, global, status } from '@/modules/pinia'
import HomeView from '@/views/HomeView.vue'

describe('HomeView', () => {
  beforeEach(() => {
    vi.clearAllMocks()

    Object.assign(config, {
      gravity_unit: 'G',
      temp_unit: 'C',
      pressure_unit: 'PSI',
      device_settings: []
    })

    Object.assign(global, {
      app_ver: '1.2.3',
      app_build: '45',
      uiVersion: '2.5.0',
      uiBuild: '99',
      platform: 'ESP32',
      board: 'S3'
    })

    Object.assign(status, {
      gravity_device: [
        {
          device: 'gravity-1',
          name: 'Fermenter',
          update_time: 120,
          push_time: 3600,
          gravity: 1.05,
          temp: 20.5,
          source: 'BLE',
          type: 'Tilt'
        }
      ],
      pressure_device: [
        {
          device: 'pressure-1',
          name: 'Pressure Tank',
          update_time: 59,
          push_time: 86400,
          pressure: 12.345,
          pressure1: 11.2,
          temp: 19.5,
          source: 'BLE',
          type: 'Pressuremon'
        }
      ],
      temperature_device: [
        {
          device: 'temp-1',
          name: 'Chamber',
          update_time: 61,
          chamber_temp: 18.3,
          beer_temp: 20.1,
          source: 'BLE',
          type: 'Probe'
        }
      ],
      cuckoo_pi_device: [
        {
          id: '001122334455',
          mqtt_connected: true,
          online: true,
          update_time: 5,
          mode: 'Beer Constant',
          state: 'Cooling',
          beer_temp_c: 20,
          beer_set_c: 18,
          fridge_temp_c: 10,
          fridge_set_c: 8,
          status_time_seconds: 3723
        }
      ],
      rssi: -55,
      wifi_ssid: 'brew-net',
      ip: '192.168.1.2',
      free_heap: 123,
      total_heap: 456,
      id: 'GW-1',
      uptime_days: 1,
      uptime_hours: 2,
      uptime_minutes: 3,
      uptime_seconds: 4,
      load: vi.fn(async () => true)
    })
  })

  it('renders device summaries and refreshes status on mount', async () => {
    const wrapper = mount(HomeView)
    await Promise.resolve()

    expect(status.load).toHaveBeenCalledTimes(1)
    expect(wrapper.text()).toContain('Gravity: 1.050')
    expect(wrapper.text()).toContain('Temperature: 20.50 C')
    expect(wrapper.text()).toContain('Pressure: 12.345')
    expect(wrapper.text()).toContain('brew-net')
    expect(wrapper.text()).toContain('Firmware: 1.2.3 (45) UI: 2.5.0 (99)')
    expect(wrapper.text()).toContain('Cuckoo Pi: 001122334455')
    expect(wrapper.text()).toContain('Beer Constant')
    const card = wrapper.get('[data-testid="cuckoo-card-001122334455"]')
    const rows = card.findAll('.cuckoo-card-row')
    expect(rows[0].text()).toContain('Mode')
    expect(rows[0].text()).toContain('Beer Constant')
    expect(rows[1].text()).toContain('Beer')
    expect(rows[1].find('span').element.textContent).toBe(
      '20.00 °C\u00a0\u00a0\u00a0\u00a018.00 °C'
    )
    expect(rows[2].text()).toContain('Fridge')
    expect(rows[2].find('span').element.textContent).toBe(
      '10.00 °C\u00a0\u00a0\u00a0\u00a08.00 °C'
    )
    expect(rows).toHaveLength(4)
    const stateRow = rows[3]
    expect(stateRow.text()).toContain('State')
    expect(stateRow.element.textContent).toBe(
      'StateCooling\u00a0\u00a0\u00a0\u00a01h 2m 3s'
    )
    expect(stateRow.text()).not.toContain('State time')
    expect(stateRow.findAll('.cuckoo-card-group')).toHaveLength(0)
    expect(wrapper.text()).toContain('Alarm settings')
    expect(wrapper.text()).not.toContain('Last update')
    expect(wrapper.text()).not.toContain('Runtime')
    wrapper.unmount()
  })

  it('labels Gravitymon devices as Cuckoo Tilt on the home card', () => {
    status.gravity_device = [
      {
        device: 'gravity-1',
        name: 'Fermenter',
        update_time: 120,
        push_time: 3600,
        gravity: 1.05,
        temp: 20.5,
        source: 'BLE',
        type: 'Gravitymon'
      }
    ]

    const wrapper = mount(HomeView)

    expect(wrapper.text()).toContain('Cuckoo Tilt')
    expect(wrapper.text()).not.toContain('Gravitymon')
    wrapper.unmount()
  })

  it('shows placeholders and offline state for incomplete Cuckoo Pi data', () => {
    status.cuckoo_pi_device = [
      { id: 'deadbeefcafe', mqtt_connected: false, online: false, update_time: 0 }
    ]

    const wrapper = mount(HomeView)

    const card = wrapper.get('[data-testid="cuckoo-card-deadbeefcafe"]')
    expect(card.text()).toContain('Offline')
    expect(card.text()).toContain('Broker disconnected')
    const rows = card.findAll('.cuckoo-card-row')
    expect(rows[0].text()).toContain('Mode')
    expect(rows[0].text()).toContain('--')
    expect(rows[1].text()).toContain('Beer')
    expect(rows[1].find('span').element.textContent).toBe('--\u00a0\u00a0\u00a0\u00a0--')
    expect(rows[2].text()).toContain('Fridge')
    expect(rows[2].find('span').element.textContent).toBe('--\u00a0\u00a0\u00a0\u00a0--')
    expect(rows).toHaveLength(4)
    expect(rows[3].text()).toContain('State')
    expect(rows[3].element.textContent).toBe('State--\u00a0\u00a0\u00a0\u00a0--')
    expect(rows[3].findAll('.cuckoo-card-group')).toHaveLength(0)
    expect(rows[3].text()).not.toContain('State time')
    expect(card.text()).toContain('Alarm settings')
    expect(card.text()).not.toContain('Last update')
    wrapper.unmount()
  })

  it.each([
    ['null', null, '--'],
    ['undefined', undefined, '--'],
    ['zero', 0, '0s']
  ])('renders %s Cuckoo Pi state duration correctly in the card', (fixture, duration, expected) => {
    const deviceId = `cuckoo-${fixture}`
    status.cuckoo_pi_device = [
      {
        ...status.cuckoo_pi_device[0],
        id: deviceId,
        status_time_seconds: duration
      }
    ]

    const wrapper = mount(HomeView)
    const card = wrapper.get(`[data-testid="cuckoo-card-${deviceId}"]`)
    const stateRow = card.findAll('.cuckoo-card-row')[3]

    expect(stateRow.element.textContent).toBe(
      `StateCooling\u00a0\u00a0\u00a0\u00a0${expected}`
    )
    expect(stateRow.text()).not.toContain('State time')
    wrapper.unmount()
  })

  it('renders Cuckoo Pi temperatures in Fahrenheit and routes to device details by name', async () => {
    const router = createRouter({
      history: createMemoryHistory(),
      routes: [
        { path: '/', name: 'home', component: { template: '<div />' } },
        {
          path: '/alarm/:deviceId',
          name: 'alarm-settings',
          component: { template: '<div />' }
        },
        { path: '/push/cuckoo-pi', name: 'cuckoo-pi', component: { template: '<div />' } },
        {
          path: '/push/cuckoo-pi/:deviceId',
          name: 'cuckoo-pi-device',
          component: { template: '<div />' }
        }
      ]
    })
    await router.push('/')
    await router.isReady()
    config.temp_unit = 'F'

    const wrapper = mount(HomeView, {
      global: {
        plugins: [router],
        stubs: { RouterLink: false }
      }
    })
    const card = wrapper.get('[data-testid="cuckoo-card-001122334455"]')
    const link = card.get('[data-testid="cuckoo-alarm-settings-001122334455"]')

    const rows = card.findAll('.cuckoo-card-row')
    expect(rows[1].text()).toContain('Beer')
    expect(rows[1].find('span').element.textContent).toBe(
      '68.00 °F\u00a0\u00a0\u00a0\u00a064.40 °F'
    )
    expect(rows[2].text()).toContain('Fridge')
    expect(rows[2].find('span').element.textContent).toBe(
      '50.00 °F\u00a0\u00a0\u00a0\u00a046.40 °F'
    )
    expect(link.attributes('href')).toBe('/push/cuckoo-pi/001122334455')
    expect(link.attributes('href')).not.toContain('/alarm/')
    await link.trigger('click')
    await flushPromises()
    expect(router.currentRoute.value.name).toBe('cuckoo-pi-device')
    expect(router.currentRoute.value.params.deviceId).toBe('001122334455')

    wrapper.unmount()
  })

  it('uses Vue Router navigation for encoded alarm settings device IDs', async () => {
    const router = createRouter({
      history: createMemoryHistory(),
      routes: [
        { path: '/', name: 'home', component: { template: '<div />' } },
        {
          path: '/alarm/:deviceId',
          name: 'alarm-settings',
          component: { template: '<div />' }
        },
        { path: '/push/cuckoo-pi', name: 'cuckoo-pi', component: { template: '<div />' } },
        {
          path: '/push/cuckoo-pi/:deviceId',
          name: 'cuckoo-pi-device',
          component: { template: '<div />' }
        }
      ]
    })
    await router.push('/')
    await router.isReady()
    status.gravity_device[0].device = 'gravity/1'

    const wrapper = mount(HomeView, {
      global: {
        plugins: [router],
        stubs: { RouterLink: false }
      }
    })
    const link = wrapper.get('[data-testid="alarm-settings-gravity/1"]')

    expect(link.attributes('href')).toBe('/alarm/gravity%2F1')
    await link.trigger('click')
    await flushPromises()
    expect(router.currentRoute.value.name).toBe('alarm-settings')
    expect(router.currentRoute.value.params.deviceId).toBe('gravity/1')

    wrapper.unmount()
  })

  it('shows a red indicator when the offline alarm was dismissed on the device', () => {
    config.device_settings = [{ id: 'gravity-1', offline_alarm_dismissed: true }]

    const wrapper = mount(HomeView)

    expect(wrapper.get('[data-testid="alarm-dismissed-gravity-1"]').classes()).toContain(
      'bg-danger'
    )
    wrapper.unmount()
  })

  it('shows the Waveshare 2.8-inch display name without changing the board identifier', () => {
    global.board = 'WAVESHARE_S3_TFT'

    const wrapper = mount(HomeView)

    expect(wrapper.text()).toContain('WAVESHARE 2.8 TFT')
    expect(wrapper.text()).not.toContain('WAVESHARE_S3_TFT')

    wrapper.unmount()
  })
})
