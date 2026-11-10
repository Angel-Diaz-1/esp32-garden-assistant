#include <WiFi.h>
#include <WebServer.h>

// School Wi-Fi (WPA2-Enterprise PEAP). Edit these in your LOCAL copy only.
// This sketch is self-contained: no wifi_secrets header is required.
// Keep real school credentials out of commits, screenshots, and shared files.
constexpr const char* WIFI_NAME = "eduroam";
constexpr const char* EAP_IDENTITY = "USER_ID";  // Outer identity: follow school IT instructions.
constexpr const char* EAP_USERNAME = "USER_ID";  // Often the full school email/user ID.
constexpr const char* EAP_PASSWORD = "USER_PASSWORD";

// Set to the school's IT-provided root CA PEM text to validate its server.
// nullptr is the certificate-free example; it does NOT verify the server.
// Obtain the correct certificate and EAP settings from school IT before
// using real credentials. A PEM can be pasted here as R"PEM(...)PEM".
constexpr const char* EAP_CA_CERT = nullptr;

// The local HTTP endpoint lets a second device verify communication with ESP32.
WebServer server(80);
constexpr unsigned long kRetryIntervalMs = 20000;
constexpr unsigned long kStatusIntervalMs = 5000;
unsigned long lastRetry = 0;
unsigned long lastStatus = 0;
bool previouslyConnected = false;

// NodeMCU-32S / classic ESP32: GPIO25 goes to the Adafruit driver's In.
constexpr uint8_t kPumpPin = 25;
constexpr unsigned long kMaxPumpRunMs = 30000;
bool pumpOn = false;
unsigned long pumpStartedAt = 0;

void stopPump() {
  digitalWrite(kPumpPin, LOW);
  if (pumpOn) Serial.println("Pump OFF");
  pumpOn = false;
}

void startPump() {
  // Repeated ON commands must not extend a run indefinitely.
  if (pumpOn) return;
  pumpStartedAt = millis();
  pumpOn = true;
  digitalWrite(kPumpPin, HIGH);
  Serial.println("Pump ON (maximum 30 seconds)");
}

void servicePump() {
  if (pumpOn && millis() - pumpStartedAt >= kMaxPumpRunMs) {
    stopPump();
    Serial.println("Automatic stop: 30-second limit reached.");
  }
}

void sendPumpPage() {
  String page = F("<!doctype html><html lang='en'><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>"
    "<title>Garden pump</title><body><h1>Garden pump</h1>"
    "<p>PASS: Your browser reached the ESP32 over local Wi-Fi!</p>"
    "<p>Commanded pump state: <strong>");
  page += pumpOn ? "ON" : "OFF";
  page += F("</strong></p><p>ON runs for up to 30 seconds. "
    "Keep the pump submerged. Refresh to update the displayed state.</p>"
    "<form method='post' action='/pump/on'><button>Turn pump ON</button></form>"
    "<br><form method='post' action='/pump/off'><button>Turn pump OFF</button></form>"
    "<p><a href='/'>Refresh status</a></p></body></html>");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/html", page);
}

void redirectToPumpPage() {
  server.sendHeader("Location", "/");
  server.sendHeader("Cache-Control", "no-store");
  server.send(303, "text/plain", "See pump controls at /");
}

void setup() {
  // Initialize the output before waiting for Serial or Wi-Fi.
  pinMode(kPumpPin, OUTPUT);
  stopPump();
  Serial.begin(115200);
  delay(1000);
  Serial.println("Pump ready on GPIO25. Send 1 for ON, 0 for OFF.");

  // Station mode joins an existing network instead of creating an access point.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Arduino-ESP32 3.x configures Enterprise auth through WiFi.begin().
  // No legacy esp_wpa2.h include is needed.
  WiFi.begin(WIFI_NAME, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME,
             EAP_PASSWORD, EAP_CA_CERT);
  lastRetry = millis();
  Serial.println("\nConnecting to Wi-Fi...");

  // Read-only health endpoint for the separate GitHub Pages website.
  // wateringAvailable refers to the separate GitHub Pages integration,
  // which remains a demo. Real pump controls live on this board's own page.
  server.on("/api/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json",
      String("{\"device\":\"garden-assistant-esp32\",\"connected\":") +
      (WiFi.status() == WL_CONNECTED ? "true" : "false") +
      ",\"wateringAvailable\":false,\"pumpOn\":" +
      (pumpOn ? "true" : "false") + "}");
  });
  // Some browsers preflight requests to private network devices.
  server.on("/api/status", HTTP_OPTIONS, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Private-Network", "true");
    server.send(204);
  });

  server.on("/", HTTP_GET, []() {
    sendPumpPage();
    Serial.println("PASS: Received a browser request over Wi-Fi.");
  });
  // The board's forms submit POST requests. Use only on a trusted LAN.
  server.on("/pump/on", HTTP_POST, []() {
    startPump();
    redirectToPumpPage();
  });
  server.on("/pump/off", HTTP_POST, []() {
    stopPump();
    redirectToPumpPage();
  });
  server.begin();
}

void loop() {
  servicePump();
  // Read one character per loop so input cannot delay pump timing or Wi-Fi.
  if (Serial.available() > 0) {
    const char command = Serial.read();
    if (command == '0') stopPump();
    else if (command == '1') startPump();
  }
  const unsigned long now = millis();
  const bool connected = WiFi.status() == WL_CONNECTED;

  // Print the current address after each connection, including reconnections.
  if (connected && !previouslyConnected) {
    Serial.println("\nPASS: Connected to Wi-Fi and received an IP address.");
    Serial.print("Open on a device on the same network: http://");
    Serial.println(WiFi.localIP());
  }
  if (!connected && previouslyConnected) {
    stopPump();
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
  servicePump();
  delay(2);
}
