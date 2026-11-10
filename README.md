# ESP32 Garden Assistant

ESP32 based garden assistant for an embedded systems project, starting with manual watering and a rain-aware outdoor plant mode.

## Run the web app

Open `web-app/index.html` in your browser. No installation or build step is required.

The **water plant** button currently shows demo feedback. It does not contact an ESP32 or operate a pump. Use the plant type button to switch between indoor and outdoor settings. For an outdoor plant, enabling water-saving mode checks the plant's US ZIP code against current rain from the free Open-Meteo API; if rain is detected, that watering cycle is skipped. ZIP-code lookup uses the free Zippopotam.us API. A weather lookup failure is reported and does not prevent normal watering.

The GitHub Actions workflow deploys `web-app/` to GitHub Pages after changes are pushed to `main` (or when manually triggered). Once deployment succeeds, the site is available at <https://angel-diaz-1.github.io/esp32-garden-assistant/>.

## Project layout

```text
web-app/
  index.html       Page structure and accessible watering control
  src/
    styles.css     Responsive page styling
    app.js         Click handling and future device integration point
```

As those components are implemented, add `firmware/garden_controller/` for ESP32 code, `server/` for the device API, `hardware/` for wiring, and `docs/` for team documentation. Static assets can go in `web-app/public/` when needed.

## Connecting the ESP32 later

For the combined Wi-Fi and pump sketch, open `firmware/wifi_connectivity_test/wifi_connectivity_test.ino` and follow the [wiring and upload guide](docs/wifi-connectivity-test.md). It reports Wi-Fi status and controls an Adafruit MOSFET driver on GPIO25. Open the board's own IP address for pump ON/OFF buttons, or send `1` / `0` in Serial Monitor. Each run is limited to 10 seconds. School Wi-Fi settings (SSID, Enterprise identity, username, password, and optional CA certificate) are directly in the `.ino`; no separate settings header is required. Enter real credentials only in a local copy and never commit them. The school must support the configured PEAP authentication; follow IT's certificate and identity settings.

With the updated Wi-Fi firmware uploaded, enter the board's IP in the website and click **Connect board** for live status. Allow local-network access in a supported browser. Successful health checks show **Board connected**; watering remains a demo until a pump API is added. See the guide for permission and connectivity troubleshooting.

Replace the demo implementation of `requestWatering()` in `web-app/src/app.js` with a request to the watering API. The UI already handles pending requests and errors. Confirm device acknowledgement before reporting success; pump duration and safety limits should be enforced by the controller.

## Manual check

Open the page, press **water plant**, and confirm that it reports the ESP32 needs connecting. Switch to outdoor mode, enter a valid US ZIP code, enable water-saving mode, and confirm the page checks current rain before watering. Tab to the controls and activate them with Enter or Space. Check that the layout fits a phone screen.
