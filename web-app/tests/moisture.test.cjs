const { test } = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

function ui() {
  const elements = Object.fromEntries(['#moisture-value', '#moisture-status', '#board-address', '#board-form', '#board-status']
    .map(id => [id, { value: '10.0.0.44', dataset: {}, listeners: {}, textContent: '',
      addEventListener(event, fn) { this.listeners[event] = fn; } }]));
  let now = 1000;
  let tick;
  let poll;
  let data = { moistureAvailable: true, moistureRaw: 2000, moistureCalibrated: false, moisturePercent: null };
  let fail = false;
  const context = vm.createContext({ document: { querySelector: id => elements[id], addEventListener() {} },
    Date: { now: () => now }, setInterval: fn => { tick = fn; }, setTimeout: fn => { poll = fn; }, clearTimeout() {},
    GardenBoard: { check: async () => { if (fail) throw new Error('offline'); return data; } },
  });
  vm.runInContext(fs.readFileSync(require.resolve('../src/moisture.js'), 'utf8'), context);
  return { elements, context, value: elements['#moisture-value'], status: elements['#moisture-status'],
    advance: ms => { now += ms; tick(); }, poll: () => poll(), fail: value => { fail = value; },
    setData: value => { data = value; }, update: (value, connected = true) => {
      context.data = value; context.connected = connected;
      vm.runInContext('updateMoisture(data, connected)', context);
    } };
}

test('uncalibrated sensor shows raw ADC data instead of an invented percentage', () => {
  const view = ui();
  view.update({ moistureAvailable: true, moistureRaw: 2048, moistureCalibrated: false, moisturePercent: null });
  assert.equal(view.value.textContent, '2048 raw');
  assert.equal(view.value.dataset.state, 'live');
  assert.match(view.status.textContent, /calibration needed/);
});

test('calibrated sensor displays relative percentage including dry/wet endpoints', () => {
  const view = ui();
  for (const value of [0, 38.25, 100]) {
    view.update({ moistureAvailable: true, moistureRaw: 2100, moistureCalibrated: true, moisturePercent: value });
    assert.equal(view.value.textContent, `${value.toFixed(1)}%`);
    assert.match(view.status.textContent, /relative moisture/);
  }
});

test('invalid or uncalibrated percentages fall back to raw readings', () => {
  const view = ui();
  for (const percent of [null, '50', NaN, Infinity, -1, 101]) {
    view.update({ moistureAvailable: true, moistureRaw: 2000, moistureCalibrated: true, moisturePercent: percent });
    assert.equal(view.value.textContent, '2000 raw');
  }
});

test('missing firmware, clipping, malformed ADC data, and disconnection clear readings', () => {
  const view = ui();
  view.update({});
  assert.match(view.status.textContent, /Upload/);
  for (const raw of [null, '2000', 0, 4095, -1, 5000, 1.5, NaN]) {
    view.update({ moistureAvailable: true, moistureRaw: raw });
    assert.equal(view.value.dataset.state, 'unavailable');
    assert.equal(view.value.textContent, '—');
  }
  view.update({ moistureAvailable: false, moistureRaw: 2000 });
  assert.equal(view.value.dataset.state, 'unavailable');
  view.update({ moistureAvailable: true, moistureRaw: 2000 }, false);
  assert.match(view.status.textContent, /not connected/);
});

test('stale moisture clears after ten seconds and fresh samples restore it', () => {
  const view = ui();
  view.update({ moistureAvailable: true, moistureRaw: 2000 });
  view.advance(9999);
  assert.equal(view.value.dataset.state, 'live');
  view.advance(1);
  assert.match(view.status.textContent, /stale/);
  view.update({ moistureAvailable: true, moistureRaw: 1800 });
  assert.equal(view.value.textContent, '1800 raw');
});

test('actual board polling updates moisture even when temperature is absent, and recovers after failure', async () => {
  const view = ui();
  vm.runInContext(fs.readFileSync(require.resolve('../src/board-status.js'), 'utf8'), view.context);
  view.elements['#board-form'].listeners.submit({ preventDefault() {} });
  await new Promise(resolve => setImmediate(resolve));
  assert.equal(view.value.textContent, '2000 raw');
  view.fail(true); await view.poll();
  assert.equal(view.value.dataset.state, 'unavailable');
  view.fail(false); await view.poll();
  assert.equal(view.value.dataset.state, 'live');
  view.elements['#board-address'].listeners.input();
  assert.equal(view.value.dataset.state, 'unavailable');
});
