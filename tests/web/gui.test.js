// Tests for the GUI's logic (web/js). Run with:  node --test tests/web
//
// The GUI scripts are plain browser scripts that share one global scope, so they are loaded into a
// single vm context with a stub `document`. Only the logic scripts are loaded (state, config,
// keyboard, actions); the ones that wire up the page (devices, app) need a real DOM.
const { test } = require('node:test');
const assert = require('node:assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

function loadGui() {
  const ctx = vm.createContext({
    console,
    document: { addEventListener() {}, getElementById() { return null; }, createElement() { return {}; } },
  });
  for (const f of ['state.js', 'config.js', 'keyboard.js', 'actions.js']) {
    const file = path.join(__dirname, '..', '..', 'web', 'js', f);
    vm.runInContext(fs.readFileSync(file, 'utf8'), ctx, { filename: file });
  }
  const run = code => vm.runInContext(code, ctx);
  // Key names the server would send, and a small built-in action library.
  run(`validKeys = new Set(['a','q','z','c','v','tab','f4','capslock','left','volumeup','h','j','k','l','=','up']);
       allKeyNames = [...validKeys];
       builtin = [
         { name: 'switch-window', desc: 'Switch window', steps: 'alt+tab', category: 'Windows', shortcuts: ['alt+q'] },
         { name: 'undo', desc: 'Undo', steps: 'ctrl+z', category: 'Editing', shortcuts: [] },
         { name: 'copy', desc: 'Copy', steps: 'ctrl+c', category: 'Editing', shortcuts: [] },
         { name: 'loose', desc: 'No category', steps: 'f4', category: '', shortcuts: [] },
       ];`);
  return run;
}

// ---- parsing and serialising ----

test('parse and serialize round-trip: base first, then actions, then layers', () => {
  const run = loadGui();
  const text = 'tab = capslock\nalt+q = @switch-window\n\n[action mine]\ndescription: My thing\nctrl+shift+n\n\n[layer CapsLock]\nh = left\n';
  run(`state = parse(${JSON.stringify(text)})`);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.base)')), [['tab', 'capslock'], ['alt+q', '@switch-window']]);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.actions)')), [{ name: 'mine', desc: 'My thing', steps: ['ctrl+shift+n'] }]);
  assert.strictEqual(run('state.layers[0].trigger'), 'CapsLock');
  assert.strictEqual(run('serialize(state)'), text);
});

test('a "from = to" line after an action block ends the block', () => {
  const run = loadGui();
  run(`state = parse('[action a]\\nalt+tab\\nalt+q = @a\\n')`);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.actions[0].steps)')), ['alt+tab']);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.base)')), [['alt+q', '@a']]);
});

test('findEq skips the = of a combo like alt+=', () => {
  const run = loadGui();
  assert.strictEqual(run(`findEq('alt+= = ctrl+c')`), 6);
  assert.strictEqual(run(`findEq('tab = a')`), 4);
  assert.strictEqual(run(`findEq('alt+=')`), -1);
});

// ---- chords and validation ----

test('normChord puts modifiers in a fixed order', () => {
  const run = loadGui();
  assert.strictEqual(run(`normChord('Shift + Ctrl + A')`), 'ctrl+shift+a');
  assert.strictEqual(run(`normChord('q')`), 'q');
});

test('validChord / validSteps / validTarget', () => {
  const run = loadGui();
  assert.strictEqual(run(`validChord('alt+q')`), true);
  assert.strictEqual(run(`validChord('foo+q')`), false);   // unknown modifier
  assert.strictEqual(run(`validChord('alt+nokey')`), false);
  assert.strictEqual(run(`validSteps('ctrl+c, ctrl+v')`), true);
  assert.strictEqual(run(`validSteps('')`), false);
  assert.strictEqual(run(`validTarget('@switch-window')`), true);  // built-in
  assert.strictEqual(run(`validTarget('@nope')`), false);
  assert.strictEqual(run(`validTarget('none')`), true);            // clears a combo
  assert.strictEqual(run(`validTarget('volumeup')`), true);
});

test('steps text <-> editor steps', () => {
  const run = loadGui();
  assert.deepStrictEqual(JSON.parse(run(`JSON.stringify(stepsFromText('ctrl+c, ctrl+v'))`)),
    [{ mods: ['ctrl'], key: 'c' }, { mods: ['ctrl'], key: 'v' }]);
  assert.strictEqual(run(`stepsText(stepsFromText('shift+ctrl+tab'))`), 'ctrl+shift+tab');
});

// ---- actions ----

test('built-in actions group by category in file order; no category becomes Other', () => {
  const run = loadGui();
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(builtinGroups())')), [
    { category: 'Windows', names: ['switch-window'] },
    { category: 'Editing', names: ['undo', 'copy'] },
    { category: 'Other', names: ['loose'] },
  ]);
});

test('your own version of a built-in action wins in actionInfo', () => {
  const run = loadGui();
  assert.strictEqual(run(`actionInfo('undo').steps`), 'ctrl+z');
  assert.strictEqual(run(`actionInfo('undo').builtin`), true);
  run(`state = parse('[action undo]\\ndescription: Undo\\nalt+z\\n')`);
  assert.strictEqual(run(`actionInfo('undo').steps`), 'alt+z');
  assert.strictEqual(run(`actionInfo('undo').builtin`), false);
  assert.strictEqual(run(`actionInfo('nope')`), null);
});

// ---- shortcuts: defaults, clearing, removing ----

test('a default shortcut shows until the config binds or clears that combo', () => {
  const run = loadGui();
  run(`state = parse('')`);
  assert.deepStrictEqual(JSON.parse(run(`JSON.stringify(bindingsFor('switch-window').map(b => [b.from, !!b.isDefault]))`)), [['alt+q', true]]);
  run(`state = parse('alt+q = none\\n')`);
  assert.strictEqual(run(`bindingsFor('switch-window').length`), 0);          // cleared
  run(`state = parse('alt+q = @copy\\n')`);
  assert.strictEqual(run(`bindingsFor('switch-window').length`), 0);          // rebound to another action
  assert.strictEqual(run(`bindingsFor('copy').length`), 1);
});

test('removing a default shortcut writes none; removing your own deletes the line', () => {
  const run = loadGui();
  run(`state = parse('alt+w = @copy\\n')`);
  run(`bindingsFor('switch-window').forEach(clearShortcut)`);       // the default
  run(`bindingsFor('copy').forEach(clearShortcut)`);                // your own binding
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.base)')), [['alt+q', 'none']]);
});

test('setTarget adds, replaces and removes by normalised combo', () => {
  const run = loadGui();
  run(`state = parse('Shift+Ctrl+A = tab\\n')`);
  run(`setTarget(state.base, 'ctrl+shift+a', 'q')`);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.base)')), [['Shift+Ctrl+A', 'q']]);
  run(`setTarget(state.base, 'alt+z', '@undo')`);
  assert.strictEqual(run('state.base.length'), 2);
  run(`setTarget(state.base, 'ctrl+shift+a', '')`);
  assert.deepStrictEqual(JSON.parse(run('JSON.stringify(state.base)')), [['alt+z', '@undo']]);
});
