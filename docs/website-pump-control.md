# Control the ESP32 pump from the website

## Setup

1. Open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` in Arduino IDE. Install **OneWire by Paul Stoffregen** and **DallasTemperature by Miles Burton** through Library Manager. Use the project's Arduino-ESP32 **3.3.2** core and **NodeMCU-32S** board selection.
2. Fill in your school SSID, EAP identity, username, password, and school IT's CA settings in a local copy. Keep real credentials out of GitHub. Upload this revised sketch: earlier firmware does not implement the website pump API.
3. Wire the driver/pump as described in [the wiring guide](wifi-connectivity-test.md). Keep the pump submerged and electronics dry. GPIO25 drives the MOSFET; GPIO27 still reads the DS18B20. The pump starts OFF and has a 30-second software limit. Disconnecting established Wi-Fi also stops it.
4. Open Serial Monitor at **115200 baud** and copy the board's current IP address.
5. On the [Garden Assistant website](https://angel-diaz-1.github.io/esp32-garden-assistant/), enter that IP and click **Connect board**. Use the same network as the ESP32 and grant local-network access when prompted. For a local preview, run `python -m http.server 8000 --directory web-app` and open `http://localhost:8000`. Direct `file://` use is unsupported for pump control.

## Controls

- **Water for 30 seconds** sends one START request. It enables only after firmware identity and pump capability have been verified, and disables while a request or run is active. Repeated ON commands do not extend an active run.
- **Stop pump** sends OFF even when a weather check would skip START. It remains available for the last commanded device after a lost reply or an IP edit. A STOP clicked while START is in flight waits for that request to settle, then sends OFF. If STOP cannot be confirmed, switch off the pump's supply.
- The page polls the commanded pump state every five seconds. It reports an ESP32 acknowledgement, not proof of water flow. An automatic or Serial stop will appear after the next status check; remaining time is the latest board reading, not a locally inferred countdown.
- A lost START response may mean the pump is already running. The browser never retries START automatically. Use STOP or disconnect pump power; the firmware timer operates independently of the page.
- Outdoor water-saving mode can skip START on detected rain. A failed weather lookup preserves the existing policy: warn and proceed without that check. STOP has no weather dependency.

The board's local `http://<ip>/` page and Serial `1`/`0` commands remain available. Temperature conversion stays nonblocking. The main website does not add a temperature display in this change.

## Network and API

`GET /api/status` returns device identity, Wi-Fi state, `wateringAvailable: true`, `pumpOn`, `maxRunMs: 30000`, `remainingMs`, and existing temperature fields. Firmware without the pump API can connect for status, but START remains disabled.

`POST /api/pump` accepts the compact JSON body `{"action":"on"}` or `{"action":"off"}` with `Content-Type: application/json`. It responds with the same status JSON after setting the output. Unknown actions are rejected without changing the output. OPTIONS supports preflight; allowed browser origins are `https://angel-diaz-1.github.io` and HTTP localhost/127.0.0.1 preview servers. Existing board-local forms use their separate POST routes.

Browser support matters: current Chrome supports permission-gated HTTPS-to-local-HTTP access; other browsers or school policies may block it. See [Chrome's Local Network Access documentation](https://developer.chrome.com/blog/local-network-access). School client isolation may also prevent access despite successful Wi-Fi login. Ask IT for an approved device/project network. Do not disable browser protections or expose the controller to the internet.

## Verify on hardware

Confirm the driver Sig LED and pump respond to START and STOP, then verify automatic shutoff after 30 seconds. Repeat with Serial OFF, Wi-Fi loss, and page reload. Remove the temperature probe and confirm pump control still works. Check that incorrect IPs, older firmware, denied browser permission, and power loss produce an error/unknown state rather than a success claim.

Automated tests run with `node --test web-app/tests/*.test.cjs`; they simulate the board and cannot verify actual hardware, school network permissions, or water flow.
