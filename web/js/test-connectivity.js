// test-connectivity.js — WiFi & Bluetooth hardware tests
// Uses polling + callback to wait for C++ host WMI response.

(function() {
  'use strict';

  // Shared helper: wait for host data to arrive
  function waitForHostData(key, timeoutMs, callback) {
    var start = Date.now();

    // Check immediately
    var existing = window[key];
    if (existing && existing.length > 0) {
      callback(existing);
      return;
    }

    // Poll
    var interval = setInterval(function() {
      var data = window[key];
      if (data && data.length > 0) {
        clearInterval(interval);
        callback(data);
        return;
      }
      if (Date.now() - start > timeoutMs) {
        clearInterval(interval);
        callback(null); // timeout
      }
    }, 150);
  }

  // ===== WiFi Test =====
  function WifiTest() { HardwareTest.call(this, 'Wifi', 'WiFi', { timeoutMs: 20000 }); }
  WifiTest.prototype = Object.create(HardwareTest.prototype);
  WifiTest.prototype.constructor = WifiTest;

  WifiTest.prototype.onStart = function() {
    var self = this;

    // Show scanning state
    showOverlay(
      '<div class="overlay-title"><i class="fas fa-wifi"></i>WiFi 测试</div>' +
      '<div class="scanning-state">' +
      '<div class="scan-spinner"></div>' +
      '<div style="font-size:12px;color:#5A6F87;margin-top:8px;">正在扫描 WiFi 适配器...</div>' +
      '</div>' +
      '<div class="overlay-actions">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Wifi\'].stop(false)">跳过</button>' +
      '</div>'
    );

    // Request scan from C++
    postToHost('scan_wifi');

    // Wait for data
    waitForHostData('_hostWifiData', 8000, function(adapters) {
      if (!adapters || adapters.length === 0) {
        // Fallback: check hardware info
        adapters = [];
        if (window.g_data && window.g_data.lines) {
          var lines = window.g_data.lines;
          for (var i = 0; i < lines.length; i++) {
            var v = (lines[i].value || '').toLowerCase();
            if (v.indexOf('wireless') !== -1 || v.indexOf('wi-fi') !== -1 || v.indexOf('wlan') !== -1) {
              adapters.push({ name: lines[i].value, description: '' });
            }
          }
        }
      }

      self._adapters = adapters;
      self._renderUI(adapters);
    });
  };

  WifiTest.prototype._renderUI = function(adapters) {
    var hasAdapter = adapters && adapters.length > 0;

    var html = '<div class="overlay-title"><i class="fas fa-wifi"></i>WiFi 测试</div>';

    if (hasAdapter) {
      html += '<div class="result-ok">' +
        '<div class="result-icon-wrap success-icon"><i class="fas fa-check-circle"></i></div>' +
        '<div class="result-text">检测到 <b>' + adapters.length + '</b> 个 WiFi 适配器</div>' +
        '</div>';
      html += '<div class="adapter-list">';
      for (var a = 0; a < adapters.length; a++) {
        var name = adapters[a].name || adapters[a].description || '未知适配器';
        var desc = adapters[a].description && adapters[a].description !== name ? adapters[a].description : '';
        html += '<div class="adapter-item">' +
          '<i class="fas fa-wifi adapter-icon"></i>' +
          '<div><div class="adapter-name">' + escapeHtml(name) + '</div>' +
          (desc ? '<div class="adapter-desc">' + escapeHtml(desc) + '</div>' : '') +
          '</div></div>';
      }
      html += '</div>';
    } else {
      html += '<div class="result-fail">' +
        '<div class="result-icon-wrap fail-icon"><i class="fas fa-exclamation-circle"></i></div>' +
        '<div class="result-text">未检测到 WiFi 适配器</div>' +
        '<div class="result-hint">可能原因: 驱动未安装 · 硬件开关关闭 · 设备不支持 WiFi</div>' +
        '</div>';
    }

    html += '<div class="overlay-actions">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Wifi\'].stop(false)">' +
      (hasAdapter ? '标记失败' : '未检测到') + '</button>' +
      '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Wifi\'].stop(true)">WiFi 正常</button>' +
      '</div>';

    // Update overlay content
    var card = document.getElementById('overlayCard');
    if (card) card.innerHTML = html;
  };

  WifiTest.prototype.onStop = function(pass) {
    hideOverlay();
    var count = (this._adapters && this._adapters.length) ? this._adapters.length : 0;
    this.resultDetail = pass ? (count > 0 ? count + ' 个适配器' : '正常') : '未检测到';
  };

  WifiTest.prototype.onCleanup = function() {};

  // ===== Bluetooth Test =====
  function BluetoothTest() { HardwareTest.call(this, 'Bt', '蓝牙', { timeoutMs: 20000 }); }
  BluetoothTest.prototype = Object.create(HardwareTest.prototype);
  BluetoothTest.prototype.constructor = BluetoothTest;

  BluetoothTest.prototype.onStart = function() {
    var self = this;

    showOverlay(
      '<div class="overlay-title"><i class="fas fa-bluetooth-b"></i>蓝牙测试</div>' +
      '<div class="scanning-state">' +
      '<div class="scan-spinner"></div>' +
      '<div style="font-size:12px;color:#5A6F87;margin-top:8px;">正在扫描蓝牙设备...</div>' +
      '</div>' +
      '<div class="overlay-actions">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Bt\'].stop(false)">跳过</button>' +
      '</div>'
    );

    postToHost('scan_bluetooth');

    waitForHostData('_hostBtData', 8000, function(devices) {
      if (!devices || devices.length === 0) devices = [];
      self._devices = devices;
      self._renderUI(devices);
    });
  };

  BluetoothTest.prototype._renderUI = function(devices) {
    var hasAdapter = devices && devices.length > 0;

    var html = '<div class="overlay-title"><i class="fas fa-bluetooth-b"></i>蓝牙测试</div>';

    if (hasAdapter) {
      html += '<div class="result-ok">' +
        '<div class="result-icon-wrap success-icon"><i class="fas fa-check-circle"></i></div>' +
        '<div class="result-text">检测到 <b>' + devices.length + '</b> 个蓝牙设备</div>' +
        '</div>';
      html += '<div class="adapter-list">';
      for (var d = 0; d < devices.length; d++) {
        var name = devices[d].name || devices[d].description || '未知设备';
        var desc = devices[d].description && devices[d].description !== name ? devices[d].description : '';
        html += '<div class="adapter-item">' +
          '<i class="fas fa-bluetooth-b adapter-icon bt-icon"></i>' +
          '<div><div class="adapter-name">' + escapeHtml(name) + '</div>' +
          (desc ? '<div class="adapter-desc">' + escapeHtml(desc) + '</div>' : '') +
          '</div></div>';
      }
      html += '</div>';
    } else {
      html += '<div class="result-fail">' +
        '<div class="result-icon-wrap fail-icon"><i class="fas fa-exclamation-circle"></i></div>' +
        '<div class="result-text">未检测到蓝牙适配器</div>' +
        '<div class="result-hint">可能原因: 驱动未安装 · 硬件开关关闭 · 设备不支持蓝牙</div>' +
        '</div>';
    }

    html += '<div class="overlay-actions">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Bt\'].stop(false)">' +
      (hasAdapter ? '标记失败' : '未检测到') + '</button>' +
      '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Bt\'].stop(true)">蓝牙正常</button>' +
      '</div>';

    var card = document.getElementById('overlayCard');
    if (card) card.innerHTML = html;
  };

  BluetoothTest.prototype.onStop = function(pass) {
    hideOverlay();
    var count = (this._devices && this._devices.length) ? this._devices.length : 0;
    this.resultDetail = pass ? (count > 0 ? count + ' 个设备' : '正常') : '未检测到';
  };

  BluetoothTest.prototype.onCleanup = function() {};

  // ===== Register =====
  if (window.hwt_mgr) {
    window.hwt_mgr.register(new WifiTest());
    window.hwt_mgr.register(new BluetoothTest());
  }

  window.startWifiTest = function() {
    if (window.hwt_mgr && window.hwt_mgr.tests['Wifi']) window.hwt_mgr.tests['Wifi'].start();
  };
  window.startBluetoothTest = function() {
    if (window.hwt_mgr && window.hwt_mgr.tests['Bt']) window.hwt_mgr.tests['Bt'].start();
  };

})();
