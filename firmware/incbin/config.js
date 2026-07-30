document.addEventListener('DOMContentLoaded', function () {
  const form = document.getElementById('configForm');
  const reloadButton = document.getElementById('reloadButton');
  const restartButton = document.getElementById('restartButton');
  const statusDiv = document.getElementById('status');
  const errorDiv = document.getElementById('error');
  const tabs = Array.from(document.querySelectorAll('[role="tab"]'));
  const mcpFields = Array.from(document.querySelectorAll('[data-mcp-server]')).map(function (server) {
    return {
      name: server.querySelector('[data-mcp-name]'),
      disabled: server.querySelector('[data-mcp-disabled]'),
      url: server.querySelector('[data-mcp-url]'),
      port: server.querySelector('[data-mcp-port]')
    };
  });

  const fields = {
    wifiSsid: document.getElementById('wifiSsid'),
    wifiPassword: document.getElementById('wifiPassword'),
    aiService: document.getElementById('aiService'),
    enableMemory: document.getElementById('enableMemory'),
    aiApiKey: document.getElementById('aiApiKey'),
    servoType: document.getElementById('servoType'),
    takaoBase: document.getElementById('takaoBase'),
    servoPinPreset: document.getElementById('servoPinPreset'),
    pinX: document.getElementById('pinX'),
    pinY: document.getElementById('pinY'),
    offsetX: document.getElementById('offsetX'),
    offsetY: document.getElementById('offsetY'),
    centerX: document.getElementById('centerX'),
    centerY: document.getElementById('centerY'),
    lowerX: document.getElementById('lowerX'),
    lowerY: document.getElementById('lowerY'),
    upperX: document.getElementById('upperX'),
    upperY: document.getElementById('upperY')
  };

  const servoPresets = {
    PWM: {
      pins: [
        ['Core2 PortA', 33, 32],
        ['Core2 PortB', 36, 26],
        ['Core2 PortC', 13, 14],
        ['CoreS3 PortA', 2, 1],
        ['CoreS3 PortB', 9, 8],
        ['CoreS3 PortC', 17, 18]
      ],
      offset: [0, 0],
      center: [90, 90],
      lower: [0, 60],
      upper: [180, 90],
      takaoBase: false
    },
    SCS: {
      pins: [
        ['Core2 PortA', 33, 32],
        ['Core2 PortB', 36, 26],
        ['Core2 PortC', 13, 14],
        ['CoreS3 PortA', 2, 1],
        ['CoreS3 PortB', 9, 8],
        ['CoreS3 PortC', 17, 18]
      ],
      offset: [0, 0],
      center: [150, 150],
      lower: [0, 120],
      upper: [300, 150],
      takaoBase: false
    },
    DYN_XL330: {
      pins: [
        ['Core2 PortA', 33, 32],
        ['Core2 PortB', 36, 26],
        ['Core2 PortC', 13, 14],
        ['CoreS3 PortA', 2, 1],
        ['CoreS3 PortB', 9, 8],
        ['CoreS3 PortC', 17, 18]
      ],
      offset: [0, 0],
      center: [180, 270],
      lower: [0, 220],
      upper: [360, 270],
      takaoBase: false
    },
    RT_DYN_XL330: {
      pins: [['Default', 6, 7]],
      offset: [0, 0],
      center: [180, 5],
      lower: [90, -5],
      upper: [270, 15],
      takaoBase: false
    },
    M5_SCS: {
      pins: [['Default', 7, 6]],
      offset: [0, 0],
      center: [150, 85],
      lower: [0, 0],
      upper: [300, 90],
      takaoBase: false
    }
  };

  let currentServoType = fields.servoType.value;

  function activateTab(tab, moveFocus) {
    tabs.forEach(function (item) {
      const selected = item === tab;
      item.setAttribute('aria-selected', selected ? 'true' : 'false');
      item.tabIndex = selected ? 0 : -1;
      document.getElementById(item.getAttribute('aria-controls')).hidden = !selected;
    });
    if (moveFocus) {
      tab.focus();
    }
  }

  tabs.forEach(function (tab, index) {
    tab.addEventListener('click', function () {
      activateTab(tab, false);
    });
    tab.addEventListener('keydown', function (event) {
      let nextIndex = index;
      if (event.key === 'ArrowRight') {
        nextIndex = (index + 1) % tabs.length;
      } else if (event.key === 'ArrowLeft') {
        nextIndex = (index - 1 + tabs.length) % tabs.length;
      } else if (event.key === 'Home') {
        nextIndex = 0;
      } else if (event.key === 'End') {
        nextIndex = tabs.length - 1;
      } else {
        return;
      }
      event.preventDefault();
      activateTab(tabs[nextIndex], true);
    });
  });

  function showStatus(message) {
    statusDiv.textContent = message;
    errorDiv.textContent = '';
  }

  function showError(message) {
    errorDiv.textContent = message;
    statusDiv.textContent = '';
  }

  async function readTextResponse(res) {
    const text = await res.text();
    if (!res.ok) {
      throw new Error(text || ('HTTP ' + res.status));
    }
    return text;
  }

  function numberValue(input, fallback) {
    const value = Number(input.value);
    return Number.isFinite(value) ? value : fallback;
  }

  function setNumber(input, value) {
    input.value = Number.isFinite(Number(value)) ? String(value) : '0';
  }

  function setBoolSelect(select, value) {
    select.value = value ? 'true' : 'false';
  }

  function boolSelectValue(select) {
    return select.value === 'true';
  }

  function updatePinPresetOptions(type, selectedX, selectedY) {
    const preset = servoPresets[type] || servoPresets.PWM;
    fields.servoPinPreset.innerHTML = '';
    preset.pins.forEach(function (pinSet, index) {
      const option = document.createElement('option');
      option.value = String(index);
      option.textContent = pinSet[0] + ' x:' + pinSet[1] + ' y:' + pinSet[2];
      fields.servoPinPreset.appendChild(option);
      if (pinSet[1] === selectedX && pinSet[2] === selectedY) {
        fields.servoPinPreset.value = String(index);
      }
    });
    const match = preset.pins.some(function (pinSet) {
      return pinSet[1] === selectedX && pinSet[2] === selectedY;
    });
    if (!match) {
      const custom = document.createElement('option');
      custom.value = 'custom';
      custom.textContent = 'Custom';
      fields.servoPinPreset.appendChild(custom);
      fields.servoPinPreset.value = 'custom';
    }
  }

  function applyServoPreset(type) {
    const preset = servoPresets[type] || servoPresets.PWM;
    const pins = preset.pins[0];
    fields.servoType.value = type;
    updatePinPresetOptions(type, pins[1], pins[2]);
    fields.servoPinPreset.value = '0';
    setNumber(fields.pinX, pins[1]);
    setNumber(fields.pinY, pins[2]);
    setNumber(fields.offsetX, preset.offset[0]);
    setNumber(fields.offsetY, preset.offset[1]);
    setNumber(fields.centerX, preset.center[0]);
    setNumber(fields.centerY, preset.center[1]);
    setNumber(fields.lowerX, preset.lower[0]);
    setNumber(fields.lowerY, preset.lower[1]);
    setNumber(fields.upperX, preset.upper[0]);
    setNumber(fields.upperY, preset.upper[1]);
    setBoolSelect(fields.takaoBase, preset.takaoBase);
    currentServoType = type;
  }

  function applyConfig(config) {
    const sec = config.sec || {};
    const wifi = sec.wifi || {};
    const apikey = sec.apikey || {};
    const basic = config.basic || {};
    const servo = basic.servo || {};
    const ex = config.ex || {};
    const llm = ex.llm || {};

    fields.wifiSsid.value = wifi.ssid || '';
    fields.wifiPassword.value = wifi.password || '';
    fields.aiApiKey.value = apikey.aiservice || '';
    fields.aiService.value = String(llm.type === 3 ? 3 : 0);
    setBoolSelect(fields.enableMemory, !!llm.enableMemory);
    const mcpServers = Array.isArray(llm.mcpServers) ? llm.mcpServers : [];
    mcpFields.forEach(function (mcp, index) {
      const server = mcpServers[index] || {};
      mcp.name.value = server.name || '';
      setBoolSelect(mcp.disabled, !!server.disabled);
      mcp.url.value = server.url || '';
      mcp.port.value = Number.isInteger(Number(server.port)) && Number(server.port) > 0
        ? String(server.port)
        : '';
    });

    const type = basic.servo_type || 'PWM';
    fields.servoType.value = servoPresets[type] ? type : 'PWM';
    currentServoType = fields.servoType.value;

    const pin = servo.pin || {};
    const offset = servo.offset || {};
    const center = servo.center || {};
    const lower = servo.lower_limit || {};
    const upper = servo.upper_limit || {};
    const pinX = Number.isFinite(Number(pin.x)) ? Number(pin.x) : 33;
    const pinY = Number.isFinite(Number(pin.y)) ? Number(pin.y) : 32;
    updatePinPresetOptions(currentServoType, pinX, pinY);
    setNumber(fields.pinX, pinX);
    setNumber(fields.pinY, pinY);
    setNumber(fields.offsetX, Number.isFinite(Number(offset.x)) ? Number(offset.x) : 0);
    setNumber(fields.offsetY, Number.isFinite(Number(offset.y)) ? Number(offset.y) : 0);
    setNumber(fields.centerX, Number.isFinite(Number(center.x)) ? Number(center.x) : 90);
    setNumber(fields.centerY, Number.isFinite(Number(center.y)) ? Number(center.y) : 90);
    setNumber(fields.lowerX, Number.isFinite(Number(lower.x)) ? Number(lower.x) : 0);
    setNumber(fields.lowerY, Number.isFinite(Number(lower.y)) ? Number(lower.y) : 60);
    setNumber(fields.upperX, Number.isFinite(Number(upper.x)) ? Number(upper.x) : 180);
    setNumber(fields.upperY, Number.isFinite(Number(upper.y)) ? Number(upper.y) : 90);
    setBoolSelect(fields.takaoBase, !!basic.takao_base);
  }

  function collectConfig() {
    const mcpServers = [];
    mcpFields.forEach(function (mcp, index) {
      const name = mcp.name.value.trim();
      const url = mcp.url.value.trim();
      const portText = mcp.port.value.trim();
      if (name === '' && url === '' && portText === '') {
        return;
      }
      const port = Number(portText);
      if (name === '' || url === '' || portText === '') {
        throw new Error('MCP Server ' + (index + 1) + ': Name, URL / Host, and Port are required.');
      }
      if (!Number.isInteger(port) || port < 1 || port > 65535) {
        throw new Error('MCP Server ' + (index + 1) + ': Port must be an integer from 1 to 65535.');
      }
      mcpServers.push({
        name: name,
        disabled: boolSelectValue(mcp.disabled),
        url: url,
        port: port
      });
    });
    return {
      sec: {
        wifi: {
          ssid: fields.wifiSsid.value,
          password: fields.wifiPassword.value
        },
        apikey: {
          aiservice: fields.aiApiKey.value,
          tts: '',
          stt: ''
        }
      },
      basic: {
        servo_type: fields.servoType.value,
        takao_base: boolSelectValue(fields.takaoBase),
        servo: {
          pin: {
            x: numberValue(fields.pinX, 33),
            y: numberValue(fields.pinY, 32)
          },
          offset: {
            x: numberValue(fields.offsetX, 0),
            y: numberValue(fields.offsetY, 0)
          },
          center: {
            x: numberValue(fields.centerX, 90),
            y: numberValue(fields.centerY, 90)
          },
          lower_limit: {
            x: numberValue(fields.lowerX, 0),
            y: numberValue(fields.lowerY, 60)
          },
          upper_limit: {
            x: numberValue(fields.upperX, 180),
            y: numberValue(fields.upperY, 90)
          }
        }
      },
      ex: {
        llm: {
          type: Number(fields.aiService.value),
          enableMemory: boolSelectValue(fields.enableMemory),
          mcpServers: mcpServers
        }
      }
    };
  }

  async function loadConfig() {
    try {
      const res = await fetch('/config', { cache: 'no-store' });
      const text = await readTextResponse(res);
      applyConfig(JSON.parse(text));
      showStatus('Loaded.');
    } catch (err) {
      showError('Load failed: ' + err.message);
    }
  }

  document.querySelectorAll('[data-toggle-secret]').forEach(function (button) {
    button.addEventListener('click', function () {
      const input = document.getElementById(button.getAttribute('data-toggle-secret'));
      if (input.type === 'password') {
        input.type = 'text';
        button.textContent = 'Hide';
      } else {
        input.type = 'password';
        button.textContent = 'Show';
      }
    });
  });

  fields.servoType.addEventListener('change', function () {
    const nextType = fields.servoType.value;
    if (window.confirm('Apply default values for the selected servo type? Current servo values will be overwritten.')) {
      applyServoPreset(nextType);
    } else {
      fields.servoType.value = currentServoType;
    }
  });

  fields.servoPinPreset.addEventListener('change', function () {
    const preset = servoPresets[fields.servoType.value] || servoPresets.PWM;
    const selected = fields.servoPinPreset.value;
    if (selected === 'custom') {
      return;
    }
    const pinSet = preset.pins[Number(selected)];
    if (pinSet) {
      setNumber(fields.pinX, pinSet[1]);
      setNumber(fields.pinY, pinSet[2]);
    }
  });

  form.addEventListener('submit', async function (event) {
    event.preventDefault();
    try {
      const res = await fetch('/config', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json;charset=UTF-8' },
        body: JSON.stringify(collectConfig())
      });
      await readTextResponse(res);
      showStatus('Saved. Remove the SD card before restarting to use the saved SPIFFS settings.');
    } catch (err) {
      showError('Save failed: ' + err.message);
    }
  });

  reloadButton.addEventListener('click', loadConfig);

  restartButton.addEventListener('click', async function () {
    if (!window.confirm('Restart now?')) {
      return;
    }
    try {
      const res = await fetch('/config/restart', { method: 'POST' });
      await readTextResponse(res);
      showStatus('Restarting...');
    } catch (err) {
      showError('Restart request failed: ' + err.message);
    }
  });

  applyServoPreset('PWM');
  loadConfig();
});
