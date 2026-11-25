const temperatureValue = document.querySelector('#temperature-value');
const temperatureStatus = document.querySelector('#temperature-status');
let temperatureReceivedAt = null;
const temperatureStaleMs = 10000;

function clearTemperature(message) {
  temperatureReceivedAt = null;
  temperatureValue.textContent = '— °C / — °F';
  temperatureValue.dataset.state = 'unavailable';
  temperatureStatus.textContent = message;
}

function updateTemperature(data, connected) {
  if (!connected) {
    clearTemperature('Board not connected · temperature unavailable.');
    return;
  }
  const celsius = data?.temperatureC;
  if (data?.temperatureAvailable !== true || typeof celsius !== 'number' ||
      !Number.isFinite(celsius) || celsius < -55 || celsius > 125) {
    clearTemperature('No valid probe reading · check the DS18B20 connection.');
    return;
  }
  temperatureReceivedAt = Date.now();
  // textContent keeps sensor/network data out of HTML parsing.
  temperatureValue.textContent = `${celsius.toFixed(1)} °C / ${(celsius * 1.8 + 32).toFixed(1)} °F`;
  temperatureValue.dataset.state = 'live';
  temperatureStatus.textContent = 'Live probe reading · refreshes every 2 seconds.';
}

function checkTemperatureFreshness() {
  if (temperatureReceivedAt !== null && Date.now() - temperatureReceivedAt >= temperatureStaleMs) {
    clearTemperature('Reading is stale · waiting for a fresh board response.');
  }
}
setInterval(checkTemperatureFreshness, 1000);
document.addEventListener('visibilitychange', checkTemperatureFreshness);
