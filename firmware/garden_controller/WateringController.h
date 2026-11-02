#pragma once
#include <stdint.h>

// Hardware-independent state machine: tests supply a clock without waiting.
class WateringController {
 public:
  static constexpr uint32_t kDurationMs = 5000;

  bool start(uint32_t now) {
    // A second command must not extend an already-running cycle.
    if (active_) return false;
    startedAt_ = now;
    active_ = true;
    return true;
  }

  void stop() { active_ = false; }
  bool active() const { return active_; }

  void update(uint32_t now) {
    // Unsigned subtraction keeps elapsed time correct across millis() rollover.
    if (active_ && uint32_t(now - startedAt_) >= kDurationMs) stop();
  }

 private:
  bool active_ = false;  // Power-up always starts with watering OFF.
  uint32_t startedAt_ = 0;
};
