# v0.2.0-test

This prerelease provides diagnostic application images for GMT1.56, Waveshare 2.8 and Waveshare 3.5. Select the matching board image and verify the update method and flash layout before installation.

The candidate introduces PSRAM-backed LVGL pools and display buffers, corrects PSRAM alignment handling, improves settings page layout and timing, and updates the red ring color. Cuckoo Pi MQTT broker, port, username, password and device ID are not preconfigured.

The 3.5 settings screen was reported to respond much better with a prior candidate. The rebuilt release images, and the latest 2.8 and GMT1.56 candidates, still require independent hardware validation. A successful build or image integrity check does not establish device stability, touch, audio, network, alarm, or storage acceptance.

The images expose an unauthenticated, read-only settings diagnostic endpoint containing runtime and memory statistics. Use them on a trusted network. These are test images, not a stable release.

This project builds on [GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway). Third-party component notices and unresolved provenance questions are documented in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
