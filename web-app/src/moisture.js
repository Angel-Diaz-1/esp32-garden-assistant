const moistureValue = document.querySelector('#moisture-value');
const moistureStatus = document.querySelector('#moisture-status');
let moistureReceivedAt = null;

function clearMoisture(message) {
  moistureReceivedAt = null;
  moistureValue.textContent = '—';
  moistureValue.dataset.state = 'unavailable';
  moistureStatus.textContent = message;
}

function updateMoisture(data, connected) {
  if (!connected) {
    clearMoisture('Board not connected · moisture unavailable.');
    return;
  }
  if (typeof data?.moistureAvailable !== 'boolean') {
    clearMoisture('Upload the moisture-enabled ESP32 sketch.');
    return;
  }
  const raw = data.moistureRaw;
  if (!data.moistureAvailable || !Number.isInteger(raw) || raw <= 0 || raw >= 4095) {
    clearMoisture('No valid analog reading · check AOUT, 3V3, and GND.');
    return;
  }
  moistureReceivedAt = Date.now();
  moistureValue.dataset.state = 'live';
  const percent = data.moisturePercent;
  if (data.moistureCalibrated === true && typeof percent === 'number' &&
      Number.isFinite(percent) && percent >= 0 && percent <= 100) {
    moistureValue.textContent = `${percent.toFixed(1)}%`;
    moistureStatus.textContent = `Calibrated relative moisture · raw ${raw} · refreshes every 2 seconds.`;
  } else {
    moistureValue.textContent = `${raw} raw`;
    moistureStatus.textContent = 'Live ADC reading (0–4095) · dry/wet calibration needed for a percentage.';
  }
}

function checkMoistureFreshness() {
  if (moistureReceivedAt !== null && Date.now() - moistureReceivedAt >= 10000) {
    clearMoisture('Reading is stale · waiting for a fresh board response.');
  }
}
setInterval(checkMoistureFreshness, 1000);
document.addEventListener('visibilitychange', checkMoistureFreshness);
