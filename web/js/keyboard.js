// The drawn keyboard and the target picker.
// Keyboard picture. Rows of [name, width in units] or a number (blank gap).
// A Mac keyboard has F13-F15 where a PC one has PrintScreen/ScrollLock/Pause, and Control, Option,
// Command in that order. Keys macOS has no code for (F21-F24, the browser keys...) are left out there.
function kbRows() {
  const main = [
    isMac
      ? [['esc',1], 1, ['f1',1], ['f2',1], ['f3',1], ['f4',1], .5, ['f5',1], ['f6',1], ['f7',1], ['f8',1], .5, ['f9',1], ['f10',1], ['f11',1], ['f12',1], .5, ['f13',1], ['f14',1], ['f15',1]]
      : [['esc',1], 1, ['f1',1], ['f2',1], ['f3',1], ['f4',1], .5, ['f5',1], ['f6',1], ['f7',1], ['f8',1], .5, ['f9',1], ['f10',1], ['f11',1], ['f12',1], .5, ['printscreen',1], ['scrolllock',1], ['pause',1]],
    [..."`1234567890-=".split('').map(c => [c,1]), ['backspace',2], .5, ['insert',1], ['home',1], ['pageup',1]],
    [['tab',1.5], ..."qwertyuiop[]".split('').map(c => [c,1]), ['\\',1.5], .5, ['delete',1], ['end',1], ['pagedown',1]],
    [['capslock',1.75], ..."asdfghjkl;'".split('').map(c => [c,1]), ['enter',2.25]],
    [['lshift',2.25], ..."zxcvbnm,./".split('').map(c => [c,1]), ['rshift',2.75], 1.5, ['up',1]],
    isMac
      ? [['lctrl',1.25], ['lalt',1.25], ['lwin',1.25], ['space',6.25], ['rwin',1.25], ['ralt',1.25], ['apps',1.25], ['rctrl',1.25], .5, ['left',1], ['down',1], ['right',1]]
      : [['lctrl',1.25], ['lwin',1.25], ['lalt',1.25], ['space',6.25], ['ralt',1.25], ['rwin',1.25], ['apps',1.25], ['rctrl',1.25], .5, ['left',1], ['down',1], ['right',1]],
    'sep',
  ];
  if (isMac) return [...main,
    [['f16',1], ['f17',1], ['f18',1], ['f19',1], ['f20',1]],
    [['volumedown',1.5], ['volumeup',1.5], ['mute',1.5], ['playpause',1.5], ['previoustrack',1.5], ['nexttrack',1.5]],
  ];
  return [...main,
    [['f13',1], ['f14',1], ['f15',1], ['f16',1], .5, ['f17',1], ['f18',1], ['f19',1], ['f20',1], .5, ['f21',1], ['f22',1], ['f23',1], ['f24',1]],
    [['volumedown',1.5], ['volumeup',1.5], ['mute',1.5], ['playpause',1.5], ['previoustrack',1.5], ['nexttrack',1.5], ['mediastop',1.5], .5,
     ['browserback',1.5], ['browserforward',1.5], ['browserrefresh',1.5], ['browserhome',1.5], ['launchmail',1.5]],
  ];
}
// Mac names for the same keys: win is Command, alt is Option, insert arrives as Help, and Backspace
// is the key a Mac calls Delete (delete is forward delete).
const MAC_LABELS = { lctrl: 'Ctrl', rctrl: 'Ctrl', lwin: 'Cmd', rwin: 'Cmd', lalt: 'Opt', ralt: 'Opt', insert: 'Help', backspace: 'Delete', delete: 'Del ⌦' };
const MOD_LABELS = { ctrl: 'Ctrl', alt: 'Alt', shift: 'Shift', win: 'Win' };
const MAC_MOD_LABELS = { ctrl: 'Control', alt: 'Option', shift: 'Shift', win: 'Cmd' };
const modLabel = m => (isMac ? MAC_MOD_LABELS : MOD_LABELS)[m];
const KB_LABELS = { esc: 'Esc', printscreen: 'PrtSc', scrolllock: 'ScrLk', pause: 'Pause', backspace: 'Bksp', insert: 'Ins', home: 'Home', pageup: 'PgUp', delete: 'Del', end: 'End', pagedown: 'PgDn', tab: 'Tab', capslock: 'Caps', enter: 'Enter', lshift: 'Shift', rshift: 'Shift', lctrl: 'Ctrl', rctrl: 'Ctrl', lwin: 'Win', rwin: 'Win', lalt: 'Alt', ralt: 'AltGr', apps: 'Menu', space: 'Space', up: '\u2191', down: '\u2193', left: '\u2190', right: '\u2192',
  volumeup: 'Vol+', volumedown: 'Vol\u2212', mute: 'Mute', playpause: 'Play', nexttrack: 'Next', previoustrack: 'Prev', mediastop: 'Stop',
  browserback: 'Back', browserforward: 'Fwd', browserrefresh: 'Refresh', browserhome: 'Web', launchmail: 'Mail' };
const keyLabel = n => (isMac && MAC_LABELS[n]) || KB_LABELS[n] || (n.length === 1 || /^f\d+$/.test(n) ? n.toUpperCase() : n);
const chordLabel = c => c.split('+').map(x => MODS.includes(x) ? modLabel(x) : keyLabel(x)).join('+');
// Short text for what a key becomes: a key name, a chord, or "@action".
const targetLabel = t => t.trim().toLowerCase() === 'none' ? 'cleared' : t.startsWith('@') ? t : (t.includes(',') ? t : chordLabel(normChord(t)));
let selKey = '', picking = false, selMods = [];

const fromStr = name => [...MODS.filter(m => selMods.includes(m)), name].join('+');

function buildKeyboard(list, trigger) {
  const wrap = document.createElement('div');
  const find = name => list.findIndex(x => normChord(x[0]) === fromStr(name));

  // Modifier checkboxes: pick Alt, then click Q to edit "alt+q".
  const mods = document.createElement('div'); mods.className = 'kmods';
  mods.append('With:');
  for (const m of MODS) {
    const l = document.createElement('label'), c = document.createElement('input');
    c.type = 'checkbox'; c.checked = selMods.includes(m);
    c.onchange = () => { selMods = c.checked ? [...selMods, m] : selMods.filter(x => x !== m); picking = false; render(); };
    l.append(c, modLabel(m)); mods.appendChild(l);
  }
  wrap.appendChild(mods);

  const scroll = document.createElement('div'); scroll.className = 'kbd-scroll';
  const kbd = document.createElement('div'); kbd.className = 'kbd';
  for (const row of kbRows()) {
    if (row === 'sep') { const g = document.createElement('div'); g.style.height = '10px'; kbd.appendChild(g); continue; }
    const r = document.createElement('div'); r.className = 'krow';
    for (const item of row) {
      if (typeof item === 'number') { const g = document.createElement('div'); g.className = 'gap'; g.style.width = `calc(var(--u) * ${item} + ${item * 4}px)`; r.appendChild(g); continue; }
      const [name, w] = item;
      const k = document.createElement('div'); k.className = 'key';
      k.style.width = `calc(var(--u) * ${w} + ${(w - 1) * 4}px)`;
      const main = document.createElement('span'); main.textContent = keyLabel(name); k.appendChild(main);
      const i = find(name);
      const isTrig = !selMods.length && !!trigger && trigger.toLowerCase() === name;
      const isLayerKey = !selMods.length && !trigger && state.layers.some(l => l.trigger.toLowerCase() === name);
      if (isTrig) { k.classList.add('trig'); k.title = 'Layer key'; const sm = document.createElement('small'); sm.textContent = 'layer'; k.appendChild(sm); }
      else if (i >= 0 && list[i][1]) { k.classList.add('mapped'); const sm = document.createElement('small'); sm.textContent = '\u2192 ' + targetLabel(list[i][1]); k.appendChild(sm); k.title = `${list[i][0]} \u2192 ${list[i][1]}`; }
      else if (isLayerKey) { const sm = document.createElement('small'); sm.textContent = 'layer'; k.appendChild(sm); }
      if (name === selKey) k.classList.add('sel');
      if (picking) k.classList.add('picking');
      k.onclick = () => {
        if (isTrig) return;
        if (picking && selKey) { setTarget(list, fromStr(selKey), name); picking = false; return render(); }
        selKey = name === selKey ? '' : name; picking = false; render();
      };
      r.appendChild(k);
    }
    kbd.appendChild(r);
  }
  scroll.appendChild(kbd); wrap.appendChild(scroll);

  const ed = document.createElement('div'); ed.className = 'kedit';
  if (!selKey) ed.textContent = 'Click a key to choose what it should become. Tick modifiers first to bind a combo like ' + chordLabel('alt+q') + '.';
  else {
    const i = find(selKey);
    const input = targetSelect(i >= 0 ? list[i][1] : '', v => { setTarget(list, fromStr(selKey), v); render(); });
    const pick = document.createElement('button'); pick.textContent = picking ? 'Click a key\u2026' : 'Pick on keyboard'; pick.className = picking ? 'on' : '';
    pick.onclick = () => { picking = !picking; render(); };
    const clear = document.createElement('button'); clear.textContent = 'Clear'; clear.disabled = i < 0;
    clear.onclick = () => { setTarget(list, fromStr(selKey), ''); render(); };
    const strong = document.createElement('b'); strong.textContent = chordLabel(fromStr(selKey)); strong.style.color = 'var(--text)';
    ed.append(strong, ' becomes ', input, pick, clear);
    const target = (i >= 0 ? list[i][1] : '').trim();
    const info = target.startsWith('@') ? actionInfo(target.slice(1)) : null;
    if (info) {
      const d = document.createElement('div'); d.className = 'kinfo';
      d.textContent = `@${info.name}: ${info.desc || 'no description'} (${info.steps})`;
      ed.appendChild(d);
    }
    const hint = document.createElement('div'); hint.style.flexBasis = '100%';
    hint.textContent = 'Pick a key or an action from the list, or use "Pick on keyboard" and then click the key it should become.';
    ed.appendChild(hint);
  }
  wrap.appendChild(ed);
  return wrap;
}

// What a key becomes, picked from a list: actions (yours, then built-in) and keys.
// A value that isn't in the list (a combo or steps written by hand in the config) stays selectable
// as "Custom" so it isn't lost. Picking calls onPick(value) with "@name" or a key name.
function targetSelect(value, onPick) {
  const sel = document.createElement('select');
  const cur = value.trim();
  const mine = state.actions.map(x => x.name).filter(n => ACTION_NAME.test(n));
  const userOnly = mine.filter(n => !builtin.some(b => b.name === n));  // your own actions; changed built-ins stay in their group
  const keyNames = allKeyNames;
  const known = new Set(['none', ...mine.map(n => '@' + n), ...builtin.map(b => '@' + b.name), ...keyNames]);
  const isKnown = known.has(cur.toLowerCase());

  const add = (parent, v, label) => { const o = document.createElement('option'); o.value = v; o.textContent = label; parent.appendChild(o); };
  add(sel, '', 'Choose\u2026');
  if (cur && !isKnown) add(sel, cur, 'Custom: ' + cur);
  const group = (label, items) => {
    if (!items.length) return;
    const g = document.createElement('optgroup'); g.label = label;
    for (const [v, text] of items) add(g, v, text);
    sel.appendChild(g);
  };
  const actionItem = n => { const info = actionInfo(n); return ['@' + n, info && info.desc ? `${n} \u2014 ${info.desc}` : n]; };
  group('Clear', [['none', 'None \u2014 clear this combo']]);
  group('Your actions', userOnly.map(actionItem));
  for (const g of builtinGroups()) group(g.category, g.names.map(actionItem));
  group('Keys', keyNames.map(k => [k, keyLabel(k) === k ? k : `${k} (${keyLabel(k)})`]));
  sel.value = !cur ? '' : isKnown ? cur.toLowerCase() : cur;
  sel.onchange = () => { if (sel.value) onPick(sel.value); };
  return sel;
}

// Set (or with '' remove) the mapping for `from` (a canonical chord string) in this list.
function setTarget(list, from, to) {
  const i = list.findIndex(x => normChord(x[0]) === from);
  if (!to) { if (i >= 0) list.splice(i, 1); }
  else if (i >= 0) list[i][1] = to;
  else list.push([from, to]);
}
