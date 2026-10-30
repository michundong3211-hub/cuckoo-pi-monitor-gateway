# Cuckoo Pi Monitor Gateway

基于 [GravityMon Gateway](https://github.com/mp-se/gravitymon-gateway) 的二次开发版本，感谢 Magnus 和原项目贡献者。

## 硬件目标

| 硬件 | PlatformIO 环境 |
|---|---|
| GMT1.56（工程名 GMT154-06） | `gmt154-06` |
| Waveshare 2.8 | `gateway32s3wave-tft-sd` |
| Waveshare 3.5 | `gateway35s3wave-tft-sd` |

固件为独立应用镜像，不是完整首刷包；三种板型不能交叉刷写。刷写前必须核对板型、Flash、分区、OTA 槽并备份。候选构建、刷写和实机功能验收分别确认，不应等同。

## 构建

在 `web-ui` 运行 `npm ci` 和 `npm run build`，将 `dist/assets/index.js.gz` 与 `style.css.gz` 复制为 `firmware/html/app.js.gz` 和 `app.css.gz`。在 `firmware` 使用 PlatformIO 对上述环境分别执行 `pio run -e <环境名>`。

## 版权与许可

原项目来源见上游链接。固件目录保留 `firmware/LICENSE`（MIT 及原作者版权），网页目录保留 `web-ui/LICENSE`（GPLv3）。第三方依赖和字体另有许可，尚须逐项核对；这份仓库不能整体概括为 MIT。

## 验证状态

当前为发布准备候选。用户报告实机测试一切正常；主机测试及镜像校验结果见 [发布说明](RELEASE_NOTES.md)。

Cuckoo Pi MQTT 默认服务器、端口、客户端 ID、用户名、密码和设备 ID 均为空。使用时请在设置页填写所需连接信息和设备 ID。
