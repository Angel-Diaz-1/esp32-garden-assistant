#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>

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

// DS18B20 probe: red -> 3V3, black -> GND, yellow -> GPIO27.
// Add a 4.7 kOhm resistor between yellow/DATA and 3V3. Use three-wire power.
constexpr uint8_t kTemperaturePin = 27;
constexpr unsigned long kTemperatureIntervalMs = 2000;
constexpr unsigned long kTemperatureConversionMs = 750;  // 12-bit maximum.
OneWire temperatureBus(kTemperaturePin);
DallasTemperature temperatureSensor(&temperatureBus);
DeviceAddress temperatureAddress;
bool temperatureSensorFound = false;
bool temperaturePending = false;
bool temperatureAvailable = false;
float temperatureC = 0.0f;
unsigned long lastTemperatureRequest = 0;
unsigned long temperatureConversionStartedAt = 0;

void setupTemperature() {
  // Initialization runs while the pump is still OFF, before accepting commands.
  temperatureSensor.begin();
  temperatureSensor.setAutoSaveScratchPad(false);
  temperatureSensor.setWaitForConversion(false);
  lastTemperatureRequest = millis() - kTemperatureIntervalMs;
  Serial.println("DS18B20 ready on GPIO27; sampling every 2 seconds.");
}

void serviceTemperature() {
  const unsigned long now = millis();
  if (temperaturePending) {
    // Do not delay(750): Wi-Fi and the pump timer must keep running.
    if (now - temperatureConversionStartedAt < kTemperatureConversionMs) return;
    temperaturePending = false;
    const float reading = temperatureSensor.getTempC(temperatureAddress);
    temperatureAvailable = reading != DEVICE_DISCONNECTED_C &&
      isfinite(reading) && reading >= -55.0f && reading <= 125.0f;
    if (temperatureAvailable) {
      temperatureC = reading;
      Serial.print("Temperature: ");
      Serial.print(temperatureC, 2);
      Serial.print(" C / ");
      Serial.print(temperatureC * 1.8f + 32.0f, 2);
      Serial.println(" F");
    } else {
      temperatureSensorFound = false;
      Serial.println("DS18B20 reading unavailable. Check wiring and the 4.7k pull-up.");
    }
    return;
  }

  if (now - lastTemperatureRequest < kTemperatureIntervalMs) return;
  lastTemperatureRequest = now;
  if (!temperatureSensorFound) {
    // Search again after a missing/failed probe, without begin()'s startup delays.
    // Address-based reads also allow a probe connected after boot to recover.
    temperatureBus.reset_search();
    temperatureSensorFound = temperatureBus.search(temperatureAddress) &&
      temperatureAddress[0] == 0x28 &&  // DS18B20 family.
      OneWire::crc8(temperatureAddress, 7) == temperatureAddress[7];
    if (temperatureSensorFound) {
      temperatureSensorFound = temperatureSensor.setResolution(temperatureAddress, 12);
    }
    if (!temperatureSensorFound) {
      temperatureAvailable = false;
      Serial.println("DS18B20 not found. Check GPIO27, 3V3, GND, and the 4.7k pull-up.");
      return;
    }
  }

  if (!temperatureSensor.requestTemperaturesByAddress(temperatureAddress)) {
    temperatureAvailable = false;
    temperatureSensorFound = false;
    Serial.println("DS18B20 conversion could not start; retrying next sample.");
    return;
  }
  temperatureConversionStartedAt = millis();
  temperaturePending = true;
}

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
  page += F("</strong></p><p>Temperature: <strong>");
  if (temperatureAvailable) {
    page += String(temperatureC, 2) + " &deg;C / " +
      String(temperatureC * 1.8f + 32.0f, 2) + " &deg;F";
  } else {
    page += F("Unavailable / waiting for DS18B20. Check sensor wiring.");
  }
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

void sendApiHeaders() {
  // Allow the project's published page and local HTTP previews only.
  const String origin = server.header("Origin");
  if (origin == "https://angel-diaz-1.github.io" || origin.startsWith("http://localhost:") ||
      origin.startsWith("http://127.0.0.1:")) {
    server.sendHeader("Access-Control-Allow-Origin", origin);
  }
  server.sendHeader("Vary", "Origin");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.sendHeader("Access-Control-Allow-Private-Network", "true");
  server.sendHeader("Cache-Control", "no-store");
}

void sendApiStatus() {
  servicePump();
  sendApiHeaders();
  const unsigned long elapsed = millis() - pumpStartedAt;
  const unsigned long remaining = pumpOn && elapsed < kMaxPumpRunMs ? kMaxPumpRunMs - elapsed : 0;
  server.send(200, "application/json",
    String("{\"device\":\"garden-assistant-esp32\",\"connected\":") +
    (WiFi.status() == WL_CONNECTED ? "true" : "false") +
    ",\"wateringAvailable\":true,\"pumpOn\":" + (pumpOn ? "true" : "false") +
    ",\"maxRunMs\":" + String(kMaxPumpRunMs) + ",\"remainingMs\":" + String(remaining) +
    ",\"temperatureAvailable\":" + (temperatureAvailable ? "true" : "false") +
    ",\"temperatureC\":" + (temperatureAvailable ? String(temperatureC, 2) : String("null")) + "}");
}

void handlePumpCommand() {
  // JSON triggers a browser preflight; plain cross-site form posts cannot start it.
  if (server.header("Content-Type") != "application/json") {
    sendApiHeaders();
    server.send(415, "application/json", "{\"error\":\"Use application/json\"}");
    return;
  }
  const String body = server.arg("plain");
  if (body == "{\"action\":\"on\"}") startPump();
  else if (body == "{\"action\":\"off\"}") stopPump();
  else {
    sendApiHeaders();
    server.send(400, "application/json", "{\"error\":\"Unknown pump action\"}");
    return;
  }
  sendApiStatus();
}

void setup() {
  // Initialize the output before waiting for Serial or Wi-Fi.
  pinMode(kPumpPin, OUTPUT);
  stopPump();
  Serial.begin(115200);
  delay(1000);
  Serial.println("Pump ready on GPIO25. Send 1 for ON, 0 for OFF.");
  setupTemperature();

  // Station mode joins an existing network instead of creating an access point.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Arduino-ESP32 3.x configures Enterprise auth through WiFi.begin().
  // No legacy esp_wpa2.h include is needed.
  WiFi.begin(WIFI_NAME, WPA2_AUTH_PEAP, EAP_IDENTITY, EAP_USERNAME,
             EAP_PASSWORD, EAP_CA_CERT);
  lastRetry = millis();
  Serial.println("\nConnecting to Wi-Fi...");

  const char* apiHeaders[] = {"Origin", "Content-Type"};
  server.collectHeaders(apiHeaders, 2);
  server.on("/api/status", HTTP_GET, sendApiStatus);
  server.on("/api/pump", HTTP_POST, handlePumpCommand);
  for (const char* path : {"/api/status", "/api/pump"}) {
    server.on(path, HTTP_OPTIONS, []() { sendApiHeaders(); server.send(204); });
  }

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
  serviceTemperature();
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
