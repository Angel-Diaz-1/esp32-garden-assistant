const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

function element() {
  return { value: '10.0.0.44', type: 'text', dataset: {}, attributes: {}, listeners: {}, textContent: '',
    addEventListener(event, fn) { this.listeners[event] = fn; },
    setAttribute(name, value) { this.attributes[name] = value; } };
}

test('privacy toggles masking without changing the IP or emitting connection-edit events', () => {
  const field = element();
  const toggle = element();
  let edits = 0;
  field.addEventListener('input', () => edits++);
  const context = vm.createContext({ document: { querySelector: id => id === '#board-address' ? field : toggle } });
  vm.runInContext(fs.readFileSync(require.resolve('../src/privacy.js'), 'utf8'), context);
  toggle.listeners.click();
  assert.equal(field.type, 'password');
  assert.equal(field.value, '10.0.0.44');
  assert.equal(toggle.attributes['aria-pressed'], 'true');
  assert.equal(toggle.textContent, 'Show IP address');
  toggle.listeners.click();
  assert.equal(field.type, 'text');
  assert.equal(toggle.attributes['aria-pressed'], 'false');
  assert.equal(toggle.textContent, 'Hide IP address');
  assert.equal(edits, 0);
});

function temperatureUi() {
  const value = element();
  const status = element();
  let now = 1000;
  let tick;
  const events = {};
  const context = vm.createContext({ document: {
    querySelector: id => id === '#temperature-value' ? value : status,
    addEventListener: (event, fn) => { events[event] = fn; },
  }, Date: { now: () => now }, setInterval: fn => { tick = fn; } });
  vm.runInContext(fs.readFileSync(require.resolve('../src/temperature.js'), 'utf8'), context);
  return { value, status, context, events, advance: ms => { now += ms; tick(); },
    update: (data, connected = true) => { context.data = data; context.connected = connected;
      vm.runInContext('updateTemperature(data, connected)', context); } };
}

test('temperature renders real sensor readings in C and F and handles zero/negative temperatures', () => {
  const ui = temperatureUi();
  for (const [c, text] of [[25, '25.0 °C / 77.0 °F'], [0, '0.0 °C / 32.0 °F'], [-10, '-10.0 °C / 14.0 °F']]) {
    ui.update({ temperatureAvailable: true, temperatureC: c });
    assert.equal(ui.value.textContent, text);
    assert.equal(ui.value.dataset.state, 'live');
  }
  ui.update({ temperatureAvailable: true, temperatureC: 26.25 });
  assert.equal(ui.value.textContent, '26.3 °C / 79.3 °F');
});

test('unavailable, malformed, missing, and disconnected readings never appear live', () => {
  const ui = temperatureUi();
  for (const data of [{}, { temperatureAvailable: false, temperatureC: 25 },
    ...[null, '25', NaN, Infinity, -127, 126].map(temperatureC => ({ temperatureAvailable: true, temperatureC }))]) {
    ui.update({ temperatureAvailable: true, temperatureC: 25 });
    ui.update(data);
    assert.equal(ui.value.textContent, '— °C / — °F');
    assert.equal(ui.value.dataset.state, 'unavailable');
  }
  ui.update({ temperatureAvailable: true, temperatureC: 25 }, false);
  assert.match(ui.status.textContent, /not connected/);
});

test('stale readings disappear and fresh readings recover automatically', () => {
  const ui = temperatureUi();
  ui.update({ temperatureAvailable: true, temperatureC: 25 });
  ui.advance(9999);
  assert.equal(ui.value.dataset.state, 'live');
  ui.advance(1);
  assert.equal(ui.value.dataset.state, 'unavailable');
  assert.match(ui.status.textContent, /stale/);
  ui.update({ temperatureAvailable: true, temperatureC: 26 });
  assert.equal(ui.value.dataset.state, 'live');
  assert.match(ui.value.textContent, /26.0/);
});

test('board polling updates temperature, clears it on failure, and recovers every 2 seconds', async () => {
  const ui = temperatureUi();
  const fields = Object.fromEntries(['#board-form', '#board-address', '#board-status'].map(id => [id, element()]));
  const tempDocument = ui.context.document;
  const priorQuery = tempDocument.querySelector;
  tempDocument.querySelector = id => fields[id] || priorQuery(id);
  let fail = false;
  let next;
  let interval;
  ui.context.GardenBoard = { check: async () => {
    if (fail) throw new Error('network');
    return { wateringAvailable: true, pumpOn: false, temperatureAvailable: true, temperatureC: 23 };
  } };
  ui.context.setTimeout = (fn, ms) => { next = fn; interval = ms; };
  ui.context.clearTimeout = () => {};
  vm.runInContext(fs.readFileSync(require.resolve('../src/board-status.js'), 'utf8'), ui.context);
  fields['#board-form'].listeners.submit({ preventDefault() {} });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(ui.value.dataset.state, 'live');
  assert.equal(interval, 2000);
  fail = true; await next();
  assert.equal(ui.value.dataset.state, 'unavailable');
  fail = false; await next();
  assert.equal(ui.value.dataset.state, 'live');
  fields['#board-address'].listeners.input();
  assert.equal(ui.value.dataset.state, 'unavailable');
});
