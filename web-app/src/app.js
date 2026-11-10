// Board health checks run separately; the firmware does not yet expose watering.
async function requestWatering() {
  // Future integration: send a request to the server that controls the ESP32.
  // Only report successful watering after the device confirms the command.
  return { connected: boardConnected, wateringAvailable: false };
}

async function isRainingAtZipCode(zipCode) {
  const zipResponse = await fetch(`https://api.zippopotam.us/us/${encodeURIComponent(zipCode)}`);
  if (!zipResponse.ok) {
    throw new Error(zipResponse.status === 404 ? 'That ZIP code was not found.' : 'Could not look up that ZIP code.');
  }

  const zipData = await zipResponse.json();
  const place = zipData.places?.[0];
  const latitude = Number(place?.latitude);
  const longitude = Number(place?.longitude);
  if (!Number.isFinite(latitude) || !Number.isFinite(longitude)) {
    throw new Error('The ZIP code lookup returned no usable location.');
  }

  const weatherUrl = new URL('https://api.open-meteo.com/v1/forecast');
  weatherUrl.search = new URLSearchParams({
    latitude: String(latitude),
    longitude: String(longitude),
    current: 'rain',
    timezone: 'auto',
  });
  const weatherResponse = await fetch(weatherUrl);
  if (!weatherResponse.ok) {
    throw new Error('The weather service is unavailable.');
  }

  const weatherData = await weatherResponse.json();
  const currentRain = weatherData.current?.rain;
  if (typeof currentRain !== 'number' || !Number.isFinite(currentRain)) {
    throw new Error('The weather service returned no current rain reading.');
  }

  return currentRain > 0;
}

const plantTypeToggle = document.querySelector('#plant-type-toggle');
const outdoorSettings = document.querySelector('#outdoor-settings');
const zipCodeInput = document.querySelector('#zip-code');
const waterSavingToggle = document.querySelector('#water-saving-toggle');
const waterButton = document.querySelector('#water-button');
const wateringStatus = document.querySelector('#watering-status');
let isOutdoorPlant = false;

plantTypeToggle.addEventListener('click', () => {
  isOutdoorPlant = !isOutdoorPlant;
  plantTypeToggle.textContent = `Plant type: ${isOutdoorPlant ? 'Outdoor' : 'Indoor'}`;
  plantTypeToggle.setAttribute('aria-pressed', String(isOutdoorPlant));
  plantTypeToggle.setAttribute('aria-label', `Switch to ${isOutdoorPlant ? 'indoor' : 'outdoor'} plant`);
  outdoorSettings.hidden = !isOutdoorPlant;
  zipCodeInput.required = isOutdoorPlant && waterSavingToggle.checked;
  wateringStatus.textContent = `${isOutdoorPlant ? 'Outdoor' : 'Indoor'} plant selected.`;
});

waterSavingToggle.addEventListener('change', () => {
  zipCodeInput.required = isOutdoorPlant && waterSavingToggle.checked;
});

waterButton.addEventListener('click', async () => {
  // Prevent repeated requests while a future device command is in progress.
  waterButton.disabled = true;
  wateringStatus.textContent = 'Checking watering connection…';
  let weatherCheckWarning = '';

  try {
    if (isOutdoorPlant && waterSavingToggle.checked) {
      if (!zipCodeInput.checkValidity()) {
        zipCodeInput.reportValidity();
        wateringStatus.textContent = 'Enter a valid 5-digit US ZIP code to check local rain.';
        return;
      }

      try {
        if (await isRainingAtZipCode(zipCodeInput.value.trim())) {
          wateringStatus.textContent = 'Rain detected near your plant. Reservoir watering skipped to save water.';
          return;
        }
      } catch (error) {
        console.error('Weather check failed:', error);
        weatherCheckWarning = `${error.message} Watering will continue without the rain check.`;
      }
    }

    const result = await requestWatering();
    const wateringMessage = result.connected
      ? 'Board connected. Pump control is not enabled; no watering command was sent.'
      : 'Watering demo: board connection has not been confirmed. Click Connect board.';
    wateringStatus.textContent = [weatherCheckWarning, wateringMessage].filter(Boolean).join(' ');
  } catch (error) {
    // A failed request must never appear to the user as successful watering.
    wateringStatus.textContent = 'Could not reach the watering controller. Please try again.';
    console.error('Watering request failed:', error);
  } finally {
    // Restore the control after either success or failure.
    waterButton.disabled = false;
  }
});
