#include "WateringController.h"
#include <string.h>

// Keep this true until the team has reviewed the MOSFET circuit and pump supply.
// Dry run does not configure or write ANY GPIO, so no pump hardware is required.
constexpr bool kDryRun = true;
constexpr uint8_t kPumpPin = 25;  // Proposed active-HIGH MOSFET control output.
WateringController watering;
bool outputActive = false;
char command[24];
size_t commandLength = 0;
bool commandOverflow = false;

void applyOutput() {
  const bool next = watering.active();
  if (next == outputActive) return;
  if (!kDryRun) digitalWrite(kPumpPin, next ? HIGH : LOW);
  outputActive = next;
  Serial.println(next ? "SIMULATION/OUTPUT: ON" : "SIMULATION/OUTPUT: OFF");
}

void handleCommand() {
  if (strcmp(command, "water") == 0) {
    Serial.println(watering.start(millis())
      ? "Accepted: 5-second cycle (see mode above)."
      : "Rejected: watering already active; timer unchanged.");
  } else if (strcmp(command, "stop") == 0) {
    watering.stop();
    Serial.println("Stopped.");
  } else if (strcmp(command, "status") == 0) {
    Serial.println(watering.active() ? "State: WATERING" : "State: IDLE");
  } else {
    Serial.println("Commands: water, stop, status (lowercase, newline terminated).");
  }
  applyOutput();
}

void setup() {
  // Real output, when explicitly enabled, starts LOW before becoming an output.
  if (!kDryRun) {
    digitalWrite(kPumpPin, LOW);
    pinMode(kPumpPin, OUTPUT);
  }
  Serial.begin(115200);
  Serial.println(kDryRun ? "DRY RUN: GPIO disabled; no water is dispensed."
                         : "HARDWARE MODE: pump output enabled.");
  Serial.println("Commands: water, stop, status. Select Newline in Serial Monitor.");
}

void loop() {
  // No delay() or blocking string reads: the automatic stop keeps being serviced.
  watering.update(millis());
  applyOutput();
  // Read at most one byte per loop so continuous serial input cannot starve timing.
  if (Serial.available() > 0) {
    const char incoming = char(Serial.read());
    if (incoming == '\r' || incoming == '\n') {
      if (commandOverflow) Serial.println("Rejected: command too long.");
      else if (commandLength > 0) {
        command[commandLength] = '\0';
        handleCommand();
      }
      commandLength = 0;
      commandOverflow = false;
    } else if (!commandOverflow) {
      if (commandLength < sizeof(command) - 1) command[commandLength++] = incoming;
      else commandOverflow = true;  // Discard the entire oversized line.
    }
  }
}
