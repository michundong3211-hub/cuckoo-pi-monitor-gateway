# 第三方许可核查记录（待完成）

- 项目来源：[GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway)。此仓库中的 `firmware/LICENSE` 是 MIT，并保留 Magnus 版权；上游历史版本 [20064cd6508685e00ae930320991340229710646 的 LICENSE](https://github.com/mp-se/gravitymon-gateway/blob/20064cd6508685e00ae930320991340229710646/LICENSE) 为 MIT；上游当前 [master/LICENSE](https://github.com/mp-se/gravitymon-gateway/blob/master/LICENSE) 为 GPLv3，不能以旧版本许可证描述当前上游。
- `web-ui/LICENSE` 为 GPLv3；各源码文件内版权声明仍需保留。不能把仓库整体标为 MIT。
- `firmware/lib/cst328` 与上游历史版本 [lib/cst328](https://github.com/mp-se/gravitymon-gateway/tree/20064cd6508685e00ae930320991340229710646/lib/cst328) 对应文件忽略空白比较一致，按该历史版本的 MIT 许可及原版权声明保留；这不证明其他文件来源相同。
- `firmware/lib/es8311` 的源文件标注 Copyright 2015–2022 Espressif Systems 和 SPDX Apache-2.0，保留源文件声明；Apache-2.0 正文见 `licenses/Apache-2.0.txt`。
- `firmware/lib/cst3530`、`firmware/lib/qmi8658` 的来源和适用许可尚未核实，不应推定为 MIT；公开前须补证据及授权。
- `firmware/src/lv_font_montserrat_*` 含 Montserrat 字体数据：项目 `文档/fonts` 的 2024 OFL 许可副本见 `licenses/Montserrat-OFL.txt`；LVGL 9.4.0 内置字体的 2011 OFL 许可见 `licenses/Montserrat-LVGL-OFL.txt`。具体生成字体与两种源字体的逐字节来源对应关系仍需确认。
- GMT 选用依赖证据：ArduinoJson、DallasTemperature、espframework、gravitymon、MQTT 为 MIT；AsyncTCP 3.4.9 和 ESPAsyncWebServer 3.9.3 为 LGPL-3.0；NimBLE-Arduino 为 Apache-2.0；OneWireNg 为 BSD 类。此清单未覆盖所有目标和传递依赖。
- 公开固件之前还须核实 LGPL 静态链接后用户替换/重链接条件、对应源码与构建材料；网页 bundle 传递依赖和字体版权清单也未完整核查。此文件不是法律意见。