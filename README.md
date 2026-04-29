## 项目简介
- 提供统一的 LED 设备接口封装，面向用户态控制指示灯与 RGB 灯带。
- 通过可插拔驱动适配 sysfs LED 与 SPI WS2812/SK6812，解决不同硬件接口下的快速控制问题。

## 功能特性
- 支持：sysfs 亮灭控制（`/sys/class/leds/<name>/brightness`），SPI WS2812/SK6812 灯带控制（spidev）。
- 支持：基础 API（开关/亮度/颜色）与效果（闪烁/呼吸），效果需周期调用 `led_tick()`。
- 不支持：WS2812 单颗粒独立颜色控制、硬件读取回传、复杂动画脚本。

## 快速开始
### 环境准备
- Linux 用户态环境，具备 sysfs LED 节点或 spidev 设备节点。
- 确认权限：可写 `/sys/class/leds/<name>/brightness`，可读写 `/dev/spidevX.Y`。
- 普通的led控制，需要在dts方案里面配置节点，如下

```
 166     leds {
 167         compatible = "gpio-leds";
 168 
 169         led1 {
 170             label = "sys-led";
 171             gpios = <&gpio 96 0>;
 172             linux,default-trigger = "none";
 173             default-state = "on";
 174             status = "okay";
 175         };
 176 
 177         led2 {
 178             label = "sys-led";
 179             gpios = <&gpio 97 0>;
 180             linux,default-trigger = "none";
 181             default-state = "on";
 182             status = "okay";
 183         };
 184     };
```
- 通过spi控制WS2812灯带，dts上需要打开spi节点
```
 230 &spi2 {
 231     pinctrl-names = "default";
 232     pinctrl-0 = <&pinctrl_ssp2_0>;
 233     status = "okay";
 234     k1x,ssp-clock-rate = <6400000>;
 235 
 236     spi@3 {
 237         compatible = "cisco,spi-petra";
 238         reg = <0x0>;
 239         spi-max-frequency = <6400000>;
 240         status = "okay";
 241     };
 255 };
```
以及内核需要开启spi-dev配置
```
CONFIG_SPI_SPIDEV=y
```



### 构建编译
- 仅脱离 SDK 的独立构建示例：
```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON -DSROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS=drv_generic;drv_spi_ws2812
make
```

### 运行示例
- sysfs LED 示例：
```bash
./test_led_generic sys-led high
```
- WS2812 SPI 示例：
```bash
./test_led_ws2812 /dev/spidev0.0 16 grb 6400000 80
```

## 详细使用
详细 API 与配置说明请参考后续官方文档。

## 常见问题
- 提示 `[LED] driver 'xxx' not found`：确认在 CMake 中通过 `SROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS` 启用了对应驱动。
- 闪烁/呼吸无效：需要周期调用 `led_tick()`，并确保运行时长覆盖 `count * period`。
- WS2812 无响应：检查 `spidev` 设备节点、SPI 频率、颜色顺序与权限。
- sysfs 写入失败：检查 LED 节点名称与权限配置。

## 版本与发布
- 本组件随 SDK 版本发布，变更记录以仓库提交/发布说明为准。
- 兼容性依赖 Linux sysfs LED 与 spidev 接口。

## 贡献方式
- 欢迎提交 Issue/PR，遵循现有编码风格并补充必要测试。
- 贡献者与维护者名单见：`CONTRIBUTORS.md`

## License
本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
