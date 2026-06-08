// test-framework.js — Hardware test base class & batch runner
// All interactive hardware tests extend HardwareTest for unified lifecycle,
// timeout handling, error messages, and state persistence.

(function() {
  'use strict';

  // ===== Friendly error messages for common failures =====
  var FRIENDLY_ERRORS = {
    'NotAllowedError': '权限被拒绝 — 请在系统弹窗中点击"允许"',
    'NotFoundError': '未检测到设备 — 请确认硬件已连接',
    'NotReadableError': '设备被占用 — 请关闭其他使用该设备的程序',
    'OverconstrainedError': '设备不支持请求的参数',
    'AbortError': '操作被取消',
    'TimeoutError': '测试超时 — 设备可能无响应',
    'default': '设备访问失败，请检查硬件连接和驱动'
  };

  function friendlyError(err) {
    if (!err) return FRIENDLY_ERRORS['default'];
    var name = err.name || '';
    if (FRIENDLY_ERRORS[name]) return FRIENDLY_ERRORS[name];
    var msg = (err.message || '').toLowerCase();
    if (msg.indexOf('not allowed') !== -1 || msg.indexOf('permission') !== -1)
      return FRIENDLY_ERRORS['NotAllowedError'];
    if (msg.indexOf('not found') !== -1 || msg.indexOf('not found') !== -1)
      return FRIENDLY_ERRORS['NotFoundError'];
    if (msg.indexOf('not readable') !== -1 || msg.indexOf('in use') !== -1)
      return FRIENDLY_ERRORS['NotReadableError'];
    return FRIENDLY_ERRORS['default'];
  }

  // ===== Test state machine: idle → running → passed/failed =====
  var STATE = { IDLE: 'idle', RUNNING: 'running', PASSED: 'passed', FAILED: 'failed' };

  // ===== HardwareTest base class =====
  function HardwareTest(id, name, opts) {
    opts = opts || {};
    this.id = id;           // short id, e.g. 'Cam', 'Mic'
    this.name = name;       // display name, e.g. '摄像头'
    this.state = STATE.IDLE;
    this.timeoutMs = opts.timeoutMs || 30000;
    this._timeoutHandle = null;
    this._startTime = 0;
    this.resultDetail = ''; // extra detail for report, e.g. resolution, key count
  }

  HardwareTest.prototype = {

    // ── lifecycle ────────────────────────────────────────────

    /** Start the test. Subclasses override onStart(). */
    start: function() {
      if (this.state === STATE.RUNNING) return;
      this.state = STATE.RUNNING;
      this._startTime = Date.now();
      this._setStatus('testing', '测试中...');
      this._setBtn('测试中', 'stop', true);
      this._clearTimeout();
      this._armTimeout();
      // Hide the first-use guide hint
      var hint = document.getElementById('guideHint');
      if (hint) hint.style.display = 'none';
      try {
        this.onStart();
      } catch (e) {
        this.fail(friendlyError(e));
      }
    },

    /** Stop the test. pass=true → passed, pass=false → failed.
     *  Subclasses override onStop(pass). */
    stop: function(pass) {
      if (this.state !== STATE.RUNNING) return;
      this._clearTimeout();
      this.state = pass ? STATE.PASSED : STATE.FAILED;
      try { this.onStop(pass); } catch (e) { /* ignore cleanup errors */ }
      this._updateUI(pass);
      this._persist();
    },

    /** Force-fail (e.g. on error / timeout). */
    fail: function(detail) {
      if (this.state !== STATE.RUNNING) return;
      this._clearTimeout();
      this.state = STATE.FAILED;
      this.resultDetail = detail || '';
      try { this.onStop(false); } catch (e) {}
      this._setStatus('fail', detail || '失败');
      this._setBtn('重试', 'start');
      showToast(this.name + ' 测试失败');
      this._persist();
    },

    /** Clean up resources. Subclasses override onCleanup(). */
    cleanup: function() {
      this._clearTimeout();
      try { this.onCleanup(); } catch (e) {}
    },

    /** Reset to idle. */
    reset: function() {
      this.cleanup();
      this.state = STATE.IDLE;
      this.resultDetail = '';
      this._setStatus('pending', '未测试');
      this._setBtn('测试', 'start');
    },

    // ── subclasses override these ────────────────────────────

    /** @override — acquire hardware, show overlay */
    onStart: function() {},

    /** @override — release hardware, hide overlay, report result */
    onStop: function(pass) {},

    /** @override — release any lingering resources */
    onCleanup: function() {},

    // ── UI helpers ───────────────────────────────────────────

    _setStatus: function(cls, text) {
      setTestStatus(this.id, cls, text);
    },

    _setBtn: function(label, cls, disabled) {
      setTestBtn(this.id, label, cls, disabled);
    },

    _updateUI: function(pass) {
      if (pass) {
        this._setStatus('pass', '正常' + (this.resultDetail ? ' (' + this.resultDetail + ')' : ''));
        this._setBtn('通过', 'start', true);
        showToast(this.name + ' 测试通过');
      } else {
        this._setStatus('fail', this.resultDetail || '未检测到');
        this._setBtn('重试', 'start');
      }
    },

    // ── timeout ──────────────────────────────────────────────

    _armTimeout: function() {
      var self = this;
      this._timeoutHandle = setTimeout(function() {
        self.fail('测试超时 — 设备无响应');
      }, this.timeoutMs);
    },

    _clearTimeout: function() {
      if (this._timeoutHandle) {
        clearTimeout(this._timeoutHandle);
        this._timeoutHandle = null;
      }
    },

    // ── persistence ──────────────────────────────────────────

    _persist: function() {
      var results = loadTestResults();
      results[this.id] = {
        state: this.state,
        name: this.name,
        detail: this.resultDetail || '',
        time: new Date().toISOString()
      };
      saveTestResults(results);
    },

    /** Restore state from localStorage. */
    restore: function() {
      var results = loadTestResults();
      var saved = results[this.id];
      if (!saved) return;
      if (saved.state === STATE.PASSED) {
        this.state = STATE.PASSED;
        this.resultDetail = saved.detail || '';
        this._setStatus('pass', '正常' + (this.resultDetail ? ' (' + this.resultDetail + ')' : ''));
        this._setBtn('通过', 'start', true);
      } else if (saved.state === STATE.FAILED) {
        this.state = STATE.FAILED;
        this.resultDetail = saved.detail || '';
        this._setStatus('fail', this.resultDetail || '未检测到');
        this._setBtn('重试', 'start');
      }
    }
  };

  // ===== localStorage helpers =====
  var STORAGE_KEY = 'hwinfo_test_results';

  function loadTestResults() {
    try {
      var raw = localStorage.getItem(STORAGE_KEY);
      return raw ? JSON.parse(raw) : {};
    } catch (e) {
      return {};
    }
  }

  function saveTestResults(results) {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(results));
    } catch (e) {
      // storage full or unavailable — silently ignore
    }
  }

  function clearTestResults() {
    try { localStorage.removeItem(STORAGE_KEY); } catch (e) {}
  }

  // ===== Batch test runner =====
  function runAllTests(tests, onProgress, onDone) {
    var total = tests.length;
    var completed = 0;
    var passed = 0;
    var failed = 0;
    var currentIndex = 0;

    function next() {
      if (currentIndex >= tests.length) {
        // All done — update progress to show completion
        if (onProgress) onProgress({ completed: completed, total: total, current: null, allDone: true });
        if (onDone) onDone({ total: total, passed: passed, failed: failed });
        return;
      }
      var test = tests[currentIndex];

      // Show about-to-start test name
      if (onProgress) onProgress({ completed: completed, total: total, current: test, starting: true });

      test.start();

      // Wait for test to finish (poll state)
      var poll = setInterval(function() {
        if (test.state !== STATE.RUNNING) {
          clearInterval(poll);
          completed++;
          if (test.state === STATE.PASSED) passed++;
          else if (test.state === STATE.FAILED) failed++;
          if (onProgress) onProgress({ completed: completed, total: total, current: test });
          currentIndex++;
          setTimeout(next, 600); // small gap between tests
        }
      }, 200);
    }

    next();
  }

  // ===== Confirmation dialog =====
  function showConfirm(msg, onYes, onNo) {
    showOverlay(
      '<div class="overlay-title"><i class="fas fa-question-circle"></i>确认操作</div>' +
      '<div style="text-align:center;padding:12px 0;font-size:13px;color:#2C3E50;">' + msg + '</div>' +
      '<div class="overlay-actions" style="justify-content:center;">' +
      '<button class="overlay-btn secondary" id="confirmNo">取消</button>' +
      '<button class="overlay-btn danger" id="confirmYes">确认</button>' +
      '</div>'
    );
    document.getElementById('confirmYes').onclick = function() { hideOverlay(); if (onYes) onYes(); };
    document.getElementById('confirmNo').onclick = function() { hideOverlay(); if (onNo) onNo(); };
  }

  // ===== Exports =====
  window.HardwareTest = HardwareTest;
  window.TestState = STATE;
  window.friendlyError = friendlyError;
  window.loadTestResults = loadTestResults;
  window.saveTestResults = saveTestResults;
  window.clearTestResults = clearTestResults;
  window.runAllTests = runAllTests;
  window.showConfirm = showConfirm;
  window.STORAGE_KEY = STORAGE_KEY;

})();
