(function() {
  const LLM_TYPES = ['ChatGPT', 'ModuleLLM', 'ModuleLLM-FnCall', 'Gemini'];
  const TTS_TYPES = ['VOICEVOX', 'ElevenLabs', 'OpenAI', 'AquesTalk', 'ModuleLLM'];
  const STT_TYPES = ['Google', 'OpenAI Whisper', 'ModuleLLM-ASR', 'ModuleLLM-Whisper'];
  const WAKEWORD_TYPES = ['SimpleVox', 'ModuleLLM-KWS'];

  function fmtBytes(n) {
    if (n == null) return '—';
    if (n < 1024) return n + ' B';
    if (n < 1024 * 1024) return (n / 1024).toFixed(1) + ' KB';
    if (n < 1024 * 1024 * 1024) return (n / 1024 / 1024).toFixed(2) + ' MB';
    return (n / 1024 / 1024 / 1024).toFixed(2) + ' GB';
  }

  function fmtUptime(ms) {
    if (ms == null) return '—';
    const s = Math.floor(ms / 1000);
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    const sec = s % 60;
    if (h > 0) return h + 'h ' + m + 'm';
    if (m > 0) return m + 'm ' + sec + 's';
    return sec + 's';
  }

  function setText(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
  }

  function setBar(id, percent) {
    const el = document.getElementById(id);
    if (el) el.style.width = Math.max(0, Math.min(100, percent)) + '%';
  }

  let muteUpdating = false;
  let volUpdating = false;

  async function refresh() {
    try {
      const res = await fetch('/api/status');
      if (!res.ok) return;
      const d = await res.json();

      // Mute トグル（ユーザー操作中は上書きしない）
      if (!muteUpdating) {
        const t = document.getElementById('mute_toggle');
        if (t) t.checked = !!d.mute;
      }

      // Volume スライダー（ユーザー操作中は上書きしない）
      if (!volUpdating && typeof d.volume === 'number') {
        const s = document.getElementById('vol_slider');
        const v = document.getElementById('vol_value');
        if (s) s.value = d.volume;
        if (v) v.textContent = d.volume;
      }

      // System
      setText('fw_version', d.system.fw_version);
      setText('uptime', fmtUptime(d.system.uptime_ms));
      setText('chip', d.system.chip);
      setText('cpu_mhz', d.system.cpu_mhz + ' MHz');
      setText('free_heap', fmtBytes(d.system.free_heap));
      setText('min_heap', fmtBytes(d.system.min_heap));

      // Power
      setText('battery_level', d.power.battery_level + ' %');
      setBar('battery_bar', d.power.battery_level);
      setText('charging', d.power.charging ? '⚡ Yes' : 'No');
      setText('voltage', d.power.voltage_mv + ' mV');

      // Network
      setText('ssid', d.network.ssid);
      setText('ip', d.network.ip);
      setText('rssi', d.network.rssi + ' dBm');
      setText('mac', d.network.mac);

      // Storage
      const spfTotal = d.storage.spiffs_total || 0;
      const spfUsed = d.storage.spiffs_used || 0;
      setText('spiffs', fmtBytes(spfUsed) + ' / ' + fmtBytes(spfTotal));
      setBar('spiffs_bar', spfTotal > 0 ? (spfUsed / spfTotal * 100) : 0);
      const sdTotal = d.storage.sd_total || 0;
      const sdUsed = d.storage.sd_used || 0;
      setText('sd', sdTotal > 0 ? fmtBytes(sdUsed) + ' / ' + fmtBytes(sdTotal) : 'N/A');
      setBar('sd_bar', sdTotal > 0 ? (sdUsed / sdTotal * 100) : 0);

      // Config
      setText('llm_type', LLM_TYPES[d.config.llm_type] || ('Type ' + d.config.llm_type));
      setText('tts_type', TTS_TYPES[d.config.tts_type] || ('Type ' + d.config.tts_type));
      setText('tts_voice', d.config.tts_voice || '(default)');
      setText('tts_model', d.config.tts_model || '(default)');
      setText('stt_type', STT_TYPES[d.config.stt_type] || ('Type ' + d.config.stt_type));
      setText('wakeword_type', WAKEWORD_TYPES[d.config.wakeword_type] || ('Type ' + d.config.wakeword_type));

      // Role / Memory
      setText('role', d.role || '(empty)');
      setText('memory', d.memory || '(empty)');
    } catch (e) {
      console.error('Failed to fetch status:', e);
    }
  }

  // Mute トグルのハンドラ
  function setupMuteToggle() {
    const t = document.getElementById('mute_toggle');
    if (!t) return;
    t.addEventListener('change', async () => {
      muteUpdating = true;
      try {
        // ESP32WebServer は POST だと URL query を取らないので GET で送る
        await fetch('/api/mute?value=' + (t.checked ? 'true' : 'false'));
      } catch (e) {
        console.error('Failed to set mute:', e);
      } finally {
        // 1 秒間は refresh による上書きを防ぐ
        setTimeout(() => { muteUpdating = false; }, 1000);
      }
    });
  }

  // 音量スライダーのハンドラ（debounce で過剰送信を防ぐ）
  function setupVolumeSlider() {
    const s = document.getElementById('vol_slider');
    const v = document.getElementById('vol_value');
    if (!s) return;
    let timer = null;
    const send = async (val) => {
      try {
        await fetch('/api/volume?value=' + val);
      } catch (e) {
        console.error('Failed to set volume:', e);
      } finally {
        setTimeout(() => { volUpdating = false; }, 600);
      }
    };
    s.addEventListener('input', () => {
      volUpdating = true;
      if (v) v.textContent = s.value;
      if (timer) clearTimeout(timer);
      timer = setTimeout(() => send(s.value), 180);
    });
  }

  // アクションボタンのハンドラ（LED / Servo / Sound / Restart）
  function setupActions() {
    document.querySelectorAll('[data-led]').forEach(b => {
      b.addEventListener('click', async () => {
        const [r, g, b2] = b.dataset.led.split(',').map(s => parseInt(s, 10));
        try { await fetch(`/api/led?r=${r}&g=${g}&b=${b2}`); } catch (e) { console.error(e); }
      });
    });
    document.querySelectorAll('[data-servo]').forEach(b => {
      b.addEventListener('click', async () => {
        const [x, y] = b.dataset.servo.split(',').map(s => parseInt(s, 10));
        try { await fetch(`/api/servo-test?x=${x}&y=${y}`); } catch (e) { console.error(e); }
      });
    });
    document.querySelectorAll('[data-sound]').forEach(b => {
      b.addEventListener('click', async () => {
        try { await fetch(`/api/sound-test?type=${b.dataset.sound}`); } catch (e) { console.error(e); }
      });
    });
    document.querySelectorAll('[data-face]').forEach(b => {
      b.addEventListener('click', async () => {
        try { await fetch(`/face?expression=${b.dataset.face}`); } catch (e) { console.error(e); }
      });
    });
    const restart = document.getElementById('act_restart');
    if (restart) restart.addEventListener('click', async () => {
      if (!confirm('再起動するッピ？\n（接続が一時的に切れるッピ）')) return;
      try {
        await fetch('/api/restart', { method: 'POST' });
        alert('再起動コマンド送信したッピ。30 秒ほど待ってからリロードッピ');
      } catch (e) { console.error(e); }
    });
  }

  // Wakeword カード
  async function loadWakeword() {
    try {
      const res = await fetch('/api/wakeword');
      if (!res.ok) return;
      const d = await res.json();
      const sel = document.getElementById('ww_type');
      const kw  = document.getElementById('ww_keyword');
      if (sel) sel.value = String(d.type);
      if (kw)  kw.value  = d.keyword || '';
    } catch (e) {
      console.error('Failed to load wakeword:', e);
    }
  }

  function setupWakeword() {
    const btn = document.getElementById('ww_save');
    if (!btn) return;
    btn.addEventListener('click', async () => {
      const type = document.getElementById('ww_type').value;
      const keyword = document.getElementById('ww_keyword').value.trim();
      const status = document.getElementById('ww_status');
      if (!keyword) {
        status.className = 'ww-status err';
        status.textContent = 'Keyword を入力してください';
        return;
      }
      if (!confirm('Wakeword を変更して再起動するッピ？\n( type=' + type + ', keyword="' + keyword + '" )')) return;
      btn.disabled = true;
      status.className = 'ww-status';
      status.textContent = '保存中...';
      try {
        const url = '/api/wakeword?type=' + encodeURIComponent(type) + '&keyword=' + encodeURIComponent(keyword);
        const res = await fetch(url, { method: 'POST' });
        if (!res.ok) throw new Error('HTTP ' + res.status);
        status.className = 'ww-status ok';
        status.textContent = '保存完了。再起動します...';
        await fetch('/api/restart', { method: 'POST' });
        setTimeout(() => {
          status.textContent = '再起動コマンド送信済み。30 秒後にリロード';
          btn.disabled = false;
        }, 1000);
      } catch (e) {
        status.className = 'ww-status err';
        status.textContent = 'エラー: ' + e.message;
        btn.disabled = false;
      }
    });
    loadWakeword();
  }

  // Personality preset カード
  function detectActivePreset(presets, currentRole) {
    if (!currentRole) return null;
    for (const p of presets) {
      if (p.role && currentRole.indexOf(p.role.substring(0, 30)) >= 0) return p.id;
    }
    return null;
  }

  async function loadPersonality() {
    try {
      const res = await fetch('/api/personality');
      if (!res.ok) return;
      const d = await res.json();
      const grid = document.getElementById('pp_grid');
      if (!grid) return;
      grid.innerHTML = '';
      d.presets.forEach(p => {
        const btn = document.createElement('button');
        btn.className = 'pp-preset';
        btn.title = p.description;
        btn.dataset.id = p.id;
        btn.innerHTML = '<span class="pp-emoji">' + p.emoji + '</span>' + p.name;
        btn.addEventListener('click', () => applyPreset(p));
        grid.appendChild(btn);
      });
      const cur = document.getElementById('pp_current');
      if (cur) cur.textContent = d.current_role ? '現在: ' + (d.current_role.substring(0, 80) + (d.current_role.length > 80 ? '…' : '')) : '(未設定)';
    } catch (e) {
      console.error('Failed to load personality:', e);
    }
  }

  async function applyPreset(preset) {
    const cur = document.getElementById('pp_current');
    if (cur) cur.textContent = '適用中: ' + preset.name + '...';
    try {
      const res = await fetch('/api/personality?id=' + encodeURIComponent(preset.id), { method: 'POST' });
      if (!res.ok) throw new Error('HTTP ' + res.status);
      const d = await res.json();
      if (cur) cur.textContent = '現在: ' + (d.current_role.substring(0, 80) + (d.current_role.length > 80 ? '…' : ''));
      // ハイライト切替
      document.querySelectorAll('.pp-preset').forEach(b => {
        b.classList.toggle('active', b.dataset.id === preset.id);
      });
    } catch (e) {
      console.error('Failed to apply preset:', e);
      if (cur) cur.textContent = 'エラー: ' + e.message;
    }
  }

  setupMuteToggle();
  setupVolumeSlider();
  setupActions();
  setupWakeword();
  loadPersonality();
  refresh();
  setInterval(refresh, 5000);
})();
