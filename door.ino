// adapted from https://github.com/squix78/espaper-weatherstation

#include <Arduino.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#include <MiniGrafx.h>
#include <EPD_WaveShare.h>
#include <MiniGrafxFonts.h>

#include <simpleDSTadjust.h>

#include "settings.h"
/*
#define UTC_OFFSET -7
struct dstRule StartRule = {"MDT", Second, Sun, Mar, 2, 3600};
struct dstRule EndRule = {"MST", First, Sun, Nov, 1, 0};

#define CAL_ID "mycalendar@group.calendar.google.com"
#define GKEY "Google API key"

#define WIFI_SSID "foo"
#define WIFI_PASS "bar"
*/

uint16_t palette[] = {ILI9341_BLACK, // 0
                      ILI9341_WHITE, // 1
                     };

#define SCREEN_HEIGHT 128
#define SCREEN_WIDTH 296
#define BITS_PER_PIXEL 1

#define MINI_BLACK 0
#define MINI_WHITE 1

#define CS 15  // D8
#define RST 2  // D4
#define DC 5   // D1
#define BUSY 4 // D2
#define USR_BTN 12 // D6

#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"

String DAYS[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
String MONTHS[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

EPD_WaveShare epd(EPD2_9, CS, RST, DC, BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);
simpleDSTadjust dstAdjusted(StartRule, EndRule);

#define WIDTH SCREEN_WIDTH/5

void drawCal(struct tm *timeinfo) {
  gfx.setColor(MINI_BLACK);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);

  char timeMin[26];
  sprintf(timeMin, "%4d-%02d-%02dT00:00:00-07:00", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);

  char dates[5][11];

  int offset = 0;
  time_t tim = mktime(timeinfo);
  while(offset < 5) {
    timeinfo = localtime(&tim);
    if(timeinfo->tm_wday != 0 && timeinfo->tm_wday != 6) { // exclude weekend
      gfx.drawString(SCREEN_WIDTH / 5 * offset + WIDTH / 2, 2, String(DAYS[timeinfo->tm_wday]) + " " + timeinfo->tm_mday);
      gfx.drawLine((SCREEN_WIDTH / 5 * offset) + 2, 16, (SCREEN_WIDTH / 5 * offset) + WIDTH - 2, 16);

      sprintf(dates[offset], "%4d-%02d-%02d", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);
      offset++;
    }
    tim += 86400;
  }

  char timeMax[26];
  sprintf(timeMax, "%4d-%02d-%02dT23:59:59-07:00", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday);

  // get calendar
  WiFiClientSecure client;
  bool flag = false;
  for(int i = 0; i < 5; i++) {
    int retval = client.connect("www.googleapis.com", 443);
    if(retval == 1) {
      flag = true;
      break;
    }
  }

  if(!flag) {
    gfx.setFont(ArialMT_Plain_16);
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, "Could not connect to server!");
    return;
  }
  
  String url = "/calendar/v3/calendars/" + String(CAL_ID) + "/events?key=" + String(GKEY) + "&singleEvents=true&timeMin=" + timeMin + "&timeMax=" + timeMax + "&orderBy=startTime";
  //Serial.println("Retrieving " + url);
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: www.googleapis.com\r\n" +
               "User-Agent: DoorUpdaterESP8266\r\n" +
               "Connection: close\r\n\r\n");

  while(client.connected()) {
    String line = client.readStringUntil('\n');
    if(line == "\r") {
      break;
    }
  }

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  int dayOffset = 0;
  int evtOffsets[5] = { 0, 0, 0, 0, 0 };

  boolean isStart = false;
  boolean isEnd = false;
  boolean parsed = false;

  String name = "";
  String evTime = "";
  String multiDayEnd = "";
  while(client.connected()) {
    String line = client.readStringUntil('\n');
    line.trim();
    //Serial.println("Processing line: " + line);
    
    if(line.startsWith(String("\"summary\": "))) {
      name = line.substring(12, line.lastIndexOf('"'));
      //Serial.println("Found name: " + name);
    } else if(line.startsWith(String("\"start\": {"))) {
      isStart = true;
    } else if(line.startsWith(String("\"end\": {"))) {
      isEnd = true;
    } else if(line.startsWith(String("\"dateTime\":")) && isStart) {
      String thisDay = line.substring(13, 23);
      dayOffset = 0;
      while(dayOffset < 5) {
        if(thisDay.equals(String(dates[dayOffset]))) {
          break;
        }
        dayOffset++;
      }

      evTime = line.substring(24, 29);
      //Serial.println("Found start time: " + evTime);
      isStart = false;
    } else if(line.startsWith(String("\"date\":")) && isStart) {
      String thisDay = line.substring(9, 19);
      dayOffset = 0;
      if(thisDay.compareTo(String(dates[dayOffset])) > 0) {
        while(dayOffset < 5) {
          if(thisDay.equals(String(dates[dayOffset]))) {
            break;
          }
          dayOffset++;
        }
      }

      evTime = "all day";
      isStart = false;
    } else if(line.startsWith(String("\"dateTime\":")) && isEnd) {
      evTime += "-" + line.substring(24, 29);
      //Serial.println("Found end time: " + evTime);
      isEnd = false;
      parsed = true;
    } else if(line.startsWith(String("\"date\":")) && isEnd) {
      multiDayEnd = line.substring(9, 19);
      isEnd = false;
      parsed = true;
    }

    if(parsed) {
      gfx.drawString(2 + (dayOffset * SCREEN_WIDTH / 5), 18 + (24 * evtOffsets[dayOffset]), evTime);
      gfx.drawString(2 + (dayOffset * SCREEN_WIDTH / 5), 18 + 10 + (24 * evtOffsets[dayOffset]), name);

      isStart = false;
      isEnd = false;
      parsed = false;

      if(multiDayEnd.length() > 0) {
        int i = dayOffset + 1;
        while(multiDayEnd.compareTo(String(dates[i])) > 0 && i < 5) {
          gfx.drawString(2 + (i * SCREEN_WIDTH / 5), 18 + (24 * evtOffsets[i]), evTime);
          gfx.drawString(2 + (i * SCREEN_WIDTH / 5), 18 + 10 + (24 * evtOffsets[i]), name);
          evtOffsets[i]++;
          i++;
        }
        multiDayEnd = "";
      }

      name = "";
      evTime = "";
      evtOffsets[dayOffset]++;
    }
  }
}

void drawStatus(struct tm *timeinfo) {
  gfx.setColor(MINI_WHITE);
  gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
  gfx.setColor(MINI_BLACK);

  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);

  char time_str[6];
  sprintf(time_str, "%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min);
  gfx.drawString(2, SCREEN_HEIGHT - 12, "Last update: " + DAYS[timeinfo->tm_wday] + " " + timeinfo->tm_mday + " " + MONTHS[timeinfo->tm_mon] + " " + (timeinfo->tm_year + 1900) + " " + time_str);
  gfx.drawLine(0, SCREEN_HEIGHT - 13, SCREEN_WIDTH, SCREEN_HEIGHT - 13);

  
  uint8_t percentage = 100;
  float adcVoltage = analogRead(A0) / 1024.0;
  // These values were empirically collected
  float batteryVoltage = adcVoltage * 4.945945946 -0.3957657658;
  if (batteryVoltage > 4.2) percentage = 100;
  else if (batteryVoltage < 3.3) percentage = 0;
  else percentage = (batteryVoltage - 3.3) * 100 / (4.2 - 3.3);

  gfx.drawRect(SCREEN_WIDTH - 22, SCREEN_HEIGHT - 10, 19, 10);
  gfx.fillRect(SCREEN_WIDTH - 2, SCREEN_HEIGHT - 8, 2, 6);
  gfx.fillRect(SCREEN_WIDTH - 20, SCREEN_HEIGHT - 8, 16 * percentage / 100, 6);
}

boolean connectWifi() {
  if(WiFi.status() == WL_CONNECTED) return true;

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int i = 0;
  while(WiFi.status() != WL_CONNECTED) {
    delay(300);
    i++;
    if (i > 30) {
      return false;
    }
  }
  return true;
}

struct tm *updateTime() {
  configTime(UTC_OFFSET * 3600, 0, NTP_SERVERS);

  time_t this_second = 0;
  while(this_second < 86400 * 365) {
    time(&this_second);
    delay(200);
  }

  char *dstAbbrev;
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm *timeinfo = localtime(&now);

  return timeinfo;
}



void setup() {
  gfx.init();
  gfx.setRotation(1);
  gfx.setFastRefresh(false);
  gfx.fillBuffer(MINI_WHITE);

  boolean connected = connectWifi();
    if(connected) {
      gfx.setColor(MINI_BLACK);
      gfx.fillRect(0, SCREEN_HEIGHT - 12, SCREEN_WIDTH, 12);
      gfx.setColor(MINI_WHITE);
      gfx.setFont(ArialMT_Plain_10);
      gfx.setTextAlignment(TEXT_ALIGN_CENTER);
      gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT - 12, "Refreshing data...");
      gfx.commit();

      struct tm *timeinfo = updateTime();
    
      drawStatus(timeinfo);
      drawCal(timeinfo);
    
      gfx.commit();

      ESP.deepSleep(4000e6);
    } else {
      gfx.fillBuffer(MINI_WHITE);
      gfx.setColor(MINI_BLACK);
      gfx.setFont(ArialMT_Plain_16);
      gfx.setTextAlignment(TEXT_ALIGN_CENTER);
      gfx.drawString(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 30, "Could not connect to WiFi!");
      gfx.commit();

      // sleep 10 minutes
      ESP.deepSleep(600e6);
    }
}

void loop() {
}
