// Rendering, tabs, validation, status bar and start-up. Loaded last.
function maps() { return current === 'base' ? state.base : state.layers[current].maps; }

function render() {
  const tabs = $('tabs'); tabs.innerHTML = '';
  const mk = (label, key, cls) => {
    const b = document.createElement('button');
    b.className = 'tab ' + (cls || ''); b.textContent = label; b.setAttribute('role', 'tab');
    b.setAttribute('aria-selected', String(current === key));
    b.onclick = () => { if (key === 'add') addLayer(); else { current = key; selKey = ''; picking = false; selMods = []; bind = null; render(); } };
    tabs.appendChild(b);
  };
  mk('Base', 'base');
  state.layers.forEach((l, i) => mk(`Layer: ${l.trigger || '?'}`, i));
  mk('Actions', 'actions');
  mk('+ Layer', 'add', 'add');

  const p = $('panel'); p.innerHTML = '';
  if (current === 'actions') { renderActions(p); updateMeta(); return; }
  const isLayer = current !== 'base';
  const list = maps();

  if (isLayer) {
    const t = document.createElement('div'); t.className = 'trigger';
    t.append('While holding ', keyInput(state.layers[current].trigger, v => { state.layers[current].trigger = v; renderTabsOnly(); }), ' these apply:');
    p.appendChild(t);
  }
  p.appendChild(buildKeyboard(list, isLayer ? state.layers[current].trigger : ''));
  if (!list.length) { const e = document.createElement('div'); e.className = 'empty'; e.textContent = 'No mappings yet.'; p.appendChild(e); }

  list.forEach((pair, i) => {
    const r = document.createElement('div'); r.className = 'row';
    const arrow = document.createElement('span'); arrow.className = 'arrow'; arrow.textContent = '→';
    const d = document.createElement('button'); d.textContent = '×'; d.title = 'Remove';
    d.onclick = () => { list.splice(i, 1); render(); };
    r.append(keyInput(pair[0], v => pair[0] = v, 'from'), arrow, targetSelect(pair[1], v => { pair[1] = v; render(); }), d);
    p.appendChild(r);
  });

  const act = document.createElement('div'); act.className = 'actions';
  const add = document.createElement('button'); add.textContent = '+ Add mapping';
  add.onclick = () => { list.push(['', '']); render(); const ins = p.querySelectorAll('.row input'); ins[ins.length - 2].focus(); };
  act.appendChild(add);
  if (isLayer) {
    const vim = document.createElement('button'); vim.textContent = 'Add HJKL arrows';
    vim.onclick = () => { for (const [a, b] of [['h','left'],['j','down'],['k','up'],['l','right']]) if (!list.some(x => x[0].toLowerCase() === a)) list.push([a, b]); render(); };
    const del = document.createElement('button'); del.textContent = 'Delete layer';
    del.onclick = () => { state.layers.splice(current, 1); current = 'base'; render(); };
    act.append(vim, del);
  }
  p.appendChild(act);
  updateMeta();
}

function renderTabsOnly() { // keep input focus while typing a layer key
  const tabs = $('tabs').children;
  state.layers.forEach((l, i) => tabs[i + 1].textContent = `Layer: ${l.trigger || '?'}`);
  updateMeta();
}

// kind: 'key' = one key, 'from' = key or combo, 'to' = key, combo, steps or @action, 'step' = combo or steps.
function keyInput(value, onChange, kind = 'key') {
  const i = document.createElement('input');
  const ok = { key: v => validKeys.has(v.toLowerCase()), from: validChord, to: validTarget, step: validSteps }[kind];
  i.value = value; i.setAttribute('list', 'keys'); i.autocomplete = 'off'; i.spellcheck = false;
  const check = () => i.classList.toggle('invalid', !!i.value.trim() && validKeys.size > 0 && !ok(i.value.trim()));
  i.oninput = () => { onChange(i.value.trim()); check(); updateMeta(); };
  check();
  return i;
}

function problems() {
  const out = [];
  const known = validKeys.size > 0;
  const scan = (label, list) => list.forEach(([a, b]) => {
    if (!a && !b) return;
    if (known && !validChord(a)) out.push(`${label}: unknown key "${a}"`);
    if (known && !validTarget(b)) out.push(`${label}: unknown key or action "${b}"`);
  });
  scan('Base', state.base);
  state.layers.forEach(l => { if (known && !validKeys.has(l.trigger.toLowerCase())) out.push(`Layer: unknown key "${l.trigger}"`); scan(`Layer ${l.trigger}`, l.maps); });
  const seen = new Set();
  state.actions.forEach(a => {
    if (!ACTION_NAME.test(a.name)) out.push(`Action "${a.name}": use letters, digits, - and _`);
    else if (seen.has(a.name)) out.push(`Action "${a.name}" is defined twice`);
    seen.add(a.name);
    if (!a.steps.some(Boolean)) out.push(`Action "${a.name}" has no steps`);
    a.steps.forEach(st => { if (st && known && !validSteps(st)) out.push(`Action "${a.name}": bad step "${st}"`); });
  });
  return out;
}

function updateMeta() {
  const text = serialize(state);
  $('preview').textContent = text;
  const dirty = text !== saved;
  $('save').disabled = !dirty; $('revert').disabled = !dirty;
  const probs = problems();
  if (probs.length) setStatus(probs[0] + (probs.length > 1 ? ` (+${probs.length - 1} more)` : ''), 'err');
  else setStatus(dirty ? 'Unsaved changes' : 'Saved and active', dirty ? '' : 'ok');
}

function setStatus(msg, cls) { const s = $('status'); s.textContent = msg; s.className = cls || ''; }

function addLayer() {
  state.layers.push({ trigger: '', maps: [] });
  current = state.layers.length - 1; render();
  document.querySelector('.trigger input').focus();
}

$('device').onchange = e => switchDevice(e.target.value);
$('detect').onclick = async () => { await refreshDevices(); if (lastKb) switchDevice(lastKb); else setStatus('Press a key on the keyboard first', ''); };
$('reset').onclick = async () => {
  if (!confirm("Delete this keyboard's own config and use the default?")) return;
  await fetch(cfgUrl(), { method: 'DELETE', headers: HDR });
  await refreshDevices(); await loadConfig();
};
setInterval(() => { if (document.activeElement !== $('device')) refreshDevices().catch(() => {}); }, 3000);

$('stop').onclick = async () => {
  if (!confirm('Stop Keymapper? Remapping stops until you start it again.')) return;
  try { await fetch('/api/quit', { method: 'POST', headers: HDR }); } catch (e) { /* it may close the connection as it exits */ }
  saved = serialize(state);  // nothing left to lose: no "leave page?" prompt
  document.querySelectorAll('button, select, input').forEach(el => { el.disabled = true; });
  setStatus('Keymapper has stopped. You can close this tab.', 'err');
};
$('save').onclick = () => save().catch(e => setStatus('Save failed: ' + e.message, 'err'));
$('revert').onclick = () => { state = parse(saved); current = 'base'; render(); };
window.addEventListener('beforeunload', e => { if (serialize(state) !== saved) e.preventDefault(); });
load().catch(e => setStatus('Cannot reach keymapper: ' + e.message, 'err'));
