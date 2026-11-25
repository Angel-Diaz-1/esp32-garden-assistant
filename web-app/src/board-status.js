// Connection checks report commanded state, not measured pump flow.
const boardForm = document.querySelector('#board-form');
const boardAddress = document.querySelector('#board-address');
const boardStatus = document.querySelector('#board-status');
let boardConnected = false;
let boardGeneration = 0;
let boardPoll;
let boardTarget = null;
let latestBoardStatus = null;
let boardStatusRevision = 0;

function showBoardStatus(state, message) {
  boardConnected = state === 'connected';
  boardStatus.dataset.state = state;
  boardStatus.textContent = message;
  if (typeof updateTemperature === 'function') updateTemperature(latestBoardStatus, boardConnected);
  if (typeof refreshPumpControls === 'function') refreshPumpControls();
}

// Changing the target invalidates pending results, so an old board cannot turn
// the indicator green after the user starts checking another address.
boardAddress.addEventListener('input', () => {
  boardGeneration++;
  clearTimeout(boardPoll);
  boardTarget = null;
  latestBoardStatus = null;
  showBoardStatus('disconnected', 'Address changed · click Connect board');
});

boardForm.addEventListener('submit', event => {
  event.preventDefault();
  const generation = ++boardGeneration;
  const address = boardAddress.value.trim();
  boardTarget = address;
  latestBoardStatus = null;
  clearTimeout(boardPoll);
  showBoardStatus('checking', 'Checking board…');

  async function poll() {
    const revision = boardStatusRevision;
    try {
      const data = await GardenBoard.check(address);
      if (generation !== boardGeneration) return;
      if (revision === boardStatusRevision) {
        latestBoardStatus = data;
        showBoardStatus('connected', data.wateringAvailable
          ? `Board connected · pump ${data.pumpOn ? 'ON' : 'OFF'}`
          : 'Board connected · upload the new sketch to enable pump control');
      }
    } catch (error) {
      if (generation !== boardGeneration) return;
      if (revision === boardStatusRevision) {
        showBoardStatus('disconnected', error.message.includes('sketch') || error.message.includes('address')
          ? error.message
          : 'Board unreachable · check power, IP, Wi-Fi, and browser local-network permission');
      }
    }
    // Schedule after completion so requests cannot overlap within this session.
    if (generation === boardGeneration) boardPoll = setTimeout(poll, 2000);
  }
  poll();
});
