// Connection state is separate from watering feedback: reaching Wi-Fi never
// means a pump command succeeded. This firmware only supports health checks.
const boardForm = document.querySelector('#board-form');
const boardAddress = document.querySelector('#board-address');
const boardStatus = document.querySelector('#board-status');
let boardConnected = false;
let boardGeneration = 0;
let boardPoll;

function showBoardStatus(state, message) {
  boardConnected = state === 'connected';
  boardStatus.dataset.state = state;
  boardStatus.textContent = message;
}

// Changing the target invalidates pending results, so an old board cannot turn
// the indicator green after the user starts checking another address.
boardAddress.addEventListener('input', () => {
  boardGeneration++;
  clearTimeout(boardPoll);
  showBoardStatus('disconnected', 'Address changed · click Connect board');
});

boardForm.addEventListener('submit', event => {
  event.preventDefault();
  const generation = ++boardGeneration;
  const address = boardAddress.value.trim();
  clearTimeout(boardPoll);
  showBoardStatus('checking', 'Checking board…');

  async function poll() {
    try {
      await GardenBoard.check(address);
      if (generation !== boardGeneration) return;
      showBoardStatus('connected', 'Board connected · watering is still a demo');
    } catch (error) {
      if (generation !== boardGeneration) return;
      showBoardStatus('disconnected', error.message.includes('sketch') || error.message.includes('address')
        ? error.message
        : 'Board unreachable · check power, IP, Wi-Fi, and browser local-network permission');
    }
    // Schedule after completion so requests cannot overlap within this session.
    if (generation === boardGeneration) boardPoll = setTimeout(poll, 5000);
  }
  poll();
});
