// Actions tab: shortcuts, changing what an action sends, recording keys.
// ---- Actions tab: give an action a shortcut. Actions come from the lists; only the combo is new. ----
let edit = null;  // changing what an action sends: { action, steps: [{ mods: [], key }], rec: step being recorded or -1 }
let bind = null;  // { action, mods: [], key: '', scope: 'base' | layer index, recording: bool }

const CODE_KEYS = { Space: 'space', Tab: 'tab', Enter: 'enter', Escape: 'esc', Backspace: 'backspace', ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'up', ArrowDown: 'down',
  Home: 'home', End: 'end', PageUp: 'pageup', PageDown: 'pagedown', Insert: 'insert', Delete: 'delete', CapsLock: 'capslock', Minus: '-', Equal: '=',
  BracketLeft: '[', BracketRight: ']', Backslash: '\\', Semicolon: ';', Quote: "'", Comma: ',', Period: '.', Slash: '/', Backquote: '`',
  PrintScreen: 'printscreen', ScrollLock: 'scrolllock', Pause: 'pause', ContextMenu: 'apps' };
function keyFromEvent(e) {
  if (/^Key[A-Z]$/.test(e.code)) return e.code.slice(3).toLowerCase();
  if (/^Digit\d$/.test(e.code)) return e.code.slice(5);
  if (/^F\d{1,2}$/.test(e.code)) return e.code.toLowerCase();
  return CODE_KEYS[e.code] || '';  // modifier keys and anything unknown: not a key for a shortcut
}
// Record the next shortcut pressed in the browser. (The OS keeps some, like Alt+Tab or Cmd+Tab, for
// itself: use the checkboxes and key list for those.) Cmd on a Mac arrives as metaKey, like Win.
document.addEventListener('keydown', e => {
  const target = bind && bind.recording ? bind : edit && edit.rec >= 0 ? edit : null;
  if (!target) return;
  e.preventDefault();
  const stop = () => { if (target === bind) bind.recording = false; else edit.rec = -1; };
  const noMods = !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey;
  if (e.key === 'Escape' && noMods) { stop(); return render(); }
  const key = keyFromEvent(e);
  if (!key) return;  // only a modifier so far: keep waiting
  const mods = MODS.filter(m => ({ ctrl: e.ctrlKey, alt: e.altKey, shift: e.shiftKey, win: e.metaKey })[m]);
  if (target === bind) { bind.mods = mods; bind.key = key; }
  else edit.steps[edit.rec] = { mods, key };
  stop();
  render();
}, true);

function scopeList(scope) { return scope === 'base' ? state.base : state.layers[scope].maps; }
function scopeLabel(scope) { return scope === 'base' ? 'Base' : `Layer: ${state.layers[scope].trigger || '?'}`; }

// Shortcuts currently bound to an action, across the base and all layers.
function bindingsFor(name) {
  const out = [];
  const scan = (scope, list) => list.forEach(([from, to]) => { if (to.trim().toLowerCase() === '@' + name) out.push({ scope, list, from }); });
  scan('base', state.base);
  state.layers.forEach((l, i) => scan(i, l.maps));
  // Default shortcut of a built-in action: shown until the config binds or clears that combo.
  const lib = builtin.find(b => b.name === name);
  for (const c of lib ? lib.shortcuts : [])
    if (!state.base.some(([a]) => normChord(a) === c)) out.push({ scope: 'base', list: state.base, from: c, isDefault: true });
  return out;
}

// Remove a shortcut. A default combo can't just be deleted (it would come back), so it is cleared with "none".
function clearShortcut(x) {
  const combo = normChord(x.from);
  const isDefault = x.scope === 'base' && builtin.some(b => b.shortcuts.includes(combo));
  setTarget(x.list, combo, isDefault ? 'none' : '');
}

function bindEditor(name) {
  const ed = document.createElement('div'); ed.className = 'bedit';
  const rec = document.createElement('button'); rec.className = bind.recording ? 'on' : '';
  rec.textContent = bind.recording ? 'Press the shortcut\u2026 (Esc cancels)' : 'Record shortcut';
  rec.onclick = () => { bind.recording = !bind.recording; render(); };
  ed.appendChild(rec);

  ed.append('or');
  for (const m of MODS) {
    const l = document.createElement('label'), c = document.createElement('input');
    c.type = 'checkbox'; c.checked = bind.mods.includes(m);
    c.onchange = () => { bind.mods = c.checked ? [...bind.mods, m] : bind.mods.filter(x => x !== m); render(); };
    l.append(c, modLabel(m)); ed.appendChild(l);
  }
  const key = document.createElement('select');
  const none = document.createElement('option'); none.value = ''; none.textContent = 'Key\u2026'; key.appendChild(none);
  for (const k of allKeyNames) { const o = document.createElement('option'); o.value = k; o.textContent = keyLabel(k) === k ? k : `${k} (${keyLabel(k)})`; key.appendChild(o); }
  key.value = bind.key; key.onchange = () => { bind.key = key.value; render(); };
  ed.appendChild(key);

  if (state.layers.length) {
    const sc = document.createElement('select');
    const add = (v, t) => { const o = document.createElement('option'); o.value = String(v); o.textContent = t; sc.appendChild(o); };
    add('base', 'in Base');
    state.layers.forEach((l, i) => add(i, `in ${scopeLabel(i)}`));
    sc.value = String(bind.scope);
    sc.onchange = () => { bind.scope = sc.value === 'base' ? 'base' : Number(sc.value); render(); };
    ed.appendChild(sc);
  }

  const combo = bind.key ? [...MODS.filter(m => bind.mods.includes(m)), bind.key].join('+') : '';
  const ok = document.createElement('button'); ok.className = 'primary'; ok.disabled = !combo;
  ok.textContent = combo ? `Bind ${chordLabel(combo)}` : 'Bind';
  ok.onclick = () => { setTarget(scopeList(bind.scope), combo, '@' + name); bind = null; render(); };
  const cancel = document.createElement('button'); cancel.textContent = 'Cancel';
  cancel.onclick = () => { bind = null; render(); };
  ed.append(ok, cancel);

  if (combo) {
    const hit = scopeList(bind.scope).find(([a]) => normChord(a) === combo);
    const note = document.createElement('div'); note.className = 'note';
    const owner = !hit && bind.scope === 'base' ? builtin.find(b => b.name !== name && b.shortcuts.includes(combo)) : null;
    if (owner) note.textContent = `${chordLabel(combo)} is the default shortcut of ${owner.name}; binding will replace that.`;
    else if (hit && hit[1].trim().toLowerCase() !== '@' + name) note.textContent = `${chordLabel(combo)} is currently \u2192 ${hit[1]}; binding will replace that.`;
    else if (hit) note.textContent = `${chordLabel(combo)} already runs this action.`;
    if (note.textContent) ed.appendChild(note);
  }
  return ed;
}

// "ctrl+c, ctrl+v" -> [{ mods: ['ctrl'], key: 'c' }, ...]
function stepsFromText(text) {
  return text.split(',').map(x => x.trim()).filter(Boolean).map(x => {
    const parts = x.split('+').map(y => y.trim().toLowerCase());
    const key = parts.pop();
    return { mods: MODS.filter(m => parts.includes(m)), key };
  });
}
const stepCombo = st => [...MODS.filter(m => st.mods.includes(m)), st.key].join('+');
const stepsText = steps => steps.map(stepCombo).join(', ');

// Editor for what an action sends. Saved as your own version of the action; Reset restores the built-in one.
function stepEditor(name, info) {
  const ed = document.createElement('div'); ed.className = 'bedit';
  edit.steps.forEach((st, i) => {
    const row = document.createElement('div'); row.className = 'srow';
    const lbl = document.createElement('span'); lbl.className = 'lbl'; lbl.textContent = edit.steps.length > 1 ? `Step ${i + 1}` : 'Sends';
    const rec = document.createElement('button'); rec.className = edit.rec === i ? 'on' : '';
    rec.textContent = edit.rec === i ? 'Press the keys\u2026 (Esc cancels)' : 'Record';
    rec.onclick = () => { edit.rec = edit.rec === i ? -1 : i; render(); };
    row.append(lbl, rec, 'or');
    for (const m of MODS) {
      const l = document.createElement('label'), c = document.createElement('input');
      c.type = 'checkbox'; c.checked = st.mods.includes(m);
      c.onchange = () => { st.mods = c.checked ? [...st.mods, m] : st.mods.filter(x => x !== m); render(); };
      l.append(c, modLabel(m)); row.appendChild(l);
    }
    const key = document.createElement('select');
    const none = document.createElement('option'); none.value = ''; none.textContent = 'Key\u2026'; key.appendChild(none);
    for (const k of allKeyNames) { const o = document.createElement('option'); o.value = k; o.textContent = keyLabel(k) === k ? k : `${k} (${keyLabel(k)})`; key.appendChild(o); }
    key.value = st.key; key.onchange = () => { st.key = key.value; render(); };
    row.appendChild(key);
    if (edit.steps.length > 1) {
      const x = document.createElement('button'); x.textContent = '\u00d7'; x.title = 'Remove this step';
      x.onclick = () => { edit.steps.splice(i, 1); edit.rec = -1; render(); };
      row.appendChild(x);
    }
    ed.appendChild(row);
  });

  const addStep = document.createElement('button'); addStep.textContent = '+ Step';
  addStep.onclick = () => { edit.steps.push({ mods: [], key: '' }); render(); };
  const complete = edit.steps.every(st => st.key);
  const ok = document.createElement('button'); ok.className = 'primary'; ok.disabled = !complete;
  ok.textContent = complete ? `Send ${stepsText(edit.steps)}` : 'Send';
  ok.onclick = () => {
    const text = stepsText(edit.steps);
    const orig = builtin.find(b => b.name === name);
    const mine = state.actions.findIndex(a => a.name === name);
    if (orig && stepsText(stepsFromText(orig.steps)) === text) { if (mine >= 0) state.actions.splice(mine, 1); }  // same as built-in: no override needed
    else if (mine >= 0) state.actions[mine].steps = [text];
    else state.actions.push({ name, desc: info.desc || '', steps: [text] });
    edit = null; render();
  };
  const cancel = document.createElement('button'); cancel.textContent = 'Cancel';
  cancel.onclick = () => { edit = null; render(); };
  ed.append(addStep, ok, cancel);
  return ed;
}

function renderActions(p) {
  const hint = document.createElement('p'); hint.className = 'hint';
  hint.textContent = 'Pick an action and give it a shortcut, or change the keys it sends (for example make Undo send alt+z). Removing a shortcut keeps the action. Press Save to apply. ' + (isMac
    ? '(macOS keeps some shortcuts, like Cmd+Tab, for itself; enter those with the boxes instead of recording. In the config, win is Command and alt is Option. The built-in actions send Windows shortcuts, so most of them mean something else on macOS: change their keys or bind your own.)'
    : '(Windows keeps some shortcuts, like Alt+Tab, for itself; enter those with the boxes instead of recording.)');
  p.appendChild(hint);

  const section = (title, names) => {
    if (!names.length) return;
    const det = document.createElement('details'); det.className = 'builtin'; det.open = true;
    const sum = document.createElement('summary'); sum.textContent = `${title} (${names.length})`;
    det.appendChild(sum);
    for (const name of names) {
      const info = actionInfo(name);
      const orig = builtin.find(b => b.name === name);
      const changed = !!orig && !info.builtin;  // your version of a built-in action
      const r = document.createElement('div'); r.className = 'brow';
      const nm = document.createElement('span'); nm.className = 'bname'; nm.textContent = name;
      if (changed) { const t = document.createElement('span'); t.className = 'tag'; t.textContent = 'changed'; t.title = `Built-in sends: ${orig.steps}`; nm.appendChild(t); }
      const ds = document.createElement('span'); ds.className = 'bdesc'; ds.textContent = info.desc;
      const st = document.createElement('code'); st.textContent = info.steps;
      r.append(nm, ds, st);

      const bd = document.createElement('div'); bd.className = 'bbind';
      const bound = bindingsFor(name);
      const lbl = document.createElement('span'); lbl.className = 'bdesc';
      lbl.textContent = bound.length ? 'Shortcut:' : 'Shortcut: none yet';
      bd.appendChild(lbl);
      for (const x of bound) {
        const chip = document.createElement('span'); chip.className = 'chip';
        chip.append((x.scope === 'base' ? '' : scopeLabel(x.scope) + ': ') + chordLabel(normChord(x.from)) + (x.isDefault ? ' (default)' : ''));
        if (x.isDefault) chip.title = 'Default shortcut. Click \u00d7 to clear it.';
        const rm = document.createElement('button'); rm.textContent = '\u00d7'; rm.title = 'Remove this shortcut';
        rm.onclick = () => { clearShortcut(x); render(); };
        chip.appendChild(rm); bd.appendChild(chip);
      }
      const bindOpen = bind && bind.action === name;
      const editOpen = edit && edit.action === name;
      if (bound.length && !bindOpen) {
        const rmAll = document.createElement('button');
        rmAll.textContent = bound.length > 1 ? 'Remove shortcuts' : 'Remove shortcut';
        rmAll.title = 'Removes the key combo only; the action stays in the list.';
        rmAll.onclick = () => { bound.forEach(clearShortcut); render(); };
        bd.appendChild(rmAll);
      }
      if (!bindOpen) {
        const add = document.createElement('button'); add.textContent = '+ Shortcut';
        add.onclick = () => { edit = null; bind = { action: name, mods: [], key: '', scope: 'base', recording: false }; render(); };
        bd.appendChild(add);
      }
      if (!editOpen) {
        const chg = document.createElement('button'); chg.textContent = 'Change keys';
        chg.title = 'Change what this action sends';
        chg.onclick = () => { bind = null; edit = { action: name, steps: stepsFromText(info.steps), rec: -1 }; render(); };
        bd.appendChild(chg);
      }
      if (changed && !editOpen) {
        const reset = document.createElement('button'); reset.textContent = 'Reset keys';
        reset.title = `Back to the built-in keys (${orig.steps})`;
        reset.onclick = () => { state.actions.splice(state.actions.findIndex(a => a.name === name), 1); render(); };
        bd.appendChild(reset);
      }
      r.appendChild(bd);
      if (bindOpen) r.appendChild(bindEditor(name));
      if (editOpen) r.appendChild(stepEditor(name, info));
      det.appendChild(r);
    }
    p.appendChild(det);
  };

  const userOnly = state.actions.map(a => a.name).filter(n => !builtin.some(b => b.name === n));
  for (const g of builtinGroups()) section(g.category, g.names);
  section('In your config file', userOnly);
  if (!builtin.length && !state.actions.length) {
    const e = document.createElement('div'); e.className = 'empty';
    e.textContent = 'No actions found (actions.txt next to the program).';
    p.appendChild(e);
  }
}
