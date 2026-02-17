/*
- SCK : 13
- MOSI : 14
- CS : 15
- RST : 26
- DC : 27
- BUSY : 25
*/


#include "DEV_Config.h"
#include "Local_EPD_4in2.h"
#include "GUI_Paint.h"

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

#include <stdlib.h>
// #include <LittleFS.h> // Already included
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "icons.h"
#include "AppConfig.h"
#include "WebHandler.h"

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <OneButton.h>
#include <vector>

extern const uint8_t u8g2_font_wqy16_t_gb2312[] U8X8_PROGMEM;
extern const uint8_t u8g2_font_open_iconic_weather_6x_t[] U8X8_PROGMEM;
extern const uint8_t u8g2_font_open_iconic_weather_4x_t[] U8X8_PROGMEM;
extern const uint8_t u8g2_font_logisoso60_tf[] U8X8_PROGMEM;
extern const uint8_t u8g2_font_logisoso30_tf[] U8X8_PROGMEM;

WebServer server(80);

WiFiClient espClient;
PubSubClient client(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP); 
UBYTE *BlackImage = NULL;
OneButton button(BUTTON_PIN, true); // GPIO defined in DEV_Config.h, Active Low

const char* DEFAULT_AP_SSID_BASE = "EPD-Display";
String ap_ssid = DEFAULT_AP_SSID_BASE;

#define MAX_CALENDAR_EVENTS 20
#define MAX_SHIFT_EVENTS 30

Config config;

struct CalendarEvent {
    time_t start_time;
    String summary;
};

struct ShiftEvent {
    int year;
    int month;
    int day;
    String content;
};

struct WeatherData {
    String date;
    String temp;
    String cond_day;
    String cond_night;
    String wind_dir;
    String wind_sc;
    String icon_day;
    String icon_night;
};

// Global Weather Data
std::vector<CalendarEvent> calendarEvents;
std::vector<ShiftEvent> shiftEvents;
WeatherData currentForecast[7];
int currentForecastCount = 0;
String currentCity = "绍兴";
String solarDate = "";
String weekDay = "";
String lunarDate = "";
String termInfo = "";
String indoorTemp = "";
String indoorHumi = "";
String airPm2p5 = "";
String airCategory = "";
int lastMinute = -1;

// Flags for deferred display update
bool updateWeatherPending = false;
bool updateCalendarPending = false;
bool updateEnvPending = false;
bool updateDatePending = false;
unsigned long lastUpdateTrigger = 0;
const unsigned long UPDATE_DELAY_MS = 1000; // Wait 1s after last message to update (debounce)

Page currentPage = PAGE_WEATHER;
bool switchPagePending = false;

class Paint_GFX : public Adafruit_GFX {
public:
  float scale = 1.0;
  Paint_GFX() : Adafruit_GFX(EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT) {}
  
  void setScale(float s) {
      scale = (s > 0) ? s : 1.0;
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) override {
    uint16_t targetColor;
    if (config.invert_display) {
        // Inverted: Foreground(1) -> WHITE, Background(0) -> BLACK
        targetColor = (color == 1) ? WHITE : BLACK;
    } else {
        // Normal: Foreground(1) -> BLACK, Background(0) -> WHITE
        targetColor = (color == 1) ? BLACK : WHITE;
    }

    if (scale == 1.0) {
        if (x < 0 || x >= EPD_4IN2_WIDTH || y < 0 || y >= EPD_4IN2_HEIGHT) return;
        Paint_SetPixel(x, y, targetColor);
    } else {
        // Scaled Drawing with Float
        int16_t start_x = floor(x * scale);
        int16_t end_x = ceil((x + 1) * scale);
        int16_t start_y = floor(y * scale);
        int16_t end_y = ceil((y + 1) * scale);
        
        for (int16_t sy = start_y; sy < end_y; sy++) {
            for (int16_t sx = start_x; sx < end_x; sx++) {
                if (sx < 0 || sx >= EPD_4IN2_WIDTH || sy < 0 || sy >= EPD_4IN2_HEIGHT) continue;
                Paint_SetPixel(sx, sy, targetColor);
            }
        }
    }
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
      int16_t temp = x0; x0 = y0; y0 = temp;
      temp = x1; x1 = y1; y1 = temp;
    }

    if (x0 > x1) {
      int16_t temp = x0; x0 = x1; x1 = temp;
      temp = y0; y0 = y1; y1 = temp;
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
      ystep = 1;
    } else {
      ystep = -1;
    }

    for (; x0<=x1; x0++) {
      if (steep) {
        drawPixel(y0, x0, color);
      } else {
        drawPixel(x0, y0, color);
      }
      err -= dy;
      if (err < 0) {
        y0 += ystep;
        err += dx;
      }
    }
  }
};

Paint_GFX paint_gfx;
U8G2_FOR_ADAFRUIT_GFX u8g2;

void loadConfig() {
  if (LittleFS.begin(true)) {
    if (LittleFS.exists("/config.json")) {
      File configFile = LittleFS.open("/config.json", "r");
      if (configFile) {
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, buf.get());
        if (!error) {
          strlcpy(config.wifi_ssid, doc["wifi_ssid"] | "", sizeof(config.wifi_ssid));
          strlcpy(config.wifi_pass, doc["wifi_pass"] | "", sizeof(config.wifi_pass));
          strlcpy(config.mqtt_server, doc["mqtt_server"] | "", sizeof(config.mqtt_server));
          config.mqtt_port = doc["mqtt_port"] | 1883;
          strlcpy(config.mqtt_user, doc["mqtt_user"] | "", sizeof(config.mqtt_user));
          strlcpy(config.mqtt_pass, doc["mqtt_pass"] | "", sizeof(config.mqtt_pass));
          strlcpy(config.mqtt_topic, doc["mqtt_topic"] | "epd/text", sizeof(config.mqtt_topic));
          strlcpy(config.mqtt_weather_topic, doc["mqtt_weather_topic"] | "epd/weather", sizeof(config.mqtt_weather_topic));
          strlcpy(config.mqtt_date_topic, doc["mqtt_date_topic"] | "epd/date", sizeof(config.mqtt_date_topic));
          strlcpy(config.mqtt_env_topic, doc["mqtt_env_topic"] | "epd/env", sizeof(config.mqtt_env_topic));
          strlcpy(config.mqtt_calendar_topic, doc["mqtt_calendar_topic"] | "epd/calendar", sizeof(config.mqtt_calendar_topic));
          strlcpy(config.mqtt_shift_topic, doc["mqtt_shift_topic"] | "epd/shift", sizeof(config.mqtt_shift_topic));
          strlcpy(config.mqtt_air_quality_topic, doc["mqtt_air_quality_topic"] | "epd/air_quality", sizeof(config.mqtt_air_quality_topic));
          config.invert_display = doc["invert_display"] | false;
        }
      }
    }
  }

  // Pre-allocate BlackImage to avoid fragmentation
  if (BlackImage == NULL) {
      UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;
      BlackImage = (UBYTE *)malloc(Imagesize);
      if (BlackImage == NULL) {
          Serial.println("FATAL: BlackImage Malloc Failed in Setup!");
      } else {
          Serial.printf("BlackImage Allocated: %u bytes\n", Imagesize);
          // Initialize with White
          memset(BlackImage, 0xFF, Imagesize);
      }
  }
}

void saveConfig() {
  JsonDocument doc;
  doc["wifi_ssid"] = config.wifi_ssid;
  doc["wifi_pass"] = config.wifi_pass;
  doc["mqtt_server"] = config.mqtt_server;
  doc["mqtt_port"] = config.mqtt_port;
  doc["mqtt_user"] = config.mqtt_user;
  doc["mqtt_pass"] = config.mqtt_pass;
  doc["mqtt_topic"] = config.mqtt_topic;
  doc["mqtt_weather_topic"] = config.mqtt_weather_topic;
  doc["mqtt_date_topic"] = config.mqtt_date_topic;
  doc["mqtt_env_topic"] = config.mqtt_env_topic;
  doc["mqtt_calendar_topic"] = config.mqtt_calendar_topic;
  doc["mqtt_shift_topic"] = config.mqtt_shift_topic;
  doc["mqtt_air_quality_topic"] = config.mqtt_air_quality_topic;
  doc["invert_display"] = config.invert_display;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    return;
  }
  serializeJson(doc, configFile);
  configFile.close();
}

void displayMessage(String text) {
    Serial.println("displayMessage start");
    Serial.printf("Free Heap: %u\n", ESP.getFreeHeap());
    
    DEV_Module_Init();
    Local_EPD_4IN2_Init();
    
    if (BlackImage == NULL) {
         Serial.println("Error: BlackImage is NULL (Should be allocated in setup)");
         // Try emergency alloc
         UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;
         BlackImage = (UBYTE *)malloc(Imagesize);
         if (BlackImage == NULL) Serial.println("BlackImage Malloc Failed");
         else Serial.println("BlackImage Malloc Success (Emergency)");
    }
    
    if (BlackImage != NULL) {
        Serial.println("Preparing Image");
        UWORD InitColor = config.invert_display ? BLACK : WHITE;
        Paint_NewImage(BlackImage, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, 0, InitColor);
        Paint_SelectImage(BlackImage);
        Paint_Clear(InitColor);
        
        Serial.println("U8G2 begin");
        u8g2.begin(paint_gfx);
        
        // --- Enable 1.0x Scaling ---
        float scale = 1.0;
        paint_gfx.setScale(scale); 
        int logicalWidth = EPD_4IN2_WIDTH / scale;
        int logicalHeight = EPD_4IN2_HEIGHT / scale;
        
        u8g2.setFont(u8g2_font_wqy16_t_gb2312); // Use WenQuanYi 16pt GB2312 font
        u8g2.setFontMode(1); // Transparent mode
        u8g2.setForegroundColor(1); // Black
        u8g2.setBackgroundColor(0); // White
        
        Serial.println("Processing Text");
        // --- 1. Wrap Text into Lines ---
        std::vector<String> lines;
        String currentLine = "";
        int len = text.length();
        int i = 0;
        
        while (i < len) {
            
            // Handle explicit newline
            if (text[i] == '\n') {
                lines.push_back(currentLine);
                currentLine = "";
                i++;
                continue;
            }
            
            int charLen = 1;
            unsigned char c = text[i];
            if (c >= 0xF0) charLen = 4;
            else if (c >= 0xE0) charLen = 3;
            else if (c >= 0xC0) charLen = 2;
            
            if (i + charLen > len) {
                // Incomplete multibyte char at end
                break; 
            }

            String nextChar = text.substring(i, i + charLen);
            
            // Basic bounds check to prevent crash in getUTF8Width
            if (nextChar.length() > 0) {
                 if (u8g2.getUTF8Width((currentLine + nextChar).c_str()) > (logicalWidth - 10)) {
                    lines.push_back(currentLine);
                    currentLine = nextChar;
                 } else {
                    currentLine += nextChar;
                 }
            }
            i += charLen;
        }
        if (currentLine.length() > 0) {
            lines.push_back(currentLine);
        }
        
        Serial.println("Calculating Layout");
        // --- 2. Calculate Centering ---
        int lineHeight = 20; // Fixed line height for safety
        int totalHeight = lines.size() * lineHeight;
        
        int startY = (logicalHeight - totalHeight) / 2 + 16;
        
        if (startY < 20) startY = 20; // Safety clamp

        // --- 3. Draw Lines (Centered) ---
        Serial.println("Drawing Lines");
        for (const String& line : lines) {
            int w = u8g2.getUTF8Width(line.c_str());
            int x = (logicalWidth - w) / 2;
            if (x < 0) x = 0;
            
            u8g2.drawUTF8(x, startY, line.c_str());
            
            startY += lineHeight;
        }
        
        // Reset Scale
        paint_gfx.setScale(1.0);

        Serial.println("Displaying");
        Local_EPD_4IN2_Display(BlackImage);
        Serial.println("Sleeping");
        Local_EPD_4IN2_Sleep();
        // free(BlackImage); // Keep allocated to prevent fragmentation
        // BlackImage = NULL;
        Serial.println("displayMessage done");
    }
}

void displayCalendarPage(bool partial_update = false) {
    Serial.println("Displaying Calendar Page...");
    Serial.print("Shift Events count: ");
    Serial.println(shiftEvents.size());

    DEV_Module_Init();
    
    if (BlackImage == NULL) {
         Serial.println("Error: BlackImage is NULL (Should be allocated in setup)");
         // Try emergency alloc
         UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;
         BlackImage = (UBYTE *)malloc(Imagesize);
         if (BlackImage == NULL) Serial.println("BlackImage Malloc Failed (Calendar)");
    }
    
    if (BlackImage != NULL) {
        UWORD InitColor = config.invert_display ? BLACK : WHITE;
        Paint_NewImage(BlackImage, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, 0, InitColor);
        Paint_SelectImage(BlackImage);
        Paint_Clear(InitColor);
        
        u8g2.begin(paint_gfx);
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(1);
        u8g2.setBackgroundColor(0);
        
        // Title removed as per request
        // u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        // u8g2.drawUTF8(10, 20, "两周日历 (Two Weeks)");
        // paint_gfx.drawFastHLine(0, 25, EPD_4IN2_WIDTH, 2);

        // Grid Settings
        int cols = 7;
        int rows = 2;
        int cellW = EPD_4IN2_WIDTH / cols;
        int cellH = EPD_4IN2_HEIGHT / rows; // Use full height since title is gone
        int startY = 0; // Start from top

        time_t now = timeClient.getEpochTime();
        const char* weekDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

        for (int i = 0; i < 14; i++) {
            time_t future = now + i * 86400;
            struct tm * t = gmtime(&future);
            
            int c = i % cols;
            int r = i / cols;
            
            int x = c * cellW;
             int y = startY + r * cellH;
             
             // Save "t" (current cell date) to local vars immediately because gmtime returns static pointer
             // which will be overwritten by other gmtime calls (e.g. inside event loop)
             int t_year = t->tm_year;
             int t_mon = t->tm_mon;
             int t_mday = t->tm_mday;
             int t_wday = t->tm_wday;
             
             // Draw Cell Border
            // paint_gfx.drawRect(x, y, cellW, cellH, 1);
            paint_gfx.drawFastHLine(x, y, cellW, 1);
            paint_gfx.drawFastHLine(x, y + cellH - 1, cellW, 1);
            paint_gfx.drawFastVLine(x, y, cellH, 1);
            paint_gfx.drawFastVLine(x + cellW - 1, y, cellH, 1);
            
            // Highlight Weekend
            // If it's Saturday (6) or Sunday (0), use black background for header
            if (t_wday == 0 || t_wday == 6) {
                 for(int k=0; k<20; k++) {
                     paint_gfx.drawFastHLine(x, y + k, cellW, 1);
                 }
                 u8g2.setForegroundColor(0); // White text
                 u8g2.setBackgroundColor(1);
            } else {
                 u8g2.setForegroundColor(1); // Black text
                 u8g2.setBackgroundColor(0);
            }

            // Draw Weekday
            u8g2.setFont(u8g2_font_wqy12_t_gb2312);
            String wdayStr = weekDays[t_wday];
            int wW = u8g2.getUTF8Width(wdayStr.c_str());
            u8g2.drawUTF8(x + (cellW - wW)/2, y + 14, wdayStr.c_str());
            
            // Draw horizontal line below weekday (header separator)
            paint_gfx.drawFastHLine(x, y + 20, cellW, 1);

            // Reset Color for Date
            u8g2.setForegroundColor(1);
            u8g2.setBackgroundColor(0);
            
            // Draw Date
            u8g2.setFont(u8g2_font_logisoso30_tf);
            String dateStr = String(t_mday);
            int dW = u8g2.getUTF8Width(dateStr.c_str());
           // Center date vertically in the remaining space below header
            // Header height is 20. Cell Height is ~85. Remaining is 65.
            // Move date up to avoid overlap with events
            u8g2.drawUTF8(x + (cellW - dW)/2, y + 60, dateStr.c_str());

            // Draw Event Marker or Summary (if any)
            // Check for Shift
            bool hasShift = false;
            String shiftText = "";
            for (const auto& sev : shiftEvents) {
                 if (sev.year == (t_year + 1900) && sev.month == (t_mon + 1) && sev.day == t_mday) {
                     hasShift = true;
                     shiftText = sev.content;
                     break;
                 }
            }
            
            // Check for Event
            bool hasEvent = false;
            CalendarEvent targetEvent;
            for (const auto& ev : calendarEvents) {
                 struct tm* evTm = gmtime(&ev.start_time);
                 if (evTm->tm_year == t_year && evTm->tm_mon == t_mon && evTm->tm_mday == t_mday) {
                     hasEvent = true;
                     targetEvent = ev;
                     break;
                 }
            }

            if (hasShift || hasEvent) {
                 // Draw separator line between date and content
                 int sepMargin = 5;
                 paint_gfx.drawFastHLine(x + sepMargin, y + 70, cellW - (2 * sepMargin), 1);
            }

            if (hasShift) {
                 u8g2.setFont(u8g2_font_wqy12_t_gb2312);
                 int sW = u8g2.getUTF8Width(shiftText.c_str());
                 // Draw centered below separator (y+70)
                 // Text baseline at y+86 (approx 16px height)
                 u8g2.drawUTF8(x + (cellW - sW)/2, y + 86, shiftText.c_str());
            }

            if (hasEvent) {
                 // Found event for this day
                 // Draw summary with word wrap (4 chars per line, max 5 lines)
                  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
                  String sum = targetEvent.summary;
                  
                  int lineHeight = 14;
                  int maxLines = 5;
                  int charsPerLine = 4;
                  int currentLine = 0;
                  // Start below Shift area. Shift ends at ~88. 
                  // If Shift exists, start at 105. If not, could start earlier but consistent alignment is better.
                  // User said "Event starts from line below Shift".
                  // Let's use y+100 as base.
                  int startY_Text = y + 100;
                  
                  int k = 0;
                  while (k < sum.length() && currentLine < maxLines) {
                       String lineStr = "";
                       int charsInThisLine = 0;
                       
                       // Collect chars for this line
                       while (k < sum.length() && charsInThisLine < charsPerLine) {
                           int charLen = 1;
                           unsigned char c = sum[k];
                           if (c >= 0xC0) charLen = 2;
                           if (c >= 0xE0) charLen = 3;
                           if (c >= 0xF0) charLen = 4;
                           
                           lineStr += sum.substring(k, k + charLen);
                           k += charLen;
                           charsInThisLine++;
                       }
                       
                       // Check if we need to truncate (if this is the last allowed line and there is more text)
                       if (currentLine == maxLines - 1 && k < sum.length()) {
                           // Force draw ".." on next line to indicate more content
                           u8g2.drawUTF8(x + (cellW - u8g2.getUTF8Width(".."))/2, startY_Text + (currentLine + 1) * lineHeight, "..");
                       }
                       
                       // Draw this line centered
                       int sW = u8g2.getUTF8Width(lineStr.c_str());
                       u8g2.drawUTF8(x + (cellW - sW)/2, startY_Text + currentLine * lineHeight, lineStr.c_str());
                       currentLine++;
                   }
            }
         }

        if (partial_update) {
            Local_EPD_4IN2_Init_Partial();
            Local_EPD_4IN2_PartialDisplay(0, 0, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, BlackImage);
        } else {
            Local_EPD_4IN2_Init();
            Local_EPD_4IN2_Display(BlackImage);
        }
        Local_EPD_4IN2_Sleep();
        // free(BlackImage); // Keep allocated
        // BlackImage = NULL;
    }
}

char getIconChar(String cond) {
    char iconChar = 64; // Default Sun
    String c = cond;
    c.toLowerCase();
    
    // Chinese matches
    if (c.indexOf("多云") >= 0 || c.indexOf("阴") >= 0) iconChar = 65; // Cloud/Overcast
    else if (c.indexOf("雨") >= 0) iconChar = 67; // Rain
    else if (c.indexOf("雪") >= 0) iconChar = 67; // Snow
    else if (c.indexOf("雷") >= 0) iconChar = 69; // Thunder
    else if (c.indexOf("晴") >= 0) iconChar = 64; // Sun
    
    // English matches
    else if (c.indexOf("cloud") >= 0 || c.indexOf("overcast") >= 0) iconChar = 65; 
    else if (c.indexOf("rain") >= 0 || c.indexOf("drizzle") >= 0) iconChar = 67;
    else if (c.indexOf("storm") >= 0 || c.indexOf("thunder") >= 0) iconChar = 69;
    else if (c.indexOf("snow") >= 0) iconChar = 67;
    
    return iconChar;
}

void drawBmp(String filename, int x, int y, int scale = 1) {
    File bmpFile = LittleFS.open(filename, "r");
    if (!bmpFile) {
        Serial.print("File not found: ");
        Serial.println(filename);
        return;
    }

    if (bmpFile.read() != 'B' || bmpFile.read() != 'M') {
        Serial.println("Not a BMP file");
        bmpFile.close();
        return;
    }

    // Read header
    uint32_t dataOffset;
    bmpFile.seek(0x0A);
    bmpFile.read((uint8_t*)&dataOffset, 4);
    
    uint32_t width, height;
    bmpFile.seek(0x12);
    bmpFile.read((uint8_t*)&width, 4);
    bmpFile.read((uint8_t*)&height, 4);
    
    uint16_t depth;
    bmpFile.seek(0x1C);
    bmpFile.read((uint8_t*)&depth, 2);

    if (depth != 24 && depth != 32) {
        bmpFile.close();
        return;
    }
    
    // Auto-adjust scale if image is large (e.g. user provided high-res 2x image)
    // Original icon ~40px wide. If width > 60:
    // - If scale=2 (Main Icon), set scale=1 (display 1:1, full size 80x74)
    // - If scale=1 (Forecast), set downscale=true (display 1:2, half size 40x37)
    //
    // BUT user explicitly calls drawBmp(..., 1) for -L icons (which are large).
    // If width > 60 (e.g. 72) and scale is passed as 1.
    // The current logic: if (width > 60) { if (scale == 1) downscale = true; }
    // This forces scale 1 -> 0.5 (36x36) ! This is why user sees half size.
    
    // We need to differentiate between:
    // 1. "I want to draw a large icon normally" (scale 1 passed, large icon) -> Keep scale 1
    // 2. "I want to draw a large icon in a small spot" (scale 1 passed? no, usually we wouldn't)
    //
    // The previous logic assumed if we passed scale 1, we meant "small icon slot", so if image was large, we downscaled.
    // But now for Main Icon -L, we pass scale 1 explicitly.
    // And for Forecast Icon, we pass scale 1? No, forecast loop uses standard icons.
    // If forecast loop encountered a large icon (e.g. user replaced standard icon with large one), then scale 1 -> downscale is correct.
    
    // How to fix?
    // We can change the calling code to pass scale 0 for "auto"? No.
    // Or we remove this auto-downscale logic for scale 1?
    // If we remove it:
    // - Main Icon -L (72x72, scale 1) -> Draws 72x72. Correct.
    // - Forecast Icon (Standard 36x36, scale 1) -> Draws 36x36. Correct.
    // - Forecast Icon (User replaced with 72x72, scale 1) -> Draws 72x72. Too big for forecast slot!
    
    // So the auto-downscale is valuable for "User replaced small icon with large file".
    // But it breaks "User wants to draw large icon in large slot using scale 1".
    
    // Solution:
    // In displayWeatherDashboard, for -L icons, we can pass a special scale or just use scale 1.
    // If we want to keep auto-downscale for forecast, we need a way to say "Don't downscale".
    // Maybe pass scale = 0 to mean "Native size"?
    // Or check if y position implies Main Icon?
    // Or just change the logic:
    // If scale == 1 and width > 60 -> Downscale.
    // This is exactly what hurts us.
    
    // User calls: drawBmp(iconPathL, iconX, 30, 1);
    // Width = 72. Scale = 1. -> Downscale -> 36x36.
    
    // If we change call to: drawBmp(iconPathL, iconX, 30, 0); // 0 means native?
    // Or we just remove the downscale logic for scale 1?
    // If user puts 72x72 icon in forecast (where 36x36 is expected), it will overlap.
    // Maybe that's user error?
    // But the auto-logic was "smart".
    
    // Let's modify the logic to only downscale if we really need to?
    // Or better: In displayWeatherDashboard, call with scale = 2 for -L icon?
    // No, 72x72 * 2 = 144.
    
    // Let's change the condition.
    // If we pass scale 1, we usually mean "draw as is".
    // The auto-downscale for scale 1 was probably added to handle "Large icon in small slot".
    // But now we have a valid use case for "Large icon in large slot" (Main Dashboard).
    // Main Dashboard usually uses scale 2 for small icons.
    // So if we have a large icon, we want scale 1.
    
    // What if we use a flag? Or negative scale?
    // scale = -1 means "Force 1:1, no auto-downscale"?
    
    // Let's change the logic to:
    // If scale == 1 && width > 60:
    //    We assume it's for forecast slot?
    //    Forecast slot Y is usually > 100.
    //    Main slot Y is 30.
    //    We could check Y?
    //    if (y < 100) -> Main slot -> Don't downscale.
    //    if (y > 100) -> Forecast slot -> Downscale.
    
    bool downscale = false;
    if (width > 60) {
        if (scale == 2) {
            scale = 1;
        } else if (scale == 1) {
             // Only downscale if we are NOT in the main icon area (y=30)
             // This is a bit hacky but safe for this specific layout
             if (y > 60) { 
                 downscale = true;
             }
        }
    }

    bmpFile.seek(dataOffset);

    // BMP rows are padded to 4 bytes
    uint32_t rowSize = (width * (depth / 8) + 3) & ~3;
    uint8_t *rowBuff = new (std::nothrow) uint8_t[rowSize];
    if (!rowBuff) {
        Serial.println("Memory allocation failed for rowBuff");
        bmpFile.close();
        return;
    }

    // BMP is stored bottom-up
    for (int row = 0; row < height; row++) {
        bmpFile.read(rowBuff, rowSize);
        
        if (downscale && (row % 2 != 0)) continue; // Skip odd rows for downscaling
        
        // Target Y (flip vertically)
        int ty;
        if (downscale) {
             ty = y + (height - 1 - row) / 2;
        } else {
             ty = y + (height - 1 - row) * scale;
        }
        
        for (int col = 0; col < width; col++) {
             if (downscale && (col % 2 != 0)) continue; // Skip odd cols for downscaling

             int b, g, r;
             int pxOffset = col * (depth / 8);
             
             b = rowBuff[pxOffset];
             g = rowBuff[pxOffset+1];
             r = rowBuff[pxOffset+2];
             
             // Simple thresholding
             int brightness = (r + g + b) / 3;
             uint16_t color = (brightness < 128) ? 1 : 0; // 1 = Black, 0 = White
             
             // Draw scaled pixel
             if (downscale) {
                 paint_gfx.drawPixel(x + col / 2, ty, color);
             } else {
                 for (int dy = 0; dy < scale; dy++) {
                     for (int dx = 0; dx < scale; dx++) {
                         paint_gfx.drawPixel(x + col * scale + dx, ty + dy, color);
                     }
                 }
             }
        }
    }
    
    delete[] rowBuff;
    bmpFile.close();
}

// Add font declaration
extern const uint8_t u8g2_font_logisoso50_tf[] U8G2_FONT_SECTION("u8g2_font_logisoso50_tf");

void drawIconFromProgmem(const unsigned char* data, int x, int y, int w, int h, int scale = 1) {
    if (!data) return;

    // Auto-adjust scale if image is large (e.g. user provided high-res 2x image)
    bool downscale = false;
    if (w > 60) {
        if (scale == 2) {
            scale = 1;      // Main Icon: Use full resolution
        } else if (scale == 1) {
             // Only downscale if we are NOT in the main icon area (y=30)
             if (y > 60) { 
                 downscale = true; // Forecast Icon: Downscale to 50%
             }
        }
    }
    
    // Calculate bytes per row (stride) based on width
    // Standard bitmap format: rows are byte-aligned
    int bytesPerRow = (w + 7) / 8;
    
    for (int row = 0; row < h; row++) {
        if (downscale && (row % 2 != 0)) continue; // Skip odd rows

        for (int col = 0; col < w; col++) {
            if (downscale && (col % 2 != 0)) continue; // Skip odd cols

            // Calculate byte index and bit index
            int byteIdx = row * bytesPerRow + (col / 8);
            int bitIdx = 7 - (col % 8); // MSB first
            
            uint8_t b = pgm_read_byte(&data[byteIdx]);
            
            if (b & (1 << bitIdx)) {
                // Draw pixel with scaling
                if (downscale) {
                    paint_gfx.drawPixel(x + col / 2, y + row / 2, 1);
                } else {
                    if (scale == 1) {
                        paint_gfx.drawPixel(x + col, y + row, 1);
                    } else {
                        // Draw a block for scaled pixel
                        for (int dy = 0; dy < scale; dy++) {
                            for (int dx = 0; dx < scale; dx++) {
                                paint_gfx.drawPixel(x + col * scale + dx, y + row * scale + dy, 1);
                            }
                        }
                    }
                }
            }
        }
    }
}

void displayWeatherDashboard(bool partial_update = false) {
    DEV_Module_Init();
    
    if (BlackImage == NULL) {
         Serial.println("Error: BlackImage is NULL (Should be allocated in setup)");
         // Try emergency alloc
         UWORD Imagesize = ((EPD_4IN2_WIDTH % 8 == 0) ? (EPD_4IN2_WIDTH / 8 ) : (EPD_4IN2_WIDTH / 8 + 1)) * EPD_4IN2_HEIGHT;
         BlackImage = (UBYTE *)malloc(Imagesize);
         if (BlackImage == NULL) Serial.println("BlackImage Malloc Failed (Weather)");
    }
    
    if (BlackImage != NULL) {
        UWORD InitColor = config.invert_display ? BLACK : WHITE;
        Paint_NewImage(BlackImage, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, 0, InitColor);
        Paint_SelectImage(BlackImage);
        Paint_Clear(InitColor);
        
        u8g2.begin(paint_gfx);
        u8g2.setFontMode(1); 
        u8g2.setForegroundColor(1);
        u8g2.setBackgroundColor(0);

        // --- Layout ---
        // 顶部垂直分割线 (x=200, y=10-150)
        paint_gfx.drawFastVLine(200, 10, 140, 1); 
        
        // === LEFT SIDE (Date & Time) ===
        u8g2.setFont(u8g2_font_logisoso50_tf); 
        String timeStr = timeClient.getFormattedTime().substring(0, 5); 
        int tWidth = u8g2.getUTF8Width(timeStr.c_str());
        int timeX = 100 - (tWidth / 2);
        u8g2.drawUTF8(timeX, 70, timeStr.c_str());
        
        // Check for today's event and draw bell icon
        // Only trigger for Calendar Events (Schedule), NOT Shifts
        time_t now = timeClient.getEpochTime();
        struct tm *t_now = gmtime(&now);
        int today_year = t_now->tm_year;
        int today_mon = t_now->tm_mon;
        int today_mday = t_now->tm_mday;
        
        bool hasEvent = false;
        for (const auto& ev : calendarEvents) {
            // Note: Must save today's date first because gmtime uses static buffer!
            // We already saved today_year/mon/mday above.
            struct tm *evTm = gmtime(&ev.start_time);
            if (evTm->tm_year == today_year && 
                evTm->tm_mon == today_mon && 
                evTm->tm_mday == today_mday) {
                hasEvent = true;
                break;
            }
        }
        
        if (hasEvent) {
              // Draw bell icon to the left of time
              // Time is at timeX, y=70 (baseline). Font height ~50.
              // Center roughly at y=45. Icon is 20x20.
              // drawIconFromProgmem(data, x, y, w, h, scale)
              // Adjusted for 20x20 size (was 16x16 at 38) -> 38-2 = 36
              // X was timeX - 25 -> timeX - 27
              drawIconFromProgmem(gImage_bell, timeX - 27, 36, 20, 20, 1);
         }
        
        if (solarDate.length() > 0) {
            u8g2.setFont(u8g2_font_wqy16_t_gb2312);
            String fullDate = solarDate;
            if (weekDay.length() > 0) fullDate += " " + weekDay;
            int sdWidth = u8g2.getUTF8Width(fullDate.c_str());
            u8g2.drawUTF8(100 - (sdWidth / 2), 95, fullDate.c_str());
            
            u8g2.setFont(u8g2_font_wqy12_t_gb2312);
            int ldWidth = u8g2.getUTF8Width(lunarDate.c_str());
            u8g2.drawUTF8(100 - (ldWidth / 2), 120, lunarDate.c_str());
            
            int tiWidth = u8g2.getUTF8Width(termInfo.c_str());
            u8g2.drawUTF8(100 - (tiWidth / 2), 140, termInfo.c_str());
        }

        // === RIGHT SIDE (Today's Weather) ===
        if (currentForecastCount > 0) {
            int iconX = 210;    
            int textCenterX = 340; // 用于温度显示的中心
            int panelCenterX = 300; // 整个右侧面板的中心 (200-400)
            
            int currentHour = timeClient.getHours();
            bool isNight = (currentHour >= 18 || currentHour < 6);
            String iconCode = isNight ? currentForecast[0].icon_night : currentForecast[0].icon_day;
            String condText = isNight ? currentForecast[0].cond_night : currentForecast[0].cond_day;
            if (iconCode.length() == 0) iconCode = currentForecast[0].icon_day;
            if (condText.length() == 0) condText = currentForecast[0].cond_day;
            
            // 1. 图标显示逻辑
            bool iconDrawn = false;
            if (iconCode.length() > 0) {
                // Check for Large icon first (-L)
                String iconPathL = "/icons/" + iconCode + "-L.bmp";
                if (LittleFS.exists(iconPathL)) {
                    drawBmp(iconPathL, iconX, 30, 1); 
                    iconDrawn = true;
                } else {
                    String iconPath = "/icons/" + iconCode + ".bmp";
                    if (LittleFS.exists(iconPath)) {
                        drawBmp(iconPath, iconX, 30, 2); 
                        iconDrawn = true;
                    }
                }
            }
            // 2. Try icons.h (Progmem)
            if (!iconDrawn && iconCode.length() > 0) {
                 // Try Large icon first
                 const unsigned char* iconDataL = getIconData(iconCode + "-L");
                 if (iconDataL) {
                     // Assume Large icon is 72x72 (User specified)
                     drawIconFromProgmem(iconDataL, iconX - 4, 30, 72, 72, 1); 
                     iconDrawn = true;
                 } else {
                     // Fallback to standard icon
                     const unsigned char* iconData = getIconData(iconCode);
                     if (iconData) {
                         // Data is 36x36, render full size to avoid cropping
                         drawIconFromProgmem(iconData, iconX, 30, 36, 36, 2); // Scale 2x (72x72)
                         iconDrawn = true;
                     }
                 }
            }
            if (!iconDrawn) {
                u8g2.setFont(u8g2_font_open_iconic_weather_6x_t);
                char iconStr[2] = {getIconChar(condText), 0};
                u8g2.drawUTF8(iconX + 12, 78, iconStr);
            }
            
            // 2. 温度显示 (高温/低温)
            String tempStr = currentForecast[0].temp;
            int slashIndex = tempStr.indexOf('/');
            u8g2.setFont(u8g2_font_logisoso42_tf); 
            if (slashIndex > 0) {
                String highTemp = tempStr.substring(0, slashIndex);
                String lowTemp = tempStr.substring(slashIndex + 1);
                int sx1 = textCenterX - 5, sy1 = 68, sx2 = textCenterX + 15, sy2 = 35;
                int thickness = 4; 
                for(int k = -(thickness/2); k < (thickness/2); k++) {
                      paint_gfx.drawLine(sx1 + k, sy1, sx2 + k, sy2, 1);
                }
                int hW = u8g2.getUTF8Width(highTemp.c_str());
                u8g2.drawUTF8(sx1 - hW + 4, 55, highTemp.c_str()); 
                u8g2.drawUTF8(sx2 - 14, 100, lowTemp.c_str());
            } else {
                 int tW = u8g2.getUTF8Width(tempStr.c_str());
                 u8g2.drawUTF8(textCenterX - (tW / 2), 75, tempStr.c_str());
            }
            
            // 3. 合并：天气文本 + 风向风速 (同一行)
            String windDirPart = "";
            if (currentForecast[0].wind_dir.length() > 0) windDirPart = currentForecast[0].wind_dir;
            
            String windScPart = "";
            String jiPart = "";
            if (currentForecast[0].wind_sc.length() > 0) {
                windScPart = currentForecast[0].wind_sc;
                jiPart = "级";
            }
            
            // 构建第一部分：天气 + 空格 + 风向 + 风速数值
            String weatherDetailPart1 = condText + "  " + windDirPart + windScPart;

            u8g2.setFont(u8g2_font_wqy16_t_gb2312);
            int wdW1 = u8g2.getUTF8Width(weatherDetailPart1.c_str());
            int wdW2 = (jiPart.length() > 0) ? u8g2.getUTF8Width(jiPart.c_str()) : 0;
            
            // Air Quality Part
            String aqPart = "";
            if (airCategory.length() > 0) {
                 aqPart = "  " + airCategory; 
            }
            int wdW3 = (aqPart.length() > 0) ? u8g2.getUTF8Width(aqPart.c_str()) : 0;

            // 增加3像素间距
            int gap = (jiPart.length() > 0) ? 3 : 0;
            int totalW = wdW1 + gap + wdW2 + wdW3;
            
            int wdX = panelCenterX - (totalW / 2); // 以右侧面板中心居中
            int wdY = 128; // 垂直位置：位于温度下方，室内温室度上方
            
            // 绘制第一部分 
            u8g2.drawUTF8(wdX, wdY, weatherDetailPart1.c_str());
            u8g2.drawUTF8(wdX, wdY, weatherDetailPart1.c_str());
            
            // 绘制第二部分 "级"
            if (jiPart.length() > 0) {
                u8g2.drawUTF8(wdX + wdW1 + gap, wdY, jiPart.c_str());
                u8g2.drawUTF8(wdX + wdW1 + gap + 1, wdY, jiPart.c_str());
            }

            // 绘制第三部分 Air Quality (正常粗细)
            if (aqPart.length() > 0) {
                u8g2.drawUTF8(wdX + wdW1 + gap + wdW2, wdY, aqPart.c_str());
            }

            // 4. 室内环境 (基准行)
            if (indoorTemp.length() > 0) {
                 u8g2.setFont(u8g2_font_wqy16_t_gb2312);
                 
                 // 分割为两部分，在%前增加间距
                 String envPart1 = "T:" + indoorTemp + "°C  H:" + indoorHumi;
                 String envPart2 = "%";
                 
                 int inW1 = u8g2.getUTF8Width(envPart1.c_str());
                 int inW2 = u8g2.getUTF8Width(envPart2.c_str());
                 int inGap = 3;
                 
                 int totalInW = inW1 + inGap + inW2;
                 int inX = panelCenterX - (totalInW / 2);
                 int inY = 153;
                 
                 // 垂直对齐：固定在底部 153 位置
                 u8g2.drawUTF8(inX, inY, envPart1.c_str()); 
                 u8g2.drawUTF8(inX + inW1 + inGap, inY, envPart2.c_str());
            }
        }
        
        // 分割线
        paint_gfx.drawFastHLine(0, 160, EPD_4IN2_WIDTH, 2); 

        // === 底部 6 天预报 ===
        int startY = 165;
        for(int i=1; i<currentForecastCount && i<=6; i++) {
            int x1 = (i - 1) * EPD_4IN2_WIDTH / 6;
            int x2 = i * EPD_4IN2_WIDTH / 6;
            int centerX = x1 + ((x2 - x1) / 2);
            
            String dateShort = currentForecast[i].date;
            if (dateShort.length() >= 10) dateShort = dateShort.substring(5);
            u8g2.setFont(u8g2_font_wqy12_t_gb2312);
            int dWidth = u8g2.getUTF8Width(dateShort.c_str());
            u8g2.drawUTF8(centerX - (dWidth/2), startY + 12, dateShort.c_str());
            
            bool dayIconDrawn = false;
            if (currentForecast[i].icon_day.length() > 0) {
                String iconPath = "/icons/" + currentForecast[i].icon_day + ".bmp";
                if (LittleFS.exists(iconPath)) {
                    drawBmp(iconPath, centerX - 16, startY + 24); 
                    dayIconDrawn = true;
                }
            }
            if (!dayIconDrawn && currentForecast[i].icon_day.length() > 0) {
                 const unsigned char* iconData = getIconData(currentForecast[i].icon_day);
                 if (iconData) drawIconFromProgmem(iconData, centerX - 18, startY + 24, 36, 36, 1); 
                 dayIconDrawn = true;
            }
            
            u8g2.setFont(u8g2_font_wqy12_t_gb2312);
            String fTempStr = currentForecast[i].temp;
            int ftWidth = u8g2.getUTF8Width(fTempStr.c_str());
            u8g2.drawUTF8(centerX - (ftWidth/2), startY + 75, fTempStr.c_str());

            // 4. Night Icon (Below Temp)
            bool nightIconDrawn = false;
            if (currentForecast[i].icon_night.length() > 0) {
                String iconPath = "/icons/" + currentForecast[i].icon_night + ".bmp";
                if (LittleFS.exists(iconPath)) {
                     drawBmp(iconPath, centerX - 16, startY + 80); 
                     nightIconDrawn = true;
                }
            }
            
            // Try icons.h (Progmem)
            if (!nightIconDrawn && currentForecast[i].icon_night.length() > 0) {
                 const unsigned char* iconData = getIconData(currentForecast[i].icon_night);
                 if (iconData) {
                     // Data is 36x36, render full size
                     drawIconFromProgmem(iconData, centerX - 18, startY + 80, 36, 36, 1); 
                     nightIconDrawn = true;
                 }
            }

            if (!nightIconDrawn) {
                u8g2.setFont(u8g2_font_open_iconic_weather_4x_t);
                char nightIconStr[2] = {getIconChar(currentForecast[i].cond_night), 0};
                u8g2.drawUTF8(centerX - 16, startY + 112, nightIconStr); 
            }

            if (currentForecast[i].wind_dir.length() > 0) {
                String fWind = currentForecast[i].wind_dir;
                int fWindWidth = u8g2.getUTF8Width(fWind.c_str());
        u8g2.drawUTF8(centerX - (fWindWidth/2), startY + 130, fWind.c_str());
    }
    if (i < 6) paint_gfx.drawFastVLine(x2, 160, 140, 1);
}

        // 刷屏控制
        if (partial_update) {
            Local_EPD_4IN2_Init_Partial();
            Local_EPD_4IN2_PartialDisplay(0, 0, EPD_4IN2_WIDTH, EPD_4IN2_HEIGHT, BlackImage);
        } else {
            Local_EPD_4IN2_Init();
            Local_EPD_4IN2_Display(BlackImage);
        }
        Local_EPD_4IN2_Sleep();
        // free(BlackImage); // Keep allocated
        // BlackImage = NULL;
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] Length: ");
  Serial.println(length);
  
  if (strcmp(topic, config.mqtt_date_topic) == 0) {
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (char*)payload, length);
      if (!error) {
          if (doc.containsKey("阳历日期")) solarDate = doc["阳历日期"].as<String>();
          if (doc.containsKey("星期")) weekDay = doc["星期"].as<String>();
          if (doc.containsKey("农历日期")) lunarDate = doc["农历日期"].as<String>();
          if (doc.containsKey("节气信息")) termInfo = doc["节气信息"].as<String>();
          
          Serial.println("Date info updated");
          updateDatePending = true;
          lastUpdateTrigger = millis();
      } else {
          Serial.print("JSON Error (Date): ");
          Serial.println(error.c_str());
      }
      return;
  }

        if (strcmp(topic, config.mqtt_env_topic) == 0) {
            Serial.println("Env updated");
            
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, (char*)payload, length);
            if (!error) {
                if (doc.containsKey("temp")) indoorTemp = doc["temp"].as<String>();
                if (doc.containsKey("humi")) indoorHumi = doc["humi"].as<String>();
                
                updateEnvPending = true;
                lastUpdateTrigger = millis();
            } else {
                Serial.print("JSON Error (Env): ");
                Serial.println(error.c_str());
            }
            return;
        }

        if (strcmp(topic, config.mqtt_air_quality_topic) == 0) {
             Serial.println("Air Quality Message Received");
             JsonDocument doc;
             DeserializationError error = deserializeJson(doc, payload, length);
             if (!error) {
                 if (doc.containsKey("pm2p5")) airPm2p5 = doc["pm2p5"].as<String>();
                 if (doc.containsKey("category")) airCategory = doc["category"].as<String>();
                 updateWeatherPending = true; // Refresh weather page to show new air quality
                 lastUpdateTrigger = millis();
                 Serial.printf("Air Quality Updated: PM2.5=%s, Category=%s\n", airPm2p5.c_str(), airCategory.c_str());
             } else {
                 Serial.print("JSON Error (Air): ");
                 Serial.println(error.c_str());
             }
             return;
        }

        if (strcmp(topic, config.mqtt_shift_topic) == 0) {
      Serial.println("Shift updated");
      Serial.print("Payload length: ");
      Serial.println(length);

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, (char*)payload, length);
      
      if (!error) {
          shiftEvents.clear();
          Serial.println("shiftEvents cleared");
          
          // 1. Handle Array: ["2026-02-17", "Shift"] or [{"date":"...", "shift":"..."}]
          if (doc.is<JsonArray>()) {
               JsonArray arr = doc.as<JsonArray>();
               // Check if it's a simple [date, content] pair
               if (arr.size() == 2 && arr[0].is<String>() && arr[1].is<String>()) {
                    String dateStr = arr[0].as<String>();
                    int year, month, day;
                    if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                        ShiftEvent ev;
                        ev.year = year; ev.month = month; ev.day = day;
                        ev.content = arr[1].as<String>();
                        shiftEvents.push_back(ev);
                    }
               } 
               else {
                   // Iterate array items
                   for (JsonVariant v : arr) {
                       if (shiftEvents.size() >= MAX_SHIFT_EVENTS) break;
                       
                       if (v.is<JsonObject>()) {
                           JsonObject obj = v.as<JsonObject>();
                           String dateStr = "";
                           if (obj.containsKey("date")) dateStr = obj["date"].as<String>();
                           else if (obj.containsKey("day")) dateStr = obj["day"].as<String>();
                           
                           if (dateStr.length() > 0) {
                               int year, month, day;
                               if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                                   ShiftEvent ev;
                                   ev.year = year; ev.month = month; ev.day = day;
                                   if (obj.containsKey("shift")) ev.content = obj["shift"].as<String>();
                                   else if (obj.containsKey("content")) ev.content = obj["content"].as<String>();
                                   else if (obj.containsKey("summary")) ev.content = obj["summary"].as<String>();
                                   
                                   if (ev.content.length() > 0) shiftEvents.push_back(ev);
                               }
                           }
                       }
                   }
               }
          }
          // 2. Handle Object
          else if (doc.is<JsonObject>()) {
               JsonObject root = doc.as<JsonObject>();
               
               // Use current time as default Year/Month context
               time_t now = timeClient.getEpochTime();
               struct tm * t = gmtime(&now);
               int currentYear = t->tm_year + 1900;
               int currentMonth = t->tm_mon + 1;
               
               // First, try to iterate ALL keys to find dates like "2026-02-16" OR "16"
               bool foundDateKeys = false;
               for (JsonPair kv : root) {
                   if (shiftEvents.size() >= MAX_SHIFT_EVENTS) break;

                   String rawKey = kv.key().c_str();
                   // Sanitize key: keep only 0-9 and -
                   String key = "";
                   for (unsigned int i = 0; i < rawKey.length(); i++) {
                       char c = rawKey[i];
                       if (isdigit(c) || c == '-') {
                           key += c;
                       }
                   }

                   int year, month, day;
                   
                   // Check if key is a full date format YYYY-MM-DD
                   if (sscanf(key.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                       if (kv.value().is<String>()) {
                           ShiftEvent ev;
                           ev.year = year;
                           ev.month = month;
                           ev.day = day;
                           ev.content = kv.value().as<String>();
                           if (ev.content.length() > 0) {
                               shiftEvents.push_back(ev);
                               foundDateKeys = true;
                               // Update context for subsequent simplified dates
                               currentYear = year;
                               currentMonth = month;
                               Serial.printf("Parsed Shift (Full): %d-%d-%d = %s\n", year, month, day, ev.content.c_str());
                           }
                       }
                   } 
                   // Check if key is a simplified date format "DD" (1-31)
                   else if (sscanf(key.c_str(), "%d", &day) == 1) {
                       // Ensure parsed day is valid and key doesn't contain '-' (to avoid partial full-date matches)
                       if (key.indexOf('-') == -1 && day >= 1 && day <= 31 && kv.value().is<String>()) {
                           ShiftEvent ev;
                           ev.year = currentYear;
                           ev.month = currentMonth;
                           ev.day = day;
                           ev.content = kv.value().as<String>();
                           if (ev.content.length() > 0) {
                               shiftEvents.push_back(ev);
                               foundDateKeys = true;
                               Serial.printf("Parsed Shift (Simple): %d-%d-%d = %s\n", currentYear, currentMonth, day, ev.content.c_str());
                           }
                       }
                   }
               }
               
               // If no date keys found, check for other structures
               if (!foundDateKeys) {
                   // Check for {"date": "...", "shift": "..."} single object
                   if (root.containsKey("date") || root.containsKey("day")) {
                        String dateStr = root.containsKey("date") ? root["date"].as<String>() : root["day"].as<String>();
                        int year, month, day;
                        if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                            ShiftEvent ev;
                            ev.year = year; ev.month = month; ev.day = day;
                            if (root.containsKey("shift")) ev.content = root["shift"].as<String>();
                            else if (root.containsKey("content")) ev.content = root["content"].as<String>();
                            if (ev.content.length() > 0) shiftEvents.push_back(ev);
                        }
                   }
                   // Check for "shifts" or "data" array
                   else {
                       JsonArray shifts;
                       if (root.containsKey("shifts")) shifts = root["shifts"];
                       else if (root.containsKey("data")) shifts = root["data"];
                       
                       if (!shifts.isNull()) {
                           for (JsonVariant v : shifts) {
                               if (shiftEvents.size() >= MAX_SHIFT_EVENTS) break;
                               if (v.is<JsonObject>()) {
                                   JsonObject obj = v.as<JsonObject>();
                                   String dateStr = "";
                                   if (obj.containsKey("date")) dateStr = obj["date"].as<String>();
                                   
                                   int year, month, day;
                                   if (sscanf(dateStr.c_str(), "%d-%d-%d", &year, &month, &day) == 3) {
                                       ShiftEvent ev;
                                       ev.year = year; ev.month = month; ev.day = day;
                                       if (obj.containsKey("shift")) ev.content = obj["shift"].as<String>();
                                       if (ev.content.length() > 0) shiftEvents.push_back(ev);
                                   }
                               }
                           }
                       }
                   }
               }
          }
          
          Serial.print("Total shifts parsed: ");
          Serial.println(shiftEvents.size());
          
          updateCalendarPending = true;
          lastUpdateTrigger = millis();
      } else {
          Serial.print("JSON Error (Shift): ");
          Serial.println(error.c_str());
      }
      return;
  }

  if (strcmp(topic, config.mqtt_calendar_topic) == 0) {
      Serial.println("Calendar updated");

      JsonDocument doc;
      // Filter to reduce memory usage
      JsonDocument filter;
      filter["events"][0]["start"] = true;
      filter["events"][0]["summary"] = true;
      // Support 1 level nesting
      filter["*"]["events"][0]["start"] = true;
      filter["*"]["events"][0]["summary"] = true;
      
      DeserializationError error = deserializeJson(doc, (char*)payload, length, DeserializationOption::Filter(filter));
      if (!error) {
          calendarEvents.clear();
          // The structure is {"calendar.my_calendar":{"events":[...]}}
          // We need to find the events array. It might be nested or direct.
          
          JsonArray events;
          if (doc.containsKey("events")) {
              events = doc["events"];
          } else {
              // Iterate through keys to find one that contains "events"
              JsonObject root = doc.as<JsonObject>();
              for (JsonPair kv : root) {
                   if (kv.value().is<JsonObject>() && kv.value().as<JsonObject>().containsKey("events")) {
                       events = kv.value()["events"];
                       break;
                   }
              }
          }

          if (!events.isNull()) {
              for (JsonVariant v : events) {
                  if (calendarEvents.size() >= MAX_CALENDAR_EVENTS) break; // Limit events

                  CalendarEvent ev;
                  // Handle "start" time string: "2026-02-15T23:00:00+08:00"
                  String startStr = v["start"].as<String>();
                  // Simple parsing for now (assuming fixed format or using library if available)
                  // For this demo, let's parse basic components
                  // 2026-02-15T23:00:00+08:00
                  int year = startStr.substring(0, 4).toInt();
                  int month = startStr.substring(5, 7).toInt();
                  int day = startStr.substring(8, 10).toInt();
                  int hour = startStr.substring(11, 13).toInt();
                  int minute = startStr.substring(14, 16).toInt();
                  int second = startStr.substring(17, 19).toInt();
                  
                  struct tm t = {0};
                   t.tm_year = year - 1900;
                   t.tm_mon = month - 1;
                   t.tm_mday = day;
                   t.tm_hour = hour;
                   t.tm_min = minute;
                   t.tm_sec = second;
                   t.tm_isdst = -1;
                   
                   // mktime uses local time zone, but we don't have TZ set in environment usually.
                   // However, timeClient uses an offset.
                   // The date string IS local time (UTC+8).
                   // We want to compare this with "now" from timeClient which is also shifted by offset.
                   // So we should treat this struct tm as UTC so mktime returns the raw epoch that represents that specific time point 
                   // BUT wait, timeClient.getEpochTime() returns UTC epoch + offset?
                   // No, getEpochTime() returns Unix Epoch (UTC).
                   // But getHours() uses the offset.
                   // Wait, timeClient documentation says:
                   // "getEpochTime() returns the Unix epoch, which is the number of seconds that have elapsed since January 1, 1970 (midnight UTC/GMT), not counting leap seconds."
                   // BUT we initialize it with offset 28800 (UTC+8).
                   // Let's check NTPClient source or assume:
                   // If we init with offset, getEpochTime() returns (UTC_Epoch + Offset).
                   // So it returns "Local Epoch".
                   
                   // So if we parse "2026-02-15 23:00" which IS local time.
                   // We want to convert this YMDHMS (Local) to "Local Epoch".
                   // Since ESP8266 default mktime assumes UTC (if TZ not set),
                   // mktime(&t) will give us the epoch AS IF that time was UTC.
                   // This is exactly what we want! 
                   // Example: 
                   // Real UTC: 12:00. Local (UTC+8): 20:00.
                   // timeClient (with offset) returns epoch for 20:00.
                   // Input string "20:00".
                   // mktime("20:00") -> epoch for 20:00.
                   // So they match!
                   
                   ev.start_time = mktime(&t); 
                   
                   // Debug
                   Serial.print("Event: ");
                   Serial.print(year); Serial.print("-"); Serial.print(month); Serial.print("-"); Serial.print(day);
                   Serial.print(" -> Epoch: "); Serial.println(ev.start_time);
                   
                   ev.summary = v["summary"].as<String>();
                   calendarEvents.push_back(ev);
              }
              Serial.print("Parsed events: ");
              Serial.println(calendarEvents.size());
              
              updateCalendarPending = true;
              lastUpdateTrigger = millis();
          }
      } else {
          Serial.print("JSON Error (Calendar): ");
          Serial.println(error.c_str());
      }
      return;
  }
  
  if (strcmp(topic, "epd/weatherrequest") == 0) {
       // Ignore our own request messages if subscribed
       return; 
  }

  if (strcmp(topic, config.mqtt_weather_topic) == 0) {
      Serial.println("Weather updated");

      JsonDocument doc;
      // Filter to exclude hourly/minutely data which consumes memory
      JsonDocument filter;
      // Array root
      filter[0]["fxDate"] = true; filter[0]["date"] = true; filter[0]["日期"] = true;
      filter[0]["tempMax"] = true; filter[0]["tempMin"] = true; filter[0]["temp"] = true; filter[0]["最高温"] = true; filter[0]["最低温"] = true;
      filter[0]["textDay"] = true; filter[0]["textNight"] = true; filter[0]["白天天气"] = true; filter[0]["夜晚天气"] = true; filter[0]["天气"] = true; filter[0]["weather"] = true;
      filter[0]["iconDay"] = true; filter[0]["iconNight"] = true;
      filter[0]["windDirDay"] = true; filter[0]["windDir"] = true; filter[0]["风向"] = true;
      filter[0]["windScaleDay"] = true; filter[0]["windScale"] = true; filter[0]["windSpeedDay"] = true; filter[0]["风速"] = true;
      
      // Nested arrays
      filter["daily"] = filter; // Reuse the array filter structure for "daily" key
      filter["data"] = filter;
      filter["forecast"] = filter;
      
      // But wait, filter["daily"] = filter assigns the WHOLE filter object to "daily".
      // The filter object currently contains [0]... and "daily"...
      // This recursive assignment might be weird or efficient.
      // Actually, if I do `filter["daily"][0]["fxDate"] = true`, it's better.
      // But re-typing is tedious.
      // Let's just type it out to be safe and clear.
      
      // Re-doing the filter construction properly:
      JsonDocument f;
      JsonObject p = f.to<JsonObject>();
      // Define the element filter
      JsonObject e = p.createNestedObject("e"); // Temporary object to hold element filter
      e["fxDate"] = true; e["date"] = true; e["日期"] = true;
      e["tempMax"] = true; e["tempMin"] = true; e["temp"] = true; e["最高温"] = true; e["最低温"] = true;
      e["textDay"] = true; e["textNight"] = true; e["白天天气"] = true; e["夜晚天气"] = true; e["天气"] = true; e["weather"] = true;
      e["iconDay"] = true; e["iconNight"] = true;
      e["windDirDay"] = true; e["windDir"] = true; e["风向"] = true;
      e["windScaleDay"] = true; e["windScale"] = true; e["windSpeedDay"] = true; e["风速"] = true;
      
      // Apply to paths
      filter[0] = e;
      filter["daily"][0] = e;
      filter["data"][0] = e;
      filter["forecast"][0] = e;

      DeserializationError error = deserializeJson(doc, (char*)payload, length, DeserializationOption::Filter(filter));

      if (!error) {
          Serial.println("Parsing JSON...");
          
          int count = 0;

          // Check if root is an array (specific format requested)
          if (doc.is<JsonArray>()) {
              Serial.println("Found root array data");
              JsonArray data = doc.as<JsonArray>();
              for(JsonVariant v : data) {
                  if (count >= 7) break;
                  
                  // Mapping new JSON format keys
                  if (v.containsKey("fxDate")) currentForecast[count].date = v["fxDate"].as<String>();
                  else if (v.containsKey("日期")) currentForecast[count].date = v["日期"].as<String>();
                  else if (v.containsKey("date")) currentForecast[count].date = v["date"].as<String>();
                  
                  // Temp: High/Low range
                  if (v.containsKey("tempMax") && v.containsKey("tempMin")) {
                      currentForecast[count].temp = v["tempMax"].as<String>() + "/" + v["tempMin"].as<String>();
                  } else if (v.containsKey("最高温") && v.containsKey("最低温")) {
                      currentForecast[count].temp = v["最高温"].as<String>() + "/" + v["最低温"].as<String>();
                  } else if (v.containsKey("temp")) {
                      currentForecast[count].temp = v["temp"].as<String>();
                  }
                  
                  // Condition Day
                  if (v.containsKey("textDay")) currentForecast[count].cond_day = v["textDay"].as<String>();
                  else if (v.containsKey("白天天气")) currentForecast[count].cond_day = v["白天天气"].as<String>();
                  else if (v.containsKey("天气")) currentForecast[count].cond_day = v["天气"].as<String>();
                  else if (v.containsKey("weather")) currentForecast[count].cond_day = v["weather"].as<String>();
                  
                  // Condition Night
                  if (v.containsKey("textNight")) currentForecast[count].cond_night = v["textNight"].as<String>();
                  else if (v.containsKey("夜晚天气")) currentForecast[count].cond_night = v["夜晚天气"].as<String>();

                  // Icons
                  if (v.containsKey("iconDay")) currentForecast[count].icon_day = v["iconDay"].as<String>();
                  if (v.containsKey("iconNight")) currentForecast[count].icon_night = v["iconNight"].as<String>();
                  
                  // Wind
                  if (v.containsKey("windDirDay")) currentForecast[count].wind_dir = v["windDirDay"].as<String>();
                  else if (v.containsKey("windDir")) currentForecast[count].wind_dir = v["windDir"].as<String>();
                  else if (v.containsKey("风向")) currentForecast[count].wind_dir = v["风向"].as<String>();
                  
                  if (v.containsKey("windScaleDay")) currentForecast[count].wind_sc = v["windScaleDay"].as<String>();
                  else if (v.containsKey("windScale")) currentForecast[count].wind_sc = v["windScale"].as<String>();
                  else if (v.containsKey("windSpeedDay")) currentForecast[count].wind_sc = v["windSpeedDay"].as<String>();
                  else if (v.containsKey("风速")) currentForecast[count].wind_sc = v["风速"].as<String>();
                  
                  if (currentForecast[count].date.length() > 0) {
                      count++;
                  }
              }
          } 
          // Handle object with data array (standard formats)
          else {
              JsonArray data = doc["data"];
              if (data.isNull()) data = doc["daily"]; 
              if (data.isNull()) data = doc["forecast"];
              
              if (!data.isNull()) {
                  Serial.println("Found nested array data");
                  for(JsonVariant v : data) {
                      if (count >= 7) break;
                      
                      if (v.containsKey("date")) currentForecast[count].date = v["date"].as<String>();
                      else if (v.containsKey("day")) currentForecast[count].date = v["day"].as<String>();
                      
                      if (v.containsKey("temp")) currentForecast[count].temp = v["temp"].as<String>();
                      else if (v.containsKey("high")) currentForecast[count].temp = v["high"].as<String>();
                      
                      if (v.containsKey("weather")) currentForecast[count].cond_day = v["weather"].as<String>();
                      else if (v.containsKey("text")) currentForecast[count].cond_day = v["text"].as<String>();
                      
                      if (currentForecast[count].date.length() > 0) count++;
                  }
              }
          }

          Serial.print("Forecast count: ");
          Serial.println(count);
          currentForecastCount = count;

          if (count > 0) {
              updateWeatherPending = true;
              lastUpdateTrigger = millis();
          } else {
               String prettyJson;
               serializeJsonPretty(doc, prettyJson);
               displayMessage("JSON Data (No forecast found):\n" + prettyJson);
          }
      } else {
          Serial.print("JSON Error: ");
          Serial.println(error.c_str());
          
          String msg = "";
          for (int i = 0; i < length; i++) msg += (char)payload[i];
          displayMessage("JSON Error:\n" + String(error.c_str()) + "\n" + msg);
      }
  } else {
      String msg = "";
      for (int i = 0; i < length; i++) msg += (char)payload[i];
      displayMessage(msg);
  }
}

bool reconnect() {
  if (strlen(config.mqtt_server) == 0) return false;
  
  if (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    bool connected = false;
    if (strlen(config.mqtt_user) > 0) {
      connected = client.connect(clientId.c_str(), config.mqtt_user, config.mqtt_pass);
    } else {
      connected = client.connect(clientId.c_str());
    }

    if (connected) {
      Serial.println("connected");
      client.subscribe(config.mqtt_topic);
      if (strlen(config.mqtt_weather_topic) > 0) {
          if (client.subscribe(config.mqtt_weather_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_weather_topic);
          } else {
              Serial.println("Subscription failed");
          }
      }
      if (strlen(config.mqtt_date_topic) > 0) {
          if (client.subscribe(config.mqtt_date_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_date_topic);
          }
      }
      if (strlen(config.mqtt_env_topic) > 0) {
          if (client.subscribe(config.mqtt_env_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_env_topic);
          }
      }
      if (strlen(config.mqtt_calendar_topic) > 0) {
          if (client.subscribe(config.mqtt_calendar_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_calendar_topic);
          }
      }
      if (strlen(config.mqtt_shift_topic) > 0) {
          if (client.subscribe(config.mqtt_shift_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_shift_topic);
          }
      }
      if (strlen(config.mqtt_air_quality_topic) > 0) {
          if (client.subscribe(config.mqtt_air_quality_topic)) {
              Serial.print("Subscribed to: ");
              Serial.println(config.mqtt_air_quality_topic);
          }
      }
      
      // Send Weather Request on Connect
      client.loop();
      delay(100);
      
      if (client.publish("epd/weatherrequest", "get")) {
          Serial.println("Weather Request sent (Reconnect)");
      } else {
          Serial.println("Weather Request failed (Reconnect)");
      }
      return true;

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again later");
      return false;
    }
  }
  return true;
}

// handleRoot moved to WebHandler.cpp

// handleFileManager moved to WebHandler.cpp

// WebHandler functions now in separate file

// Button Handlers
void handleButtonClick() {
    Serial.println("Click: Switching Page...");
    if (currentPage == PAGE_WEATHER) {
        currentPage = PAGE_CALENDAR;
        displayCalendarPage(false); // Full refresh
    } else {
        currentPage = PAGE_WEATHER;
        displayWeatherDashboard(false); // Full refresh
    }
}

void handleButtonDoubleClick() {
    Serial.println("Double Click: Requesting Weather...");
    if (client.connected()) {
        client.publish("epd/weatherrequest", "get");
    } else {
        Serial.println("MQTT not connected, cannot send request");
    }
}

void handleButtonLongPress() {
    Serial.println("Long Press: Refreshing Current Page...");
    if (currentPage == PAGE_WEATHER) {
        displayWeatherDashboard(false); // Full refresh
    } else {
        displayCalendarPage(false); // Full refresh
    }
}

unsigned long lastMqttRetry = 0;
bool wifiWarningShown = false;
bool mqttWarningShown = false;
bool mqttGiveUp = false; // Add give up flag

void setup() {
  printf("EPD_4IN2 WiFi Demo\r\n");
  DEV_Module_Init();
  
  // Load Config
  loadConfig();
  
  Serial.print("Air Quality Topic: ");
  Serial.println(config.mqtt_air_quality_topic);
  
  // Setup WiFi
  bool enableAP = true;
  
  if (strlen(config.wifi_ssid) > 0) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(config.wifi_ssid, config.wifi_pass);
      
      Serial.print("Connecting to WiFi");
      int retry = 0;
      while (WiFi.status() != WL_CONNECTED && retry < 20) { // 10 seconds
          delay(500);
          Serial.print(".");
          retry++;
      }
      Serial.println();
      
      if (WiFi.status() == WL_CONNECTED) {
          Serial.println("WiFi Connected! AP Disabled.");
          enableAP = false;
          
          // Setup NTP
          if (strlen(config.ntp_server) > 0) {
              timeClient.setPoolServerName(config.ntp_server);
          } else {
              timeClient.setPoolServerName("ntp1.aliyun.com");
          }
          timeClient.setTimeOffset(28800);
          timeClient.begin(); // Start NTP
          
          // Wait for NTP Sync
          Serial.print("Waiting for NTP...");
          int retry = 0;
          bool ntpSynced = false;
          while(retry < 40) { // Try for 20 seconds
              if (timeClient.forceUpdate()) {
                  ntpSynced = true;
                  break;
              }
              delay(500);
              Serial.print(".");
              retry++;
          }
          if (ntpSynced) {
              Serial.println(" Synced");
              Serial.println(timeClient.getFormattedTime());
          } else {
              Serial.println(" Timeout (Will retry in background)");
          }

      } else {
          Serial.println("WiFi Timeout. Enabling AP.");
          WiFi.mode(WIFI_AP_STA);
      }
  } else {
      WiFi.mode(WIFI_AP);
  }
  
  if (enableAP) {
      // Append MAC suffix to AP SSID using raw bytes for reliability
      uint8_t mac[6];
      WiFi.macAddress(mac);
      Serial.printf("Raw MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      
      char macSuffix[5];
      sprintf(macSuffix, "%02X%02X", mac[4], mac[5]);
      
      ap_ssid = String(DEFAULT_AP_SSID_BASE) + "-" + String(macSuffix);
      
      WiFi.softAP(ap_ssid.c_str());
      Serial.println("AP Started: " + ap_ssid);
  }
  
  // Setup NTP
  if (strlen(config.ntp_server) > 0) {
      timeClient.setPoolServerName(config.ntp_server);
  } else {
      timeClient.setPoolServerName("ntp1.aliyun.com");
  }
  timeClient.setTimeOffset(28800);
  timeClient.begin();
  
  // Setup MQTT
  if (strlen(config.mqtt_server) > 0) {
      client.setServer(config.mqtt_server, config.mqtt_port);
      client.setCallback(mqttCallback);
      client.setBufferSize(4096); // Increase buffer for large JSON
      
      // Attempt connection
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      if (client.connect(clientId.c_str(), config.mqtt_user, config.mqtt_pass)) {
          Serial.println("MQTT Connected (Setup)");
          client.subscribe(config.mqtt_topic);
          client.subscribe(config.mqtt_weather_topic);
          client.subscribe(config.mqtt_date_topic);
          client.subscribe(config.mqtt_env_topic);
          client.subscribe(config.mqtt_calendar_topic);
          client.subscribe(config.mqtt_shift_topic);
          client.subscribe(config.mqtt_air_quality_topic);
          
          client.loop(); // Process incoming messages (e.g. SUBACK)
          delay(100);

          // Send Weather Request on Connect
          if (client.publish("epd/weatherrequest", "get")) {
              Serial.println("Weather Request sent (Setup)");
          } else {
              Serial.println("Weather Request failed (Setup)");
          }
      } else {
          Serial.print("MQTT Connect failed, rc=");
          Serial.println(client.state());
      }
  }



  server.on("/", handleRoot);
  server.on("/setPage", HTTP_POST, handleSetPage);
  server.on("/files", handleFileManager);
  server.on("/setText", handleSetText);
  server.on("/saveConfig", handleSaveConfig);
  server.on("/reboot", HTTP_POST, handleReboot);
  server.on("/update", HTTP_GET, handleUpdate);
  server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      if (Update.hasError()) {
          server.send(500, "text/plain", "Update Failed");
      } else {
          server.send(200, "text/html", "Update Success. Rebooting...");
          delay(1000);
          ESP.restart();
      }
  }, handleUpdateFirmware);
  server.on("/upload", HTTP_POST, [](){ server.send(200, "text/plain", ""); }, handleUpload);
  // handleDisplayImage route removed
  server.on("/delete", handleDelete);
  server.begin();
  
  printf("HTTP server started\r\n");

  // Setup OTA
  ArduinoOTA.setHostname("EPD-4in2-Demo");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Serial.println("Start updating " + type);
    displayMessage("OTA Updating...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  // Setup Button
  button.attachClick(handleButtonClick);
  button.attachDoubleClick(handleButtonDoubleClick);
  button.attachLongPressStart(handleButtonLongPress);

  // Initial Display
  String statusMsg = "";
  if (WiFi.status() == WL_CONNECTED) {
      statusMsg = "WiFi: " + String(config.wifi_ssid) + "\nIP: " + WiFi.localIP().toString() + "\nConfigure via Web UI (IP)";
  } else {
      statusMsg = "AP: " + String(ap_ssid) + "\nIP: 192.168.4.1\nConfigure via Web UI";
  }
  displayMessage(statusMsg);
}

void loop() {
  ArduinoOTA.handle();
  button.tick();
  server.handleClient();
  
  // WiFi Handling
  if (strlen(config.wifi_ssid) > 0) {
      if (WiFi.status() != WL_CONNECTED) {
          if (!wifiWarningShown) {
              Serial.println("WiFi Lost, showing warning");
              displayMessage("WiFi Connection Lost\nReconnecting...");
              wifiWarningShown = true;
          }
      } else {
          if (wifiWarningShown) {
              Serial.println("WiFi Restored");
              displayMessage("WiFi Connected!");
              wifiWarningShown = false;
              delay(2000); 
          }
      }
  }

  if (strlen(config.mqtt_server) > 0 && WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
          if (!mqttGiveUp) {
              unsigned long now = millis();
              if (now - lastMqttRetry > 5000) {
                  lastMqttRetry = now;
                  
                  if (!mqttWarningShown) {
                       displayMessage("MQTT Connection Lost\nReconnecting...");
                       mqttWarningShown = true;
                  }
                  
                  if (reconnect()) {
                       displayMessage("MQTT Connected!\nFetching Weather...");
                       mqttWarningShown = false;
                  } else {
                       // If reconnect fails, give up and show error
                       mqttGiveUp = true;
                       displayMessage("MQTT Connect Failed\nWill not retry");
                       Serial.println("MQTT Connect Failed, giving up");
                  }
              }
          }
      } else {
          client.loop();
          mqttGiveUp = false; // Reset give up flag on successful connection
  
  // Handle deferred updates
  if (millis() - lastUpdateTrigger > UPDATE_DELAY_MS) {
      if (updateWeatherPending || updateEnvPending || updateDatePending) {
          if (currentPage == PAGE_WEATHER) {
               Serial.println("Triggering Deferred Weather Update");
               displayWeatherDashboard(true);
          }
          updateWeatherPending = false;
          updateEnvPending = false;
          updateDatePending = false;
      }
      
      if (updateCalendarPending) {
          if (currentPage == PAGE_CALENDAR) {
               Serial.println("Triggering Deferred Calendar Update");
               displayCalendarPage(true);
          }
          updateCalendarPending = false;
      }
  }
          mqttWarningShown = false;
      }
  }
  
  // Handle Immediate UI Page Switch
  if (switchPagePending) {
      Serial.println("UI Page Switch Triggered");
      if (currentPage == PAGE_WEATHER) {
          displayWeatherDashboard(false); // Full refresh
      } else if (currentPage == PAGE_CALENDAR) {
          displayCalendarPage(false); // Full refresh
      }
      switchPagePending = false;
  }
  
  // Optimized NTP Sync Strategy
  static unsigned long lastNtpRetry = 0;
  if (!timeClient.isTimeSet()) {
      if (millis() - lastNtpRetry > 5000) { // Retry every 5 seconds
          lastNtpRetry = millis();
          Serial.println("NTP not synced, forcing update...");
          timeClient.forceUpdate();
      }
  } else {
      timeClient.update();
  }
  
  // Check for minute change
  int currentMinute = timeClient.getMinutes();
  if (lastMinute != -1 && currentMinute != lastMinute) {
      // Minute changed
      int currentHour = timeClient.getHours();
      
      // If it is midnight (00:00), refresh both pages fully to update date
      if (currentHour == 0 && currentMinute == 0) {
          Serial.println("Midnight! Refreshing page...");
          if (currentPage == PAGE_WEATHER) {
               displayWeatherDashboard(false); // Full update
          } else if (currentPage == PAGE_CALENDAR) {
               displayCalendarPage(false); // Full update
          }
      } else {
          // Normal minute update (only for weather page)
          if (currentPage == PAGE_WEATHER && currentForecastCount > 0) {
              Serial.println("Minute changed, updating display (Partial)...");
              displayWeatherDashboard(true);
          }
      }
  }
  lastMinute = currentMinute;
}
