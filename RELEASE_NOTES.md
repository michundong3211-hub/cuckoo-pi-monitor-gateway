# Test firmware release (2026-10-30)

This project is based on [GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway). Select the application image for your exact board and verify its flash layout and update method before installing.

| Hardware | Firmware file | SHA-256 |
| --- | --- | --- |
| GMT1.56 | `cuckoo-pi-gateway-gmt156.bin` | `A6AF07638BD8A8DC55C3DB6A964DE2FDF3FD71BD6BEEEDE581748A9E91A5CB02` |
| Waveshare 2.8 | `cuckoo-pi-gateway-waveshare28.bin` | `F60554166274AA8D490FD0DF99BE897FB9B8BB069F7C0CE607284403CF8B8B0E` |
| Waveshare 3.5 | `cuckoo-pi-gateway-waveshare35.bin` | `82440F9B434626C1434F1C235416449B656B52BC93EBFB162BFA028A5A935611` |

All three targets built successfully and passed image validation. The user reported that device testing was successful. Targeted MQTT web tests passed 27/27; host tests passed 67/74, with remaining failures involving existing UI assertions and C3 test dependencies. These results do not represent separate acceptance of every feature.

Cuckoo Pi MQTT connection details are blank by default and must be configured by the user. GMT1.56 uses the same per-device alarm sound scheduling as the Waveshare targets.

Some third-party provenance and license questions remain unresolved; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
