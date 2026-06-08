// hw-render.js — Hardware data rendering engine
// All functions are exported on window for cross-module access

(function() {
  'use strict';

  var gpuSvg = '<svg viewBox="0 0 1024 1024" width="14" height="14" style="vertical-align:-1px;"><path d="M947.23 256.21l-28.75-28.75c-13.49-13.49-32.78-26.99-44.23-34.53-6.21-4.1-13.51-6.12-20.95-6.12H268.1v-40.65c0-22.45-18.2-40.65-40.65-40.65H105.52c-22.45 0-40.65 18.2-40.65 40.65v58.57c0 13.93 7.87 26.66 20.32 32.89a36.76 36.76 0 0 1 20.32 32.89v607.32c0 22.45 18.2 40.65 40.65 40.65h81.3c22.45 0 40.65-18.2 40.65-40.65V755.89h40.65v40.65c0 22.45 18.2 40.65 40.65 40.65h81.3c7.41 0 14.34-1.99 20.32-5.45a40.454 40.454 0 0 0 20.32 5.45h81.3c7.41 0 14.34-1.99 20.32-5.45a40.454 40.454 0 0 0 20.32 5.45h81.3c7.41 0 14.34-1.99 20.32-5.45a40.454 40.454 0 0 0 20.32 5.45h81.3c22.45 0 40.65-18.2 40.65-40.65v-40.65h81.3c22.45 0 40.65-18.2 40.65-40.65V284.95c0-10.78-4.28-21.11-11.9-28.74z m-466.56-28.75l-3.56 6.44a20.32 20.32 0 0 1-17.79 10.5h-78.76c-7.4 0-14.21-4.02-17.79-10.5l-3.56-6.44h121.46zM405.3 572.97H268.11V369.73H405.3c-13.55 67.72-13.55 135.52 0 203.24zM227.46 877.83h-81.3V270.51c0-28.51-15.67-54.65-40.65-68.13v-56.22h121.94V877.83zM430.7 715.24h-71.49l3.56-6.44a20.32 20.32 0 0 1 17.79-10.5h78.76c7.4 0 14.21 4.02 17.79 10.5l3.56 6.44H430.7z m-81.29 81.3v-40.65h81.3v40.65h-81.3z m121.94 0v-40.65h81.3v40.65h-81.3z m121.95 0v-40.65h81.3v40.65h-81.3z m203.24 0h-81.3v-40.65h81.3v40.65z m121.94-81.3H527.1l-14.41-26.1a60.984 60.984 0 0 0-53.37-31.49h-78.76a60.96 60.96 0 0 0-53.37 31.49l-14.42 26.1H268.1V613.62h188.62l-6.31-25.25c-19.51-78.06-19.51-155.97 0-234.03l6.31-25.25H268.11V227.46h44.67l14.42 26.1a60.96 60.96 0 0 0 53.37 31.49h78.76c22.19 0 42.64-12.06 53.37-31.49l14.41-26.1h325.67c11.09 7.36 26.84 18.63 36.96 28.74l28.74 28.74v430.3z" fill="currentColor"/><path d="M677.25 268.15a119.974 119.974 0 0 0-5.32 0C560.91 269.58 471.35 360 471.35 471.36c0 112.25 90.99 203.24 203.24 203.24s203.24-90.99 203.24-203.24c0-111.37-89.56-201.79-200.58-203.21z m142.81 161.63c2.54 0.37 4.06 1.23 5.89 3.35 2.39 2.76 5.13 7.72 7.21 15.22 2.03 7.33 2.91 15.23 2.91 21.89s-0.88 14.56-2.91 21.89c-2.08 7.5-4.82 12.47-7.21 15.23-1.83 2.12-3.36 2.97-5.89 3.35-3.33 0.49-11.68 0.48-27.07-6.36-23.19-10.3-28.77-25.04-28.77-34.1 0-9.06 5.58-23.8 28.77-34.1 15.39-6.85 23.74-6.86 27.07-6.37zM814.8 389c-10.91 0.4-23.59 3.45-38.31 9.99-14.99 6.66-26.78 15.51-35.4 25.62a81.748 81.748 0 0 0-21.07-20.69c10.19-8.63 19.12-20.49 25.83-35.58 6.45-14.52 9.5-27.05 9.97-37.87A163.413 163.413 0 0 1 814.8 389z m-99.75 226.71c-0.37 2.54-1.23 4.06-3.35 5.9-2.76 2.39-7.72 5.13-15.22 7.2-7.33 2.03-15.23 2.91-21.89 2.91s-14.56-0.88-21.89-2.91c-7.5-2.08-12.47-4.81-15.22-7.2-2.12-1.84-2.98-3.36-3.35-5.9-0.49-3.32-0.48-11.68 6.36-27.06 10.3-23.19 25.04-28.77 34.1-28.77 9.06 0 23.8 5.57 34.1 28.77 6.84 15.37 6.86 23.73 6.36 27.06zM674.59 512c-22.45 0-40.65-18.2-40.65-40.65s18.2-40.65 40.65-40.65 40.65 18.2 40.65 40.65S697.04 512 674.59 512z m-40.46-187.23c0.37-2.53 1.23-4.06 3.35-5.9 2.76-2.39 7.72-5.13 15.22-7.2 7.33-2.03 15.23-2.91 21.89-2.91s14.56 0.88 21.89 2.91c7.5 2.08 12.47 4.81 15.22 7.2 2.12 1.83 2.98 3.36 3.35 5.9 0.49 3.33 0.48 11.68-6.36 27.07-10.3 23.19-25.04 28.77-34.1 28.77-9.06 0-23.8-5.58-34.1-28.77-6.83-15.39-6.85-23.74-6.36-27.07z m-40.76 5.7c0.47 10.81 3.52 23.35 9.97 37.87 6.71 15.09 15.63 26.94 25.83 35.58-8.23 5.55-15.36 12.58-21.07 20.69-8.62-10.11-20.41-18.96-35.4-25.62-14.71-6.54-27.4-9.59-38.31-9.99a163.335 163.335 0 0 1 58.98-58.53zM529.12 510.7c-2.54-0.37-4.06-1.23-5.89-3.35-2.39-2.76-5.13-7.72-7.21-15.23-2.03-7.33-2.91-15.23-2.91-21.89s0.88-14.56 2.91-21.89c2.08-7.5 4.82-12.47 7.21-15.22 1.83-2.12 3.36-2.97 5.89-3.35 3.33-0.49 11.68-0.47 27.07 6.36 23.19 10.3 28.77 25.04 28.77 34.1 0 9.06-5.58 23.8-28.77 34.1-15.39 6.84-23.74 6.86-27.07 6.37z m3.99 40.81c11.19-0.2 24.29-3.24 39.58-10.03 14.49-6.44 26-14.92 34.54-24.61a81.704 81.704 0 0 0 20.52 20.9c-9.6 8.52-18.01 19.97-24.41 34.36-6.89 15.51-9.91 28.76-10.03 40.06-25.06-14.49-45.89-35.49-60.2-60.68z m222.76 60.69c-0.12-11.3-3.14-24.55-10.03-40.06-6.39-14.39-14.8-25.84-24.41-34.36 8.03-5.67 15-12.75 20.52-20.9 8.54 9.69 20.05 18.17 34.54 24.61 15.29 6.79 28.38 9.83 39.58 10.03a163.408 163.408 0 0 1-60.2 60.68z" fill="currentColor"/></svg>';

  var sectionIcons = {
    '处理器': 'fa-microchip',
    '显卡': 'svg-gpu',
    '显示器': 'fa-display',
    '内存': 'fa-memory',
    '硬盘': 'fa-hdd',
    '电池': 'fa-battery-quarter'
  };

  function getIcon(name) {
    for (var key in sectionIcons) {
      if (name.indexOf(key) !== -1) return sectionIcons[key];
    }
    return 'fa-info-circle';
  }

  function escapeHtml(str) {
    var div = document.createElement('div');
    div.appendChild(document.createTextNode(str));
    return div.innerHTML;
  }

  function formatNow() {
    var now = new Date();
    return now.getFullYear() + '年' +
      String(now.getMonth() + 1).padStart(2, '0') + '月' +
      String(now.getDate()).padStart(2, '0') + '日 ' +
      String(now.getHours()).padStart(2, '0') + ':' +
      String(now.getMinutes()).padStart(2, '0') + ':' +
      String(now.getSeconds()).padStart(2, '0');
  }

  function groupLines(lines) {
    var sections = [];
    var currentSection = null;

    for (var i = 0; i < lines.length; i++) {
      var line = lines[i];
      var label = line.label || '';
      var value = line.value || '';
      var span = line.span || 1;

      if (span === 2 && label) {
        currentSection = { title: label, icon: getIcon(label), rows: [] };
        if (value) currentSection.rows.push({ type: 'value', label: '', value: value });
        sections.push(currentSection);
        continue;
      }

      if (label.indexOf('显卡') !== -1 && span === 1) {
        var gpu = null;
        for (var s = sections.length - 1; s >= 0; s--) {
          if (sections[s].title === '显卡') { gpu = sections[s]; break; }
        }
        if (!gpu) { gpu = { title: '显卡', icon: 'svg-gpu', rows: [] }; sections.push(gpu); }
        gpu.rows.push({ type: 'gpu-sub', label: label, value: value });
        currentSection = gpu;
        continue;
      }

      if (label.indexOf('显示器') !== -1 && span === 1) {
        var mon = null;
        for (var s2 = sections.length - 1; s2 >= 0; s2--) {
          if (sections[s2].title === '显示器') { mon = sections[s2]; break; }
        }
        if (!mon) { mon = { title: '显示器', icon: 'fa-display', rows: [] }; sections.push(mon); }
        mon.rows.push({ type: 'value', label: label, value: value });
        currentSection = mon;
        continue;
      }

      if (label.indexOf('内存') !== -1 && span === 1) {
        var mem = null;
        for (var s3 = sections.length - 1; s3 >= 0; s3--) {
          if (sections[s3].title === '内存') { mem = sections[s3]; break; }
        }
        if (!mem) { mem = { title: '内存', icon: 'fa-memory', rows: [] }; sections.push(mem); }
        mem.rows.push({ type: 'value', label: label, value: value });
        currentSection = mem;
        continue;
      }

      if (label.indexOf('槽位') !== -1 || label.indexOf('板载') !== -1) {
        if (currentSection && currentSection.title === '内存') {
          currentSection.rows.push({ type: 'value', label: label, value: value });
        }
        continue;
      }

      if (label.indexOf('电池') !== -1 && span === 1) {
        var bat = null;
        for (var s4 = sections.length - 1; s4 >= 0; s4--) {
          if (sections[s4].title === '电池') { bat = sections[s4]; break; }
        }
        if (!bat) { bat = { title: '电池', icon: 'fa-battery-quarter', rows: [] }; sections.push(bat); }
        bat.rows.push({ type: 'battery', label: label, value: value });
        currentSection = bat;
        continue;
      }

      if (label === '' && value && currentSection) {
        currentSection.rows.push({ type: 'value', label: '', value: value });
        continue;
      }

      if (label && value && span === 1) {
        if (!currentSection || (currentSection.title !== label && currentSection.rows.length > 0)) {
          currentSection = { title: label, icon: getIcon(label), rows: [] };
          sections.push(currentSection);
        }
        currentSection.rows.push({ type: 'value', label: label, value: value });
      }
    }
    return sections;
  }

  function renderValueWithBadges(value) {
    var html = escapeHtml(value);
    var m;
    m = value.match(/L3\s+(\d+)\s*MB/);
    if (m) html = html.replace(m[0], '<span class="badge badge-blue">' + escapeHtml(m[0]) + '</span>');
    m = value.match(/健康状态\s+(\d+%)/);
    if (m) html = html.replace(m[0], '健康状态 <span class="badge badge-green">' + escapeHtml(m[1]) + ' 良好</span>');
    m = value.match(/通电次数\s+(\d+)\s*次/);
    if (m) html = html.replace(m[0], '<span class="badge">' + escapeHtml(m[0]) + '</span>');
    m = value.match(/通电时间\s+(\d+)\s*小时/);
    if (m) html = html.replace(m[0], '<span class="badge">' + escapeHtml(m[0]) + '</span>');
    return html;
  }

  function renderData(data) {
    var container = document.getElementById('hwContent');
    if (!data || !data.lines || data.lines.length === 0) {
      container.innerHTML =
        '<div class="skeleton-wrap">' +
        '<div class="skeleton-section"><div class="skeleton-title"></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w80"></div></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w60"></div></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w40"></div></div></div>' +
        '<div class="skeleton-section"><div class="skeleton-title"></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w80"></div></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w60"></div></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w40"></div></div></div>' +
        '<div class="skeleton-section"><div class="skeleton-title"></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w80"></div></div>' +
        '<div class="skeleton-row"><div class="skeleton-label"></div><div class="skeleton-value w60"></div></div></div>' +
        '</div>';
      return;
    }
    var sections = groupLines(data.lines);
    var html = '';
    for (var si = 0; si < sections.length; si++) {
      var sec = sections[si];
      html += '<div class="hw-section fade-in" style="animation-delay:' + (si * 0.06) + 's">';
      var iconHtml = sec.icon === 'svg-gpu' ? gpuSvg : '<i class="fas ' + sec.icon + '"></i>';
      html += '<div class="hw-section-title">' + iconHtml + ' ' + escapeHtml(sec.title) + '</div>';
      for (var ri = 0; ri < sec.rows.length; ri++) {
        var row = sec.rows[ri];
        if (row.type === 'gpu-sub') {
          html += '<div class="hw-sub-block"><div class="sub-title">' + escapeHtml(row.label) + '</div>';
          html += '<div class="hw-row"><div class="hw-label"></div><div class="hw-value">' + renderValueWithBadges(row.value) + '</div></div></div>';
        } else if (row.type === 'battery') {
          html += '<div class="hw-row"><div class="hw-label">' + escapeHtml(row.label) + '</div>';
          if (row.value.indexOf('未检测到') !== -1) {
            html += '<div class="hw-value"><i class="fas fa-minus-circle battery-empty"></i>' + escapeHtml(row.value) + ' (台式机/无电池)</div>';
          } else {
            html += '<div class="hw-value">' + renderValueWithBadges(row.value) + '</div>';
          }
          html += '</div>';
        } else {
          html += '<div class="hw-row"><div class="hw-label">' + escapeHtml(row.label) + '</div>';
          html += '<div class="hw-value">' + renderValueWithBadges(row.value) + '</div></div>';
        }
      }
      html += '</div>';
    }
    container.innerHTML = html;
  }

  function showToast(msg) {
    var el = document.getElementById('toast');
    el.textContent = msg;
    el.classList.remove('celebrate');
    if (msg.indexOf('🎉') !== -1) el.classList.add('celebrate');
    el.classList.add('show');
    setTimeout(function() { el.classList.remove('show', 'celebrate'); }, 2500);
  }

  function setTestStatus(id, status, text) {
    var el = document.getElementById('status' + id);
    if (!el) return;
    el.className = 'test-status ' + status;
    el.textContent = text;

    // Card effects: pulse when testing, flash on pass/fail
    var card = document.getElementById('test' + id);
    if (card) {
      card.classList.remove('testing-highlight', 'flash-pass', 'flash-fail');
      if (status === 'testing') {
        card.classList.add('testing-highlight');
      } else if (status === 'pass') {
        card.classList.add('flash-pass');
        setTimeout(function() { card.classList.remove('flash-pass'); }, 700);
      } else if (status === 'fail') {
        card.classList.add('flash-fail');
        setTimeout(function() { card.classList.remove('flash-fail'); }, 700);
      }
    }
  }

  function setTestBtn(id, label, cls, disabled) {
    var btn = document.getElementById('btn' + id);
    if (!btn) return;
    btn.textContent = label;
    btn.className = 'test-btn ' + cls;
    btn.disabled = !!disabled;
  }

  function showOverlay(html) {
    var card = document.getElementById('overlayCard');
    card.innerHTML = html;
    document.getElementById('testOverlay').classList.add('active');
  }

  function hideOverlay() {
    document.getElementById('testOverlay').classList.remove('active');
  }

  var activeStreams = [];

  function stopAllStreams() {
    for (var i = 0; i < activeStreams.length; i++) {
      var tracks = activeStreams[i].getTracks();
      for (var j = 0; j < tracks.length; j++) tracks[j].stop();
    }
    activeStreams = [];
  }

  function postToHost(msg) {
    try {
      if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
        window.chrome.webview.postMessage(msg);
      }
    } catch (e) {}
  }

  // Export all functions to window
  window.renderData = renderData;
  window.renderValueWithBadges = renderValueWithBadges;
  window.groupLines = groupLines;
  window.escapeHtml = escapeHtml;
  window.formatNow = formatNow;
  window.updateClock = function() {
    var el = document.getElementById('headerDatetime');
    if (el) el.textContent = formatNow();
  };
  window.showToast = showToast;
  window.setTestStatus = setTestStatus;
  window.setTestBtn = setTestBtn;
  window.showOverlay = showOverlay;
  window.hideOverlay = hideOverlay;
  window.stopAllStreams = stopAllStreams;
  window.postToHost = postToHost;
  window.activeStreams = activeStreams;
  window.g_data = null;

})();
