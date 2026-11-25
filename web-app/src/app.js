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
const stopButton = document.querySelector('#stop-button');
const pumpState = document.querySelector('#pump-state');
let isOutdoorPlant = false;
let startingPump = false;
let stoppingPump = false;
let actionGeneration = 0;
let lastCommandAddress = null;
let pendingOnCommand = null;

function refreshPumpControls() {
  waterButton.disabled = startingPump || stoppingPump || !boardConnected ||
    !latestBoardStatus?.wateringAvailable || latestBoardStatus.pumpOn;
  // Keep STOP available for the last commanded board even if contact is lost
  // or the IP field is edited. Never silently send OFF to a different device.
  stopButton.disabled = stoppingPump || !(lastCommandAddress || boardTarget);
  if (!boardConnected || !latestBoardStatus) {
    pumpState.textContent = 'Pump state unknown · check the board connection';
  } else if (!latestBoardStatus.wateringAvailable) {
    pumpState.textContent = 'Upload the updated sketch to enable pump control';
  } else {
    pumpState.textContent = latestBoardStatus.pumpOn
      ? `Pump commanded ON · up to ${Math.ceil(latestBoardStatus.remainingMs / 1000)} seconds remaining`
      : 'Pump commanded OFF';
  }
}
refreshPumpControls();

stopButton.addEventListener('click', async () => {
  const address = lastCommandAddress || boardTarget;
  if (!address || stoppingPump) return;
  ++actionGeneration; // Cancels any START still waiting on a weather lookup.
  stoppingPump = true;
  boardStatusRevision++;
  refreshPumpControls();
  wateringStatus.textContent = 'Sending stop command…';
  try {
    // Do not let a later ON response overtake OFF on the single-client ESP32.
    if (pendingOnCommand) await pendingOnCommand.catch(() => {});
    const data = await GardenBoard.command(address, 'off');
    boardStatusRevision++;
    if (boardTarget === address) {
      latestBoardStatus = data;
      showBoardStatus('connected', 'Board connected · pump OFF');
    }
    lastCommandAddress = null;
    wateringStatus.textContent = `ESP32 at ${address} acknowledged pump OFF.`;
  } catch (error) {
    if (boardTarget === address) showBoardStatus('disconnected', 'Board unreachable · pump state unknown');
    boardStatusRevision++;
    wateringStatus.textContent = 'Stop not confirmed. Switch off the pump power supply if needed. The board has a 30-second automatic limit.';
  } finally {
    stoppingPump = false;
    refreshPumpControls();
  }
});

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
  if (startingPump || stoppingPump || !boardConnected || !latestBoardStatus?.wateringAvailable) return;
  const address = boardTarget;
  const generation = ++actionGeneration;
  const connectionGeneration = boardGeneration;
  startingPump = true;
  boardStatusRevision++;
  refreshPumpControls();
  wateringStatus.textContent = 'Preparing watering command…';
  let weatherCheckWarning = '';

  try {
    if (isOutdoorPlant && waterSavingToggle.checked) {
      if (!zipCodeInput.checkValidity()) {
        zipCodeInput.reportValidity();
        wateringStatus.textContent = 'Enter a valid 5-digit US ZIP code to check local rain.';
        return;
      }

      try {
        const raining = await isRainingAtZipCode(zipCodeInput.value.trim());
        if (generation !== actionGeneration) return;
        if (raining) {
          wateringStatus.textContent = 'Rain detected near your plant. Reservoir watering skipped to save water.';
          return;
        }
      } catch (error) {
        console.error('Weather check failed:', error);
        weatherCheckWarning = `${error.message} Watering will continue without the rain check.`;
      }
    }

    if (generation !== actionGeneration) return;
    if (connectionGeneration !== boardGeneration || address !== boardTarget || !boardConnected) {
      wateringStatus.textContent = 'Board connection changed. Connect the board again before watering.';
      return;
    }
    lastCommandAddress = address;
    pendingOnCommand = GardenBoard.command(address, 'on');
    const result = await pendingOnCommand;
    boardStatusRevision++;
    if (generation !== actionGeneration) return;
    if (connectionGeneration === boardGeneration) {
      latestBoardStatus = result;
      showBoardStatus('connected', 'Board connected · pump ON');
    }
    wateringStatus.textContent = [weatherCheckWarning,
      `ESP32 at ${address} acknowledged pump ON. It stops automatically within 30 seconds; use Stop pump to finish sooner.`
    ].filter(Boolean).join(' ');
  } catch (error) {
    if (generation !== actionGeneration) return;
    boardStatusRevision++;
    if (boardTarget === address) showBoardStatus('disconnected', 'Board unreachable · pump state unknown');
    // A lost response may follow a successful start: do not claim OFF or retry.
    wateringStatus.textContent = 'Start not confirmed; the pump may be running. Use Stop pump or switch off its supply. No automatic retry was sent.';
    console.error('Watering request failed:', error);
  } finally {
    pendingOnCommand = null;
    startingPump = false;
    refreshPumpControls();
  }
});
