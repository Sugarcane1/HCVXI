// monitor.js — Real-time hardware performance monitoring in sidebar (CPU/GPU/RAM vertical stack)
(function() {
  var monitorActive = false;
  var monitorInterval = null;
  var dataReceived = false;

  // Smooth value interpolation state
  var current = { cpu: 0, gpu: 0, ram: 0 };
  var target  = { cpu: 0, gpu: 0, ram: 0 };
  var animFrame = null;
  var LERP_SPEED = 0.12; // interpolation factor per frame
  var gpuAvailable = null; // null = unknown, true/false

  function lerp(a, b, t) { return a + (b - a) * t; }

  // HiDPI canvas setup — scale buffer to match device pixel ratio
  function setupHiDPICanvas(canvas) {
    if (!canvas) return;
    var dpr = window.devicePixelRatio || 1;
    var rect = canvas.getBoundingClientRect();
    canvas.width = Math.round(rect.width * dpr);
    canvas.height = Math.round(rect.height * dpr);
    var ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
  }

  // Get logical (CSS) dimensions of a canvas
  function canvasLogicalSize(canvas) {
    var dpr = window.devicePixelRatio || 1;
    return { w: canvas.width / dpr, h: canvas.height / dpr };
  }

  // Canvas-based gauge rendering (compact for sidebar)
  function drawGauge(canvasId, value, label, color, maxVal) {
    var canvas = document.getElementById(canvasId);
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var size = canvasLogicalSize(canvas);
    var w = size.w, h = size.h;
    var cx = w / 2, cy = h / 2;
    var radius = Math.min(cx, cy) - 10;

    ctx.clearRect(0, 0, w, h);

    // Background arc
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0.8 * Math.PI, 2.2 * Math.PI);
    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 8;
    ctx.stroke();

    // Value arc
    var ratio = Math.min(1, value / (maxVal || 100));
    var angle = 0.8 * Math.PI + ratio * 1.4 * Math.PI;
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0.8 * Math.PI, angle);
    ctx.strokeStyle = color;
    ctx.lineWidth = 8;
    ctx.lineCap = 'round';
    ctx.stroke();

    // Center text — percentage
    ctx.fillStyle = '#F0EFEC';
    ctx.font = 'bold 22px Inter, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText(Math.round(value) + '%', cx, cy - 2);

    // Label
    ctx.fillStyle = '#6E6E6E';
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(label, cx, cy + 16);
  }

  // Draw a pulsing "waiting" gauge
  function drawWaitingGauge(canvasId, label) {
    var canvas = document.getElementById(canvasId);
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var size = canvasLogicalSize(canvas);
    var w = size.w, h = size.h;
    var cx = w / 2, cy = h / 2;
    var radius = Math.min(cx, cy) - 10;

    ctx.clearRect(0, 0, w, h);

    // Pulsing background arc
    var t = Date.now() / 1000;
    var pulse = 0.04 + 0.04 * Math.sin(t * 2);
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0.8 * Math.PI, 2.2 * Math.PI);
    ctx.strokeStyle = 'rgba(255,255,255,' + pulse.toFixed(3) + ')';
    ctx.lineWidth = 8;
    ctx.stroke();

    // "—" placeholder
    ctx.fillStyle = '#6E6E6E';
    ctx.font = 'bold 22px Inter, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('—', cx, cy - 2);

    // Label
    ctx.fillStyle = '#6E6E6E';
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(label, cx, cy + 16);
  }

  // Draw "N/A" gauge for unavailable hardware
  function drawUnavailableGauge(canvasId, label) {
    var canvas = document.getElementById(canvasId);
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    var size = canvasLogicalSize(canvas);
    var w = size.w, h = size.h;
    var cx = w / 2, cy = h / 2;
    var radius = Math.min(cx, cy) - 10;

    ctx.clearRect(0, 0, w, h);

    // Dim background arc
    ctx.beginPath();
    ctx.arc(cx, cy, radius, 0.8 * Math.PI, 2.2 * Math.PI);
    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    ctx.lineWidth = 8;
    ctx.stroke();

    // "N/A" text
    ctx.fillStyle = '#4A4A4A';
    ctx.font = 'bold 16px Inter, sans-serif';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('N/A', cx, cy - 2);

    // Label
    ctx.fillStyle = '#4A4A4A';
    ctx.font = '10px Inter, sans-serif';
    ctx.fillText(label, cx, cy + 16);
  }

  // Waiting animation (before first data)
  var waitingAnimFrame = null;
  function animateWaiting() {
    if (dataReceived) return;
    drawWaitingGauge('gaugeCPU', 'CPU');
    drawWaitingGauge('gaugeGPU', 'GPU');
    drawWaitingGauge('gaugeRAM', 'RAM');
    waitingAnimFrame = requestAnimationFrame(animateWaiting);
  }

  // Smooth gauge animation loop
  function animateGauges() {
    var needsUpdate = false;
    var keys = ['cpu', 'gpu', 'ram'];

    for (var i = 0; i < keys.length; i++) {
      var k = keys[i];
      // Skip GPU interpolation if not available
      if (k === 'gpu' && gpuAvailable === false) continue;
      var diff = Math.abs(target[k] - current[k]);
      if (diff > 0.3) {
        current[k] = lerp(current[k], target[k], LERP_SPEED);
        needsUpdate = true;
      } else {
        current[k] = target[k];
      }
    }

    // Redraw all gauges with interpolated values
    drawGauge('gaugeCPU', current.cpu, 'CPU', '#E8600A', 100);
    if (gpuAvailable === false) {
      drawUnavailableGauge('gaugeGPU', 'GPU');
    } else {
      drawGauge('gaugeGPU', current.gpu, 'GPU', '#FFB300', 100);
    }
    drawGauge('gaugeRAM', current.ram, 'RAM', '#8D6E63', 100);

    if (needsUpdate) {
      animFrame = requestAnimationFrame(animateGauges);
    } else {
      animFrame = null;
    }
  }

  function startSidebarMonitor() {
    monitorActive = true;
    dataReceived = false;
    current = { cpu: 0, gpu: 0, ram: 0 };
    target  = { cpu: 0, gpu: 0, ram: 0 };
    // Setup HiDPI canvases
    setupHiDPICanvas(document.getElementById('gaugeCPU'));
    setupHiDPICanvas(document.getElementById('gaugeGPU'));
    setupHiDPICanvas(document.getElementById('gaugeRAM'));
    monitorInterval = setInterval(pollMonitorData, 1000);
    pollMonitorData();
    // Start waiting animation
    animateWaiting();
  }

  function pollMonitorData() {
    if (!monitorActive) return;
    if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage('poll_monitor');
    }
  }

  // Called by host (C++) with monitoring data
  window.updateMonitorData = function(data) {
    if (!monitorActive) return;

    if (typeof data === 'string') {
      try { data = JSON.parse(data); } catch(e) { return; }
    }

    // Stop waiting animation on first real data
    if (!dataReceived) {
      dataReceived = true;
      if (waitingAnimFrame) cancelAnimationFrame(waitingAnimFrame);
    }

    // Check GPU availability
    if (typeof data.gpu_available !== 'undefined') {
      gpuAvailable = data.gpu_available;
    }

    // Set new targets (smooth interpolation will animate toward these)
    target.cpu = (data.cpu_utilization || 0) * 100;
    target.gpu = gpuAvailable ? (data.gpu_utilization || 0) : 0;
    target.ram = (data.ram_used_pct || 0);

    // Start animation loop if not already running
    if (!animFrame) {
      animFrame = requestAnimationFrame(animateGauges);
    }

    // Update detail text
    var freqEl = document.getElementById('monCpuFreq');
    if (freqEl) freqEl.textContent = data.cpu_freq_mhz ? ('CPU 频率: ' + Math.round(data.cpu_freq_mhz) + ' MHz') : 'CPU 频率: --';
    var vramEl = document.getElementById('monGpuVram');
    if (vramEl) vramEl.textContent = gpuAvailable === false ? 'GPU 显存: N/A' : (data.gpu_vram_used_mb !== undefined ? ('GPU 显存: ' + data.gpu_vram_used_mb + '/' + data.gpu_vram_total_mb + ' MB') : 'GPU 显存: --');
    var ramEl = document.getElementById('monRamFree');
    if (ramEl) ramEl.textContent = data.ram_free_gb !== undefined ? ('RAM 空闲: ' + data.ram_free_gb + ' GB') : 'RAM 空闲: --';
  };

  // Auto-start sidebar monitor when DOM is ready
  window.startSidebarMonitor = startSidebarMonitor;
})();
