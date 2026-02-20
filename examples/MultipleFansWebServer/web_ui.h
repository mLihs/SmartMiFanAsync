#pragma once

// Web UI HTML with State Machine Pattern
const char WEB_UI_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>SmartMi Fan Control</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
  <style>
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
    }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
    }
    .header {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    h1 {
      color: #333;
      margin-bottom: 10px;
    }
    .status {
      display: inline-block;
      padding: 8px 16px;
      border-radius: 20px;
      font-size: 14px;
      font-weight: 500;
      margin-right: 10px;
    }
    .status.connected {
      background: #d4edda;
      color: #155724;
    }
    .status.disconnected {
      background: #f8d7da;
      color: #721c24;
    }
    .status.idle { background: #e2e3e5; color: #383d41; }
    .status.scanning { background: #fff3cd; color: #856404; }
    .status.ready { background: #d4edda; color: #155724; }
    .status.error { background: #f8d7da; color: #721c24; }
    .controls {
      background: white;
      border-radius: 15px;
      padding: 30px;
      margin-bottom: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .control-group {
      margin-bottom: 25px;
    }
    .control-group:last-child {
      margin-bottom: 0;
    }
    .control-label {
      display: block;
      font-weight: 600;
      margin-bottom: 10px;
      color: #333;
    }
    .button-group {
      display: flex;
      gap: 10px;
    }
    button {
      padding: 12px 24px;
      border: none;
      border-radius: 8px;
      font-size: 16px;
      font-weight: 500;
      cursor: pointer;
      transition: all 0.2s;
      flex: 1;
    }
    button:hover:not(:disabled) {
      transform: translateY(-2px);
      box-shadow: 0 4px 8px rgba(0,0,0,0.2);
    }
    button:active:not(:disabled) {
      transform: translateY(0);
    }
    button:disabled {
      opacity: 0.5;
      cursor: not-allowed;
    }
    .btn-primary {
      background: #667eea;
      color: white;
    }
    .btn-danger {
      background: #dc3545;
      color: white;
    }
    .btn-success {
      background: #28a745;
      color: white;
    }
    .slider-container {
      display: flex;
      align-items: center;
      gap: 15px;
    }
    input[type="range"] {
      flex: 1;
      height: 8px;
      border-radius: 5px;
      background: #ddd;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
    }
    input[type="range"]::-moz-range-thumb {
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
      border: none;
    }
    .speed-value {
      font-size: 24px;
      font-weight: bold;
      color: #667eea;
      min-width: 60px;
      text-align: center;
    }
    .fans-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
      gap: 20px;
    }
    .fan-card {
      background: white;
      border-radius: 15px;
      padding: 20px;
      box-shadow: 0 4px 6px rgba(0,0,0,0.1);
    }
    .fan-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 15px;
    }
    .fan-title {
      font-size: 18px;
      font-weight: 600;
      color: #333;
    }
    .fan-toggle {
      position: relative;
      width: 50px;
      height: 26px;
    }
    .fan-toggle input {
      opacity: 0;
      width: 0;
      height: 0;
    }
    .fan-toggle-slider {
      position: absolute;
      cursor: pointer;
      top: 0;
      left: 0;
      right: 0;
      bottom: 0;
      background-color: #ccc;
      transition: .4s;
      border-radius: 26px;
    }
    .fan-toggle-slider:before {
      position: absolute;
      content: "";
      height: 18px;
      width: 18px;
      left: 4px;
      bottom: 4px;
      background-color: white;
      transition: .4s;
      border-radius: 50%;
    }
    .fan-toggle input:checked + .fan-toggle-slider {
      background-color: #667eea;
    }
    .fan-toggle input:checked + .fan-toggle-slider:before {
      transform: translateX(24px);
    }
    .fan-info {
      margin-bottom: 10px;
    }
    .fan-info-item {
      display: flex;
      justify-content: space-between;
      padding: 5px 0;
      font-size: 14px;
    }
    .fan-info-label {
      color: #666;
    }
    .fan-info-value {
      color: #333;
      font-weight: 500;
    }
    .fan-state {
      display: inline-block;
      padding: 4px 12px;
      border-radius: 12px;
      font-size: 12px;
      font-weight: 500;
      margin-top: 10px;
    }
    .fan-state.active {
      background: #d4edda;
      color: #155724;
    }
    .fan-state.inactive {
      background: #fff3cd;
      color: #856404;
    }
    .fan-state.error {
      background: #f8d7da;
      color: #721c24;
    }
    .empty-state {
      text-align: center;
      padding: 40px;
      color: #666;
    }
    .progress-bar {
      width: 100%;
      height: 8px;
      background: #e2e3e5;
      border-radius: 4px;
      overflow: hidden;
      margin-top: 10px;
    }
    .progress-fill {
      height: 100%;
      background: #667eea;
      transition: width 0.3s;
    }
    .log-message {
      padding: 8px;
      margin: 4px 0;
      border-radius: 4px;
      font-size: 12px;
    }
    .log-message.error {
      background: #f8d7da;
      color: #721c24;
    }
    .log-message.info {
      background: #d1ecf1;
      color: #0c5460;
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🌬️ SmartMi Fan Control</h1>
      <div>
        <span class="status disconnected" id="ws-status">WebSocket: Disconnected</span>
        <span class="status idle" id="system-status">System: IDLE</span>
      </div>
    </div>
    
    <div class="controls">
      <div class="control-group">
        <label class="control-label">Actions</label>
        <div class="button-group">
          <button class="btn-primary" onclick="startScan()" id="scan-btn">Start Scan</button>
        </div>
      </div>
      
      <div class="control-group">
        <label class="control-label">Global Power Control</label>
        <div class="button-group">
          <button class="btn-success" onclick="setAllPower(true)" id="power-on-btn">Turn All ON</button>
          <button class="btn-danger" onclick="setAllPower(false)" id="power-off-btn">Turn All OFF</button>
        </div>
      </div>
      
      <div class="control-group">
        <label class="control-label">Global Speed Control</label>
        <div class="slider-container">
          <input type="range" id="speed-slider" min="1" max="100" value="30" oninput="updateSpeedDisplay(this.value)" onchange="setAllSpeed(this.value)">
          <span class="speed-value" id="speed-display">30%</span>
        </div>
      </div>
    </div>
    
    <div class="controls">
      <h2 style="margin-bottom: 20px;">Connected Fans</h2>
      <div id="progress-container" style="display: none;">
        <div class="progress-bar">
          <div class="progress-fill" id="progress-fill" style="width: 0%"></div>
        </div>
        <div id="progress-text" style="margin-top: 5px; font-size: 14px; color: #666;"></div>
      </div>
      <div class="fans-grid" id="fans-container">
        <div class="empty-state">No fans connected</div>
      </div>
    </div>
  </div>
  
  <script>
    // State Machine Pattern
    var systemState = 'IDLE';
    var currentJobId = null;
    var websocket = null;
    var fans = [];
    
    window.addEventListener('load', onload);
    
    function onload(event) {
      // Load initial state via HTTP
      loadInitialState();
      initWebSocket();
    }
    
    function loadInitialState() {
      fetch('/api/state')
        .then(response => response.json())
        .then(data => {
          console.log('Initial state loaded:', data);
          
          // Update system state
          updateSystemState(data.systemState);
          
          // Update fans
          if (data.fans) {
            updateFansDisplay(data.fans);
          }
          
          // Update settings
          if (data.settings) {
            if (data.settings.globalSpeed !== undefined) {
              document.getElementById('speed-slider').value = data.settings.globalSpeed;
              document.getElementById('speed-display').textContent = data.settings.globalSpeed + '%';
            }
          }
          
          // Update current job
          if (data.currentJobId) {
            currentJobId = data.currentJobId;
          }
        })
        .catch(error => {
          console.error('Error loading initial state:', error);
        });
    }
    
    function initWebSocket() {
      var gateway = `ws://${window.location.hostname}/ws`;
      console.log('Connecting to WebSocket:', gateway);
      websocket = new WebSocket(gateway);
      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage;
      websocket.onerror = onError;
    }
    
    function onOpen(event) {
      console.log('WebSocket connected');
      document.getElementById('ws-status').className = 'status connected';
      document.getElementById('ws-status').textContent = 'WebSocket: Connected';
    }
    
    function onClose(event) {
      console.log('WebSocket disconnected');
      document.getElementById('ws-status').className = 'status disconnected';
      document.getElementById('ws-status').textContent = 'WebSocket: Disconnected';
      setTimeout(initWebSocket, 2000);
    }
    
    function onError(event) {
      console.error('WebSocket error:', event);
    }
    
    function onMessage(event) {
      // Handle binary messages
      if (event.data instanceof ArrayBuffer) {
        const data = new Uint8Array(event.data);
        const type = data[0];
        let offset = 1;
        
        try {
          switch(type) {
            case 0x01: // MSG_STATE_CHANGED
              const stateVal = data[offset];
              const states = ['IDLE', 'SCANNING', 'READY', 'ERROR'];
              updateSystemState(states[stateVal] || 'IDLE');
              break;
              
            case 0x02: // MSG_PROGRESS
              const jobIdLen = data[offset++];
              const jobId = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + jobIdLen)));
              offset += jobIdLen;
              const statusLen = data[offset++];
              const status = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + statusLen)));
              updateProgress(jobId, status);
              break;
              
            case 0x03: // MSG_TELEMETRY
              const fanCount = data[offset++];
              const fans = [];
              
              for (let i = 0; i < fanCount; i++) {
                const index = data[offset++];
                const ip = data[offset] + '.' + data[offset+1] + '.' + data[offset+2] + '.' + data[offset+3];
                offset += 4;
                
                const did = data[offset] | (data[offset+1] << 8) | (data[offset+2] << 16) | (data[offset+3] << 24);
                offset += 4;
                
                const modelLen = data[offset++];
                const model = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + modelLen)));
                offset += modelLen;
                
                const ready = data[offset++] === 1;
                const enabled = data[offset++] === 1;
                const state = data[offset++];
                
                const stateNames = ['ACTIVE', 'INACTIVE', 'ERROR'];
                fans.push({
                  index: index,
                  ip: ip,
                  did: did,
                  model: model,
                  ready: ready,
                  enabled: enabled,
                  participationState: stateNames[state] || 'ERROR'
                });
              }
              updateFansDisplay(fans);
              break;
              
            case 0x04: // MSG_ERROR
              const errLen = data[offset++];
              const error = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + errLen)));
              showError(error);
              break;
              
            case 0x05: // MSG_LOG
              const levelLen = data[offset++];
              const level = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + levelLen)));
              offset += levelLen;
              const msgLen = data[offset++];
              const msg = String.fromCharCode.apply(null, Array.from(data.slice(offset, offset + msgLen)));
              showLog(level, msg);
              break;
              
            default:
              console.warn('Unknown message type:', type);
          }
        } catch (e) {
          console.error('Failed to parse binary message:', e);
        }
      } else {
        // Fallback for text messages (shouldn't happen with binary protocol)
        console.warn('Received text message, expected binary:', event.data);
      }
    }
    
    function updateSystemState(state) {
      systemState = state;
      var statusEl = document.getElementById('system-status');
      statusEl.textContent = 'System: ' + state;
      statusEl.className = 'status ' + state.toLowerCase();
      
      // Update button states based on system state
      var scanBtn = document.getElementById('scan-btn');
      var powerOnBtn = document.getElementById('power-on-btn');
      var powerOffBtn = document.getElementById('power-off-btn');
      
      if (state === 'SCANNING') {
        scanBtn.disabled = true;
        scanBtn.textContent = 'Scanning...';
        powerOnBtn.disabled = true;
        powerOffBtn.disabled = true;
      } else if (state === 'READY') {
        scanBtn.disabled = false;
        scanBtn.textContent = 'Start Scan';
        powerOnBtn.disabled = false;
        powerOffBtn.disabled = false;
      } else {
        scanBtn.disabled = false;
        powerOnBtn.disabled = true;
        powerOffBtn.disabled = true;
      }
    }
    
    function updateProgress(jobId, status) {
      if (jobId !== currentJobId) {
        currentJobId = jobId;
      }
      
      var container = document.getElementById('progress-container');
      var fill = document.getElementById('progress-fill');
      var text = document.getElementById('progress-text');
      
      container.style.display = 'block';
      text.textContent = status;
      
      // Simple progress calculation (can be improved)
      var progress = 0;
      if (status.includes('VALIDATING')) progress = 25;
      else if (status.includes('DISCOVERING')) progress = 50;
      else if (status.includes('COMPLETE')) progress = 100;
      else if (status.includes('ERROR')) progress = 0;
      
      fill.style.width = progress + '%';
      
      if (progress === 100 || progress === 0) {
        setTimeout(() => {
          container.style.display = 'none';
        }, 2000);
      }
    }
    
    function updateFansDisplay(fansData) {
      fans = fansData || [];
      var container = document.getElementById('fans-container');
      
      if (fans.length === 0) {
        container.innerHTML = '<div class="empty-state">No fans connected</div>';
        return;
      }
      
      container.innerHTML = fans.map((fan, index) => {
        var stateClass = fan.participationState === 'ACTIVE' ? 'active' : 
                         fan.participationState === 'INACTIVE' ? 'inactive' : 'error';
        var stateText = fan.participationState === 'ACTIVE' ? 'Active' : 
                        fan.participationState === 'INACTIVE' ? 'Inactive' : 'Error';
        
        return `
          <div class="fan-card">
            <div class="fan-header">
              <div class="fan-title">Fan ${fan.index + 1}</div>
              <label class="fan-toggle">
                <input type="checkbox" ${fan.enabled ? 'checked' : ''} 
                       onchange="setFanEnabled(${fan.index}, this.checked)">
                <span class="fan-toggle-slider"></span>
              </label>
            </div>
            <div class="fan-info">
              <div class="fan-info-item">
                <span class="fan-info-label">IP:</span>
                <span class="fan-info-value">${fan.ip}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">Model:</span>
                <span class="fan-info-value">${fan.model || 'Unknown'}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">Ready:</span>
                <span class="fan-info-value">${fan.ready ? 'Yes' : 'No'}</span>
              </div>
              <div class="fan-info-item">
                <span class="fan-info-label">DID:</span>
                <span class="fan-info-value">${fan.did || 'N/A'}</span>
              </div>
            </div>
            <div class="fan-state ${stateClass}">${stateText}</div>
          </div>
        `;
      }).join('');
    }
    
    function showError(message) {
      console.error('Error:', message);
      // Could show a toast notification here
    }
    
    function showLog(level, message) {
      console.log('[' + level + ']', message);
      // Could show in a log panel
    }
    
    // STEP 3: Action handlers - NO JSON, query params, text/plain responses
    function startScan() {
      fetch('/api/action/scan/start', {
        method: 'POST'
      })
      .then(response => response.text())
      .then(text => {
        console.log('Scan response:', text);
        if (text === 'OK') {
          // Scan started successfully, progress will come via WebSocket
        } else if (text === 'BUSY') {
          console.log('Scan already in progress');
        } else if (text.startsWith('ERR:')) {
          console.error('Scan error:', text);
        }
      })
      .catch(error => {
        console.error('Error starting scan:', error);
      });
    }
    
    function setAllPower(on) {
      fetch('/api/action/power?power=' + (on ? 'true' : 'false'), {
        method: 'POST'
      })
      .then(response => response.text())
      .then(text => {
        console.log('Power response:', text);
        if (text === 'OK') {
          // Success
        } else if (text === 'BUSY') {
          console.log('System busy');
        } else if (text.startsWith('ERR:')) {
          console.error('Power error:', text);
        }
      })
      .catch(error => {
        console.error('Error setting power:', error);
      });
    }
    
    function setAllSpeed(speed) {
      fetch('/api/action/speed?speed=' + parseInt(speed), {
        method: 'POST'
      })
      .then(response => response.text())
      .then(text => {
        console.log('Speed response:', text);
        if (text === 'OK') {
          // Save setting using new atomic API
          saveSpeedSetting(parseInt(speed));
        } else if (text === 'BUSY') {
          console.log('System busy');
        } else if (text.startsWith('ERR:')) {
          console.error('Speed error:', text);
        }
      })
      .catch(error => {
        console.error('Error setting speed:', error);
      });
    }
    
    function updateSpeedDisplay(value) {
      document.getElementById('speed-display').textContent = value + '%';
    }
    
    function setFanEnabled(fanIndex, enabled) {
      fetch('/api/action/fan-enabled?fanIndex=' + fanIndex + '&enabled=' + (enabled ? 'true' : 'false'), {
        method: 'POST'
      })
      .then(response => response.text())
      .then(text => {
        console.log('Fan enabled response:', text);
        if (text === 'OK') {
          // Save setting using new atomic API
          saveFanEnabledSetting(fanIndex, enabled);
        } else if (text === 'BUSY') {
          console.log('System busy');
        } else if (text.startsWith('ERR:')) {
          console.error('Fan enabled error:', text);
        }
      })
      .catch(error => {
        console.error('Error setting fan enabled:', error);
      });
    }
    
    // STEP 2: Atomic Settings API
    function loadSettings() {
      // Use legacy API for now (backward compatibility)
      fetch('/api/settings')
        .then(response => response.json())
        .then(data => {
          if (data.globalSpeed !== undefined) {
            document.getElementById('speed-slider').value = data.globalSpeed;
            document.getElementById('speed-display').textContent = data.globalSpeed + '%';
          }
        })
        .catch(error => {
          console.error('Error loading settings:', error);
        });
    }
    
    function saveSpeedSetting(speed) {
      // Use new atomic API
      const formData = new URLSearchParams();
      formData.append('key', 'globalSpeed');
      formData.append('value', speed.toString());
      
      fetch('/api/settings/set', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: formData
      })
      .then(response => response.text())
      .then(text => {
        if (text === 'OK') {
          console.log('Speed setting saved');
        } else {
          console.error('Error saving speed:', text);
        }
      })
      .catch(error => {
        console.error('Error saving speed setting:', error);
      });
    }
    
    function saveFanEnabledSetting(fanIndex, enabled) {
      // Use new atomic API
      const formData = new URLSearchParams();
      formData.append('key', 'fan.' + fanIndex + '.enabled');
      formData.append('value', enabled ? 'true' : 'false');
      
      fetch('/api/settings/set', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: formData
      })
      .then(response => response.text())
      .then(text => {
        if (text === 'OK') {
          console.log('Fan enabled setting saved');
        } else {
          console.error('Error saving fan enabled:', text);
        }
      })
      .catch(error => {
        console.error('Error saving fan enabled setting:', error);
      });
    }
    
    // Legacy function (for backward compatibility)
    function saveSettings() {
      var settings = {
        globalSpeed: parseInt(document.getElementById('speed-slider').value)
      };
      
      fetch('/api/settings', {
        method: 'PUT',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(settings)
      })
      .then(response => response.json())
      .then(data => {
        console.log('Settings saved:', data);
      })
      .catch(error => {
        console.error('Error saving settings:', error);
      });
    }
  </script>
</body>
</html>
)rawliteral";

