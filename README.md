# ESP32 墨水屏智能仪表盘

这是一个基于 ESP32 (DOIT ESP32 DEVKIT V1) 和微雪 (Waveshare) 4.2英寸电子墨水屏的智能显示项目。

## 效果展示

![项目效果图](images/demo.jpg)

## 功能特性

- **实时天气**：显示当前温度、天气状况、风向和风速。
- **7天预报**：显示未来7天的每日和夜间预报，包括温度范围和天气图标。
- **日期与日历**：显示公历日期、星期、农历日期和节气。
- **室内环境**：通过 MQTT 监测并显示室内温度和湿度。
- **日程/排班**：支持显示日历事件和排班信息。
- **空气质量**：显示空气质量指数 (AQI) 和 PM2.5 信息。
- **智能刷新**：时间更新时进行局部刷新；主要数据变化时进行全屏刷新。
- **昼夜模式**：根据时间自动切换白天和夜间的天气图标。
- **Web 配置**：内置 Web 服务器，可直接通过浏览器配置 WiFi、MQTT、NTP 服务器，并支持文件管理。
- **MQTT 集成**：支持断线自动重连（带超时放弃机制），可与 Home Assistant 或其他 MQTT 服务器无缝集成。
- **NTP 同步**：支持自定义 NTP 服务器进行时间同步。
- **一键控制**：单击按钮切换页面，双击请求更新天气，长按刷新当前页面。

## 硬件要求

- **ESP32 开发板**：DOIT ESP32 DEVKIT V1 (或兼容板)。
- **电子墨水屏**：微雪 (Waveshare) 4.2英寸电子墨水屏模块 (SPI接口)。
- **连接线若干**。

### 引脚配置

| 墨水屏引脚 | ESP32 GPIO | 描述 |
| :--- | :--- | :--- |
| BUSY | GPIO 25 | 忙信号 |
| RST | GPIO 26 | 复位 |
| DC | GPIO 27 | 数据/命令选择 |
| CS | GPIO 15 | 片选 |
| CLK | GPIO 13 | SPI 时钟 |
| DIN | GPIO 14 | SPI 数据 (MOSI) |
| GND | GND | 地 |
| VCC | 3.3V | 电源 |

**其他硬件：**
- **按钮**：GPIO 0 (Boot 按钮) - 单击切换页面，双击请求天气，长按刷新页面。

## 软件设置

1. **安装 Arduino IDE**。
2. **安装依赖库**（在库管理器中搜索并安装）：
   - `Adafruit GFX Library`
   - `U8g2_for_Adafruit_GFX`
   - `ArduinoJson` (v6 或 v7)
   - `PubSubClient`
   - `NTPClient`
   - `OneButton`
3. **打开项目**：打开 `examples/esp32-waveshare-epd/examples/epd4in2-demo-esp32/epd4in2-demo-esp32.ino`。
4. **开发板设置**：
   - Board: `DOIT ESP32 DEVKIT V1`
   - Partition Scheme: `Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)` **(重要：否则编译会报错空间不足)**
5. **编译与上传**：连接开发板并上传代码。

## 配置指南

### 初始设置
1. 给设备上电。
2. 如果无法连接预设 WiFi，设备将开启 AP 模式，热点名为 `EPD-Display-XXXX` (XXXX为MAC地址后四位)。
3. 连接热点后，打开浏览器访问 `http://192.168.4.1`。
4. 配置 WiFi、MQTT 服务器、NTP 服务器等信息。
5. 点击 **Save & Restart**（保存并重启）。

### MQTT 主题 (Topic)

| 主题配置项 | 默认值 | 方向 | 描述 |
| :--- | :--- | :--- | :--- |
| Text Topic | `epd/text` | 订阅 | 接收并在屏幕上显示文本消息。 |
| Weather Topic | `epd/weather` | 订阅 | 接收 7 天天气预报的 JSON 数据。 |
| Date Topic | `epd/date` | 订阅 | 接收日历信息（阳历、农历、节气）。 |
| Env Topic | `epd/env` | 订阅 | 接收室内环境数据 (`{"temp": "24", "humi": "50"}`)。 |
| Calendar Topic | `epd/calendar` | 订阅 | 接收日历事件列表。 |
| Shift Topic | `epd/shift` | 订阅 | 接收排班信息。 |
| AQI Topic | `epd/air_quality` | 订阅 | 接收空气质量数据。 |
| Weather Request | `epd/weatherrequest` | 发布 | 设备启动或双击按键时发送 "get" 消息请求更新天气。 |

### MQTT 数据格式示例

**天气数据 (`epd/weather`)**:
```json
[
  {
    "date": "2023-10-27",
    "temp": "22/15",
    "textDay": "晴",
    "iconDay": "100",
    "textNight": "晴",
    "iconNight": "150",
    "windDir": "西北风",
    "windScale": "3"
  },
  ...
]
```

**日期信息 (`epd/date`)**:
```json
{
  "阳历日期": "2023-10-27",
  "星期": "星期五",
  "农历日期": "癸卯年 九月十三",
  "节气信息": "霜降第4天"
}
```

**环境数据 (`epd/env`)**:
```json
{
  "temp": 25.5,
  "humi": 60.0
}
```
