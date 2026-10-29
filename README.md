# ESP32 Garden Assistant

ESP32 based garden assistant for an embedded systems project, starting with manual watering.

## Run the web app

Open `web-app/index.html` in your browser. No installation or build step is required.

The **water plant** button currently shows demo feedback. It does not contact an ESP32 or operate a pump.

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

Replace the demo implementation of `requestWatering()` in `web-app/src/app.js` with a request to the watering API. The UI already handles pending requests and errors. Confirm device acknowledgement before reporting success; pump duration and safety limits should be enforced by the controller.

## Manual check

Open the page, press **water plant**, and confirm that it reports the ESP32 needs connecting. Tab to the button and activate it with Enter or Space. Check that the layout fits a phone screen.
