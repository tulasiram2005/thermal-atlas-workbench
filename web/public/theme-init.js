try {
  const choice = localStorage.getItem('thermal-theme');
  document.documentElement.dataset.theme = choice || (matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark');
} catch { document.documentElement.dataset.theme = 'dark'; }
