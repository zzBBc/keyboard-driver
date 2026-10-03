// The drawn keyboard and the target picker.
// Keyboard picture, key labels and modifier names come from web/layouts/<os>.json (see layout in
// state.js). A row is a list of "name" (1 unit wide), ["name", width in units] or a number (blank
// gap); an empty row is a separator.
const modLabel = m => layout.mods[m] || m[0].toUpperCase() + m.slice(1);
const keyLabel = n => layout.labels[n] || (n.length === 1 || /^f\d+$/.test(n) ? n.toUpperCase() : n);
const chordLabel = c => c.split('+').map(x => modId(x) ? modLabel(modId(x)) : keyLabel(x)).join('+');
// Short text for what a key becomes: a key name, a chord, or "@action".
const targetLabel = t => t.trim().toLowerCase() === 'none' ? 'cleared' : t.startsWith('@') ? t : (t.includes(',') ? t : chordLabel(normChord(t)));
let selKey = '', picking = false, selMods = [];

const fromStr = name => comboText(selMods, name);

function buildKeyboard(list, trigger) {
  const wrap = document.createElement('div');
  const find = name => list.findIndex(x => normChord(x[0]) === fromStr(name));

  // Modifier checkboxes: pick Alt, then click Q to edit "alt+q".
  const mods = document.createElement('div'); mods.className = 'kmods';
  mods.append('With:');
  for (const m of SHORTCUT_MODS) {
    const l = document.createElement('label'), c = document.createElement('input');
    c.type = 'checkbox'; c.checked = selMods.includes(m);
    c.onchange = () => { selMods = c.checked ? [...selMods, m] : selMods.filter(x => x !== m); picking = false; render(); };
    l.append(c, modLabel(m)); mods.appendChild(l);
  }
  wrap.appendChild(mods);

  const scroll = document.createElement('div'); scroll.className = 'kbd-scroll';
  const kbd = document.createElement('div'); kbd.className = 'kbd';
  for (const row of layout.rows) {
    if (!row.length) { const g = document.createElement('div'); g.style.height = '10px'; kbd.appendChild(g); continue; }
    const r = document.createElement('div'); r.className = 'krow';
    for (const item of row) {
      if (typeof item === 'number') { const g = document.createElement('div'); g.className = 'gap'; g.style.width = `calc(var(--u) * ${item} + ${item * 4}px)`; r.appendChild(g); continue; }
      const [name, w] = typeof item === 'string' ? [item, 1] : item;
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
