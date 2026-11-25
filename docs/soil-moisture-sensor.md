# Capacitive soil moisture sensor v1.2

The pictured board is an analog capacitive soil-moisture probe labeled **GND**, **VCC**, and **AOUT**. Connect it directly to the ESP32; it needs **no MOSFET driver**, external power supply, pull-up resistor, voltage divider, or additional Arduino library when powered from **3V3**. Keep the existing MOSFET for the pump only.

## Wiring (power off first)

| Sensor pin label | NodeMCU-32S ESP32 |
| --- | --- |
| VCC | 3V3 |
| GND | GND (shared with temperature sensor and MOSFET driver) |
| AOUT | P34 / GPIO34 |

Follow the sensor's printed labels rather than cable colors, which vary. Use the actual **P34** label, not physical pin number 34. GPIO34 is input-only ADC1 and supports analog sampling while Wi-Fi is active. Keep the temperature data wire on **GPIO27** with its existing 4.7 kΩ pull-up, and MOSFET control on **GPIO25**. The moisture sensor does not need its own pull-up.

Do not connect sensor VCC or AOUT to the pump's 5 V supply. ESP32 GPIO inputs must not receive 5 V; powering the sensor at 3.3 V keeps the analog circuit on the ESP32 supply. The photo identifies a generic v1.2 board, not an authenticated manufacturer: similar sensors are documented by [DFRobot](https://wiki.dfrobot.com/sen0193/), but exact electrical characteristics vary by clone. If this unit does not operate from 3.3 V, verify its seller's specifications before changing the supply; do not feed a higher output directly into the ESP32.

Only insert the sensing paddle into soil. Keep the connector, components, and upper electronics dry; do not submerge the whole board. Place the probe in the root-zone soil, with good soil contact, away from the direct water jet.

## Upload and view

Upload the revised `firmware/wifi_connectivity_test/wifi_connectivity_test.ino`, complete the existing Serial Wi-Fi prompts, and connect its IP on the website. No new library is required beyond the existing OneWire/DallasTemperature libraries. The firmware averages eight 12-bit ADC samples every two seconds without adding watering delays. The website displays **Soil moisture** next to **Plant temperature** (stacked on narrow screens) using the same status requests.

Serial Monitor and the board-local page also show moisture. Raw ADC values are **0–4095**. With no calibration, the website intentionally displays **raw** and a calibration reminder instead of guessing a percentage. Rail/clipped readings are marked unavailable; a disconnected analog input may float to a plausible number, so software cannot reliably prove the sensor is attached. Check the cable if the readings are erratic. An optional 0.1 µF capacitor from AOUT to GND near the ESP32 may help noisy readings; recalibrate with the final wiring.

## Calibrate for relative moisture percentage

1. Power the sensor from 3V3 with its final wiring. Put the sensing paddle in representative **dry soil** and note the stable raw value from the website or Serial Monitor.
2. Water the same type of soil thoroughly, let excess water drain, and record the **wet-soil** raw value. Keep electronics dry. Do not assume every sensor's reading increases with moisture.
3. Near the top of the sketch, replace `kMoistureDryRaw = -1` and `kMoistureWetRaw = -1` with your measured values. Both must be between 1 and 4094 and differ by at least 100 counts to avoid a noise-sensitive calibration. If they are too close, check insertion depth, supply, and sensor operation.
4. Upload again and repeat Wi-Fi setup. The website now displays **relative moisture %**, with dry = 0% and wet = 100%, clamped to that interval. This works with either increasing or decreasing sensor output. It is a two-point relative soil index, not laboratory volumetric water content; recalibrate for a different soil, probe, or power/wiring arrangement.

Moisture monitoring does **not** automatically start the pump. Manual START/STOP, ten-second shutoff, Wi-Fi setup, and temperature monitoring remain in place.

## API and checks

`/api/status` adds `moistureAvailable`, `moistureRaw`, `moistureCalibrated`, and `moisturePercent`. Unavailable readings and uncalibrated percentages use JSON `null`. The website clears moisture on network loss, address changes, missing/old firmware, or stale responses, and restores it after fresh data arrives. Existing firmware continues to report temperature and pump state but needs this update for moisture.

Test dry/wet readings while Wi-Fi is connected and repeat START, STOP, and automatic shutoff with the pump submerged. Confirm the DS18B20 still reports normally. Check the browser on a phone and desktop and in both themes. Automated website tests cannot verify sensor voltages or soil calibration.

References: [Espressif ADC APIs and 12-bit readings](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html), [ADC2 and Wi-Fi restrictions](https://docs.espressif.com/projects/esp-idf/en/v4.2.1/esp32/api-reference/peripherals/adc.html), [DFRobot calibration guidance for comparable capacitive probes](https://wiki.dfrobot.com/sen0193/).
