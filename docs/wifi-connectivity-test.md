# ESP32 Wi-Fi and pump controller

This combined Arduino sketch joins Wi-Fi, reports connection status, and controls a pump through an Adafruit STEMMA MOSFET + Diode Driver (5648). It targets the project's NodeMCU-32S / classic ESP32 and uses GPIO25. Uploading it replaces the current ESP32 program. You can still test Wi-Fi with the pump disconnected.

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

1. Copy `firmware/wifi_connectivity_test/wifi_secrets.example.h` to `wifi_secrets.h` in the same directory. Enter your Wi-Fi name and password in that local copy. Git ignores `wifi_secrets.h`; never put credentials into the example file.
2. Open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` in Arduino IDE. Use **esp32 by Espressif Systems**, select **NodeMCU-32S**, and select the connected COM port. The WiFi and WebServer libraries are included with this core.
3. Use a 2.4 GHz network with a normal Wi-Fi password. Enterprise authentication and captive portal login are outside this example's scope.
4. Upload, open Serial Monitor at **115200 baud**, then press EN/reset if startup messages were missed.
5. Expect `PASS: Connected to Wi-Fi and received an IP address.` followed by an `http://` address. Connection status and signal strength print every five seconds.
6. Open that exact HTTP address on a phone or computer on the same local network. Expect `PASS: Your browser reached the ESP32 over local Wi-Fi!` and a matching Serial Monitor message. This confirms local HTTP communication, not internet access.
7. To test reconnection, briefly disable the access point (or move the board out of range), then restore it. Expect the disconnection message, reconnection, and the current IP address. Repeat the browser test using the new address if it changed.

## Turn the pump on and off

1. Keep the pump submerged in water with its outlet directed into a container. Keep the ESP32 and driver dry. Verify wiring and 5 V output, then enable the supply.
2. Open the board's `http://<board-ip>/` address on the same Wi-Fi network. The page now includes **Turn pump ON** and **Turn pump OFF** buttons. It reports the commanded output state, not measured water flow. Refresh the page to update its state after an automatic stop or Serial command.
3. Click ON: the pump should run immediately. Click OFF: it should stop immediately.
4. Click ON again and wait: it should stop automatically after about **10 seconds**. Repeated ON commands while running do not extend this timer. There is no long watering delay blocking Wi-Fi processing.
5. Serial Monitor at **115200 baud** also works: send **1** for ON or **0** for OFF. Newline characters are ignored. Serial commands work even before Wi-Fi connects, and have the same 10-second limit.
6. Reset the board: it should remain OFF until a new command. Losing an established Wi-Fi connection also stops the pump; reconnecting does not restart it. A new Serial ON command can still be used while offline.

`kPumpPin` and `kMaxPumpRunMs` near the top of the sketch set the GPIO and maximum run time. The 10-second stop is a software safeguard, not a hardware guarantee: if the processor hangs or a driver fails, switch off the pump power supply. Initial hardware tests should be supervised.

The local control page has no login and is intended for a trusted local network. Do not expose it with router port forwarding. The separate GitHub Pages **water plant** button remains a demo; use the board's own page for actual control. `/pump/on` and `/pump/off` accept POST requests, not clickable GET links. The read-only `/api/status` endpoint additionally reports `pumpOn` while retaining `wateringAvailable: false` for compatibility with the separate website.

If connection never succeeds, check credentials, 2.4 GHz availability, and signal strength. If the ESP32 connects but the browser times out, check guest-network client isolation, VPN routing, and that the URL starts with `http://`. The sample has no connection to the GitHub Pages watering button.

GitHub Actions compiles this sketch for NodeMCU-32S using placeholder credentials. Real network connectivity must be checked on the physical board using the steps above.

## Live status on the website

Upload the updated sketch in this PR first: the earlier connectivity sketch only serves `/` and cannot supply the website's new `/api/status` health check. Opening `http://<board-ip>/api/status` should return JSON identifying `garden-assistant-esp32` with `connected: true` and `wateringAvailable: false`.

On the Garden Assistant website, enter the current IP from Serial Monitor (the form initially suggests `10.0.0.44`) and click **Connect board**. Allow local-network access if prompted. The indicator turns green only after the firmware responds with the expected status. It checks again five seconds after each response, with a four-second request timeout. Unplug the board to verify the indicator changes to **Board unreachable** within about nine seconds while the page is active; plug it back in to verify recovery. If its IP changes, enter the new one and connect again.

Browser support for HTTPS websites reaching local HTTP devices varies. Use a current browser that supports Local Network Access permissions, such as current Chrome; Edge behavior depends on its version and policy. Unsupported browsers or denied permission show **Board unreachable**, which means the website could not verify the connection, not necessarily that the ESP32 lost Wi-Fi. See [Chrome's Local Network Access guidance](https://developer.chrome.com/docs/modern-web-guidance/guides/security/local-network-access). For troubleshooting, run the site from a local HTTP server on your LAN, or verify the board directly at its HTTP address. Do not disable browser security globally.

The endpoint is read-only and allows cross-origin status checks without cookies or credentials. Connectivity is separate from pump control: the website explicitly says watering remains a demo and does not report a successful watering command.

Run software tests with `node --test web-app/tests/board-status.test.cjs`. These use simulated network responses; they cannot verify your browser permissions, Wi-Fi, or physical board.
