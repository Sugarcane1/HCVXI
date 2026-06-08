// keyboard.js — Virtual keyboard test with SVG rendering
// Exact SVG layout replicated from standard ANSI 104-key keyboard.

(function() {
  'use strict';

  // ===== Cherry arrow paths (exact from SVG source) =====
  var CHERRY_UP    = 'M0.087-1.235c-0.986-2.093-2.031-4.198-3.179-6.208C-3.988-9.014-4.684-7.73-5.234-6.687c-0.594,1.128-1.184,2.258-1.762,3.394C-7.44-2.417-8.488-0.997-8.253,0.013c0.438,1.885,2.808-0.842,3.382-1.354c0,2.758-0.034,5.519,0,8.277c0.007,0.535,0.011,1.131,0.656,1.316c0.702,0.202,1.112-0.401,1.159-1.016c0.11-1.42,0-2.892,0-4.316c0-1.42,0-2.84,0-4.261c0.66,0.587,1.411,1.501,2.21,1.886C0.322,1.107,0.54-0.408,0.087-1.235C-0.898-3.325,0.246-0.943,0.087-1.235z';
  var CHERRY_DOWN  = 'M-8.014,1.235c0.753,1.599,1.579,3.164,2.399,4.729c0.256,0.488,0.898,2.321,1.651,2.321c0.699-0.074,1.133-1.335,1.405-1.854c0.776-1.475,1.549-2.952,2.283-4.448c0.315-0.642,1.286-2.986-0.325-2.632c-0.847,0.186-1.821,1.424-2.456,1.989c0-2.758,0.034-5.519,0-8.277c-0.007-0.535-0.011-1.131-0.656-1.316c-0.702-0.202-1.112,0.401-1.159,1.016c-0.11,1.42,0,2.892,0,4.316c0,1.42,0,2.84,0,4.261c-0.66-0.587-1.411-1.501-2.21-1.886C-8.249-1.107-8.467,0.408-8.014,1.235C-7.029,3.325-8.174,0.943-8.014,1.235z';
  var CHERRY_LEFT  = 'M-1.234-0.165c-2.093,0.986-4.198,2.031-6.208,3.179C-9.013,3.91-7.729,4.606-6.686,5.156c-1.128,0.594-2.258,1.184-3.395,1.762c0.875,0.445,2.296,1.492,3.306,1.257c1.885-0.438-0.842-2.808-1.354-3.382c2.758,0,5.519,0.034,8.277,0c0.535-0.007,1.131-0.011,1.316-0.656c0.202-0.702-0.401-1.112-1.016-1.159c-1.42-0.11-2.892,0-4.316,0c-1.42,0-2.84,0-4.261,0c0.587-0.66,1.501-1.411,1.886-2.21C1.108-0.4-0.407-0.618-1.234-0.165C-3.324,0.82-0.942-0.324-1.234-0.165z';
  var CHERRY_RIGHT = 'M1.235,7.935c2.093-0.986,4.198-2.031,6.208-3.179C9.013,3.86,7.73,3.164,6.687,2.614c-1.128-0.594-2.258-1.184-3.395-1.762c-0.875-0.445-2.296-1.492-3.306-1.257c-1.885,0.438,0.842,2.808,1.354,3.382c-2.758,0-5.519-0.034-8.277,0c-0.535,0.007-1.131,0.011-1.316,0.656c-0.202,0.702,0.401,1.112,1.016,1.159c1.42,0.11,2.892,0,4.316,0c1.42,0,2.84,0,4.261,0c-0.587,0.66-1.501,1.411-1.886,2.21C-1.107,8.171,0.408,8.388,1.235,7.935C3.325,6.95,0.943,8.095,1.235,7.935z';

  // ===== Key geometry =====
  // Widths for rect elements
  var WR = {
    '1u':   { w: 51.85826,  h: 51.85826 },
    '1u25': { w: 65.322825, h: 51.85826 },
    '1u5':  { w: 78.78739,  h: 51.85826 },
    '1u75': { w: 92.25196,  h: 51.85826 },
    '2u':   { w: 105.71652, h: 51.85826 },
    '2u25': { w: 119.181085, h: 51.85826 },
    '2u75': { w: 146.110215, h: 51.85826 },
    '6u25': { w: 334.614125, h: 51.85826 },
    '2h':   { w: 51.85826,  h: 105.71652 }
  };

  // Row Y coordinates
  var ROW_Y = [0, 80.78739, 134.64565, 188.50391000000002, 242.36217, 296.22043];

  // ===== Keyboard layout — exact coordinates from source SVG =====
  // Each entry: [x, size, dataKey, ...labels]
  // label: [text, yOffset, fontSize, anchor]
  //   anchor: 'c'=central, 'b'=baseline, 'h'=hanging
  // special: 'arrow:up' etc.

  var ROWS = [
    // Row 0: Function keys
    [
      [0,           '1u', 'Esc',    ['ESC',0,10,'c']],
      [107.71652,   '1u', 'F1',     ['F1',0,12,'c']],
      [161.57478,   '1u', 'F2',     ['F2',0,12,'c']],
      [215.43304,   '1u', 'F3',     ['F3',0,12,'c']],
      [269.2913,    '1u', 'F4',     ['F4',0,12,'c']],
      [350.07869,   '1u', 'F5',     ['F5',0,12,'c']],
      [403.93695,   '1u', 'F6',     ['F6',0,12,'c']],
      [457.79521,   '1u', 'F7',     ['F7',0,12,'c']],
      [511.65347,   '1u', 'F8',     ['F8',0,12,'c']],
      [592.44086,   '1u', 'F9',     ['F9',0,12,'c']],
      [646.29912,   '1u', 'F10',    ['F10',0,12,'c']],
      [700.15738,   '1u', 'F11',    ['F11',0,12,'c']],
      [754.01564,   '1u', 'F12',    ['F12',0,12,'c']],
      [821.338465,  '1u', 'PrtSc',  ['PRTSC',0,9,'c']],
      [875.196725,  '1u', 'ScrLk',  ['LOCK',0,10,'c']],
      [929.054985,  '1u', 'Pause',  ['PAUSE',0,9,'c']]
    ],
    // Row 1: Number row + editing + numpad top
    [
      [0,           '1u', '`',  ['`',13.92913,15,'b'], ['~',-13.92913,14,'h']],
      [53.85826,    '1u', '1',  ['1',13.92913,15,'b'], ['!',-13.92913,14,'h']],
      [107.71652,   '1u', '2',  ['2',13.92913,15,'b'], ['@',-13.92913,14,'h']],
      [161.57478,   '1u', '3',  ['3',13.92913,15,'b'], ['#',-13.92913,14,'h']],
      [215.43304,   '1u', '4',  ['4',13.92913,15,'b'], ['$',-13.92913,14,'h']],
      [269.2913,    '1u', '5',  ['5',13.92913,15,'b'], ['%',-13.92913,14,'h']],
      [323.14956,   '1u', '6',  ['6',13.92913,15,'b'], ['^',-13.92913,14,'h']],
      [377.00782,   '1u', '7',  ['7',13.92913,15,'b'], ['&',-13.92913,14,'h']],
      [430.86608,   '1u', '8',  ['8',13.92913,15,'b'], ['*',-13.92913,14,'h']],
      [484.72434,   '1u', '9',  ['9',13.92913,15,'b'], ['(',-13.92913,14,'h']],
      [538.5826,    '1u', '0',  ['0',13.92913,15,'b'], [')',-13.92913,14,'h']],
      [592.44086,   '1u', '-',  ['-',13.92913,15,'b'], ['_',-13.92913,14,'h']],
      [646.29912,   '1u', '=',  ['=',13.92913,15,'b'], ['+',-13.92913,14,'h']],
      [700.15738,   '2u', 'Backspace', ['BACKSPACE',0,10,'c']],
      [821.338465,  '1u', 'Ins',  ['INS',0,10,'c']],
      [875.196725,  '1u', 'Home', ['HOME',0,10,'c']],
      [929.054985,  '1u', 'PgUp', ['PGUP',0,10,'c']],
      [996.37781,   '1u', 'NumLk',[['NUM',0,10,'c','tspan'],['LOCK',0,10,'c','tspan2']]],
      [1050.23607,  '1u', '/',  ['/',0,15,'c']],
      [1104.09433,  '1u', '*',  ['*',0,15,'c']],
      [1157.95259,  '1u', '-',  ['-',0,15,'c']]
    ],
    // Row 2: QWERTY row + editing + numpad
    [
      [0,           '1u5', 'Tab',  ['TAB',0,10,'c']],
      [80.78739,    '1u',  'Q',    ['Q',-13.92913,14,'h']],
      [134.64565,   '1u',  'W',    ['W',-13.92913,14,'h']],
      [188.50391,   '1u',  'E',    ['E',-13.92913,14,'h']],
      [242.36217,   '1u',  'R',    ['R',-13.92913,14,'h']],
      [296.22043,   '1u',  'T',    ['T',-13.92913,14,'h']],
      [350.07869,   '1u',  'Y',    ['Y',-13.92913,14,'h']],
      [403.93695,   '1u',  'U',    ['U',-13.92913,14,'h']],
      [457.79521,   '1u',  'I',    ['I',-13.92913,14,'h']],
      [511.65347,   '1u',  'O',    ['O',-13.92913,14,'h']],
      [565.51173,   '1u',  'P',    ['P',-13.92913,14,'h']],
      [619.36999,   '1u',  '[',    ['[',13.92913,15,'b'], ['{',-13.92913,14,'h']],
      [673.22825,   '1u',  ']',    [']',13.92913,15,'b'], ['}',-13.92913,14,'h']],
      [727.08651,   '1u5', '\\',   ['\\',13.92913,15,'b'], ['|',-13.92913,14,'h']],
      [821.338465,  '1u',  'Del',  ['DEL',0,10,'c']],
      [875.196725,  '1u',  'End',  ['END',0,10,'c']],
      [929.054985,  '1u',  'PgDn', ['PGDN',0,10,'c']],
      [996.37781,   '1u',  'num7', ['7',-13.92913,15,'h']],
      [1050.23607,  '1u',  'num8', ['8',-13.92913,15,'h']],
      [1104.09433,  '1u',  'num9', ['9',-13.92913,15,'h']],
      [1157.95259,  '2h',  '+',    ['+',26.92913,15,'cv']]
    ],
    // Row 3: Home row + numpad
    [
      [0,           '1u75', 'CapsLock', ['CAPS LOCK',0,10,'c']],
      [94.251955,   '1u',   'A',    ['A',-13.92913,14,'h']],
      [148.110215,  '1u',   'S',    ['S',-13.92913,14,'h']],
      [201.968475,  '1u',   'D',    ['D',-13.92913,14,'h']],
      [255.826735,  '1u',   'F',    ['F',-13.92913,14,'h']],
      [309.684995,  '1u',   'G',    ['G',-13.92913,14,'h']],
      [363.543255,  '1u',   'H',    ['H',-13.92913,14,'h']],
      [417.401515,  '1u',   'J',    ['J',-13.92913,14,'h']],
      [471.259775,  '1u',   'K',    ['K',-13.92913,14,'h']],
      [525.118035,  '1u',   'L',    ['L',-13.92913,14,'h']],
      [578.976295,  '1u',   ';',    [';',13.92913,15,'b'], [':',-13.92913,14,'h']],
      [632.834555,  '1u',   "'",    ["'",13.92913,15,'b'], ['"',-13.92913,14,'h']],
      [686.692815,  '2u25', 'Enter',['ENTER',0,10,'c']],
      [996.37781,   '1u',   'num4', ['4',-13.92913,15,'h']],
      [1050.23607,  '1u',   'num5', ['5',-13.92913,15,'h']],
      [1104.09433,  '1u',   'num6', ['6',-13.92913,15,'h']]
    ],
    // Row 4: Shift row + arrow up + numpad
    [
      [0,           '2u25', 'Shift', ['SHIFT',0,10,'c']],
      [121.181085,  '1u',   'Z',    ['Z',-13.92913,14,'h']],
      [175.039345,  '1u',   'X',    ['X',-13.92913,14,'h']],
      [228.897605,  '1u',   'C',    ['C',-13.92913,14,'h']],
      [282.755865,  '1u',   'V',    ['V',-13.92913,14,'h']],
      [336.614125,  '1u',   'B',    ['B',-13.92913,14,'h']],
      [390.472385,  '1u',   'N',    ['N',-13.92913,14,'h']],
      [444.330645,  '1u',   'M',    ['M',-13.92913,14,'h']],
      [498.188905,  '1u',   ',',    [',',13.92913,15,'b'], ['<',-13.92913,14,'h']],
      [552.047165,  '1u',   '.',    ['.',13.92913,15,'b'], ['>',-13.92913,14,'h']],
      [605.905425,  '1u',   '/',    ['/',13.92913,15,'b'], ['?',-13.92913,14,'h']],
      [659.763685,  '2u75', 'Shift', ['SHIFT',0,10,'c']],
      [875.196725,  '1u',   'ArrowUp',    'arrow:up'],
      [996.37781,   '1u',   'num1', ['1',-13.92913,15,'h']],
      [1050.23607,  '1u',   'num2', ['2',-13.92913,15,'h']],
      [1104.09433,  '1u',   'num3', ['3',-13.92913,15,'h']],
      [1157.95259,  '2h',   'Enter',['ENTER',26.92913,9,'cv']]
    ],
    // Row 5: Bottom modifiers + arrows + numpad
    [
      [0,           '1u25', 'Ctrl', ['CTRL',0,10,'c']],
      [67.322825,   '1u25', 'Win',  ['WIN',0,10,'c']],
      [134.64565,   '1u25', 'Alt',  ['ALT',0,10,'c']],
      [201.968475,  '6u25', ' ',    []],  // spacebar — no text
      [538.5826,    '1u25', 'Alt',  ['ALT',0,10,'c']],
      [605.905425,  '1u25', 'Win',  ['WIN',0,10,'c']],
      [673.22825,   '1u25', 'Menu', ['MENU',0,10,'c']],
      [740.551075,  '1u25', 'Ctrl', ['CTRL',0,10,'c']],
      [821.338465,  '1u',   'ArrowLeft',  'arrow:left'],
      [875.196725,  '1u',   'ArrowDown',  'arrow:down'],
      [929.054985,  '1u',   'ArrowRight', 'arrow:right'],
      [996.37781,   '2u',   'num0', ['0',-13.92913,15,'h']],
      [1104.09433,  '1u',   'num.', ['.',-13.92913,15,'h']]
    ]
  ];

  // ===== SVG defs =====
  function svgDefs() {
    return '<defs>' +
      '<style type="text/css">' +
      '.key-base-color{fill:#2D2D2D;}' +
      '.key-top-color{fill:#E8E6E1;font-weight:500;text-anchor:start;font-family:\'zfArial\',sans-serif;}' +
      '</style>' +
      // Clip paths
      '<clipPath id="dsa-top-mask-1"><rect x="-25.8" y="-25.8" width="51.85826" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-2"><rect x="-25.8" y="-25.8" width="105.71652" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-1-5"><rect x="-25.8" y="-25.8" width="78.78739" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-1-75"><rect x="-25.8" y="-25.8" width="92.25196" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-2-25"><rect x="-25.8" y="-25.8" width="119.181085" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-2-75"><rect x="-25.8" y="-25.8" width="146.110215" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-6-25"><rect x="-25.8" y="-25.8" width="334.614125" height="51.85826" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-2h"><rect x="-25.8" y="-25.8" width="51.85826" height="105.71652" rx="1.5"/></clipPath>' +
      '<clipPath id="dsa-top-mask-1-25"><rect x="-25.8" y="-25.8" width="65.322825" height="51.85826" rx="1.5"/></clipPath>' +
      // Keyside paths
      '<path id="keyside-1u" d="M-18.8-18.8L18.8-18.8L18.8 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-2u" d="M-18.8-18.8L70.85826-18.8L70.85826 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-1u5" d="M-18.8-18.8L44.0-18.8L44.0 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-1u75" d="M-18.8-18.8L52.3-18.8L52.3 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-2u25" d="M-18.8-18.8L79.6-18.8L79.6 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-2u75" d="M-18.8-18.8L106.5-18.8L106.5 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-6u25" d="M-18.8-18.8L295.0-18.8L295.0 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-1u2h" d="M-18.8-18.8L18.8-18.8L18.8 70.85826L-18.8 70.85826Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '<path id="keyside-1u25" d="M-18.8-18.8L32.5-18.8L32.5 18.8L-18.8 18.8Z" fill="none" stroke="#3D3D3D" stroke-width="1.2"/>' +
      '</defs>';
  }

  // Map size to clip-path id
  function clipId(size) {
    if (size === '2h') return 'dsa-top-mask-2h';
    return 'dsa-top-mask-' + size.replace('u','');
  }

  function keysideId(size) {
    if (size === '2h') return 'keyside-1u2h';
    return 'keyside-' + size;
  }

  // Render one key's labels (text elements inside clip group)
  function renderLabels(labels, size) {
    var h = '';
    // Detect if labels use tspan (need wrapping <text>)
    var useTspan = false;
    for (var i = 0; i < labels.length; i++) {
      if (labels[i][4] === 'tspan' || labels[i][4] === 'tspan2') { useTspan = true; break; }
    }
    if (useTspan) {
      h += '<text x="-13.92913" y="0" dy="0" alignment-baseline="central" font-family="zfArial" font-size="10" class="key-top-color">';
      for (var i = 0; i < labels.length; i++) {
        var L = labels[i];
        if (L[4] === 'tspan2') {
          h += '<tspan x="-13.92913" dy="1em">' + esc(L[0]) + '</tspan>';
        } else {
          h += '<tspan x="-13.92913" dy="0">' + esc(L[0]) + '</tspan>';
        }
      }
      h += '</text>';
      return h;
    }
    for (var i = 0; i < labels.length; i++) {
      var L = labels[i];
      if (!L || L.length === 0) continue;
      if (L[4] === 'cv') {
        h += '<text x="-13.92913" y="' + L[1] + '" dy="0" alignment-baseline="central" font-family="zfArial" font-size="' + L[2] + '" class="key-top-color">' + esc(L[0]) + '</text>';
      } else {
        var anchor = L[4] === 'b' ? 'baseline' : L[4] === 'h' ? 'hanging' : 'central';
        h += '<text x="-13.92913" y="' + L[1] + '" dy="0" alignment-baseline="' + anchor + '" font-family="zfArial" font-size="' + L[2] + '" class="key-top-color">' + esc(L[0]) + '</text>';
      }
    }
    return h;
  }

  // Render a cherry arrow key
  function renderArrow(dir) {
    var path;
    if (dir === 'up') path = CHERRY_UP;
    else if (dir === 'down') path = CHERRY_DOWN;
    else if (dir === 'left') path = CHERRY_LEFT;
    else path = CHERRY_RIGHT;
    return '<g class="key-top-color"><path class="cherry-' + dir + '" d="' + path + '" transform="translate(0,0)"/></g>';
  }

  function esc(s) {
    return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
  }

  // Render a single key <g> element
  function renderKey(def, rowY) {
    var x = def[0], size = def[1], dataKey = def[2];
    var dim = WR[size];
    var cid = clipId(size);
    var kid = keysideId(size);

    var y = rowY;

    var html = '<g transform="translate(' + x + ',' + y + ')" data-key="' + esc(dataKey) + '" class="kb-svg-key">';
    html += '<rect x="-25.8" y="-25.8" rx="1.5" width="' + dim.w + '" height="' + dim.h + '" class="key-base-color"/>';
    html += '<use href="#' + kid + '"/>';

    if (typeof def[3] === 'string' && def[3].indexOf('arrow:') === 0) {
      // Arrow key
      var dir = def[3].substring(6);
      html += '<g clip-path="url(#' + cid + ')">' + renderArrow(dir) + '</g>';
    } else {
      // Text labels
      var labels = [];
      for (var i = 3; i < def.length; i++) {
        labels.push(def[i]);
      }
      if (labels.length === 1 && Array.isArray(labels[0]) && Array.isArray(labels[0][0])) {
        // Nested array for NumLock tspan
        labels = labels[0];
      }
      var labelHtml = renderLabels(labels, size);
      html += '<g clip-path="url(#' + cid + ')">' + labelHtml + '</g>';
    }

    html += '</g>';
    return html;
  }

  // ===== Build full keyboard SVG HTML =====
  function buildKeyboardHTML() {
    var html = '<div class="kb-board" id="kbBoard">';
    html += '<svg version="1.1" xmlns="http://www.w3.org/2000/svg" viewBox="-20 -20 1220 370" xml:space="preserve" class="keyboard-svg">';
    html += svgDefs();
    html += '<g id="kb" transform="translate(8,8)">';

    for (var r = 0; r < ROWS.length; r++) {
      var rowY = ROW_Y[r];
      for (var c = 0; c < ROWS[r].length; c++) {
        html += renderKey(ROWS[r][c], rowY);
      }
    }

    html += '</g></svg>';
    html += '</div>';
    return html;
  }

  // ===== KeyboardTest (HardwareTest subclass) =====
  function KeyboardTest() {
    HardwareTest.call(this, 'Key', '键盘按键', { timeoutMs: 180000 });
    this._pressedKeys = {};
    this._totalCount = 0;
    this._active = false;
  }
  KeyboardTest.prototype = Object.create(HardwareTest.prototype);
  KeyboardTest.prototype.constructor = KeyboardTest;

  KeyboardTest.prototype.onStart = function() {
    var self = this;
    self._active = true;
    self._pressedKeys = {};
    self._totalCount = 0;

    var kbHTML;
    try { kbHTML = buildKeyboardHTML(); } catch (e) { self.fail('键盘渲染失败: ' + (e.message || e)); return; }
    window.showOverlay(
      '<div class="overlay-title"><i class="fas fa-keyboard"></i>键盘按键测试</div>' +
      '<div class="kb-hint">按下键盘上的每个按键，已按下的键会常亮显示 · 点击下方按钮完成测试</div>' +
      kbHTML +
      '<div class="kb-stats">' +
      '<span id="kbLastKey">等待按键...</span>' +
      '<div style="display:flex;align-items:center;gap:8px;">' +
      '<span id="kbCount">已测试 0 键</span>' +
      '<div class="kb-progress"><div class="kb-progress-fill" id="kbProgress" style="width:0%"></div></div>' +
      '</div></div>' +
      '<div class="overlay-actions">' +
      '<button class="overlay-btn success" onclick="hwt_mgr.tests[\'Key\'].stop(true)">完成测试</button>' +
      '</div>'
    );

    self._onKeyDown = function(e) { self._handleKeyDown(e); };
    self._onKeyUp = function(e) { self._handleKeyUp(e); };
    document.addEventListener('keydown', self._onKeyDown, {capture: true});
    document.addEventListener('keyup', self._onKeyUp, {capture: true});
  };

  KeyboardTest.prototype._handleKeyDown = function(e) {
    if (!this._active) return;
    e.preventDefault();
    e.stopPropagation();

    var keyName = this._getKeyName(e);
    if (!this._pressedKeys[keyName]) {
      this._pressedKeys[keyName] = true;
      this._totalCount++;
    }

    var virtualKey = this._findVirtualKey(e);
    if (virtualKey) virtualKey.classList.add('pressed');

    var lastEl = document.getElementById('kbLastKey');
    if (lastEl) lastEl.textContent = '按键: ' + keyName;
    var countEl = document.getElementById('kbCount');
    if (countEl) countEl.textContent = '已测试 ' + this._totalCount + ' 键';
    var progEl = document.getElementById('kbProgress');
    if (progEl) progEl.style.width = Math.min(100, this._totalCount * 2) + '%';
  };

  KeyboardTest.prototype._handleKeyUp = function(e) {
    // Keys stay lit persistently — no removal on keyup
  };

  KeyboardTest.prototype._getKeyName = function(e) {
    var name = e.key;
    var code = e.code;
    if (code === 'Space') return 'Space';
    if (code === 'ControlLeft') return '左Ctrl';
    if (code === 'ControlRight') return '右Ctrl';
    if (code === 'ShiftLeft') return '左Shift';
    if (code === 'ShiftRight') return '右Shift';
    if (code === 'AltLeft') return 'Alt';
    if (code === 'AltRight') return 'AltGr';
    if (code === 'MetaLeft') return 'Win';
    if (code === 'Convert') return '変換';
    if (code === 'NonConvert') return '無変換';
    if (code === 'KanaMode') return 'カタカナ';
    if (code === 'ScrollLock') return 'ScrLk';
    if (code === 'Pause') return 'Pause';
    if (code === 'PrintScreen') return 'PrtSc';
    if (code === 'Insert') return 'Ins';
    if (code === 'Delete') return 'Del';
    if (code === 'Home') return 'Home';
    if (code === 'End') return 'End';
    if (code === 'PageUp') return 'PgUp';
    if (code === 'PageDown') return 'PgDn';
    if (code === 'NumLock') return 'NumLk';
    if (code === 'NumpadDivide') return 'Num /';
    if (code === 'NumpadMultiply') return 'Num *';
    if (code === 'NumpadSubtract') return 'Num -';
    if (code === 'NumpadAdd') return 'Num +';
    if (code === 'NumpadEnter') return 'Num Enter';
    if (code === 'NumpadDecimal') return 'Num .';
    if (code && code.indexOf('Numpad') === 0 && code.length > 6) {
      return 'Num ' + code.substring(6);
    }
    if (name === ' ') return 'Space';
    if (name.length === 1 && name >= 'A' && name <= 'Z') return name.toUpperCase();
    if (name.length === 1) return name;
    if (code === 'ArrowUp') return '↑';
    if (code === 'ArrowDown') return '↓';
    if (code === 'ArrowLeft') return '←';
    if (code === 'ArrowRight') return '→';
    return name;
  };

  KeyboardTest.prototype._findVirtualKey = function(e) {
    var code = e.code;
    var codeMap = {
      'Space': ' ',
      'Tab': 'Tab', 'CapsLock': 'CapsLock', 'Enter': 'Enter',
      'Backspace': 'Backspace', 'Escape': 'Esc',
      'Delete': 'Del', 'Insert': 'Ins',
      'Home': 'Home', 'End': 'End',
      'PageUp': 'PgUp', 'PageDown': 'PgDn',
      'ScrollLock': 'ScrLk', 'Pause': 'Pause',
      'PrintScreen': 'PrtSc',
      'NumLock': 'NumLk',
      'F1':'F1','F2':'F2','F3':'F3','F4':'F4','F5':'F5','F6':'F6',
      'F7':'F7','F8':'F8','F9':'F9','F10':'F10','F11':'F11','F12':'F12',
      'ContextMenu': 'Menu',
      'ControlLeft': 'Ctrl', 'ControlRight': 'Ctrl',
      'ShiftLeft': 'Shift', 'ShiftRight': 'Shift',
      'AltLeft': 'Alt', 'AltRight': 'AltGr',
      'MetaLeft': 'Win', 'MetaRight': 'Win',
      'ArrowUp': 'ArrowUp', 'ArrowDown': 'ArrowDown',
      'ArrowLeft': 'ArrowLeft', 'ArrowRight': 'ArrowRight',
      'Numpad0': 'num0', 'Numpad1': 'num1', 'Numpad2': 'num2',
      'Numpad3': 'num3', 'Numpad4': 'num4', 'Numpad5': 'num5',
      'Numpad6': 'num6', 'Numpad7': 'num7', 'Numpad8': 'num8',
      'Numpad9': 'num9', 'NumpadDecimal': 'num.',
      'NumpadDivide': '/', 'NumpadMultiply': '*',
      'NumpadSubtract': '-', 'NumpadAdd': '+',
      'NumpadEnter': 'Enter'
    };

    var mapped = codeMap[code];
    var searchKey = null;
    if (mapped !== undefined) {
      if (mapped === null) return null;
      searchKey = mapped;
    } else if (code && code.indexOf('Key') === 0 && code.length === 4) {
      searchKey = code.charAt(3);
    } else if (code && code.indexOf('Digit') === 0 && code.length === 6) {
      searchKey = code.charAt(5);
    } else if (e.key.length === 1) {
      searchKey = e.key;
    } else if (code && /^F\d+$/.test(code)) {
      searchKey = code; // F13, etc. (F1-F12 already in codeMap)
    }

    if (!searchKey) return null;

    // CSS-escape the search key for safe querySelector use
    var cssKey = searchKey.replace(/\\/g, '\\\\').replace(/"/g, '\\"');
    var el;
    try {
      el = document.querySelector('[data-key="' + cssKey + '"]');
    } catch (qsErr) {
      el = null;
    }
    if (!el) {
      var allKeys = document.querySelectorAll('[data-key]');
      var lower = searchKey.toLowerCase();
      for (var i = 0; i < allKeys.length; i++) {
        if (allKeys[i].getAttribute('data-key').toLowerCase() === lower) {
          el = allKeys[i];
          break;
        }
      }
    }
    return el;
  };

  KeyboardTest.prototype.onStop = function(pass) {
    this._active = false;
    if (this._onKeyDown) document.removeEventListener('keydown', this._onKeyDown);
    if (this._onKeyUp) document.removeEventListener('keyup', this._onKeyUp);
    this._onKeyDown = null;
    this._onKeyUp = null;
    window.hideOverlay();
    if (pass) {
      this.resultDetail = this._totalCount + ' 键';
    }
  };

  KeyboardTest.prototype.onCleanup = function() {
    this.onStop(false);
  };

  // ===== Register with test manager =====
  if (window.hwt_mgr) {
    window.hwt_mgr.register(new KeyboardTest());
  }

  window.startKeyboardTest = function() {
    if (window.hwt_mgr && window.hwt_mgr.tests['Key']) {
      window.hwt_mgr.tests['Key'].start();
    }
  };
  window.stopKeyboardTest = function(pass) {
    if (window.hwt_mgr && window.hwt_mgr.tests['Key']) {
      window.hwt_mgr.tests['Key'].stop(pass);
    }
  };

})();