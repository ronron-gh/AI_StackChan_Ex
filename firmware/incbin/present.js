(function() {
  const PRESETS = [
    'おはようございます！',
    'こんにちは、よろしくお願いします。',
    'ありがとうございました！',
    'お疲れさまでした。',
    '次のスライドお願いします。',
    'こちらの図をご覧ください。',
    '少々お待ちください。',
    'ご質問はありますか？',
    'それでは始めます。',
    'まとめると、こうなります。',
  ];
  const MAX_HISTORY = 20;
  const HISTORY_KEY = 'aistackchan_present_history';

  function loadHistory() {
    try {
      return JSON.parse(localStorage.getItem(HISTORY_KEY) || '[]');
    } catch (e) { return []; }
  }
  function saveHistory(arr) {
    localStorage.setItem(HISTORY_KEY, JSON.stringify(arr.slice(0, MAX_HISTORY)));
  }
  function pushHistory(text) {
    const arr = loadHistory();
    arr.unshift({ text, t: Date.now() });
    saveHistory(arr);
    renderHistory();
  }
  function fmtTime(ms) {
    const d = new Date(ms);
    const hh = String(d.getHours()).padStart(2, '0');
    const mm = String(d.getMinutes()).padStart(2, '0');
    return hh + ':' + mm;
  }

  function renderPresets() {
    const grid = document.getElementById('preset_grid');
    grid.innerHTML = '';
    PRESETS.forEach(p => {
      const btn = document.createElement('button');
      btn.className = 'preset-btn';
      btn.textContent = p;
      btn.addEventListener('click', () => {
        const ta = document.getElementById('text');
        ta.value = p;
        ta.focus();
      });
      grid.appendChild(btn);
    });
  }

  function renderHistory() {
    const ul = document.getElementById('history');
    const arr = loadHistory();
    if (arr.length === 0) {
      ul.innerHTML = '<li class="history-empty">まだ発話履歴はありません</li>';
      return;
    }
    ul.innerHTML = '';
    arr.forEach(item => {
      const li = document.createElement('li');
      const span = document.createElement('span');
      span.textContent = item.text.length > 50 ? item.text.substring(0, 50) + '…' : item.text;
      const time = document.createElement('span');
      time.className = 'time';
      time.textContent = fmtTime(item.t);
      li.appendChild(span);
      li.appendChild(time);
      li.addEventListener('click', () => {
        document.getElementById('text').value = item.text;
      });
      ul.appendChild(li);
    });
  }

  async function speak(text) {
    const status = document.getElementById('status');
    const btn = document.getElementById('btn_speak');
    if (!text || !text.trim()) {
      status.className = 'status err';
      status.textContent = 'テキストを入力してください';
      return;
    }
    btn.disabled = true;
    status.className = 'status';
    status.textContent = '送信中...';
    try {
      const url = '/speech?say=' + encodeURIComponent(text);
      const res = await fetch(url);
      if (!res.ok) throw new Error('HTTP ' + res.status);
      status.className = 'status ok';
      status.textContent = '✓ 発話リクエスト送信完了';
      pushHistory(text);
    } catch (e) {
      status.className = 'status err';
      status.textContent = 'エラー: ' + e.message;
    } finally {
      btn.disabled = false;
    }
  }

  async function toggleMute() {
    const status = document.getElementById('status');
    try {
      // 現在の状態を取得 → 反転して送信
      const cur = await fetch('/api/mute').then(r => r.json());
      const next = !cur.mute;
      const res = await fetch('/api/mute?value=' + (next ? 'true' : 'false'));
      if (!res.ok) throw new Error('HTTP ' + res.status);
      status.className = 'status ok';
      status.textContent = next ? '🔇 Mute ON（発話されません）' : '🔊 Mute OFF';
    } catch (e) {
      status.className = 'status err';
      status.textContent = 'Mute エラー: ' + e.message;
    }
  }

  document.getElementById('btn_speak').addEventListener('click', () => {
    speak(document.getElementById('text').value);
  });
  document.getElementById('btn_clear').addEventListener('click', () => {
    document.getElementById('text').value = '';
    document.getElementById('status').textContent = '';
    document.getElementById('text').focus();
  });
  document.getElementById('btn_mute_toggle').addEventListener('click', toggleMute);

  // Cmd/Ctrl + Enter で送信
  document.getElementById('text').addEventListener('keydown', (e) => {
    if ((e.metaKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      speak(document.getElementById('text').value);
    }
  });

  renderPresets();
  renderHistory();
})();
