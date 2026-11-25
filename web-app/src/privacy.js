// Visual privacy only: retain the real address for board requests.
const privacyToggle = document.querySelector('#privacy-toggle');
const privateAddress = document.querySelector('#board-address');
let ipHidden = false;

privacyToggle.addEventListener('click', () => {
  ipHidden = !ipHidden;
  privateAddress.type = ipHidden ? 'password' : 'text';
  privateAddress.setAttribute('aria-label', ipHidden ? 'ESP32 IP address (hidden)' : 'ESP32 IP address');
  privacyToggle.setAttribute('aria-pressed', String(ipHidden));
  privacyToggle.textContent = ipHidden ? 'Show IP address' : 'Hide IP address';
});
