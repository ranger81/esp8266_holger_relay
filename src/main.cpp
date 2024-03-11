/**
 * @brief ESP8266-based relay switch with URL polling function
 **/

// ------------ INCLUDES --------------------------------------------------------------------------

#include <Arduino.h> // Main Arduino library
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <WiFiManager.h>

#include "config.h"

// ------------ CONFIGURATION / CUSTOMIZING -------------------------------------------------------

#define PIN_RELAY LED_BUILTIN
#define PIN_BTN 0           // Force config portal start
#define DEFAULT_INTERVAL 60 // seconds
#define HTTP_TIMEOUT 1000   // milliseconds
#define AP_NAME "pollplug"
#define AP_PWD "tollespasswort"

// ------------ GLOBAL VARIABLES ------------------------------------------------------------------

WiFiManager wifiManager;
WiFiManagerParameter *customHost, *customPort, *customPath, *customInterval, *customCACert;
WiFiClientSecure client;
unsigned long ts_lastRequest = 0, ts_requestStart = 0, interval = DEFAULT_INTERVAL;
byte state = 0;

// ------------ FUNCTIONS ------------------------------------------------------------------

void saveParamsCallback() {
  strcpy(config.host, customHost->getValue());
  strcpy(config.port, customPort->getValue());
  strcpy(config.path, customPath->getValue());
  strcpy(config.interval, customInterval->getValue());
  strcpy(config.cacert, customCACert->getValue());

  saveObjectToFS(config, "config.bin");

  ESP.reset();
}

void initWiFiManager(bool forceConfigPortal = false) {

  customHost = new WiFiManagerParameter("customURL", "Server Hostname", config.host, sizeof(config.host) - 1);
  customPort = new WiFiManagerParameter("customPort", "Port", config.port, sizeof(config.port) - 1);
  customPath = new WiFiManagerParameter("customPath", "Polling URL", config.path, sizeof(config.path) - 1);
  customInterval = new WiFiManagerParameter("customInterval", "Polling Interval", config.interval, sizeof(config.interval) - 1);
  customCACert = new WiFiManagerParameter("customCACert", "CA Certificate", config.cacert, sizeof(config.cacert) - 1);

  wifiManager.setDebugOutput(false);

  wifiManager.setSaveParamsCallback(saveParamsCallback);

  wifiManager.addParameter(customHost);
  wifiManager.addParameter(customPort);
  wifiManager.addParameter(customPath);
  wifiManager.addParameter(customInterval);
  wifiManager.addParameter(customCACert);

  wifiManager.setTimeout(300);

  wifiManager.setTitle("Configuration Portal");

  std::vector<const char *> wm_menu = {"wifi", "exit"};
  wifiManager.setShowInfoUpdate(false);
  wifiManager.setShowInfoErase(false);
  wifiManager.setMenu(wm_menu);

  char *wiFiPassword = (char *)AP_PWD;

  if (forceConfigPortal || !digitalRead(PIN_BTN)) {
    Serial.println("Starting config portal");
    wifiManager.startConfigPortal(AP_NAME, wiFiPassword);
  } else {
    if (!wifiManager.autoConnect(AP_NAME, wiFiPassword)) {
      delay(3000);
      ESP.reset();
      delay(5000);
    }
  }
}

void replaceNewline(char *str) {
  char *pos = str;
  while ((pos = strstr(pos, "\\n")) != NULL) {
    *pos = '\n';
    memmove(pos + 1, pos + 2, strlen(pos + 2) + 1);
    pos++;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);
  digitalWrite(PIN_RELAY, LOW);
  Serial.println(sizeof(config.port));
  bool loadResult = loadObjectFromFS(config, "config.bin");
  Serial.println(loadResult);

  if (loadResult) {
    int port = atoi(config.port);
    if (port == 443) {
      replaceNewline(config.cacert);
      client.setTrustAnchors(new BearSSL::X509List(config.cacert));
    }

    interval = atoi(config.interval) * 1000;
    if (interval < 10000) {
      interval = DEFAULT_INTERVAL * 1000;
    }
  }
  delay(5000);
  initWiFiManager(!loadResult);
}

void loop() {
  if (state == 0) {
    if (millis() - ts_lastRequest >= interval) {
      Serial.println("Begin request");

      if (!client.connect(config.host, atoi(config.port))) {
        state = 2;
        return;
      }

      client.print(String("GET ") + config.path + " HTTP/1.1\r\n" +
                   "Host: " + config.host + "\r\n" +
                   "Connection: close\r\n\r\n");

      ts_requestStart = millis();
      state++;
    }
  } else if (state == 1) {
    if (millis() - ts_requestStart <= HTTP_TIMEOUT) {
      if (client.available()) {
        digitalWrite(PIN_RELAY, client.read());
        state++;
        Serial.println("Request ok");
      }
    } else {
      Serial.println("Request timed out");
      state++;
    }
  } else if (state == 2) {
    client.stop();
    state = 0;
    ts_lastRequest = millis();
    Serial.println("Finishing request");
  }

  delay(1);
  yield();
}