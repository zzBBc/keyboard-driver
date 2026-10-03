// Talking to the server: save/load config, keyboards list.
const cfgUrl = () => '/api/config' + (device ? '?device=' + encodeURIComponent(device) : '');
const HDR = { 'X-Requested-With': 'keymapper' };

async function save() {
  if (problems().length) return updateMeta();
  const text = serialize(state);
  const res = await fetch(cfgUrl(), { method: 'PUT', headers: { ...HDR, 'Content-Type': 'text/plain' }, body: text });
  if (res.ok) { saved = text; updateMeta(); refreshDevices(); }
  else setStatus('Server rejected the config:\n' + await res.text(), 'err');
}

async function loadConfig() {
  state = parse(await fetch(cfgUrl()).then(r => r.text())); saved = serialize(state); current = 'base'; render();
}

function renderDevices() {
  const sel = $('device');
  sel.innerHTML = '';
  const add = (id, label) => { const o = document.createElement('option'); o.value = id; o.textContent = label; sel.appendChild(o); };
  add('', 'Default (keyboards without their own config)');
  for (const d of devices) add(d.id, (d.name === d.id ? d.id : `${d.name} (${d.id})`) + (d.own ? ' ✓' : '') + (d.id === lastKb ? ' ← last used' : ''));
  if (device && !devices.some(d => d.id === device)) add(device, device + ' (not connected)');
  sel.value = device;
  const cur = devices.find(d => d.id === device);
  $('reset').hidden = !(device && (!cur || cur.own));
}

async function refreshDevices() {
  const lines = (await fetch('/api/devices').then(r => r.text())).split('\n').filter(Boolean);
  lastKb = '';
  devices = [];
  for (const l of lines) {
    const [id, own, ...name] = l.split('\t');
    if (id === 'last') lastKb = own || ''; else devices.push({ id, own: own === '1', name: name.join('\t') });
  }
  renderDevices();
}

async function switchDevice(id) {
  if (serialize(state) !== saved && !confirm('Discard unsaved changes?')) return renderDevices();
  device = id; renderDevices(); await loadConfig();
}

async function load() {
  isMac = (await fetch('/api/platform').then(r => r.text())).trim() === 'macos';
  const keys = await fetch('/api/keys').then(r => r.text());
  const names = keys.split('\n').filter(Boolean);
  validKeys = new Set(names.map(n => n.toLowerCase()));
  allKeyNames = names;
  $('keys').innerHTML = names.map(n => `<option value="${n}">`).join('');
  builtin = (await fetch('/api/actions').then(r => r.text())).split('\n').filter(Boolean).map(l => {
    const [name, desc, steps, shortcuts, category] = l.split('\t');
    return { name, desc: desc || '', steps: steps || '', category: category || '', shortcuts: (shortcuts || '').split(',').map(x => x.trim()).filter(Boolean).map(normChord) };
  });
  await refreshDevices();
  await loadConfig();
}
