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

  setupMuteToggle();
  refresh();
  setInterval(refresh, 5000);
})();
