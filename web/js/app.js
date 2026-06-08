(function(){
  // ===== Message handling =====
  function handleMessage(event) {
    try {
      var raw = event.data;
      var data = typeof raw === 'string' ? JSON.parse(raw) : raw;

      // Route: monitor data has ram_used_pct
      if (data && typeof data.ram_used_pct !== 'undefined') {
        if (window.updateMonitorData) window.updateMonitorData(data);
        return;
      }

      // Route: keyboard layout info from C++ host (may come standalone or with lines)
      if (data && typeof data.keyboard_layout !== 'undefined' && !data.lines) {
        if (window.setKeyboardLayout) window.setKeyboardLayout(data.keyboard_layout);
        return;
      }

      // Route: WiFi/Bluetooth hardware info
      if (data && (typeof data.wifi_adapters !== 'undefined' || typeof data.bluetooth_devices !== 'undefined')) {
        window._hostWifiData = data.wifi_adapters || [];
        window._hostBtData = data.bluetooth_devices || [];
        return;
      }

      // Route: hardware info (has 'lines')
      if (data && data.lines) {
        window.g_data = data;
        // Also handle keyboard layout if present
        if (data.keyboard_layout && window.setKeyboardLayout) {
          window.setKeyboardLayout(data.keyboard_layout);
        }
        try {
          window.renderData(data);
        } catch (e2) {
          var dbg = document.getElementById('hwContent');
          if (dbg) dbg.innerHTML = '<div style="padding:20px;color:red;">renderData错误: ' + e2.message + ' | stack:' + (e2.stack||'none') + '</div>';
        }
        return;
      }

      // Fallback: try rendering
      if (data && typeof data === 'object') {
        try { window.renderData(data); } catch (e) {}
      }
    } catch (e) {
      var dbg2 = document.getElementById('hwContent');
      if (dbg2) dbg2.innerHTML = '<div style="padding:20px;color:red;">JS错误: ' + e.message + '</div>';
    }
  }

  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', handleMessage);
  } else {
    window.addEventListener('message', handleMessage);
  }

  window.renderHardwareData = function(data) {
    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch(e) { return; }
    }
    window.renderData(data);
  };

  window.handleRefresh = function() {
    window.postToHost('refresh');
  };

  window.handleClose = function() {
    window.postToHost('close');
  };

  // Init
  console.log('[app] init: sending refresh, test-framework v2');
  window.renderData(null);
  window.postToHost('refresh');
  window.updateClock();
  setInterval(window.updateClock, 1000);

  // Title bar controls
  var btnMin = document.getElementById('btnMinimize');
  var btnMax = document.getElementById('btnMaximize');
  var btnClose = document.getElementById('btnClose');
  var titleDrag = document.getElementById('titleBarDrag');
  var isMaximized = false;

  if (btnMin) btnMin.addEventListener('click', function(){ window.postToHost('minimize'); });
  if (btnMax) btnMax.addEventListener('click', function(){
    window.postToHost(isMaximized ? 'restore' : 'maximize');
  });
  if (btnClose) btnClose.addEventListener('click', function(){ window.postToHost('close'); });
  if (titleDrag) titleDrag.addEventListener('dblclick', function(){
    window.postToHost(isMaximized ? 'restore' : 'maximize');
  });

  // Drag support via mousedown on title bar drag area
  var titleBar = document.getElementById('titleBar');
  if (titleBar) {
    titleBar.addEventListener('mousedown', function(e) {
      if (e.target.closest('.title-bar-controls')) return;
      window.postToHost('drag');
    });
  }

  // Sidebar toggle
  var sidebar = document.getElementById('leftSidebar');
  var mainWrapper = document.querySelector('.main-wrapper');
  var toggleBtn = document.getElementById('sidebarToggle');
  if (toggleBtn && sidebar && mainWrapper) {
    toggleBtn.addEventListener('click', function() {
      sidebar.classList.toggle('collapsed');
      mainWrapper.classList.toggle('sidebar-collapsed');
    });
  }

  // Auto-start sidebar performance monitor
  if (window.startSidebarMonitor) {
    window.startSidebarMonitor();
  }

})();
