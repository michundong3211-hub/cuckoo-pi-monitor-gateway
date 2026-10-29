import { describe, it, expect } from 'vitest'
import { normalizeCuckooPiConfig } from '../cuckooPi'

describe('user MQTT credentials', () => {
  it('retains configured broker and credentials when normalizing', () => {
    const input = { broker: 'broker.example', port: 1883, username: 'sample-user', password: 'sample-password', client_id: 'sample-client' }
    expect(normalizeCuckooPiConfig(input)).toMatchObject(input)
  })
})
