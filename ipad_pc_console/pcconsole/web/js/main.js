import {
  activeDevice, api, applyTheme, bus, deviceName, devices, getToken, prefs, rememberInfo, savePrefs, setToken, state, ws,
} from './api.js';
import { icon } from './icons.js';
import { fmtRate, h } from './ui.js';
import { openSwitcher, osIcon } from './devices.js';
import { bindAutoFullscreen, fullscreenButton, installBanner, updateManifestLink } from './display.js';
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

// 让 iOS Safari 上的 :active 按压效果生效
document.addEventListener('touchstart', () => {}, { passive: true });

function moveIndicator(animate = true) {
  const nav = document.getElementById('nav');
  const ind = nav.querySelector('.indicator');
  const btn = nav.querySelector('button.on');
  if (!ind || !btn) return;
  if (!animate) ind.style.transition = 'none';
  const horizontal = getComputedStyle(nav).flexDirection === 'row';
  ind.style.transform = horizontal ? `translateX(${btn.offsetLeft}px)` : `translateY(${btn.offsetTop}px)`;
  ind.style.width = horizontal ? `${btn.offsetWidth}px` : '';
  ind.style.height = horizontal ? '' : `${btn.offsetHeight}px`;
  if (!animate) { void ind.offsetWidth; ind.style.transition = ''; }
}

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
  const view = views[tab.id];
  view.classList.remove('hidden', 'enter');
  void view.offsetWidth;
  view.classList.add('enter');
  document.querySelectorAll('#nav button').forEach((b) => b.classList.toggle('on', b.dataset.id === tab.id));
  moveIndicator(!!current);
  document.getElementById('title').textContent = tab.title;
  current = tab;
  tab.show?.();
  prefs.lastTab = tab.id;
  savePrefs();
}

function buildNav() {
  const nav = document.getElementById('nav');
  nav.replaceChildren(h('i', { class: 'indicator' }), ...TABS.map((t) => h('button', { 'data-id': t.id, onclick: () => showTab(t.id) },
    icon(t.icon), t.short || t.title)));
  window.addEventListener('resize', () => moveIndicator(false));
}

function paintDevicePill() {
  const d = activeDevice();
  document.getElementById('dev-pill').replaceChildren(
    h('i', { class: `dot ${ws.connected ? 'on' : ''}`, id: 'conn-dot' }),
    icon(osIcon(state.info?.os_type || d?.os)),
    h('b', {}, d ? deviceName(d) : '电脑'),
    icon('chevron.updown'));
}

function bindStatus() {
  document.getElementById('dev-pill').onclick = openSwitcher;
  bus.on('ws', (on) => document.getElementById('conn-dot')?.classList.toggle('on', on));
  bus.on('ws-error', (msg) => import('./ui.js').then(({ toast }) => toast(msg, 'error')));
  const set = (id, iconName, text) => document.getElementById(id).replaceChildren(icon(iconName), h('span', {}, text));
  bus.on('stats', (s) => {
    set('st-cpu', 'cpu', `${Math.round(s.cpu)}%`);
    set('st-mem', 'memory', `${Math.round(s.mem.percent)}%`);
    set('st-net', 'network', `${fmtRate(s.net.down)}`);
    const bat = document.getElementById('st-bat');
    if (s.battery) {
      bat.classList.remove('hidden');
      set('st-bat', s.battery.plugged ? 'bolt' : 'battery', `${s.battery.percent}%`);
    }
  });
}

// ---------------- 登录 / 离线 ----------------
function loginScreen({ title, tip, offline = false }) {
  document.getElementById('app').classList.add('hidden');
  const box = document.getElementById('login');
  box.classList.remove('hidden');
  const switchBtn = h('button', { class: 'btn sm ghost', onclick: openSwitcher }, icon('shuffle'), '切换或添加电脑');
  if (offline) {
    box.replaceChildren(h('div', { class: 'login-card' },
      h('div', { style: { color: 'var(--muted)' } }, icon('wifi', 'big')),
      h('h1', {}, title),
      h('div', { class: 'muted', style: { textAlign: 'center', maxWidth: '360px' } }, tip),
      h('div', { class: 'row' }, h('button', { class: 'btn primary', onclick: () => location.reload() }, icon('refresh'), '重试'), switchBtn)));
    box.querySelector('svg.big').style.cssText = 'width:54px;height:54px';
    return;
  }
  let pin = '';
  const dots = h('div', { class: 'pin-dots' });
  const tipEl = h('div', { class: 'muted' }, tip);
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
      tipEl.textContent = e.message;
    }
  };
  const press = (k) => {
    if (k === 'del') pin = pin.slice(0, -1);
    else if (k === 'ok') return submit();
    else if (pin.length < 12) pin += k;
    paint();
    if (pin.length === 6) setTimeout(submit, 160);
  };
  const keys = ['1', '2', '3', '4', '5', '6', '7', '8', '9', 'ok', '0', 'del'];
  const pad = h('div', { class: 'pin-pad' }, keys.map((k) => h('button', {
    class: k.length > 1 ? 'fn' : '', onclick: () => press(k),
  }, k === 'del' ? icon('chevron.left') : k === 'ok' ? '确定' : k)));
  document.onkeydown = (e) => {
    if (box.classList.contains('hidden')) return;
    if (/^\d$/.test(e.key)) press(e.key);
    else if (e.key === 'Backspace') press('del');
    else if (e.key === 'Enter') press('ok');
  };
  box.replaceChildren(h('div', { class: 'login-card' },
    h('img', { class: 'logo', src: '/static/icon-180.png', alt: '' }),
    h("h1", {}, title), tipEl, dots, pad, switchBtn));
  paint();
}

let started = false;
async function start() {
  const d = activeDevice();
  try {
    state.info = await api('/api/info', { timeout: 8000 });
  } catch (e) {
    if (e.status === 401) return loginScreen({ title: `连接到 ${deviceName(d)}`, tip: '输入电脑端显示的 PIN 码' });
    return loginScreen({ offline: true, title: `无法连接 ${deviceName(d)}`, tip: '请确认电脑已开机、控制台服务正在运行，并且与 iPad 在同一个局域网。' });
  }
  rememberInfo(state.info);
  document.getElementById('app').classList.remove('hidden');
  document.getElementById('login').classList.add('hidden');
  if (!started) {
    started = true;
    buildNav();
    bindStatus();
    const fsBtn = fullscreenButton();
    if (fsBtn) document.getElementById('dev-pill').before(fsBtn);
    const banner = installBanner();
    if (banner) document.getElementById('views').before(banner);
    bindAutoFullscreen();
    showTab(prefs.lastTab);
  }
  updateManifestLink();
  paintDevicePill();
  ws.connect();
  bus.emit('info', state.info);
}

bus.on('unauthorized', () => {
  ws.close();
  setToken('');
  loginScreen({ title: `连接到 ${deviceName(activeDevice())}`, tip: '授权已失效，请重新输入 PIN 码' });
});

// 扫描二维码进入时 URL 中带有 token（属于提供本页面的那台电脑）
const params = new URLSearchParams(location.search);
if (params.get('token')) {
  const local = devices.find((x) => x.url === '');
  local.token = params.get('token');
  if (activeDevice() !== local) { localStorage.setItem('pcc.active', local.id); }
  localStorage.setItem('pcc.devices', JSON.stringify(devices));
  history.replaceState(null, '', location.pathname);
  if (activeDevice() !== local) location.reload();
}
applyTheme();
if (getToken()) start(); else loginScreen({ title: `连接到 ${deviceName(activeDevice())}`, tip: '输入电脑端显示的 PIN 码' });

// 阻止 iOS 双指缩放整个页面
document.addEventListener('gesturestart', (e) => e.preventDefault());
