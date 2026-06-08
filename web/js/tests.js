// tests.js — Hardware tests refactored as HardwareTest subclasses
// Camera / Microphone / Speaker (with loopback) / Screen / Touchpad

(function() {
  'use strict';

  // ===== Camera Test =====
  function CameraTest() { HardwareTest.call(this, 'Cam', '摄像头', { timeoutMs: 30000 }); }
  CameraTest.prototype = Object.create(HardwareTest.prototype);
  CameraTest.prototype.constructor = CameraTest;

  CameraTest.prototype.onStart = function() {
    var self = this;
    var constraints = {
      video: {
        width: { ideal: 1280 },
        height: { ideal: 720 },
        frameRate: { ideal: 30 }
      }
    };

    navigator.mediaDevices.getUserMedia(constraints).then(function(stream) {
      window.activeStreams.push(stream);
      var track = stream.getVideoTracks()[0];
      var label = track.label || '未知设备';
      var settings = track.getSettings ? track.getSettings() : {};
      var res = (settings.width && settings.height) ? settings.width + 'x' + settings.height : '';
      var fps = settings.frameRate ? Math.round(settings.frameRate) + 'fps' : '';

      self.resultDetail = res + (fps ? ' ' + fps : '');

      showOverlay(
        '<div class="overlay-title"><i class="fas fa-camera"></i>摄像头测试</div>' +
        '<video id="camVideo" autoplay playsinline muted></video>' +
        '<div style="font-size:11px;color:#5A6F87;margin-bottom:4px;">设备: ' + escapeHtml(label) + '</div>' +
        '<div style="font-size:11px;color:#8896A7;margin-bottom:8px;">分辨率: ' + (res || '获取中...') + (fps ? ' · 帧率: ' + fps : '') + '</div>' +
        '<div class="overlay-actions">' +
        '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Cam\'].stop(false)">未检测到</button>' +
        '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Cam\'].stop(true)">摄像头正常</button>' +
        '</div>'
      );

      var video = document.getElementById('camVideo');
      if (video) video.srcObject = stream;

    }).catch(function(err) {
      self.fail(friendlyError(err));
    });
  };

  CameraTest.prototype.onStop = function(pass) {
    var video = document.getElementById('camVideo');
    if (video) { video.srcObject = null; video.load(); }
    stopAllStreams();
    hideOverlay();
  };

  CameraTest.prototype.onCleanup = function() {
    var video = document.getElementById('camVideo');
    if (video) { video.srcObject = null; video.load(); }
    stopAllStreams();
  };

  // ===== Microphone Test =====
  function MicTest() { HardwareTest.call(this, 'Mic', '麦克风', { timeoutMs: 30000 }); }
  MicTest.prototype = Object.create(HardwareTest.prototype);
  MicTest.prototype.constructor = MicTest;

  MicTest.prototype.onStart = function() {
    var self = this;

    navigator.mediaDevices.getUserMedia({ audio: true }).then(function(stream) {
      window.activeStreams.push(stream);
      var track = stream.getAudioTracks()[0];
      var label = track.label || '未知设备';

      var micContext = new (window.AudioContext || window.webkitAudioContext)();
      var source = micContext.createMediaStreamSource(stream);
      var analyser = micContext.createAnalyser();
      analyser.fftSize = 256;
      source.connect(analyser);

      showOverlay(
        '<div class="overlay-title"><i class="fas fa-microphone"></i>麦克风测试</div>' +
        '<div style="font-size:11px;color:#5A6F87;margin-bottom:8px;">设备: ' + escapeHtml(label) + '</div>' +
        '<div style="font-size:11px;color:#8896A7;margin-bottom:6px;">请对着麦克风说话，观察音量指示</div>' +
        '<div class="audio-meter"><div class="audio-meter-fill" id="micMeterFill"></div></div>' +
        '<div class="overlay-actions">' +
        '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Mic\'].stop(false)">未检测到</button>' +
        '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Mic\'].stop(true)">麦克风正常</button>' +
        '</div>'
      );

      var animFrame;
      function updateMeter() {
        if (!analyser) return;
        var data = new Uint8Array(analyser.frequencyBinCount);
        analyser.getByteFrequencyData(data);
        var sum = 0;
        for (var i = 0; i < data.length; i++) sum += data[i];
        var avg = sum / data.length;
        var pct = Math.min(100, Math.round(avg / 128 * 100));
        var fill = document.getElementById('micMeterFill');
        if (fill) fill.style.width = pct + '%';
        animFrame = requestAnimationFrame(updateMeter);
      }
      updateMeter();

      // Store cleanup refs
      self._micContext = micContext;
      self._micAnalyser = analyser;
      self._micAnim = animFrame;

    }).catch(function(err) {
      self.fail(friendlyError(err));
    });
  };

  MicTest.prototype.onStop = function(pass) {
    if (this._micAnim) cancelAnimationFrame(this._micAnim);
    if (this._micContext) { try { this._micContext.close(); } catch(e){} }
    this._micContext = null;
    this._micAnalyser = null;
    this._micAnim = null;
    stopAllStreams();
    hideOverlay();
  };

  MicTest.prototype.onCleanup = function() {
    this.onStop(false);
  };

  // ===== Speaker Test (with auto loopback detection) =====
  // Audio playback is handled by C++ via waveOut API for precise channel routing.
  // JS sends a message, C++ synthesizes and plays the chime on the specified channel.

  function SpeakerTest() { HardwareTest.call(this, 'Spk', '扬声器', { timeoutMs: 45000 }); }
  SpeakerTest.prototype = Object.create(HardwareTest.prototype);
  SpeakerTest.prototype.constructor = SpeakerTest;

  SpeakerTest.prototype.onStart = function() {
    var self = this;
    self._spkLeftOk = false;
    self._spkRightOk = false;
    self._spkLeftAuto = null; // null=untested, true=detected, false=not detected
    self._spkRightAuto = null;

    showOverlay(
      '<div class="overlay-title"><i class="fas fa-volume-up"></i>扬声器测试</div>' +
      '<div style="font-size:11px;color:#8896A7;margin-bottom:10px;">点击测试将依次播放左声道 → 右声道</div>' +
      '<div style="display:flex;gap:12px;justify-content:center;margin-bottom:4px;">' +
      '<button class="overlay-btn primary" id="spkTestBtn" onclick="hwt_mgr.tests[\'Spk\'].playBothChannels()" style="min-width:140px;">开始测试</button>' +
      '</div>' +
      '<div style="text-align:center;margin:4px 0 8px 0;">' +
      '<label style="font-size:11px;color:#5A6F87;cursor:pointer;">' +
      '<input type="checkbox" id="spkAutoDetect" onchange="hwt_mgr.tests[\'Spk\'].toggleAutoDetect()" style="margin-right:4px;">' +
      '自动检测 (需要麦克风)</label>' +
      '</div>' +
      '<div style="font-size:11px;color:#5A6F87;text-align:center;margin-bottom:8px;">' +
      '左声道: <span id="spkLeftStatus" style="color:#A0B0C4;">未测试</span> &nbsp; ' +
      '右声道: <span id="spkRightStatus" style="color:#A0B0C4;">未测试</span>' +
      '</div>' +
      '<div id="spkAutoResult" style="display:none;font-size:10px;color:#8896A7;text-align:center;margin-bottom:8px;"></div>' +
      '<div class="overlay-actions">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Spk\'].stop(false)">未检测到</button>' +
      '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Spk\'].stop(true)">扬声器正常</button>' +
      '</div>'
    );
  };

  SpeakerTest.prototype.toggleAutoDetect = function() {
    var cb = document.getElementById('spkAutoDetect');
    if (cb && cb.checked) {
      this._setupLoopback();
    } else {
      this._teardownLoopback();
    }
  };

  SpeakerTest.prototype._setupLoopback = function() {
    var self = this;
    navigator.mediaDevices.getUserMedia({ audio: true }).then(function(stream) {
      self._loopStream = stream;
      var ctx = new (window.AudioContext || window.webkitAudioContext)();
      var source = ctx.createMediaStreamSource(stream);
      var analyser = ctx.createAnalyser();
      analyser.fftSize = 2048;
      analyser.smoothingTimeConstant = 0.2;
      source.connect(analyser);
      self._loopCtx = ctx;
      self._loopAnalyser = analyser;
    }).catch(function(err) {
      var res = document.getElementById('spkAutoResult');
      if (res) { res.style.display = 'block'; res.textContent = '自动检测不可用: ' + friendlyError(err); }
      var cb = document.getElementById('spkAutoDetect');
      if (cb) cb.checked = false;
    });
  };

  SpeakerTest.prototype._teardownLoopback = function() {
    if (this._loopStream) {
      this._loopStream.getTracks().forEach(function(t) { t.stop(); });
      this._loopStream = null;
    }
    if (this._loopCtx) {
      try { this._loopCtx.close(); } catch(e) {}
      this._loopCtx = null;
      this._loopAnalyser = null;
    }
  };

  SpeakerTest.prototype.playBothChannels = function() {
    var self = this;
    var btn = document.getElementById('spkTestBtn');
    if (btn) { btn.disabled = true; btn.textContent = '播放中...'; }

    // Play left channel first, then right after a delay
    self._updateChanStatus('left', '播放中...', '#E8600A');
    self.playChannel('left');
    setTimeout(function() {
      self._updateChanStatus('right', '播放中...', '#E8600A');
      self.playChannel('right');
      setTimeout(function() {
        if (btn) { btn.disabled = false; btn.textContent = '重新测试'; }
      }, 1500);
    }, 1200);
  };

  SpeakerTest.prototype.playChannel = function(channel) {
    var self = this;
    // Send message to C++ to play chime on specified channel via waveOut
    if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage(channel === 'left' ? 'play_speaker_left' : 'play_speaker_right');
    }
    self._markChannelPlayed(channel);
  };

  SpeakerTest.prototype._markChannelPlayed = function(channel) {
    var self = this;
    if (channel === 'left') {
      self._spkLeftOk = true;
      self._updateChanStatus('left', '已播放');
    } else {
      self._spkRightOk = true;
      self._updateChanStatus('right', '已播放');
    }

    // Auto-detect if loopback is active
    if (self._loopAnalyser && self._loopCtx) {
      var checkInterval = setInterval(function() {
        if (!self._loopAnalyser) { clearInterval(checkInterval); return; }
        var data = new Uint8Array(self._loopAnalyser.frequencyBinCount);
        self._loopAnalyser.getByteFrequencyData(data);
        var sum = 0;
        for (var i = 5; i < data.length; i++) sum += data[i];
        var avg = sum / (data.length - 5);
        if (avg > 15) {
          if (channel === 'left') self._spkLeftAuto = true;
          else self._spkRightAuto = true;
          self._updateChanStatus(channel, '已检测 ✓', '#2D8A4E');
          self._updateAutoResult();
          clearInterval(checkInterval);
        }
      }, 80);
      setTimeout(function() { clearInterval(checkInterval); }, 3000);
    }
  };

  SpeakerTest.prototype._updateChanStatus = function(channel, text, color) {
    var id = channel === 'left' ? 'spkLeftStatus' : 'spkRightStatus';
    var el = document.getElementById(id);
    if (el) { el.textContent = text; if (color) el.style.color = color; else el.style.color = '#2D8A4E'; }
  };

  SpeakerTest.prototype._updateAutoResult = function() {
    var el = document.getElementById('spkAutoResult');
    if (!el) return;
    el.style.display = 'block';
    var l = this._spkLeftAuto === true ? '左✓' : (this._spkLeftAuto === false ? '左✗' : '左?');
    var r = this._spkRightAuto === true ? '右✓' : (this._spkRightAuto === false ? '右✗' : '右?');
    el.textContent = '自动检测: ' + l + ' ' + r;
    el.style.color = (this._spkLeftAuto && this._spkRightAuto) ? '#2D8A4E' : '#8896A7';
  };

  SpeakerTest.prototype.onStop = function(pass) {
    this._teardownLoopback();
    hideOverlay();
    if (pass) {
      var parts = [];
      if (this._spkLeftOk && this._spkRightOk) parts.push('双声道正常');
      else if (this._spkLeftOk) parts.push('左声道正常');
      else if (this._spkRightOk) parts.push('右声道正常');
      if (this._spkLeftAuto === true && this._spkRightAuto === true) parts.push('回路检测通过');
      this.resultDetail = parts.join(', ') || '正常';
    }
  };

  SpeakerTest.prototype.onCleanup = function() {
    this._teardownLoopback();
  };

  // ===== Screen Test =====
  var screenPatterns = [
    { type: 'color', color: '#FFFFFF', name: '白色' },
    { type: 'color', color: '#000000', name: '黑色' },
    { type: 'color', color: '#FF0000', name: '红色' },
    { type: 'color', color: '#00FF00', name: '绿色' },
    { type: 'color', color: '#0000FF', name: '蓝色' },
    { type: 'gradient', name: '灰度渐变' },
    { type: 'deadpixel', name: '坏点检测(黑白交替)' }
  ];

  function ScreenTest() { HardwareTest.call(this, 'Scr', '屏幕', { timeoutMs: 120000 }); }
  ScreenTest.prototype = Object.create(HardwareTest.prototype);
  ScreenTest.prototype.constructor = ScreenTest;

  ScreenTest.prototype.onStart = function() {
    var self = this;
    self._patternIndex = 0;
    self._applyPattern(0);

    if (document.documentElement.requestFullscreen) {
      document.documentElement.requestFullscreen().catch(function(){});
    } else if (document.documentElement.webkitRequestFullscreen) {
      document.documentElement.webkitRequestFullscreen();
    }

    // Arrow key and click navigation
    self._keyHandler = function(e) {
      if (e.key === 'Escape') { self.stop(true); return; }
      if (e.key === 'ArrowRight' || e.key === 'ArrowDown') self._advance();
      if (e.key === 'ArrowLeft' || e.key === 'ArrowUp') self._goBack();
    };
    document.addEventListener('keydown', self._keyHandler);
  };

  ScreenTest.prototype._applyPattern = function(index) {
    var area = document.getElementById('screenTestArea');
    if (!area) return;
    var pattern = screenPatterns[index];
    area.style.backgroundImage = '';
    area.style.backgroundColor = '';

    if (pattern.type === 'color') {
      area.style.backgroundColor = pattern.color;
    } else if (pattern.type === 'gradient') {
      area.style.backgroundImage = 'linear-gradient(to right, #000 0%, #333 10%, #666 20%, #999 30%, #CCC 40%, #FFF 50%, #CCC 60%, #999 70%, #666 80%, #333 90%, #000 100%)';
    } else if (pattern.type === 'deadpixel') {
      area.style.backgroundImage = 'repeating-linear-gradient(0deg, #000 0px, #000 2px, #FFF 2px, #FFF 4px)';
    }
    area.classList.add('active');

    var hint = document.getElementById('screenTestHint');
    if (hint) {
      hint.innerHTML = '<b>' + pattern.name + '</b> (' + (index + 1) + '/' + screenPatterns.length + ')<br>' +
        '<small>← → 切换 · 点击前进 · Esc 完成</small>';
    }
  };

  ScreenTest.prototype._advance = function() {
    this._patternIndex++;
    if (this._patternIndex >= screenPatterns.length) {
      this.stop(true);
      return;
    }
    this._applyPattern(this._patternIndex);
  };

  ScreenTest.prototype._goBack = function() {
    if (this._patternIndex > 0) {
      this._patternIndex--;
      this._applyPattern(this._patternIndex);
    }
  };

  ScreenTest.prototype.onStop = function(pass) {
    if (this._keyHandler) {
      document.removeEventListener('keydown', this._keyHandler);
      this._keyHandler = null;
    }
    var area = document.getElementById('screenTestArea');
    if (area) {
      area.classList.remove('active');
      area.style.backgroundColor = '';
      area.style.backgroundImage = '';
    }
    if (document.fullscreenElement) {
      if (document.exitFullscreen) document.exitFullscreen().catch(function(){});
      else if (document.webkitExitFullscreen) document.webkitExitFullscreen();
    }
  };

  ScreenTest.prototype.onCleanup = function() {
    this.onStop(false);
  };

  // ===== Touchpad Test =====
  function TouchpadTest() { HardwareTest.call(this, 'Tpd', '触控板', { timeoutMs: 60000 }); }
  TouchpadTest.prototype = Object.create(HardwareTest.prototype);
  TouchpadTest.prototype.constructor = TouchpadTest;

  TouchpadTest.prototype.onStart = function() {
    var self = this;
    self._touchCount = 0;
    self._leftOk = false;
    self._rightOk = false;

    showOverlay(
      '<div class="overlay-title"><i class="fas fa-hand-pointer"></i>触控板测试</div>' +
      '<div style="font-size:11px;color:#8896A7;margin-bottom:8px;">在触控板表面滑动手指，观察指针 · 在下方按键区域点击左键/右键</div>' +
      '<div class="tpd-virtual" id="tpdVirtual">' +
        '<div class="tpd-surface" id="tpdSurface">' +
          '<div class="tpd-pointer" id="tpdPointer"></div>' +
        '</div>' +
        '<div class="tpd-buttons">' +
          '<div class="tpd-btn tpd-btn-left" id="tpdLeft">' +
            '<span>左键</span>' +
          '</div>' +
          '<div class="tpd-btn tpd-btn-right" id="tpdRight">' +
            '<span>右键</span>' +
          '</div>' +
        '</div>' +
      '</div>' +
      '<div style="font-size:11px;color:#5A6F87;margin-top:8px;">' +
        '移动点数: <span id="touchCount">0</span> · ' +
        '触控点数: <span id="touchFingers">0</span>' +
      '</div>' +
      '<div class="overlay-actions" style="margin-top:12px;">' +
      '<button class="overlay-btn secondary" onclick="hwt_mgr.tests[\'Tpd\'].stop(false)">未检测到</button>' +
      '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Tpd\'].stop(true)">触控板正常</button>' +
      '</div>'
    );

    // Bind after DOM is ready
    requestAnimationFrame(function() {
      var surface = document.getElementById('tpdSurface');
      if (surface) {
        self._surface = surface;
        self._pointer = document.getElementById('tpdPointer');

        // Mouse move — track pointer position
        surface.addEventListener('mousemove', function(e) {
          var rect = surface.getBoundingClientRect();
          var x = ((e.clientX - rect.left) / rect.width * 100);
          var y = ((e.clientY - rect.top) / rect.height * 100);
          surface.style.setProperty('--pointer-x', x + '%');
          surface.style.setProperty('--pointer-y', y + '%');
          if (self._pointer) {
            self._pointer.style.left = (e.clientX - rect.left - 6) + 'px';
            self._pointer.style.top = (e.clientY - rect.top - 6) + 'px';
            self._pointer.style.display = 'block';
          }
          self._touchCount++;
          var countEl = document.getElementById('touchCount');
          if (countEl) countEl.textContent = self._touchCount;
        });

        surface.addEventListener('mouseleave', function() {
          if (self._pointer) self._pointer.style.display = 'none';
        });

        // Touch move — track multi-touch
        surface.addEventListener('touchmove', function(e) {
          e.preventDefault();
          var fingers = document.getElementById('touchFingers');
          if (fingers) fingers.textContent = e.touches.length;
          if (e.touches.length > 0) {
            var rect = surface.getBoundingClientRect();
            var t = e.touches[0];
            var x = ((t.clientX - rect.left) / rect.width * 100);
            var y = ((t.clientY - rect.top) / rect.height * 100);
            surface.style.setProperty('--pointer-x', x + '%');
            surface.style.setProperty('--pointer-y', y + '%');
            if (self._pointer) {
              self._pointer.style.left = (t.clientX - rect.left - 6) + 'px';
              self._pointer.style.top = (t.clientY - rect.top - 6) + 'px';
              self._pointer.style.display = 'block';
            }
            self._touchCount++;
            var countEl = document.getElementById('touchCount');
            if (countEl) countEl.textContent = self._touchCount;
          }
        }, { passive: false });

        surface.addEventListener('touchstart', function(e) {
          var fingers = document.getElementById('touchFingers');
          if (fingers) fingers.textContent = e.touches.length;
        });

        surface.addEventListener('touchend', function(e) {
          var fingers = document.getElementById('touchFingers');
          if (fingers) fingers.textContent = e.touches.length;
          if (e.touches.length === 0 && self._pointer) {
            self._pointer.style.display = 'none';
          }
        });
      }

      // Detect left/right click on the button area
      var btnsArea = document.querySelector('.tpd-buttons');
      if (btnsArea) {
        btnsArea.addEventListener('mousedown', function(e) {
          if (e.button === 0) {
            self._leftOk = true;
            var btn = document.getElementById('tpdLeft');
            if (btn) btn.classList.add('tpd-btn-pass');
          } else if (e.button === 2) {
            self._rightOk = true;
            var btn = document.getElementById('tpdRight');
            if (btn) btn.classList.add('tpd-btn-pass');
          }
        });
        btnsArea.addEventListener('contextmenu', function(e) { e.preventDefault(); });
      }
    });
  };

  TouchpadTest.prototype._addDot = function(clientX, clientY) {
    var area = this._touchArea || document.getElementById('touchArea');
    if (!area) return;
    var rect = area.getBoundingClientRect();
    var x = clientX - rect.left;
    var y = clientY - rect.top;
    if (x < 0 || y < 0 || x > rect.width || y > rect.height) return;

    var dot = document.createElement('div');
    dot.className = 'touch-trail';
    dot.style.left = (x - 4) + 'px';
    dot.style.top = (y - 4) + 'px';
    area.appendChild(dot);
    this._touchCount++;
    var countEl = document.getElementById('touchCount');
    if (countEl) countEl.textContent = this._touchCount;
    if (area.childElementCount > 300) {
      area.removeChild(area.firstChild);
    }
  };

  TouchpadTest.prototype.onStop = function(pass) {
    this._surface = null;
    this._pointer = null;
    hideOverlay();
    if (pass) {
      var parts = [];
      parts.push('轨迹 ' + this._touchCount + ' 点');
      if (this._leftOk) parts.push('左键正常');
      if (this._rightOk) parts.push('右键正常');
      this.resultDetail = parts.join(', ');
    }
  };

  TouchpadTest.prototype.onCleanup = function() {
    this.onStop(false);
  };

  // ===== Test Manager (singleton registry) =====
  function TestManager() {
    this.tests = {};
  }

  TestManager.prototype.register = function(test) {
    this.tests[test.id] = test;
    test.restore(); // restore persisted state
    return test;
  };

  TestManager.prototype.get = function(id) {
    return this.tests[id];
  };

  TestManager.prototype.resetAll = function() {
    var self = this;
    showConfirm('确定要重置所有测试结果吗？<br><small style="color:#8896A7;">通过/失败记录将被清除</small>', function() {
      clearTestResults();
      Object.keys(self.tests).forEach(function(id) {
        self.tests[id].reset();
      });
      var bar = document.getElementById('batchProgressBar');
      if (bar) bar.style.width = '0%';
      var txt = document.getElementById('batchProgressText');
      if (txt) txt.textContent = '0/0';
      var wrap = document.getElementById('batchProgressWrap');
      if (wrap) wrap.style.display = 'none';
      showToast('🔄 已重置全部测试');
    });
  };

  TestManager.prototype.runAll = function() {
    var self = this;
    var list = Object.keys(this.tests).map(function(id) { return self.tests[id]; });
    var pending = list.filter(function(t) { return t.state !== TestState.PASSED; });
    if (pending.length === 0) {
      showToast('✅ 所有测试已通过！');
      if (window.generateReport) {
        setTimeout(function() { window.generateReport(); }, 500);
      }
      return;
    }

    // Reset & show progress bar
    var bar = document.getElementById('batchProgressBar');
    if (bar) bar.style.width = '0%';
    var txt = document.getElementById('batchProgressText');
    if (txt) txt.textContent = '0/' + pending.length;
    var wrap = document.getElementById('batchProgressWrap');
    if (wrap) wrap.style.display = 'flex';
    var nameEl = document.getElementById('batchCurrentTest');
    if (nameEl) { nameEl.textContent = '准备开始...'; nameEl.style.display = 'block'; }

    showToast('🚀 开始批量测试 ' + pending.length + ' 项');
    runAllTests(pending, function(progress) {
      bar = document.getElementById('batchProgressBar');
      if (bar) bar.style.width = (progress.completed / progress.total * 100) + '%';
      txt = document.getElementById('batchProgressText');
      if (txt) txt.textContent = progress.completed + '/' + progress.total;
      nameEl = document.getElementById('batchCurrentTest');
      if (nameEl && progress.current) {
        var icon = progress.starting ? '⏳' : (progress.current.state === TestState.PASSED ? '✅' : '❌');
        nameEl.textContent = icon + ' ' + progress.current.name;
      }
      if (progress.allDone) {
        if (nameEl) nameEl.textContent = '🎉 全部完成！';
      }
    }, function(result) {
      bar = document.getElementById('batchProgressBar');
      if (bar) bar.style.width = '100%';
      txt = document.getElementById('batchProgressText');
      if (txt) txt.textContent = result.total + '/' + result.total;
      nameEl = document.getElementById('batchCurrentTest');
      if (nameEl) nameEl.textContent = '🎉 全部完成！';

      setTimeout(function() {
        var wrap2 = document.getElementById('batchProgressWrap');
        if (wrap2) wrap2.style.display = 'none';
        var ne2 = document.getElementById('batchCurrentTest');
        if (ne2) ne2.style.display = 'none';
      }, 3000);

      if (result.failed === 0) {
        showToast('🎉 全部通过！' + result.passed + '/' + result.total);
      } else {
        showToast('批量测试完成: ✅' + result.passed + ' 通过 ❌' + result.failed + ' 失败');
      }
      if (window.generateReport) {
        setTimeout(function() { window.generateReport(); }, 600);
      }
    });
  };

  // ===== Init — create and register all tests =====
  window.hwt_mgr = new TestManager();
  window.hwt_mgr.register(new CameraTest());
  window.hwt_mgr.register(new MicTest());
  window.hwt_mgr.register(new SpeakerTest());
  window.hwt_mgr.register(new ScreenTest());
  window.hwt_mgr.register(new TouchpadTest());

  // ===== Backward-compatible global entry points =====
  // These mirror the old API so index.html onclick handlers still work
  window.startCameraTest   = function() { window.hwt_mgr.tests['Cam'].start(); };
  window.startMicTest      = function() { window.hwt_mgr.tests['Mic'].start(); };
  window.startSpeakerTest  = function() { window.hwt_mgr.tests['Spk'].start(); };
  window.startScreenTest   = function() { window.hwt_mgr.tests['Scr'].start(); };
  window.startTouchpadTest = function() { window.hwt_mgr.tests['Tpd'].start(); };

  // Legacy stop functions — now delegated through hwt_mgr
  window.stopCameraTest   = function(pass) { window.hwt_mgr.tests['Cam'].stop(pass); };
  window.stopMicTest      = function(pass) { window.hwt_mgr.tests['Mic'].stop(pass); };
  window.stopSpeakerTest  = function(pass) { window.hwt_mgr.tests['Spk'].stop(pass); };
  window.stopScreenTest   = function(pass) { window.hwt_mgr.tests['Scr'].stop(pass); };
  window.stopTouchpadTest = function(pass) { window.hwt_mgr.tests['Tpd'].stop(pass); };
  window.playChannelTone  = function(ch) { window.hwt_mgr.tests['Spk'].playChannel(ch); };
  window.advanceScreenTest = function() { window.hwt_mgr.tests['Scr']._advance(); };

  // Expose manager
  window.runAllHardwareTests = function() { window.hwt_mgr.runAll(); };
  window.resetAllTests = function() { window.hwt_mgr.resetAll(); };

})();
