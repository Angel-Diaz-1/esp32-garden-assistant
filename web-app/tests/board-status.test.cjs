const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const { check, boardUrl } = require('../src/board-client.js');
const valid = { device: 'garden-assistant-esp32', connected: true, wateringAvailable: false };

test('only private IPv4 targets are accepted', () => {
  assert.equal(boardUrl(' 10.0.0.44 '), 'http://10.0.0.44/api/status');
  for (const ip of ['8.8.8.8', '172.32.0.1', '10.0.0.256', 'localhost', 'http://10.0.0.44', '10.0.0.1/path']) {
    assert.throws(() => boardUrl(ip));
  }
});

test('checks firmware identity and does not accept unrelated HTTP success', async () => {
  assert.deepEqual(await check('10.0.0.44', async (url, options) => {
    assert.equal(url, 'http://10.0.0.44/api/status');
    assert.equal(options.targetAddressSpace, 'local');
    assert.equal(options.cache, 'no-store');
    return { ok: true, json: async () => valid };
  }), valid);
  for (const payload of [{}, null, { ...valid, connected: false }, { ...valid, device: 'other' }]) {
    await assert.rejects(check('10.0.0.44', async () => ({ ok: true, json: async () => payload })));
  }
  await assert.rejects(check('10.0.0.44', async () => ({ ok: false })), /sketch/);
  await assert.rejects(check('10.0.0.44', async () => { throw new TypeError('Failed to fetch'); }));
});

test('unresponsive requests are aborted', async () => {
  await assert.rejects(check('10.0.0.44', (url, { signal }) => new Promise((resolve, reject) => {
    signal.addEventListener('abort', () => reject(new Error('timed out')));
  })), /timed out/);
});

// Exercise the actual UI script with controlled replies and timer scheduling.
function ui(checker) {
  const elements = Object.fromEntries(['#board-form', '#board-address', '#board-status'].map(id => [id, {
    value: '10.0.0.44', dataset: {}, listeners: {},
    addEventListener(event, callback) { this.listeners[event] = callback; },
  }]));
  let nextPoll;
  const context = vm.createContext({
    document: { querySelector: id => elements[id] }, GardenBoard: { check: checker },
    setTimeout(callback) { nextPoll = callback; return 1; }, clearTimeout() { nextPoll = undefined; },
  });
  vm.runInContext(fs.readFileSync(require.resolve('../src/board-status.js'), 'utf8'), context);
  return { elements, context, poll: () => nextPoll(), submit: () => elements['#board-form'].listeners.submit({ preventDefault() {} }) };
}
const flush = () => new Promise(resolve => setImmediate(resolve));

test('indicator connects only on success, drops after failure, and recovers', async () => {
  let fail = false;
  const view = ui(async () => { if (fail) throw new Error('network'); return valid; });
  view.submit(); await flush();
  assert.equal(view.elements['#board-status'].dataset.state, 'connected');
  fail = true; await view.poll();
  assert.equal(view.elements['#board-status'].dataset.state, 'disconnected');
  assert.equal(vm.runInContext('boardConnected', view.context), false);
  fail = false; await view.poll();
  assert.equal(view.elements['#board-status'].dataset.state, 'connected');
});

test('editing address discards a late successful result from the previous target', async () => {
  let finish;
  const view = ui(() => new Promise(resolve => { finish = resolve; }));
  view.submit();
  view.elements['#board-address'].listeners.input();
  finish(valid); await flush();
  assert.equal(view.elements['#board-status'].dataset.state, 'disconnected');
});
