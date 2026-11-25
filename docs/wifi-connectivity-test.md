# ESP32 Wi-Fi, pump, and temperature controller

This combined Arduino sketch joins Wi-Fi, reports connection status, and controls a pump through an Adafruit STEMMA MOSFET + Diode Driver (5648). It targets the project's NodeMCU-32S / classic ESP32 and uses GPIO25. Uploading it replaces the current ESP32 program. You can still test Wi-Fi with the pump disconnected.

The same sketch also reads a **DS18B20 temperature probe on GPIO27**. Follow the [temperature sensor wiring and library guide](ds18b20-temperature-sensor.md). The pump remains on GPIO25, and the Wi-Fi and pump controls below are preserved.

## Wiring

Turn the supply output off before wiring. Set a regulated DC supply to **5.0 V**, with at least **1 A capacity for one pump**. Power the ESP32 separately over USB.

| From | To |
| --- | --- |
| Supply positive (+5 V) | Driver V+ |
| Supply negative (-) | Driver GND |
| ESP32 GND | Driver GND (same common ground) |
| ESP32 GPIO25 (IO25, not physical pin number 25) | Driver In |
| Pump positive / red lead | White output terminal + |
| Pump negative / black lead | White output terminal - (Out) |

Use the labels printed on the board, not its orientation in a photo. The white output terminals are for the pump, not the supply input. Do not connect the pump negative directly to ground: that would bypass the switch. Leave the ESP32 5V/VIN and 3V3 pins out of the pump power circuit while using USB.

The driver already contains the MOSFET and flyback diode; no separate transistor, base resistor, or flyback diode is needed. Use a matching 3-pin JST-PH STEMMA cable or soldered headers for the input connections; loose wires pressed into bare holes are unreliable. If using Adafruit's color-coded cable, verify white = In, red = V+, black = GND against its documentation/board labels.

References: [Adafruit pinouts](https://learn.adafruit.com/adafruit-mosfet-driver/pinouts), [driver specifications and built-in diode](https://www.adafruit.com/product/5648).

## Upload and connect

1. Open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` in Arduino IDE. The sketch now contains its settings directly and requires no `wifi_secrets.h` or `wifi_secrets.example.h`. Copying the entire `.ino` into a new Arduino sketch also works.
2. Use **esp32 by Espressif Systems**, version **3.3.2** (the version checked by CI), select **NodeMCU-32S**, and select the connected COM port. WiFi and WebServer are included with this core. Install **OneWire 2.3.8** and **DallasTemperature 4.0.6** through Library Manager for the DS18B20 sensor. The modern Enterprise API does not require the legacy `esp_wpa2.h` header.
3. Upload and open Serial Monitor at **115200 baud**. Set the line ending to **Newline** or **Both NL & CR**, then press EN/reset if you missed the prompts. Enter the **Wi-Fi name**, **user ID**, and **password**, pressing Send after each answer. The prompt waits for submitted lines, not a fixed timeout; do not use No line ending during setup.
4. For **home Wi-Fi**, send an empty user ID line: this selects normal personal authentication using the Wi-Fi password. For **school Wi-Fi**, enter the user ID (often the full school email or `user@school-domain`), which selects **WPA2-Enterprise PEAP**. Both use 2.4 GHz. A blank home password selects an open network; a protected home network needs its password. School TTLS, TLS, or browser captive portals require a different setup. Passwords and SSIDs preserve spaces and are never printed back by the firmware.
5. Expect `PASS: Connected to Wi-Fi and received an IP address.` followed by an `http://` address. Connection status and signal strength print every five seconds.
6. Open that exact HTTP address on a phone or computer on the same local network. Expect `PASS: Your browser reached the ESP32 over local Wi-Fi!` and a matching Serial Monitor message. This confirms local HTTP communication, not internet access.
7. To test reconnection, briefly disable the access point (or move the board out of range), then restore it. Expect the disconnection message, reconnection, and the current IP address. Repeat the browser test using the new address if it changed.

School server verification: `EAP_CA_CERT` accepts the IT-provided root CA certificate as PEM text, for example `R"PEM(-----BEGIN CERTIFICATE----- ... -----END CERTIFICATE-----)PEM"` with the actual PEM line breaks and contents. The default `nullptr` matches Espressif's certificate-free example and **does not authenticate the Wi-Fi server**; obtain school IT's certificate and settings before using real credentials. Do not substitute an arbitrary website certificate. See [Espressif's Enterprise example](https://github.com/espressif/arduino-esp32/blob/3.3.2/libraries/WiFi/examples/WiFiClientEnterprise/WiFiClientEnterprise.ino).

The outer Enterprise identity defaults to the entered user ID. If IT requires an anonymous or empty identity, set `EAP_IDENTITY_OVERRIDE` in your local sketch accordingly (an empty string is distinct from `nullptr`). A home network never uses the Enterprise identity or CA settings.

Send **W** after setup to stop the pump, disconnect Wi-Fi, and restart the three prompts. Use this to fix a password or switch from school to home Wi-Fi; Enterprise authentication is explicitly disabled before the new configuration. Credentials are session-only in RAM and must be re-entered after reset or power loss. The firmware does not echo them or save them to flash, and no credentials belong in GitHub. Setup is through USB Serial, not the public website: the board must join Wi-Fi before the website can reach it. After changing networks, copy the new IP into the website.

Campus Wi-Fi may isolate devices even when both have internet access. If the board connects but its local page is unreachable, ask IT about device-to-device access or an approved project network; Serial Monitor pump commands still work.

## Turn the pump on and off

1. Keep the pump submerged in water with its outlet directed into a container. Keep the ESP32 and driver dry. Verify wiring and 5 V output, then enable the supply.
2. Open the board's `http://<board-ip>/` address on the same Wi-Fi network. The page now includes **Turn pump ON** and **Turn pump OFF** buttons. It reports the commanded output state, not measured water flow. Refresh the page to update its state after an automatic stop or Serial command.
3. Click ON: the pump should run immediately. Click OFF: it should stop immediately.
4. Click ON again and wait: it should stop automatically after about **10 seconds**. Repeated ON commands while running do not extend this timer. There is no long watering delay blocking Wi-Fi processing.
5. Once all Wi-Fi prompts are complete, Serial Monitor at **115200 baud** also works: send **1** for ON or **0** for OFF. Newline characters are ignored outside setup. Pump commands work while Wi-Fi is still connecting, and have the same 10-second limit. During setup, the pump remains OFF and entered characters are treated as credentials, not pump commands.
6. Reset the board: it should remain OFF until a new command. Losing an established Wi-Fi connection also stops the pump; reconnecting does not restart it. A new Serial ON command can still be used while offline.

`kPumpPin` and `kMaxPumpRunMs` near the top of the sketch set the GPIO and maximum run time. The 10-second stop is a software safeguard, not a hardware guarantee: if the processor hangs or a driver fails, switch off the pump power supply. Initial hardware tests should be supervised.

The local control page has no login and is intended for a trusted local network. Do not expose it with router port forwarding. The GitHub Pages website now supports real START/STOP through the new `/api/pump` endpoint; follow the [website-control guide](website-pump-control.md). Board-local `/pump/on` and `/pump/off` forms still work. `/api/status` reports `wateringAvailable: true`, `pumpOn`, `maxRunMs`, and `remainingMs` alongside temperature fields.

If connection never succeeds, check credentials, 2.4 GHz availability, and signal strength. If the ESP32 connects but the browser times out, check guest-network client isolation, VPN routing, and browser local-network permission. The board's own URL starts with `http://`.

GitHub Actions compiles this sketch for NodeMCU-32S using placeholder credentials. Real network connectivity must be checked on the physical board using the steps above.

## Live status on the website

Upload the updated sketch first. Opening `http://<board-ip>/api/status` should return JSON identifying `garden-assistant-esp32` with `connected: true`, `wateringAvailable: true`, and `maxRunMs: 10000`. Older firmware may connect for status but cannot enable the website's START button.

On the Garden Assistant website, enter the current IP from Serial Monitor (the form initially suggests `10.0.0.44`) and click **Connect board**. Allow local-network access if prompted. The indicator turns green only after the firmware responds with the expected status. It checks again five seconds after each response, with a four-second request timeout. Unplug the board to verify the indicator changes to **Board unreachable** within about nine seconds while the page is active; plug it back in to verify recovery. If its IP changes, enter the new one and connect again.

Browser support for HTTPS websites reaching local HTTP devices varies. Use a current browser that supports Local Network Access permissions, such as current Chrome; Edge behavior depends on its version and policy. Unsupported browsers or denied permission show **Board unreachable**, which means the website could not verify the connection, not necessarily that the ESP32 lost Wi-Fi. See [Chrome's Local Network Access guidance](https://developer.chrome.com/docs/modern-web-guidance/guides/security/local-network-access). For troubleshooting, run the site from a local HTTP server on your LAN, or verify the board directly at its HTTP address. Do not disable browser security globally.

Status checks are read-only. Pump commands use a separate JSON POST endpoint with CORS support for the published website and localhost previews. The page reports START/STOP acknowledgement only after validating the ESP32 response; it cannot measure whether water actually flows.

Run software tests with `node --test web-app/tests/board-status.test.cjs`. These use simulated network responses; they cannot verify your browser permissions, Wi-Fi, or physical board.
