#include <WiFi.h>
#include <WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <math.h>
#include <esp_eap_client.h>

// Enter SSID, optional school user ID, and password in Serial Monitor.
// Credentials stay in RAM; setup is required after each reset/power cycle.
// Optional school-specific outer identity. nullptr uses the entered user ID.
constexpr const char* EAP_IDENTITY_OVERRIDE = nullptr;

// Set to the school's IT-provided root CA PEM text to validate its server.
// nullptr is the certificate-free example; it does NOT verify the server.
// Obtain the correct certificate and EAP settings from school IT before
// using real credentials. A PEM can be pasted here as R"PEM(...)PEM".
constexpr const char* EAP_CA_CERT = nullptr;

enum class WifiSetupStep { Name, UserId, Password, Ready };
WifiSetupStep wifiSetupStep = WifiSetupStep::Name;
String wifiName;
String wifiUserId;
String wifiPassword;
String serialLine;
bool serialLineOverflow = false;
bool skipSerialLf = false;
bool skipSetupCommandNewline = false;

// The local HTTP endpoint lets a second device verify communication with ESP32.
WebServer server(80);
constexpr unsigned long kRetryIntervalMs = 20000;
constexpr unsigned long kStatusIntervalMs = 5000;
unsigned long lastRetry = 0;
unsigned long lastStatus = 0;
bool previouslyConnected = false;

// NodeMCU-32S / classic ESP32: GPIO25 goes to the Adafruit driver's In.
constexpr uint8_t kPumpPin = 25;
constexpr unsigned long kMaxPumpRunMs = 10000;
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

// Capacitive Soil Moisture Sensor v1.2: VCC -> 3V3, GND -> GND, AOUT -> P34.
// GPIO34 is ADC1, which remains usable with Wi-Fi. No MOSFET is needed.
constexpr uint8_t kMoisturePin = 34;
constexpr unsigned long kMoistureIntervalMs = 2000;
// Replace -1 with measurements from your own dry/wet soil at 3.3 V.
// Until calibrated, expose raw ADC data rather than inventing a percentage.
constexpr int kMoistureDryRaw = -1;
constexpr int kMoistureWetRaw = -1;
bool moistureAvailable = false;
int moistureRaw = 0;
float moisturePercent = 0.0f;
unsigned long lastMoistureRead = 0;

bool moistureCalibrated() {
  return kMoistureDryRaw > 0 && kMoistureDryRaw < 4095 &&
    kMoistureWetRaw > 0 && kMoistureWetRaw < 4095 &&
    abs(kMoistureDryRaw - kMoistureWetRaw) >= 100;
}

void setupMoisture() {
  pinMode(kMoisturePin, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(kMoisturePin, ADC_11db);
  lastMoistureRead = millis() - kMoistureIntervalMs;
  Serial.println("Soil moisture ready on GPIO34; power sensor from 3V3.");
}

void serviceMoisture() {
  if (millis() - lastMoistureRead < kMoistureIntervalMs) return;
  lastMoistureRead = millis();
  uint32_t total = 0;
  bool clipped = false;
  // Eight bounded ADC conversions, with no delay/wait loop between samples.
  for (uint8_t i = 0; i < 8; ++i) {
    const int sample = analogRead(kMoisturePin);
    if (sample <= 0 || sample >= 4095) clipped = true;
    total += sample;
  }
  moistureRaw = (total + 4) / 8;
  moistureAvailable = !clipped;
  if (moistureAvailable && moistureCalibrated()) {
    moisturePercent = 100.0f * (moistureRaw - kMoistureDryRaw) /
      (kMoistureWetRaw - kMoistureDryRaw);
    moisturePercent = constrain(moisturePercent, 0.0f, 100.0f);
  }
  Serial.print("Soil moisture raw (0-4095): ");
  Serial.print(moistureRaw);
  if (!moistureAvailable) Serial.println(" | ADC clipped: check wiring/power.");
  else if (moistureCalibrated()) {
    Serial.print(" | Relative moisture: ");
    Serial.print(moisturePercent, 1);
    Serial.println(" %");
  } else Serial.println(" | Set dry/wet calibration values for a percentage.");
}

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
  if (wifiSetupStep != WifiSetupStep::Ready) {
    Serial.println("Finish Wi-Fi setup before starting the pump.");
    return;
  }
  // Repeated ON commands must not extend a run indefinitely.
  if (pumpOn) return;
  pumpStartedAt = millis();
  pumpOn = true;
  digitalWrite(kPumpPin, HIGH);
  Serial.println("Pump ON (maximum 10 seconds)");
}

void servicePump() {
  if (pumpOn && millis() - pumpStartedAt >= kMaxPumpRunMs) {
    stopPump();
    Serial.println("Automatic stop: 10-second limit reached.");
  }
}

void printWifiPrompt() {
  if (wifiSetupStep == WifiSetupStep::Name) Serial.println("Wi-Fi name (SSID):");
  else if (wifiSetupStep == WifiSetupStep::UserId) {
    Serial.println("User ID (school Wi-Fi), or send an EMPTY line for home Wi-Fi:");
  } else if (wifiSetupStep == WifiSetupStep::Password) {
    Serial.println("Wi-Fi password (not echoed; empty only for an open home network):");
  }
}

void beginWifiSetup() {
  stopPump();
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, true);
  // Clear Enterprise mode before switching to a home network or new school ID.
  esp_wifi_sta_enterprise_disable();
  previouslyConnected = false;
  wifiName = "";
  wifiUserId = "";
  wifiPassword = "";
  serialLine = "";
  serialLineOverflow = false;
  wifiSetupStep = WifiSetupStep::Name;
  Serial.println("\nWi-Fi setup: select Newline or Both NL & CR in Serial Monitor.");
  Serial.println("Enter each answer and press Send. Credentials are not printed or saved to flash.");
  printWifiPrompt();
}

void connectConfiguredWifi() {
  WiFi.setAutoReconnect(true);
  if (wifiUserId.length() == 0) {
    // Blank user ID selects a normal personal/home network, not Enterprise.
    WiFi.begin(wifiName.c_str(), wifiPassword.length() ? wifiPassword.c_str() : nullptr);
    Serial.println("Connecting to personal/home Wi-Fi...");
  } else {
    const char* identity = EAP_IDENTITY_OVERRIDE == nullptr
      ? wifiUserId.c_str() : EAP_IDENTITY_OVERRIDE;
    WiFi.begin(wifiName.c_str(), WPA2_AUTH_PEAP, identity,
               wifiUserId.c_str(), wifiPassword.c_str(), EAP_CA_CERT);
    Serial.println("Connecting to school Wi-Fi (PEAP)...");
  }
  lastRetry = millis();
  lastStatus = millis();
  Serial.println("Send W to change Wi-Fi, 1 for pump ON, or 0 for OFF.");
}

void acceptWifiAnswer() {
  if (serialLineOverflow) {
    Serial.println("Answer too long; please enter it again.");
    serialLine = "";
    serialLineOverflow = false;
    printWifiPrompt();
    return;
  }
  if (wifiSetupStep == WifiSetupStep::Name) {
    if (serialLine.length() == 0 || serialLine.length() > 32) {
      Serial.println("SSID must contain 1 to 32 bytes.");
    } else {
      wifiName = serialLine; // Preserve spaces in network names.
      wifiSetupStep = WifiSetupStep::UserId;
    }
  } else if (wifiSetupStep == WifiSetupStep::UserId) {
    serialLine.trim();
    if (serialLine.length() > 127) Serial.println("User ID must be at most 127 bytes.");
    else {
      wifiUserId = serialLine;
      wifiSetupStep = WifiSetupStep::Password;
    }
  } else if (wifiSetupStep == WifiSetupStep::Password) {
    const size_t length = serialLine.length();
    bool valid = wifiUserId.length() ? (length > 0 && length <= 127)
      : (length == 0 || (length >= 8 && length <= 63));
    if (wifiUserId.length() == 0 && length == 64) {
      valid = true; // WPA personal also permits a raw 64-digit hexadecimal PSK.
      for (size_t i = 0; i < length; ++i) {
        const char c = serialLine[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) valid = false;
      }
    }
    if (!valid) {
      Serial.println(wifiUserId.length() ? "School password must contain 1 to 127 bytes."
        : "Home password must contain 8 to 63 characters, 64 hex digits, or be empty for an open network.");
    } else {
      wifiPassword = serialLine; // Do not trim or echo passwords.
      wifiSetupStep = WifiSetupStep::Ready;
      connectConfiguredWifi();
    }
  }
  serialLine = "";
  printWifiPrompt();
}

void serviceSerial() {
  // Bound work per loop; entering credentials must never block pump timing.
  for (uint8_t count = 0; count < 64 && Serial.available() > 0; ++count) {
    const char c = Serial.read();
    if (skipSerialLf) {
      skipSerialLf = false;
      if (c == '\n') continue; // Treat CRLF as one submitted answer.
    }
    if (c == '\r') skipSerialLf = true;
    if (skipSetupCommandNewline) {
      skipSetupCommandNewline = false;
      if (c == '\r' || c == '\n') continue; // The newline after W isn't an SSID.
    }
    if (wifiSetupStep == WifiSetupStep::Ready) {
      if (c == '0') stopPump();
      else if (c == '1') startPump();
      else if (c == 'w' || c == 'W') {
        beginWifiSetup();
        skipSetupCommandNewline = true;
      }
    } else if (c == '\r' || c == '\n') acceptWifiAnswer();
    else if (c == '\b' || c == 127) {
      if (serialLine.length()) serialLine.remove(serialLine.length() - 1);
    } else if (serialLine.length() < 127) serialLine += c;
    else serialLineOverflow = true;
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
  page += F("</strong></p><p>Soil moisture: <strong>");
  if (moistureAvailable) {
    page += String(moistureRaw) + " raw ADC";
    if (moistureCalibrated()) page += " / " + String(moisturePercent, 1) + " % relative";
    else page += " (calibration needed for %)";
  } else page += F("Unavailable / check GPIO34 and sensor wiring.");
  page += F("</strong></p><p>ON runs for up to 10 seconds. "
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
    ",\"temperatureC\":" + (temperatureAvailable ? String(temperatureC, 2) : String("null")) +
    ",\"moistureAvailable\":" + (moistureAvailable ? "true" : "false") +
    ",\"moistureRaw\":" + (moistureAvailable ? String(moistureRaw) : String("null")) +
    ",\"moistureCalibrated\":" + (moistureCalibrated() ? "true" : "false") +
    ",\"moisturePercent\":" + (moistureAvailable && moistureCalibrated()
      ? String(moisturePercent, 1) : String("null")) + "}");
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
  setupMoisture();

  // Keep credentials in RAM and join a network only after the setup prompts.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);

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
  beginWifiSetup();
}

void loop() {
  servicePump();
  // Avoid sensor/status log noise while a user is answering setup prompts.
  if (wifiSetupStep == WifiSetupStep::Ready) {
    serviceTemperature();
    serviceMoisture();
  }
  servicePump();
  serviceSerial();
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
  } else if (wifiSetupStep == WifiSetupStep::Ready && now - lastRetry >= kRetryIntervalMs) {
    // Retry without holding setup() in an endless connection loop.
    Serial.println("Still disconnected. Send W to correct Wi-Fi settings; retrying...");
    WiFi.reconnect();
    lastRetry = now;
  }

  // Unsigned elapsed-time comparisons also work across millis() rollover.
  if (wifiSetupStep == WifiSetupStep::Ready && now - lastStatus >= kStatusIntervalMs) {
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
