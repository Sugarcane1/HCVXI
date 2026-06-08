// test-report.js — Test result persistence & report generation
// Reads from localStorage (via test-framework.js), generates HTML/JSON reports.

(function() {
  'use strict';

  // ===== Generate an HTML report string =====
  function generateReportHTML() {
    var results = loadTestResults();
    var ids = Object.keys(results);
    if (ids.length === 0) {
      return '<div style="text-align:center;padding:30px;color:#A0B0C4;">暂无测试结果</div>';
    }

    var passed = 0, failed = 0;
    ids.forEach(function(id) {
      if (results[id].state === 'passed') passed++;
      else if (results[id].state === 'failed') failed++;
    });

    var now = formatNow();
    var model = 'Unknown';
    if (window.g_data && window.g_data.model) model = window.g_data.model;

    var html = '';
    html += '<div style="padding:12px 0;">';
    html += '<div style="font-size:13px;font-weight:600;color:#1A2332;margin-bottom:8px;">' +
      '<i class="fas fa-clipboard-check"></i> 硬件测试报告</div>';
    html += '<div style="font-size:10px;color:#8896A7;margin-bottom:10px;">' +
      '设备: ' + escapeHtml(model) + ' · 时间: ' + now + '</div>';

    // Summary bar
    html += '<div style="display:flex;gap:12px;margin-bottom:10px;">';
    html += '<div style="flex:1;text-align:center;background:#E6F7ED;border-radius:8px;padding:8px;">' +
      '<div style="font-size:18px;font-weight:700;color:#2D8A4E;">' + passed + '</div>' +
      '<div style="font-size:10px;color:#5A6F87;">通过</div></div>';
    html += '<div style="flex:1;text-align:center;background:#FFEBEE;border-radius:8px;padding:8px;">' +
      '<div style="font-size:18px;font-weight:700;color:#D32F2F;">' + failed + '</div>' +
      '<div style="font-size:10px;color:#5A6F87;">失败</div></div>';
    html += '<div style="flex:1;text-align:center;background:#F3F6FC;border-radius:8px;padding:8px;">' +
      '<div style="font-size:18px;font-weight:700;color:#1E3A5F;">' + ids.length + '</div>' +
      '<div style="font-size:10px;color:#5A6F87;">总计</div></div>';
    html += '</div>';

    // Detail rows
    html += '<table style="width:100%;border-collapse:collapse;font-size:11px;">';
    html += '<tr style="border-bottom:1px solid #E8EDF5;color:#8896A7;">' +
      '<th style="text-align:left;padding:6px 4px;">测试项</th>' +
      '<th style="text-align:left;padding:6px 4px;">结果</th>' +
      '<th style="text-align:left;padding:6px 4px;">详情</th></tr>';
    ids.forEach(function(id) {
      var r = results[id];
      var icon = r.state === 'passed' ? '✅' : (r.state === 'failed' ? '❌' : '⬜');
      var color = r.state === 'passed' ? '#2D8A4E' : (r.state === 'failed' ? '#D32F2F' : '#A0B0C4');
      html += '<tr style="border-bottom:1px solid #F3F6FC;">' +
        '<td style="padding:6px 4px;font-weight:500;">' + icon + ' ' + escapeHtml(r.name || id) + '</td>' +
        '<td style="padding:6px 4px;color:' + color + ';font-weight:500;">' +
        (r.state === 'passed' ? '通过' : (r.state === 'failed' ? '失败' : '未测')) + '</td>' +
        '<td style="padding:6px 4px;color:#8896A7;">' + escapeHtml(r.detail || '') + '</td></tr>';
    });
    html += '</table>';

    // Timestamp
    html += '<div style="font-size:9px;color:#C8D3E0;margin-top:10px;text-align:right;">' +
      '报告生成: ' + now + '</div>';
    html += '</div>';

    return html;
  }

  // ===== Generate JSON report =====
  function generateReportJSON() {
    var results = loadTestResults();
    var ids = Object.keys(results);
    var passed = 0, failed = 0;
    ids.forEach(function(id) {
      if (results[id].state === 'passed') passed++;
      else if (results[id].state === 'failed') failed++;
    });

    return {
      device: (window.g_data && window.g_data.model) ? window.g_data.model : 'Unknown',
      timestamp: new Date().toISOString(),
      summary: { total: ids.length, passed: passed, failed: failed },
      tests: results
    };
  }

  // ===== Show report in overlay =====
  function showReport() {
    var html = generateReportHTML();
    html += '<div class="overlay-actions" style="justify-content:space-between;">' +
      '<div>' +
      '<button class="overlay-btn secondary" onclick="window.exportReportJSON()">导出 JSON</button>' +
      '<button class="overlay-btn secondary" onclick="window.printReport()" style="margin-left:6px;">打印</button>' +
      '</div>' +
      '<button class="overlay-btn primary" onclick="hideOverlay()">关闭</button>' +
      '</div>';

    showOverlay(
      '<div class="overlay-title"><i class="fas fa-clipboard-check"></i>硬件测试报告</div>' +
      html
    );
  }

  // ===== Export JSON =====
  function exportReportJSON() {
    var json = generateReportJSON();
    var blob = new Blob([JSON.stringify(json, null, 2)], { type: 'application/json' });
    var url = URL.createObjectURL(blob);
    var a = document.createElement('a');
    a.href = url;
    a.download = 'hwinfo_test_report_' + new Date().toISOString().slice(0, 10) + '.json';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    showToast('报告已导出');
  }

  // ===== Print report =====
  function printReport() {
    var results = loadTestResults();
    var html = '<!DOCTYPE html><html lang="zh"><head><meta charset="UTF-8"><title>硬件测试报告</title>' +
      '<style>body{font-family:sans-serif;max-width:700px;margin:40px auto;color:#333;}' +
      'h1{font-size:20px;border-bottom:2px solid #333;padding-bottom:8px;}' +
      'table{width:100%;border-collapse:collapse;margin-top:12px;}' +
      'th,td{border:1px solid #ddd;padding:8px;text-align:left;}' +
      'th{background:#f5f5f5;}.pass{color:#2D8A4E;}.fail{color:#D32F2F;}' +
      '</style></head><body>' + generateReportHTML() + '</body></html>';

    var w = window.open('', '_blank', 'width=700,height=600');
    if (w) {
      w.document.write(html);
      w.document.close();
      setTimeout(function() { w.print(); }, 500);
    } else {
      showToast('请允许弹出窗口以打印报告');
    }
  }

  // ===== Exports =====
  window.generateReport = showReport;
  window.generateReportHTML = generateReportHTML;
  window.generateReportJSON = generateReportJSON;
  window.exportReportJSON = exportReportJSON;
  window.printReport = printReport;
  window.showReport = showReport;

})();
