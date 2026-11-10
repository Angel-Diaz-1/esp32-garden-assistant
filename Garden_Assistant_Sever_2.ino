#include "esp_wpa2.h"
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "eduroam";
#define EAP_ID "danielrodrig2023@fau.edu"
#define EAP_USERNAME "danielrodrig2023@fau.edu"
#define EAP_PASSWORD "Astolfo69@"
const char* serverName = "https://angel-diaz-1.github.io/esp32-garden-assistant/";

void setup() {
  Serial.begin(115200);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.disconnect(true);
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID));
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));
  esp_wifi_sta_wpa2_ent_enable();
}

void loop() {
  if(WiFi.status()== WL_CONNECTED){
    Serial.print("Connected");
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST("{\"temperature\":25}");
    // Handle response...
    http.end();
  }
  delay(10000);
}
