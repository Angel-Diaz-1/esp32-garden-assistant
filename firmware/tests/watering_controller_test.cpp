#include "../garden_controller/WateringController.h"
#include <assert.h>
#include <stdio.h>

int main() {
  WateringController controller;
  assert(!controller.active());
  assert(controller.start(100));
  assert(!controller.start(4000));  // Repeated commands cannot reset the timer.
  controller.update(5099);
  assert(controller.active());
  controller.update(5100);
  assert(!controller.active());
  assert(controller.start(6000));
  controller.stop();
  controller.stop();  // Stopping an idle controller is harmless.
  assert(!controller.active());
  assert(controller.start(7000));
  controller.update(20000);  // A late loop still stops the cycle.
  assert(!controller.active());
  assert(controller.start(UINT32_MAX - 999));
  controller.update(3999);
  assert(controller.active());
  controller.update(4000);  // Exactly 5000 ms, across the clock rollover.
  assert(!controller.active());
  puts("All watering controller tests passed.");
}
