// Shared state. Loaded first; every other script reads and writes these.
// state: { base: [[from, to]], layers: [{ trigger, maps: [[from, to]] }] }
const $ = id => document.getElementById(id);
let state = { base: [], layers: [], actions: [] }, saved = '', current = 'base', validKeys = new Set();
let allKeyNames = [];
let builtin = [];  // [{ name, desc, steps }] from config/actions.<os>.txt, read-only
let device = '';   // '' = default config, else a hardware id
let devices = [];  // [{ id, own, name }]
let lastKb = '';
let layout = { rows: [], labels: {}, mods: {}, actionsNote: '' };  // web/layouts/<os>.json for the OS the program was built for
