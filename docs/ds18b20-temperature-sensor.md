# DS18B20 temperature probe

The sensor in the supplied photos is a three-wire DS18B20 probe. The package labels its wires **red = VDD+, yellow = DATA, black = GND**. These instructions target the same **NodeMCU-32S / classic ESP32** used by `firmware/wifi_connectivity_test/wifi_connectivity_test.ino`.

## Wiring

Disconnect power before wiring. Keep the existing pump wiring in place: its driver input stays on **GPIO25**. Add these sensor connections, using the ESP32 board's printed pin labels rather than physical header positions:

| Probe connection | ESP32 connection |
| --- | --- |
| Red / VDD+ | **3V3 (3.3 V)** |
| Black / GND | **GND** (common with the pump driver's ground) |
| Yellow / DATA | **GPIO27 / IO27** |
| **4.7 kOhm (4700 ohm) resistor** | Between **yellow/DATA (GPIO27)** and **3V3** |

```text
ESP32 3V3 -----+---------------- Red / VDD+
              |
           [4.7 kOhm]
              |
ESP32 GPIO27 -+---------------- Yellow / DATA
ESP32 GND --------------------- Black / GND
```

The resistor is a pull-up between the data and power wires; it is not in series with the data wire. Use all three wires, with the probe powered from 3V3. Do not connect its data line or pull-up to the pump's 5 V supply: the ESP32 uses 3.3 V logic. A bare cabled probe needs the external pull-up unless a separate adapter already provides one. The ESP32's internal pull-up is not a substitute. Keep the wire joints, ESP32, and pump driver dry.

The DS18B20 supports a 3.0-5.5 V supply; its externally powered wiring uses a data-line pull-up, and a 12-bit conversion can take up to 750 ms. See the [manufacturer's datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf), especially the power and operating-mode sections. Wire colors above come from this probe's package, not the bare chip's pin numbering.

## Libraries and upload

1. Open the existing `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` in Arduino IDE.
2. Use **esp32 by Espressif Systems 3.3.2**, select **NodeMCU-32S**, and choose the board's COM port. This matches the core version used by the project's compile check.
3. In Library Manager, install **OneWire by Paul Stoffregen 2.3.8** and **DallasTemperature by Miles Burton 4.0.6**. These are the versions used by CI. WiFi and WebServer come with the ESP32 core.
4. Keep the existing school Wi-Fi settings in your local copy. No Wi-Fi or pump configuration needs to change for this sensor. Keep actual credentials out of Git.
5. Upload and open Serial Monitor at **115200 baud**.

CLI library installation:

```sh
arduino-cli lib update-index
arduino-cli lib install "OneWire@2.3.8" "DallasTemperature@4.0.6"
arduino-cli compile --fqbn esp32:esp32:nodemcu-32s firmware/wifi_connectivity_test
```

## What the sketch does

- Starts a temperature conversion about every **2 seconds**, then reads the result after **750 ms** without blocking the main loop during conversion. The [library's asynchronous example](https://github.com/milesburton/Arduino-Temperature-Control-Library/blob/master/examples/WaitForConversion2/WaitForConversion2.ino) demonstrates this conversion pattern.
- Prints Celsius and Fahrenheit to Serial Monitor, including while Wi-Fi is disconnected.
- Displays the latest valid reading on the ESP32's own pump-control page at `http://<board-ip>/`. Refresh the page to update it.
- Adds `temperatureAvailable` and `temperatureC` to `/api/status`, preserving all existing status fields and CORS behavior. Before the first valid reading, or after a failed/disconnected sensor read, availability is false and the value is JSON `null`, not a fake temperature. A previously cached reading can remain visible until the next measurement detects the fault.
- Retries a missing sensor automatically. A probe attached after boot can recover on a later measurement without restarting the board.
- Preserves Enterprise Wi-Fi, reconnection, HTTP and Serial pump commands, the 10-second run limit, and stop-on-Wi-Fi-loss behavior. Temperature is for monitoring and does not turn the pump on or off automatically.

Example extra status fields after a valid reading:

```json
{"temperatureAvailable": true, "temperatureC": 23.75}
```

The separate GitHub Pages website is unchanged and does not yet display these extra fields. Use the board's own page or Serial Monitor to view the temperature.

## Check on the physical board

1. Confirm a plausible temperature appears after startup. Warm the metal probe gently in your hand and verify the reading changes.
2. Confirm Wi-Fi still connects and the board's page shows both temperature and the pump buttons.
3. With the pump submerged, test ON, OFF, and automatic shutoff after about 10 seconds while measurements continue. Repeated ON commands must not extend a run.
4. Verify Serial commands **1** and **0** still work. Confirm loss of an established Wi-Fi connection stops the pump, as before.
5. Turn power off, disconnect the sensor, and restart. The sketch should report the sensor missing while Wi-Fi and pump controls remain available. Power down again before reconnecting it and verify readings return after startup.
6. If readings are unavailable, check the 4.7 kOhm resistor, yellow wire on IO27, red on 3V3, common ground, and the installed libraries. A CRC/bus error or the library's disconnected reading is reported as unavailable rather than a temperature.

Compiling checks software integration. It cannot verify this probe, physical wiring, pump timing under real load, or school Wi-Fi access.
