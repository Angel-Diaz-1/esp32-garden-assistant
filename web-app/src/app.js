// Keep device communication separate from the UI so the team can add it later.
// This starter deliberately makes no network request and cannot run a water pump.
async function requestWatering() {
  // Future integration: send a request to the server that controls the ESP32.
  // Only report successful watering after the device confirms the command.
  return { connected: false };
}

const waterButton = document.querySelector('#water-button');
const wateringStatus = document.querySelector('#watering-status');

waterButton.addEventListener('click', async () => {
  // Prevent repeated requests while a future device command is in progress.
  waterButton.disabled = true;
  wateringStatus.textContent = 'Checking watering connection…';

  try {
    const result = await requestWatering();
    wateringStatus.textContent = result.connected
      ? 'Watering command confirmed.'
      : 'Button pressed! Connect the ESP32 to enable watering.';
  } catch (error) {
    // A failed request must never appear to the user as successful watering.
    wateringStatus.textContent = 'Could not reach the watering controller. Please try again.';
    console.error('Watering request failed:', error);
  } finally {
    // Restore the control after either success or failure.
    waterButton.disabled = false;
  }
});
