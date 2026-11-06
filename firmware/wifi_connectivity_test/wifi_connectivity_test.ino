#include <WiFi.h>
#include <WebServer.h>

// Copy wifi_secrets.example.h to wifi_secrets.h and enter credentials locally.
// CI uses the example values only to verify compilation, not network access.
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#else
#include "wifi_secrets.example.h"
#endif

// The local HTTP endpoint lets a second device verify communication with ESP32.
WebServer server(80);
constexpr unsigned long kRetryIntervalMs = 20000;
constexpr unsigned long kStatusIntervalMs = 5000;
unsigned long lastRetry = 0;
unsigned long lastStatus = 0;
bool previouslyConnected = false;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Station mode joins an existing network instead of creating an access point.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_NAME, WIFI_PASSWORD);
  lastRetry = millis();
  Serial.println("\nConnecting to Wi-Fi...");

  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain",
      "PASS: Your browser reached the ESP32 over local Wi-Fi!\n"
      "Garden Assistant connectivity test.");
    Serial.println("PASS: Received a browser request over Wi-Fi.");
  });
  server.begin();
}

void loop() {
  const unsigned long now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  // Print the current address after each connection, including reconnections.
  if (connected && !previouslyConnected) {
    Serial.println("\nPASS: Connected to Wi-Fi and received an IP address.");
    Serial.print("Open on a device on the same network: http://");
    Serial.println(WiFi.localIP());
  }
  if (!connected && previouslyConnected) {
    Serial.println("Wi-Fi connection lost. Reconnecting...");
  }
  previouslyConnected = connected;

  if (connected) {
    server.handleClient();
  } else if (now - lastRetry >= kRetryIntervalMs) {
    // Retry without holding setup() in an endless connection loop.
    Serial.println("Still disconnected. Check credentials; retrying...");
    WiFi.reconnect();
    lastRetry = now;
  }

  // Unsigned elapsed-time comparisons also work across millis() rollover.
  if (now - lastStatus >= kStatusIntervalMs) {
    lastStatus = now;
    if (connected) {
      Serial.print("Wi-Fi connected | IP: ");
      Serial.print(WiFi.localIP());
      Serial.print(" | Signal: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    } else {
      Serial.println("Wi-Fi test: not connected yet.");
    }
  }
  delay(2);
}
