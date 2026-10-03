// Config text <-> GUI state, key/chord helpers and validation.
const MODS = ['ctrl', 'alt', 'shift', 'win'];  // modifier ids, in the order a combo lists them
// Every name the config accepts for a modifier (same as the server): the Mac names work on any OS.
const MOD_ALIASES = { ctrl: 'ctrl', control: 'ctrl', alt: 'alt', option: 'alt', opt: 'alt', shift: 'shift', win: 'win', cmd: 'win', command: 'win' };
const modId = name => MOD_ALIASES[name];
// The name the GUI writes into the config for a modifier id: this OS's (layout.names), e.g. cmd on macOS.
const modName = m => (layout.names && layout.names[m]) || m;
// Modifier ids -> config text, in the fixed order: ['win', 'ctrl'] + 'q' -> "ctrl+win+q" (macOS: "control+cmd+q").
const comboText = (mods, key) => [...MODS.filter(m => mods.includes(m)).map(modName), key].join('+');
const ACTION_NAME = /^[a-z0-9_-]+$/;

// Same rule as the server: the '=' of "from = to" is not the one in "alt+=".
function findEq(line) {
  let eq = line.indexOf('=', 1);
  while (eq > 0 && line[eq - 1] === '+') eq = line.indexOf('=', eq + 1);
  return eq;
}

function parse(text) {
  const s = { base: [], layers: [], actions: [] };
  let list = s.base, action = null;
  for (let line of text.split('\n')) {
    const hash = line.indexOf('#'); if (hash >= 0) line = line.slice(0, hash);
    line = line.trim(); if (!line) continue;
    if (line.startsWith('[') && line.endsWith(']')) {
      const inner = line.slice(1, -1).trim();
      action = null;
      if (inner.toLowerCase() === 'base') list = s.base;
      else if (/^layer\b/i.test(inner)) { const l = { trigger: inner.slice(5).trim(), maps: [] }; s.layers.push(l); list = l.maps; }
      else if (/^action\b/i.test(inner)) { action = { name: inner.slice(6).trim().toLowerCase(), desc: '', steps: [] }; s.actions.push(action); }
      continue;
    }
    const eq = findEq(line);
    if (action && /^description:/i.test(line)) { action.desc = line.slice(12).trim(); continue; }
    if (action && eq < 0) { action.steps.push(line); continue; }
    if (action) { action = null; list = s.base; }  // a "from = to" line ends the action
    if (eq < 0) continue;
    list.push([line.slice(0, eq).trim(), line.slice(eq + 1).trim()]);
  }
  return s;
}

// Base mappings first (they must precede any section header), then actions, then layers.
function serialize(s) {
  const rows = m => m.filter(([a, b]) => a || b).map(([a, b]) => `${a} = ${b}`);
  const blocks = [];
  if (rows(s.base).length) blocks.push(rows(s.base).join('\n'));
  for (const a of s.actions) blocks.push([`[action ${a.name}]`, ...(a.desc ? [`description: ${a.desc}`] : []), ...a.steps.filter(Boolean)].join('\n'));
  for (const l of s.layers) blocks.push(`[layer ${l.trigger}]\n${rows(l.maps).join('\n')}`);
  return blocks.join('\n\n') + '\n';
}

// "Alt + q" -> "alt+q"; modifiers in a fixed order and under this OS's names, so "shift+ctrl+a" equals
// "ctrl+shift+a", and on macOS "win+q" equals "cmd+q".
function normChord(str) {
  const parts = str.split('+').map(x => x.trim().toLowerCase());
  const key = parts.pop();
  return comboText(parts.map(modId), key);
}
function validChord(str) {
  const parts = str.split('+').map(x => x.trim().toLowerCase());
  const key = parts.pop();
  return !!key && (validKeys.size === 0 || validKeys.has(key)) && parts.every(modId);
}
function validSteps(str) { return !!str && (validChord(str) || str.split(',').every(x => validChord(x))); }
// Built-in actions grouped by the category: line in config/actions.<os>.txt, in the order the file lists them.
function builtinGroups() {
  const groups = [];
  for (const b of builtin) {
    const category = b.category || 'Other';
    let g = groups.find(x => x.category === category);
    if (!g) groups.push(g = { category, names: [] });
    g.names.push(b.name);
  }
  return groups;
}
function actionNames() { return [...new Set([...state.actions.map(a => a.name), ...builtin.map(b => b.name)])]; }
// Description and steps for an action name: your own definition wins over the built-in one.
function actionInfo(name) {
  name = name.toLowerCase();
  const mine = state.actions.find(a => a.name === name);
  if (mine) return { name, desc: mine.desc, steps: mine.steps.filter(Boolean).join(', '), builtin: false };
  const b = builtin.find(x => x.name === name);
  return b ? { ...b, builtin: true } : null;
}
function validTarget(str) {
  if (str.trim().toLowerCase() === 'none') return true;  // clears the combo
  if (str.startsWith('@')) return actionNames().includes(str.slice(1).trim().toLowerCase());
  return validSteps(str);
}
