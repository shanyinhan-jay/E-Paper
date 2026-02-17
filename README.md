# ESP32 墨水屏气象站

这是一个基于 ESP32 (DOIT DEVKIT V1) 和微雪 (Waveshare) 4.2英寸电子墨水屏的智能天气显示项目。

## 效果展示

![项目效果图](images/demo.jpg)


## 功能特性

- **实时天气**：显示当前温度、天气状况、风向和风速。
- **7天预报**：显示未来7天的每日和夜间预报，包括温度范围和天气图标。
- **日期与日历**：显示公历日期、星期、农历日期和节气（如“立春”）。
- **室内环境**：通过 MQTT 监测并显示室内温度和湿度。
- **智能刷新**：时间更新时进行局部刷新；主要数据变化时进行全屏刷新。
- **昼夜模式**：根据时间自动切换白天和夜间的天气图标（6:00-18:00 为白天，18:00-6:00 为夜间）。
- **Web 配置**：内置 Web 服务器，可直接通过浏览器配置 WiFi、MQTT 设置、NTP 服务器，并支持文件管理。
- **图标管理**：支持通过 Web 界面上传自定义 BMP 图标；如果文件缺失，会自动回退使用内置的 PROGMEM 图标。
- **MQTT 集成**：可与 Home Assistant 或其他 MQTT 服务器无缝集成。
- **智能重连**：MQTT 连接失败会自动重试，超时后停止重试并提示，避免无限循环。
- **一键控制**：单击按钮请求天气更新；长按按钮重置 WiFi 设置。

## 硬件要求

- **ESP32开发板**：推荐使用 DOIT ESP32 DEVKIT V1。
- **电子墨水屏**：微雪 (Waveshare) 4.2英寸电子墨水屏模块 (SPI接口)。
- **连接线若干**。

### 引脚配置

  EPD_SCK_PIN  13
  EPD_MOSI_PIN 14
  EPD_CS_PIN   15
  EPD_RST_PIN  26
  EPD_DC_PIN   27
  EPD_BUSY_PIN 25
  BUTTON_PIN   0

**其他硬件：**
- **按钮**：GPIO 0 (低电平触发/Active Low) - 单击切换页面，长按全刷当前页面。

## 软件设置

1. **安装 Arduino IDE**。
2. **安装依赖库**（在库管理器中搜索并安装）：
   - `Adafruit GFX Library`
   - `U8g2_for_Adafruit_GFX`
   - `ArduinoJson` (v6 或 v7)
   - `PubSubClient`
   - `NTPClient`
   - `OneButton`
   - `LittleFS` (ESP32 文件系统支持)
3. **打开项目**：打开 `examples/esp32-waveshare-epd/examples/epd4in2-demo-esp32/epd4in2-demo-esp32.ino`。
4. **编译与上传**：
   - 选择开发板型号：**DOIT ESP32 DEVKIT V1**
   - **重要**：将 **Partition Scheme** 设置为 **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**，否则可能会因程序体积过大导致编译失败。
   - 点击上传。
5. **上传文件系统（可选）**：如果您有自定义的 BMP 图标，可以使用 ESP32 Sketch Data Upload 工具上传到 Flash 中。代码中已包含内置图标作为备用，因此此步骤非必须。

## 配置指南

### 初始设置
1. 给设备上电。
2. 使用手机或电脑连接名为 `EPD-Display` 的 WiFi 热点。
3. 打开浏览器，访问 `http://192.168.4.1`。
4. 输入您的 WiFi 名称 (SSID)、密码、MQTT 服务器信息以及 **NTP 服务器地址**（可选，默认 ntp1.aliyun.com）。
5. 点击 **Save & Restart**（保存并重启）。

### MQTT 主题 (Topic)
| 主题 | 方向 | 描述 |
| :--- | :--- | :--- |
| `epd/weather` | 订阅 | 接收 7 天天气预报的 JSON 数据,可以从其他任一天气数据源转换成终端期望的数据格式。 |
[{"fxDate":"2026-02-14","textDay":"多云","iconDay":"101","windDirDay":"东风","windSpeedDay":"3","tempMax":"22","tempMin":"10","textNight":"多云","iconNight":"151"},{"fxDate":"2026-02-15","textDay":"多云","iconDay":"101","windDirDay":"东北风","windSpeedDay":"3","tempMax":"22","tempMin":"7","textNight":"中雨","iconNight":"306"},{"fxDate":"2026-02-16","textDay":"小雨","iconDay":"305","windDirDay":"东北风","windSpeedDay":"3","tempMax":"7","tempMin":"4","textNight":"小雨","iconNight":"305"},{"fxDate":"2026-02-17","textDay":"多云","iconDay":"101","windDirDay":"东北风","windSpeedDay":"3","tempMax":"8","tempMin":"2","textNight":"晴","iconNight":"150"},{"fxDate":"2026-02-18","textDay":"晴","iconDay":"100","windDirDay":"东风","windSpeedDay":"3","tempMax":"13","tempMin":"3","textNight":"晴","iconNight":"150"},{"fxDate":"2026-02-19","textDay":"多云","iconDay":"101","windDirDay":"北风","windSpeedDay":"3","tempMax":"14","tempMin":"5","textNight":"小雨","iconNight":"305"},{"fxDate":"2026-02-20","textDay":"晴","iconDay":"100","windDirDay":"南风","windSpeedDay":"3","tempMax":"20","tempMin":"9","textNight":"晴","iconNight":"150"}]

| `epd/date` | 订阅 | 接收日历信息（阳历、农历、节气）。 |
{"阳历日期":"2026-02-14","星期":"星期六","农历日期":"乙巳[蛇]年 腊月小廿七","节气信息":"距离下一节气【雨水】还有 4 天"}

| `epd/env` | 订阅 | 接收室内环境数据（格式：`{"temp": "24", "humi": "50"}`）。 |
{
    "temp": 25.5,
    "humi": 6.66
}
| `epd/weatherrequest` | 发布 | 设备启动或按键时发送 "get" 消息请求更新天气。 |

### 天气数据格式 (JSON 示例)
`epd/weather` 主题期望接收一个包含每日预报的 JSON 数组：
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

## 图标系统
- **格式支持**：支持上传到 LittleFS 的 **BMP** 图片（单色）。
- **命名规范**：
  - `100.bmp`: 标准图标（用于列表或小图标区域）。
  - `100-L.bmp`: 大图标（用于主天气看板）。
- **回退机制**：如果对应的 BMP 文件不存在，程序将自动使用 `icons.h` 中定义的内置 PROGMEM 图标。

## 许可证
本项目开源，欢迎随意修改和分发。
