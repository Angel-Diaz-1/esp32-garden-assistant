const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const { check, command } = require('../src/board-client.js');
const state = (on = false) => ({ device: 'garden-assistant-esp32', connected: true,
  wateringAvailable: true, pumpOn: on, maxRunMs: 10000, remainingMs: on ? 10000 : 0 });
const reply = data => ({ ok: true, json: async () => data });

test('pump commands POST JSON and require the correct device acknowledgement', async () => {
  for (const action of ['on', 'off']) {
    await command('10.0.0.44', action, async (url, options) => {
      assert.equal(url, 'http://10.0.0.44/api/pump');
      assert.equal(options.method, 'POST');
      assert.equal(options.headers['Content-Type'], 'application/json');
      assert.equal(options.body, JSON.stringify({ action }));
      assert.equal(options.credentials, 'omit');
      return reply(state(action === 'on'));
    });
  }
  for (const data of [{}, state(false), { ...state(true), device: 'wrong' },
    { ...state(true), wateringAvailable: false }, { ...state(true), maxRunMs: 30000 }]) {
    await assert.rejects(command('10.0.0.44', 'on', async () => reply(data)));
  }
  await assert.rejects(command('10.0.0.44', 'invalid'));
});

test('old firmware is recognized but malformed pump status is rejected', async () => {
  const old = { device: state().device, connected: true, wateringAvailable: false };
  assert.deepEqual(await check('10.0.0.44', async () => reply(old)), old);
  for (const data of [{ ...state(), pumpOn: 'false' }, { ...state(), remainingMs: -1 },
    { ...state(), remainingMs: 11000 }]) {
    await assert.rejects(check('10.0.0.44', async () => reply(data)));
  }
});

test('a lost command reply is never automatically retried', async () => {
  let count = 0;
  await assert.rejects(command('10.0.0.44', 'on', async () => {
    count++; throw new Error('reply lost');
  }));
  assert.equal(count, 1);
});

function ui(commander, fetcher = async () => { throw new Error('unexpected fetch'); }) {
  const ids = ['board-form', 'board-address', 'board-status', 'plant-type-toggle',
    'outdoor-settings', 'zip-code', 'water-saving-toggle', 'water-button',
    'watering-status', 'stop-button', 'pump-state'];
  const elements = Object.fromEntries(ids.map(id => [id, {
    value: id === 'board-address' ? '10.0.0.44' : '33431', dataset: {}, listeners: {},
    checked: false, textContent: '', disabled: false,
    addEventListener(type, fn) { this.listeners[type] = fn; },
    setAttribute() {}, checkValidity: () => true, reportValidity() {},
  }]));
  const context = vm.createContext({ document: { querySelector: id => elements[id.slice(1)] },
    GardenBoard: { check: async () => state(), command: commander },
    setTimeout: () => 1, clearTimeout() {}, fetch: fetcher, URL, URLSearchParams,
    console: { error() {} },
  });
  for (const file of ['board-status.js', 'app.js']) {
    vm.runInContext(fs.readFileSync(require.resolve(`../src/${file}`), 'utf8'), context);
  }
  const run = code => vm.runInContext(code, context);
  run("boardTarget = '10.0.0.44'; latestBoardStatus = " + JSON.stringify(state()) +
    "; showBoardStatus('connected', 'Board connected');");
  return { elements, run, click: id => elements[id].listeners.click() };
}

test('START waits for acknowledgement, disables duplicates, and STOP acknowledges OFF', async () => {
  let finish;
  const calls = [];
  const view = ui((address, action) => {
    calls.push([address, action]);
    return action === 'on' ? new Promise(resolve => { finish = resolve; }) : Promise.resolve(state());
  });
  const start = view.click('water-button');
  assert.equal(view.elements['water-button'].disabled, true);
  assert.doesNotMatch(view.elements['watering-status'].textContent, /acknowledged/);
  await view.click('water-button');
  assert.equal(calls.length, 1);
  finish(state(true)); await start;
  assert.match(view.elements['watering-status'].textContent, /acknowledged pump ON/);
  assert.equal(view.elements['water-button'].disabled, true);
  await view.click('stop-button');
  assert.match(view.elements['watering-status'].textContent, /acknowledged pump OFF/);
  assert.equal(view.elements['water-button'].disabled, false);
  assert.deepEqual(calls, [['10.0.0.44', 'on'], ['10.0.0.44', 'off']]);
});

test('uncertain START retains STOP for the original device after editing the IP', async () => {
  const calls = [];
  const view = ui(async (address, action) => {
    calls.push([address, action]);
    if (action === 'on') throw new Error('timeout');
    return state();
  });
  await view.click('water-button');
  assert.match(view.elements['watering-status'].textContent, /may be running/);
  view.elements['board-address'].value = '10.0.0.55';
  view.elements['board-address'].listeners.input();
  assert.equal(view.elements['stop-button'].disabled, false);
  await view.click('stop-button');
  assert.deepEqual(calls, [['10.0.0.44', 'on'], ['10.0.0.44', 'off']]);
});

test('STOP during an in-flight START is sent after START settles', async () => {
  let finish;
  const calls = [];
  const view = ui(async (address, action) => {
    calls.push(action);
    if (action === 'on') return new Promise(resolve => { finish = resolve; });
    return state();
  });
  const start = view.click('water-button');
  const stop = view.click('stop-button');
  assert.deepEqual(calls, ['on']);
  finish(state(true));
  await Promise.all([start, stop]);
  assert.deepEqual(calls, ['on', 'off']);
  assert.match(view.elements['watering-status'].textContent, /acknowledged pump OFF/);
});

test('rain skips START while STOP always bypasses the weather check', async () => {
  const calls = [];
  const view = ui(async (ip, action) => { calls.push(action); return state(); },
    async url => reply(String(url).includes('zippopotam')
      ? { places: [{ latitude: '26', longitude: '-80' }] } : { current: { rain: 1 } }));
  view.click('plant-type-toggle');
  view.elements['water-saving-toggle'].checked = true;
  await view.click('water-button');
  assert.match(view.elements['watering-status'].textContent, /Rain detected/);
  assert.deepEqual(calls, []);
  await view.click('stop-button');
  assert.deepEqual(calls, ['off']);
});

test('firmware without pump API cannot trigger START', async () => {
  let calls = 0;
  const view = ui(async () => { calls++; });
  view.run('latestBoardStatus.wateringAvailable = false; refreshPumpControls();');
  assert.equal(view.elements['water-button'].disabled, true);
  await view.click('water-button');
  assert.equal(calls, 0);
});

test('STOP failure is reported without claiming that the pump stopped', async () => {
  const view = ui(async () => { throw new Error('offline'); });
  await view.click('stop-button');
  assert.match(view.elements['watering-status'].textContent, /Stop not confirmed/);
  assert.doesNotMatch(view.elements['watering-status'].textContent, /acknowledged pump OFF/);
});

test('editing the IP during a rain lookup cancels START rather than targeting another board', async () => {
  let finish;
  const calls = [];
  const view = ui(async (ip, action) => { calls.push([ip, action]); return state(true); },
    async url => String(url).includes('zippopotam')
      ? reply({ places: [{ latitude: '26', longitude: '-80' }] })
      : new Promise(resolve => { finish = () => resolve(reply({ current: { rain: 0 } })); }));
  view.click('plant-type-toggle');
  view.elements['water-saving-toggle'].checked = true;
  const start = view.click('water-button');
  await new Promise(resolve => setImmediate(resolve));
  view.elements['board-address'].value = '10.0.0.55';
  view.elements['board-address'].listeners.input();
  finish(); await start;
  assert.deepEqual(calls, []);
  assert.match(view.elements['watering-status'].textContent, /connection changed/);
});

test('a background status reply started before a command cannot overwrite its acknowledgement', async () => {
  const view = ui(async () => state(true));
  // The checker resolves after a command, representing an old OFF response.
  view.run('GardenBoard.check = () => new Promise(resolve => { globalThis.finishStatus = resolve; });');
  view.elements['board-form'].listeners.submit({ preventDefault() {} });
  // Restore the already-confirmed connection while the background check waits.
  view.run('latestBoardStatus = ' + JSON.stringify(state()) + "; showBoardStatus('connected', 'Board connected');");
  await view.click('water-button');
  view.run('finishStatus(' + JSON.stringify(state()) + ');');
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(view.run('latestBoardStatus.pumpOn'), true);
});
