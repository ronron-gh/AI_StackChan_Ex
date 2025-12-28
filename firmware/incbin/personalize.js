document.addEventListener('DOMContentLoaded', function () {
  // Role section
  const form = document.getElementById('roleForm');
  const roleInput = document.getElementById('roleInput');
  // Memory section
  const memoryInput = document.getElementById('memoryInput');
  const clearMemoryButton = document.getElementById('clearMemoryButton');
  // Status/Error
  const statusDiv = document.getElementById('status');
  const errorDiv = document.getElementById('error');

  function showStatus(msg) {
    statusDiv.textContent = msg;
    errorDiv.textContent = '';
  }
  function showError(msg) {
    errorDiv.textContent = msg;
    statusDiv.textContent = '';
  }

  // --- Role logic ---
  // Fetch current role on page load (plain text)
  fetch('/role_get', {
    method: 'POST'
  })
    .then(async (res) => {
      if (!res.ok) throw new Error('Failed to get current role');
      const text = await res.text();
      roleInput.value = text;
    })
    .catch((err) => {
      showError('Error loading current role: ' + err.message);
    });

  // Handle form submit (send plain text)
  form.addEventListener('submit', function (e) {
    e.preventDefault();
    const role = roleInput.value;
    fetch('/role_set', {
      method: 'POST',
      headers: { 'Content-Type': 'text/plain' },
      body: role
    })
      .then(async (res) => {
        if (!res.ok) {
          let msg = 'Failed to set role';
          try {
            const errText = await res.text();
            if (errText) msg += ': ' + errText;
          } catch {}
          throw new Error(msg);
        }
        showStatus('Role updated successfully.');
      })
      .catch((err) => {
        showError('Error setting role: ' + err.message);
      });
  });

  // --- Memory logic ---
  function loadMemory() {
    fetch('/memory_get', {
      method: 'POST'
    })
      .then(async (res) => {
        if (!res.ok) throw new Error('Failed to get memory');
        const text = await res.text();
        memoryInput.value = text;
      })
      .catch((err) => {
        memoryInput.value = '';
        showError('Error loading memory: ' + err.message);
      });
  }

  clearMemoryButton.addEventListener('click', function () {
    if (!window.confirm('Are you sure you want to clear the memory? This action cannot be undone.')) {
      return;
    }
    fetch('/memory_clear', {
      method: 'POST'
    })
      .then(async (res) => {
        if (!res.ok) {
          let msg = 'Failed to clear memory';
          try {
            const errText = await res.text();
            if (errText) msg += ': ' + errText;
          } catch {}
          throw new Error(msg);
        }
        showStatus('Memory cleared successfully.');
        loadMemory();
      })
      .catch((err) => {
        showError('Error clearing memory: ' + err.message);
      });
  });

  // Initial load
  loadMemory();
});
