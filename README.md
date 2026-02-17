# ESP32 墨水屏气象站

这是一个基于 ESP32 和微雪 (Waveshare) 4.2英寸电子墨水屏的智能天气显示项目。

## 效果展示

![项目效果图](images/demo.jpg)

## 功能特性

- **实时天气**：显示当前温度、天气状况、风向和风速。
- **7天预报**：显示未来7天的每日和夜间预报，包括温度范围和天气图标。
- **日期与日历**：显示公历日期、星期、农历日期和节气（如“立春”）。
- **室内环境**：通过 MQTT 监测并显示室内温度和湿度。
- **智能刷新**：时间更新时进行局部刷新；主要数据变化时进行全屏刷新。
- **昼夜模式**：根据时间自动切换白天和夜间的天气图标（6:00-18:00 为白天，18:00-6:00 为夜间）。
- **Web 配置**：内置 Web 服务器，可直接通过浏览器配置 WiFi、MQTT 设置，并支持文件管理。
- **图标管理**：支持通过 Web 界面上传自定义 BMP 图标；如果文件缺失，会自动回退使用内置的 PROGMEM 图标。
- **MQTT 集成**：可与 Home Assistant 或其他 MQTT 服务器无缝集成。
- **一键控制**：单击按钮请求天气更新；长按按钮重置 WiFi 设置。

## 硬件要求

- **ESP32 开发板**：Waveshare e-Paper ESP32 Driver Board 或通用 ESP32 开发板。
- **电子墨水屏**：微雪 (Waveshare) 4.2英寸电子墨水屏模块 (SPI接口)。
- **连接线若干**。

### 引脚配置

| 墨水屏引脚 | ESP32 GPIO | 描述 |
| :--- | :--- | :--- |
| BUSY | GPIO 25 | 忙信号 |
| RST | GPIO 26 | 复位 |
| DC | GPIO 27 | 数据/命令控制 |
| CS | GPIO 15 | 片选 |
| CLK | GPIO 13 | SPI 时钟 |
| DIN | GPIO 14 | SPI MOSI 数据 |
| GND | GND | 地 |
| VCC | 3.3V | 电源 |

**其他硬件：**
- **按钮**：GPIO 0 (通常为开发板上的 BOOT 按钮) - 单击更新天气，长按重置配网。

## 软件设置

1. **安装 Arduino IDE**。
2. **安装 ESP32 开发板支持**：在 Arduino IDE 的“首选项” -> “附加开发板管理器网址”中添加 ESP32 的 URL，然后在“开发板管理器”中安装 ESP32。
3. **安装依赖库**（在库管理器中搜索并安装）：
   - `Adafruit GFX Library`
   - `U8g2_for_Adafruit_GFX`
   - `ArduinoJson` (v6 或 v7)
   - `PubSubClient`
   - `NTPClient`
   - `OneButton`
4. **打开项目**：打开 `examples/esp32-waveshare-epd/examples/epd4in2-demo-esp32/epd4in2-demo-esp32.ino`。
5. **编译与上传**：选择开发板型号（如 `ESP32 Dev Module` 或 `Waveshare ESP32 Driver Board`）并点击上传。
   - 注意：如果使用通用 ESP32 开发板，请确保引脚连接与代码配置一致，或者在 `DEV_Config.h` 中修改引脚定义。

## 配置指南

### 初始设置
1. 给设备上电。
2. 使用手机或电脑连接名为 `EPD-Display` 的 WiFi 热点。
3. 打开浏览器，访问 `http://192.168.4.1`。
4. 输入您的 WiFi 名称 (SSID)、密码以及 MQTT 服务器信息。
5. 点击 **Save & Restart**（保存并重启）。

### MQTT 主题 (Topic)

| 主题 | 方向 | 描述 |
| :--- | :--- | :--- |
| `epd/weather` | 订阅 | 接收 7 天天气预报的 JSON 数据。 |
| `epd/date` | 订阅 | 接收日历信息（阳历、农历、节气）。 |
| `epd/env` | 订阅 | 接收室内环境数据（格式：`{"temp": "24", "humi": "50"}`）。 |
| `epd/weatherrequest` | 发布 | 设备启动或按键时发送 "get" 消息请求更新天气。 |

### 数据格式示例

**epd/weather (JSON 数组)**:
```json
[
  {
    "fxDate": "2026-02-14",
    "textDay": "多云",
    "iconDay": "101",
    "windDirDay": "东风",
    "windSpeedDay": "3",
    "tempMax": "22",
    "tempMin": "10",
    "textNight": "多云",
    "iconNight": "151"
  },
  ...
]
```

**epd/date (JSON 对象)**:
```json
{
  "阳历日期": "2026-02-14",
  "星期": "星期六",
  "农历日期": "乙巳[蛇]年 腊月小廿七",
  "节气信息": "距离下一节气【雨水】还有 4 天"
}
```

**epd/env (JSON 对象)**:
```json
{
  "temp": 25.5,
  "humi": 60.5
}
```
