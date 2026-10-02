// Config text <-> GUI state, key/chord helpers and validation.
const MODS = ['ctrl', 'alt', 'shift', 'win'];
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

// "Alt + q" -> "alt+q"; modifiers in a fixed order so "shift+ctrl+a" equals "ctrl+shift+a".
function normChord(str) {
  const parts = str.split('+').map(x => x.trim().toLowerCase());
  const key = parts.pop();
  return [...MODS.filter(m => parts.includes(m)), key].join('+');
}
function validChord(str) {
  const parts = str.split('+').map(x => x.trim().toLowerCase());
  const key = parts.pop();
  return !!key && (validKeys.size === 0 || validKeys.has(key)) && parts.every(m => MODS.includes(m));
}
function validSteps(str) { return !!str && (validChord(str) || str.split(',').every(x => validChord(x))); }
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
