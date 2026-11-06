// A small shared client keeps network validation testable without a browser.
(function (root) {
  function boardUrl(address) {
    // Accept only private IPv4 addresses: never send checks to arbitrary websites.
    const parts = address.trim().split('.');
    if (parts.length !== 4 || parts.some(p => !/^\d{1,3}$/.test(p) || Number(p) > 255)) {
      throw new Error('Enter the board’s local IPv4 address, such as 10.0.0.44.');
    }
    const [a, b] = parts.map(Number);
    if (!(a === 10 || (a === 192 && b === 168) || (a === 172 && b >= 16 && b <= 31))) {
      throw new Error('Use a private IP address from Serial Monitor.');
    }
    return `http://${parts.map(Number).join('.')}/api/status`;
  }

  async function check(address, fetcher = root.fetch.bind(root)) {
    const url = boardUrl(address);
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 4000);
    try {
      const response = await fetcher(url, {
        signal: controller.signal,
        cache: 'no-store',
        credentials: 'omit',
        // Supporting browsers permit HTTPS-to-local-HTTP after user permission.
        targetAddressSpace: 'local',
      });
      if (!response.ok) throw new Error('Status endpoint unavailable. Upload the updated Wi-Fi sketch.');
      const data = await response.json();
      // A generic HTTP success is insufficient proof that this is our firmware.
      if (data?.device !== 'garden-assistant-esp32' || data.connected !== true ||
          data.wateringAvailable !== false) {
        throw new Error('Board status unrecognized. Upload the updated Wi-Fi sketch.');
      }
      return data;
    } finally {
      clearTimeout(timer);
    }
  }

  root.GardenBoard = { check, boardUrl };
  if (typeof module !== 'undefined') module.exports = root.GardenBoard;
})(globalThis);
