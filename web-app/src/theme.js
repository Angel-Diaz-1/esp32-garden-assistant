// Apply the saved theme before styles load to avoid flashing the wrong background.
(() => {
  const storageKey = 'garden-assistant-theme';
  let theme = 'light';

  try {
    const savedTheme = localStorage.getItem(storageKey);
    if (savedTheme === 'dark' || savedTheme === 'light') theme = savedTheme;
  } catch {
    // Storage may be unavailable; the toggle still works for this visit.
  }

  document.documentElement.dataset.theme = theme;

  document.addEventListener('DOMContentLoaded', () => {
    const toggle = document.querySelector('#theme-toggle');
    toggle.setAttribute('aria-pressed', String(theme === 'dark'));

    toggle.addEventListener('click', () => {
      theme = theme === 'dark' ? 'light' : 'dark';
      document.documentElement.dataset.theme = theme;
      toggle.setAttribute('aria-pressed', String(theme === 'dark'));

      try {
        localStorage.setItem(storageKey, theme);
      } catch {
        // Keep the selected theme even when saving the preference is blocked.
      }
    });
  }, { once: true });
})();
