(function() {
  let currentType = 'ex';
  const $yaml = document.getElementById('yaml');
  const $status = document.getElementById('status');

  function setStatus(text, color) {
    $status.textContent = text;
    $status.style.color = color || '#666';
  }

  async function load(type) {
    setStatus('Loading...', '#0077cc');
    try {
      const res = await fetch('/api/config?type=' + encodeURIComponent(type));
      const text = await res.text();
      if (!res.ok) {
        setStatus('Error: ' + text, '#c0392b');
        return;
      }
      $yaml.value = text;
      currentType = type;
      setStatus('Loaded ' + type, '#3a7d44');
    } catch (e) {
      setStatus('Error: ' + e.message, '#c0392b');
    }
  }

  async function save() {
    setStatus('Saving...', '#0077cc');
    try {
      const res = await fetch('/api/config?type=' + encodeURIComponent(currentType), {
        method: 'POST',
        headers: { 'Content-Type': 'text/plain;charset=UTF-8' },
        body: $yaml.value,
      });
      const text = await res.text();
      if (!res.ok) {
        setStatus('Error: ' + text, '#c0392b');
        return;
      }
      setStatus('✓ Saved ' + currentType + ' (Restart to apply)', '#3a7d44');
    } catch (e) {
      setStatus('Error: ' + e.message, '#c0392b');
    }
  }

  async function restart() {
    if (!confirm('Restart device now?')) return;
    setStatus('Restarting...', '#c0392b');
    try {
      await fetch('/api/restart', { method: 'POST' });
    } catch (e) {
      // 切断は想定内
    }
    setTimeout(() => { setStatus('Restart issued. Page will be stale.', '#c0392b'); }, 1000);
  }

  // タブ切替
  document.querySelectorAll('.tabs button').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tabs button').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      load(btn.dataset.type);
    });
  });

  document.getElementById('save').addEventListener('click', save);
  document.getElementById('reload').addEventListener('click', () => load(currentType));
  document.getElementById('restart').addEventListener('click', restart);

  // 初回ロード
  load('ex');
})();
