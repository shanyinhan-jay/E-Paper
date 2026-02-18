#include "WebHandler.h"
#include <Arduino.h>
#include <Update.h>
#include "Local_EPD_4in2.h"
#include "GUI_Paint.h"

// Helper to replace placeholders
String processor(const String& var) {
  if(var == "WIFI_SSID") return String(config.wifi_ssid);
  if(var == "WIFI_PASS") return String(config.wifi_pass);
  if(var == "MQTT_SERVER") return String(config.mqtt_server);
  if(var == "MQTT_PORT") return String(config.mqtt_port);
  if(var == "MQTT_USER") return String(config.mqtt_user);
  if(var == "MQTT_PASS") return String(config.mqtt_pass);
  if(var == "MQTT_TOPIC") return String(config.mqtt_topic);
  if(var == "MQTT_WEATHER") return String(config.mqtt_weather_topic);
  if(var == "MQTT_DATE") return String(config.mqtt_date_topic);
  if(var == "MQTT_ENV") return String(config.mqtt_env_topic);
  if(var == "MQTT_CALENDAR") return String(config.mqtt_calendar_topic);
  if(var == "MQTT_SHIFT") return String(config.mqtt_shift_topic);
  if(var == "MQTT_AQI") return String(config.mqtt_air_quality_topic);
  if(var == "FULL_REFRESH") return String(config.full_refresh_period);
  if(var == "DAY_START") return String(config.day_start_hour);
  if(var == "DAY_END") return String(config.day_end_hour);
  if(var == "BUILD_DATE") return String(build_date);
  if(var == "BUILD_TIME") return String(build_time);
  return String();
}

void handleRoot() {
  String html = INDEX_HTML_TEMPLATE;
  
  // Replace placeholders manually since server.send() doesn't support template processor directly like ESPAsyncWebServer
  // A more efficient way would be to stream it, but for simplicity we replace strings
  html.replace("%CSS%", COMMON_CSS);
  html.replace("%WIFI_SSID%", String(config.wifi_ssid));
  html.replace("%WIFI_PASS%", String(config.wifi_pass));
  html.replace("%MQTT_SERVER%", String(config.mqtt_server));
  html.replace("%MQTT_PORT%", String(config.mqtt_port));
  html.replace("%MQTT_USER%", String(config.mqtt_user));
  html.replace("%MQTT_PASS%", String(config.mqtt_pass));
  html.replace("%MQTT_TOPIC%", String(config.mqtt_topic));
  html.replace("%MQTT_WEATHER%", String(config.mqtt_weather_topic));
  html.replace("%MQTT_DATE%", String(config.mqtt_date_topic));
  html.replace("%MQTT_ENV%", String(config.mqtt_env_topic));
  html.replace("%MQTT_CALENDAR%", String(config.mqtt_calendar_topic));
  html.replace("%MQTT_SHIFT%", String(config.mqtt_shift_topic));
  html.replace("%MQTT_AQI%", String(config.mqtt_air_quality_topic));
  html.replace("%NTP_SERVER%", String(config.ntp_server));
  html.replace("%NTP_SERVER_2%", String(config.ntp_server_2));
  html.replace("%FULL_REFRESH%", String(config.full_refresh_period));
  html.replace("%DAY_START%", String(config.day_start_hour));
  html.replace("%DAY_END%", String(config.day_end_hour));
  
  // Handle Radio Button State
  if (config.invert_display) {
      html.replace("%INVERT_0%", "");
      html.replace("%INVERT_1%", "checked");
  } else {
      html.replace("%INVERT_0%", "checked");
      html.replace("%INVERT_1%", "");
  }
  
  html.replace("%BUILD_DATE%", String(build_date));
  html.replace("%BUILD_TIME%", String(build_time));
  
  server.send(200, "text/html", html);
}

void handleFileManager() {
  String html = FILE_MANAGER_HTML_TEMPLATE;
  html.replace("%CSS%", COMMON_CSS);
  String fileList = "";
  
  File root = LittleFS.open("/icons");
  if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while(file){
          String fileName = String(file.name());
          fileList += "<li>" + fileName + " (" + String(file.size()) + " bytes) ";
          fileList += "<a href='/delete?file=" + fileName + "' onclick='return confirm(\"Delete this file?\");'>[Delete]</a></li>";
          file = root.openNextFile();
      }
  } else {
      fileList = "<li>No files found or directory missing</li>";
  }
  
  html.replace("%FILE_LIST%", fileList);
  server.send(200, "text/html", html);
}

void handleUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    if (!filename.startsWith("/icons/")) filename = "/icons" + filename; // Force icons folder
    
    Serial.print("Upload start: "); Serial.println(filename);
    // Ensure directory exists
    if (!LittleFS.exists("/icons")) LittleFS.mkdir("/icons");
    
    File f = LittleFS.open(filename, "w");
    if (!f) Serial.println("Failed to open file for writing");
    f.close();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/" + filename;
    if (!filename.startsWith("/icons/")) filename = "/icons" + filename;
    
    File f = LittleFS.open(filename, "a");
    if (f) {
        f.write(upload.buf, upload.currentSize);
        f.close();
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    Serial.print("Upload Size: "); Serial.println(upload.totalSize);
    server.sendHeader("Location", "/files");
    server.send(303);
  }
}

void handleUpdate() {
  String html = UPDATE_HTML_TEMPLATE;
  html.replace("%CSS%", COMMON_CSS);
  server.send(200, "text/html", html);
}

void handleUpdateFirmware() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // start with max available size
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    /* flashing firmware to ESP */
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true to set the size to the current progress
      Serial.printf("Update Success: %u\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleDelete() {
    if (server.hasArg("file")) {
        String filename = server.arg("file");
        if (!filename.startsWith("/")) filename = "/" + filename;
        if (!filename.startsWith("/icons/")) filename = "/icons" + filename;
        
        if (LittleFS.exists(filename)) {
            LittleFS.remove(filename);
            Serial.print("Deleted: "); Serial.println(filename);
        }
    }
  server.sendHeader("Location", "/files");
  server.send(303);
}

void handleReboot() {
  server.send(200, "text/html", "<html><body><h1>Rebooting...</h1><p>Please wait while the device restarts.</p></body></html>");
  delay(500);
  ESP.restart();
}

void handleSaveConfig() {
  if (server.hasArg("wifi_ssid")) strlcpy(config.wifi_ssid, server.arg("wifi_ssid").c_str(), sizeof(config.wifi_ssid));
  if (server.hasArg("wifi_pass")) strlcpy(config.wifi_pass, server.arg("wifi_pass").c_str(), sizeof(config.wifi_pass));
  if (server.hasArg("mqtt_server")) strlcpy(config.mqtt_server, server.arg("mqtt_server").c_str(), sizeof(config.mqtt_server));
  if (server.hasArg("mqtt_port")) config.mqtt_port = server.arg("mqtt_port").toInt();
  if (server.hasArg("mqtt_user")) strlcpy(config.mqtt_user, server.arg("mqtt_user").c_str(), sizeof(config.mqtt_user));
  if (server.hasArg("mqtt_pass")) strlcpy(config.mqtt_pass, server.arg("mqtt_pass").c_str(), sizeof(config.mqtt_pass));
  if (server.hasArg("mqtt_topic")) strlcpy(config.mqtt_topic, server.arg("mqtt_topic").c_str(), sizeof(config.mqtt_topic));
  if (server.hasArg("mqtt_weather_topic")) strlcpy(config.mqtt_weather_topic, server.arg("mqtt_weather_topic").c_str(), sizeof(config.mqtt_weather_topic));
  if (server.hasArg("mqtt_date_topic")) strlcpy(config.mqtt_date_topic, server.arg("mqtt_date_topic").c_str(), sizeof(config.mqtt_date_topic));
  if (server.hasArg("mqtt_env_topic")) strlcpy(config.mqtt_env_topic, server.arg("mqtt_env_topic").c_str(), sizeof(config.mqtt_env_topic));
  if (server.hasArg("mqtt_calendar_topic")) strlcpy(config.mqtt_calendar_topic, server.arg("mqtt_calendar_topic").c_str(), sizeof(config.mqtt_calendar_topic));
  if (server.hasArg("mqtt_shift_topic")) strlcpy(config.mqtt_shift_topic, server.arg("mqtt_shift_topic").c_str(), sizeof(config.mqtt_shift_topic));
  if (server.hasArg("mqtt_air_quality_topic")) strlcpy(config.mqtt_air_quality_topic, server.arg("mqtt_air_quality_topic").c_str(), sizeof(config.mqtt_air_quality_topic));
  if (server.hasArg("ntp_server")) strlcpy(config.ntp_server, server.arg("ntp_server").c_str(), sizeof(config.ntp_server));
  if (server.hasArg("ntp_server_2")) strlcpy(config.ntp_server_2, server.arg("ntp_server_2").c_str(), sizeof(config.ntp_server_2));
  if (server.hasArg("full_refresh_period")) config.full_refresh_period = server.arg("full_refresh_period").toInt();
  if (server.hasArg("day_start_hour")) config.day_start_hour = server.arg("day_start_hour").toInt();
  if (server.hasArg("day_end_hour")) config.day_end_hour = server.arg("day_end_hour").toInt();
  if (server.hasArg("invert_display")) config.invert_display = (server.arg("invert_display") == "1");
  
  saveConfig();
  
  server.send(200, "text/html", "<html><body><h1>Configuration Saved</h1><p>Device is restarting...</p><a href='/'>Back</a></body></html>");
  delay(1000);
  ESP.restart();
}

void handleSetPage() {
  if (server.hasArg("page")) {
    String page = server.arg("page");
    if (page == "weather") {
        currentPage = PAGE_WEATHER;
        switchPagePending = true;
        server.sendHeader("Location", "/");
        server.send(303);
    } else if (page == "calendar") {
        currentPage = PAGE_CALENDAR;
        switchPagePending = true;
        server.sendHeader("Location", "/");
        server.send(303);
    } else {
        server.send(400, "text/plain", "Invalid page");
    }
  } else {
    server.send(400, "text/plain", "Missing page argument");
  }
}

void handleSetText() {
  if (server.hasArg("text")) {
    String text = server.arg("text");
    server.send(200, "text/plain", "Displaying: " + text);
    displayMessage(text);
  } else {
    server.send(400, "text/plain", "Missing text argument");
  }
}
