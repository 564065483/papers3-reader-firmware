import { api, applyTheme, bus, getToken, prefs, savePrefs, setToken, state, ws } from './api.js';
import { fmtRate, h, showError, toast } from './ui.js';
import dashboard from './dashboard.js';
import touchpad from './touchpad.js';
import screenView from './screen.js';
import media from './media.js';
import appsView from './apps.js';
import filesView from './files.js';
import monitor from './monitor.js';
import system from './system.js';
import settings from './settings.js';

const TABS = [dashboard, touchpad, screenView, media, appsView, filesView, monitor, system, settings];
const views = {};
let current = null;
export function showTab(id) {
  const tab = TABS.find((t) => t.id === id) || TABS[0];
  if (current === tab) return;
  if (current) {
    current.hide?.();
    views[current.id].classList.add('hidden');
  }
  if (!views[tab.id]) {
    const el = h('section', { class: `view ${tab.fill ? 'fill' : ''}` });
    document.getElementById('views').append(el);
    views[tab.id] = el;
    tab.mount(el);
  }
  views[tab.id].classList.remove('hidden');
  document.querySelectorAll('#nav button').forEach((b) => b.classList.toggle('on', b.dataset.id === tab.id));
  document.getElementById('title').textContent = tab.title;
  current = tab;
  tab.show?.();
  prefs.lastTab = tab.id;
  savePrefs();
}

function buildNav() {
  const nav = document.getElementById('nav');
  nav.replaceChildren(...TABS.map((t) => h('button', { 'data-id': t.id, onclick: () => showTab(t.id) },
    h('span', { class: 'ic' }, t.icon), t.short || t.title)));
}

function bindStatus() {
  const dot = document.getElementById('conn-dot');
  bus.on('ws', (on) => dot.classList.toggle('on', on));
  bus.on('ws-error', (msg) => toast(msg, 'error'));
  bus.on('stats', (s) => {
    document.getElementById('st-cpu').textContent = `CPU ${Math.round(s.cpu)}%`;
    document.getElementById('st-mem').textContent = `内存 ${Math.round(s.mem.percent)}%`;
    document.getElementById('st-net').textContent = `↓ ${fmtRate(s.net.down)}  ↑ ${fmtRate(s.net.up)}`;
    const bat = document.getElementById('st-bat');
    if (s.battery) {
      bat.classList.remove('hidden');
      bat.textContent = `${s.battery.plugged ? '⚡' : '🔋'} ${s.battery.percent}%`;
    }
  });
}

// ---------------- 登录 ----------------
function showLogin(tip) {
  document.getElementById('app').classList.add('hidden');
  const box = document.getElementById('login');
  box.classList.remove('hidden');
  if (tip) document.getElementById('login-tip').textContent = tip;
  let pin = '';
  const dots = document.getElementById('pin-dots');
  const pad = document.getElementById('pin-pad');
  const paint = () => dots.replaceChildren(...Array.from({ length: Math.max(6, pin.length) },
    (_, i) => h('i', { class: i < pin.length ? 'on' : '' })));
  const submit = async () => {
    if (!pin) return;
    try {
      const { token } = await api('/api/login', { body: { pin } });
      setToken(token);
      box.classList.add('hidden');
      start();
    } catch (e) {
      pin = ''; paint();
      dots.classList.remove('shake'); void dots.offsetWidth; dots.classList.add('shake');
      document.getElementById('login-tip').textContent = e.message;
    }
  };
  const press = (k) => {
    if (k === '⌫') pin = pin.slice(0, -1);
    else if (k === '确定') return submit();
    else if (pin.length < 12) pin += k;
    paint();
    if (pin.length === 6) setTimeout(submit, 120);
  };
  pad.replaceChildren(...['1', '2', '3', '4', '5', '6', '7', '8', '9', '⌫', '0', '确定'].map((k) =>
    h('button', { class: k.length > 1 || k === '⌫' ? 'fn' : '', onclick: () => press(k) }, k)));
  document.onkeydown = (e) => {
    if (box.classList.contains('hidden')) return;
    if (/^\d$/.test(e.key)) press(e.key);
    else if (e.key === 'Backspace') press('⌫');
    else if (e.key === 'Enter') press('确定');
  };
  paint();
}

let started = false;
async function start() {
  try {
    state.info = await api('/api/info');
  } catch (e) {
    if (e.status === 401) return showLogin();
    showError(e);
    setTimeout(start, 3000);
    return;
  }
  document.getElementById('host').textContent = state.info.hostname;
  document.getElementById('app').classList.remove('hidden');
  if (!started) {
    started = true;
    buildNav();
    bindStatus();
    showTab(prefs.lastTab);
  }
  ws.connect();
  bus.emit('info', state.info);
}

bus.on('unauthorized', () => {
  ws.close();
  setToken('');
  showLogin('授权已失效，请重新输入 PIN 码');
});

// 扫描二维码进入时 URL 中带有 token
const params = new URLSearchParams(location.search);
if (params.get('token')) {
  setToken(params.get('token'));
  history.replaceState(null, '', location.pathname);
}
applyTheme();
if (getToken()) start(); else showLogin();

// 阻止 iOS 双指缩放整个页面
document.addEventListener('gesturestart', (e) => e.preventDefault());
