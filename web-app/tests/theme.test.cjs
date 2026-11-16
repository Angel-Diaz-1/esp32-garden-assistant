const { test } = require('node:test');
const assert = require('node:assert/strict');
const vm = require('node:vm');
const fs = require('node:fs');
const source = fs.readFileSync(require.resolve('../src/theme.js'), 'utf8');
const storageKey = 'garden-assistant-theme';

// Run the real script before the button exists, as it runs in the document head.
function view(storage = new Map()) {
  const attributes = {};
  const listeners = {};
  const toggle = {
    setAttribute(name, value) { attributes[name] = value; },
    addEventListener(name, listener) { listeners[name] = listener; },
  };
  let ready;
  const document = {
    documentElement: { dataset: {} },
    querySelector(selector) {
      assert.equal(selector, '#theme-toggle');
      return toggle;
    },
    addEventListener(name, listener) {
      assert.equal(name, 'DOMContentLoaded');
      ready = listener;
    },
  };
  const localStorage = storage instanceof Map ? {
    getItem: key => storage.get(key) ?? null,
    setItem: (key, value) => storage.set(key, value),
  } : storage;
  vm.runInNewContext(source, { document, localStorage });
  return {
    theme: () => document.documentElement.dataset.theme,
    pressed: () => attributes['aria-pressed'],
    ready: () => ready(),
    click: () => listeners.click(),
  };
}

test('defaults to light before DOM ready and ignores invalid preferences', () => {
  for (const value of [undefined, '', 'invalid']) {
    const page = view(new Map([[storageKey, value]]));
    assert.equal(page.theme(), 'light');
    page.ready();
    assert.equal(page.pressed(), 'false');
  }
});

test('restores each saved theme before DOM ready and synchronizes the button', () => {
  for (const theme of ['light', 'dark']) {
    const page = view(new Map([[storageKey, theme]]));
    assert.equal(page.theme(), theme);
    page.ready();
    assert.equal(page.pressed(), String(theme === 'dark'));
  }
});

test('toggles both ways, updates accessible state, and persists across reloads', () => {
  const storage = new Map();
  const page = view(storage);
  page.ready();
  page.click();
  assert.equal(page.theme(), 'dark');
  assert.equal(page.pressed(), 'true');
  assert.equal(storage.get(storageKey), 'dark');
  assert.equal(view(storage).theme(), 'dark');
  page.click();
  assert.equal(page.theme(), 'light');
  assert.equal(page.pressed(), 'false');
  assert.equal(storage.get(storageKey), 'light');
  assert.equal(view(storage).theme(), 'light');
});

test('blocked storage does not prevent toggling either way', () => {
  const page = view({
    getItem() { throw new Error('Storage blocked'); },
    setItem() { throw new Error('Storage blocked'); },
  });
  page.ready();
  page.click();
  assert.equal(page.theme(), 'dark');
  assert.equal(page.pressed(), 'true');
  page.click();
  assert.equal(page.theme(), 'light');
  assert.equal(page.pressed(), 'false');
});
