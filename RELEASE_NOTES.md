# 测试固件说明（2026-10-30）

本项目基于 [GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway) 二次开发。三款固件为对应板型的独立应用镜像；刷写前请核对板型、分区及升级方式。

| 硬件 | 固件文件 | SHA-256 |
| --- | --- | --- |
| GMT1.56 | `cuckoo-pi-gateway-gmt156-alarm-unified-mqtt-blank.bin` | `A6AF07638BD8A8DC55C3DB6A964DE2FDF3FD71BD6BEEEDE581748A9E91A5CB02` |
| Waveshare 2.8 | `cuckoo-pi-gateway-waveshare28-mqtt-blank.bin` | `F60554166274AA8D490FD0DF99BE897FB9B8BB069F7C0CE607284403CF8B8B0E` |
| Waveshare 3.5 | `cuckoo-pi-gateway-waveshare35-mqtt-blank.bin` | `82440F9B434626C1434F1C235416449B656B52BC93EBFB162BFA028A5A935611` |

三款构建成功，镜像校验有效。用户报告实机测试一切正常。MQTT 网页定向测试 27/27 通过；主机测试 67/74 通过，剩余失败涉及既有界面断言和 C3 测试依赖。上述测试结果不代表每项功能均单独验收。

Cuckoo Pi MQTT 默认连接信息为空，需自行设置。GMT1.56 使用与 Waveshare 版本一致的逐设备报警声音调度。

第三方来源及许可尚有待核实项目，详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
