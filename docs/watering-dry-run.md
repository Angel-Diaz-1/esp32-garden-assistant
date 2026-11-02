# Test watering without a pump

The firmware runs on the team's NodeMCU-32S ESP32. It simulates the output in Serial Monitor; dry-run mode does not touch GPIO pins. No pump, MOSFET, Wi-Fi credentials, or additional libraries are needed.

## Upload and try it

1. Open `firmware/garden_controller/garden_controller.ino` in Arduino IDE.
2. Install **esp32 by Espressif Systems**, select **NodeMCU-32S**, and select the connected USB port.
3. Upload, then open Serial Monitor at **115200 baud** with **Newline** enabled. Press EN/reset if the startup message was missed.
4. Send `status`: expect `State: IDLE`.
5. Send `water`: expect an accepted command and `SIMULATION/OUTPUT: ON`. After five seconds expect `SIMULATION/OUTPUT: OFF`.
6. Send `water` again during a cycle: expect rejection; the original deadline stays unchanged.
7. Start another cycle and send `stop`: expect OFF immediately. `status` should report IDLE.
8. Reset during a cycle: it should return to IDLE without resuming.

Commands are lowercase. Oversized lines are rejected. Automatic timing is nonblocking, so `stop` and `status` remain usable during a cycle. Five seconds is a test duration, not a calibrated watering amount.

## Software validation

GitHub Actions tests startup, repeat-command rejection, timeout boundaries, explicit stop, late updates, and clock rollover using the same C++ controller as the sketch. It also compiles the complete sketch for NodeMCU-32S. On a computer with g++, run:

```sh
g++ -std=c++11 -Wall -Wextra -Werror firmware/tests/watering_controller_test.cpp -o watering-test
./watering-test
```

## Connecting the pump later

Keep `kDryRun = true` until the circuit is reviewed. The proposed output is GPIO25, active HIGH. Once the actual driver is selected, verify its polarity and startup behavior, then update the pin/output adapter and explicitly enable hardware mode. Use an appropriate MOSFET driver, external pump supply, shared ground, gate pulldown, and flyback protection for the motor. Do not power a pump from an ESP32 GPIO. Recheck stop/reset behavior with the real hardware; software timing is not an independent hardware cutoff.

The website is still a separate demo: this PR adds USB serial control, not a network connection. A future device API can call `watering.start(millis())` and `watering.stop()` while retaining the controller's timeout and duplicate-command checks.
