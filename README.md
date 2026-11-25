# ESP32 Garden Assistant

ESP32 based garden assistant for an embedded systems project, starting with manual watering and a rain-aware outdoor plant mode.

## Run the web app

Use the published GitHub Pages website, or run `python -m http.server 8000 --directory web-app` and open `http://localhost:8000`. No build step is required. Use HTTP hosting instead of opening the HTML file directly for pump control.

Use the **Dark mode** toggle above the plant icon to switch between light and dark backgrounds. The page starts in light mode and remembers your choice in this browser when local storage is available.

Upload the updated firmware, enter its current IP, and click **Connect board**. **Water for 30 seconds** sends a real ON command; **Stop pump** sends OFF without a weather check. Status confirms the board's commanded output, not measured water flow. A lost reply leaves the pump state uncertain and is never automatically retried. Use the plant type button to switch between indoor and outdoor settings. For an outdoor plant, water-saving mode checks the plant's US ZIP code against current rain from Open-Meteo and skips START when rain is detected. ZIP-code lookup uses Zippopotam.us. A weather lookup failure is reported and watering continues without the rain check.

The GitHub Actions workflow deploys `web-app/` to GitHub Pages after changes are pushed to `main` (or when manually triggered). Once deployment succeeds, the site is available at <https://angel-diaz-1.github.io/esp32-garden-assistant/>.

## Project layout

```text
web-app/
  index.html       Page structure and accessible watering control
  src/
    styles.css     Responsive page styling
    app.js         Pump commands, acknowledgement, and rain-aware START
    theme.js       Theme initialization and saved light/dark preference
```

As those components are implemented, add `firmware/garden_controller/` for ESP32 code, `server/` for the device API, `hardware/` for wiring, and `docs/` for team documentation. Static assets can go in `web-app/public/` when needed.

## Connect the ESP32

For the combined Wi-Fi and pump sketch, open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` and follow the [wiring and upload guide](docs/wifi-connectivity-test.md). It reports Wi-Fi status and controls an Adafruit MOSFET driver on GPIO25. It also reads a DS18B20 temperature probe on GPIO27; see the [sensor wiring and library guide](docs/ds18b20-temperature-sensor.md). Open the board's own IP address for pump ON/OFF buttons, or send `1` / `0` in Serial Monitor. Each run is limited to 30 seconds. School Wi-Fi settings (SSID, Enterprise identity, username, password, and optional CA certificate) are directly in the `.ino`; no separate settings header is required. Enter real credentials only in a local copy and never commit them. The school must support the configured PEAP authentication; follow IT's certificate and identity settings.

With the updated Wi-Fi firmware uploaded, enter the board's IP in the website and click **Connect board**. Stay on the same network and allow local-network access in a supported browser, such as current Chrome. Old firmware is detected and START remains disabled until the new pump API is installed. See the [website-control guide](docs/website-pump-control.md) for setup and troubleshooting.

The ESP32 provides `GET /api/status` and `POST /api/pump` with JSON `{"action":"on"}` or `{"action":"off"}`. CORS supports the published GitHub Pages origin and localhost HTTP previews. The board's own control page and Serial commands still work. Keep this controller on a trusted LAN; it has no user authentication and must not be exposed by router port forwarding.

## Manual check

Connect the board, keep the pump submerged, and test START, STOP, and automatic shutoff after 30 seconds. Disconnect power/network to verify that the page reports an unknown state instead of success. Switch to outdoor mode, enter a valid US ZIP code, enable water-saving mode, and check the rain gate. STOP always bypasses weather. Test both themes, keyboard controls, and a phone-width layout.

Run the web app tests with Node.js: `node --test web-app/tests/*.test.cjs`.
