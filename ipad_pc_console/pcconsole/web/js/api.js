// 与电脑端通信：多台电脑管理、HTTP 接口、WebSocket 实时通道、本地偏好设置

const PREF_KEY = 'pcc.prefs';
const DEV_KEY = 'pcc.devices';
const ACTIVE_KEY = 'pcc.active';

const DEFAULT_PREFS = {
  theme: 'dark',          // dark / light / auto
  tile: 'm',              // s / m / l
  sensitivity: 1.6,       // 触控板灵敏度
  accel: 0.6,             // 指针加速
  scrollSpeed: 1.0,
  naturalScroll: true,
  cmdAsCtrl: true,        // 控制 Windows 时，iPad 外接键盘的 ⌘ 当作 Ctrl
  screenQuality: 60,
  screenWidth: 1600,
  screenFps: 12,
  screenCursor: true,
  lastTab: 'dashboard',
};

function safeGet(key) {
  try { return localStorage.getItem(key); } catch { return null; }
}
function safeSet(key, value) {
  try {
    if (value === null) localStorage.removeItem(key); else localStorage.setItem(key, value);
  } catch { /* 隐私模式下忽略 */ }
}
function safeJSON(text, fallback) {
  try { return JSON.parse(text) ?? fallback; } catch { return fallback; }
}

export const prefs = { ...DEFAULT_PREFS, ...safeJSON(safeGet(PREF_KEY), {}) };
export function savePrefs() { safeSet(PREF_KEY, JSON.stringify(prefs)); }

export function applyTheme() {
  document.documentElement.dataset.theme = prefs.theme;
  document.documentElement.dataset.tile = prefs.tile;
}

/** 全局共享状态：info 为当前电脑的基本信息，pages 为控制台按钮配置 */
export const state = { info: null, pages: null };
export const osType = () => state.info?.os_type || activeDevice()?.os || 'windows';
export const isMac = () => osType() === 'mac';

// ---- 事件总线 ----
const listeners = {};
export const bus = {
  on(type, fn) { (listeners[type] ||= new Set()).add(fn); return () => listeners[type].delete(fn); },
  emit(type, data) { listeners[type]?.forEach((fn) => fn(data)); },
};

// ---- 多台电脑 ----
// 每台电脑：{ id, url, token, name, os, deviceId }；url 为空表示提供本页面的那台电脑
export const devices = safeJSON(safeGet(DEV_KEY), []);
let activeId = safeGet(ACTIVE_KEY) || '';
(() => {
  const legacy = safeGet('pcc.token'); // 旧版本只保存一个令牌
  if (!devices.some((d) => d.url === '')) devices.unshift({ id: 'local', url: '', token: legacy || '', name: '', os: '' });
  if (legacy) safeSet('pcc.token', null);
  if (!devices.some((d) => d.id === activeId)) activeId = devices[0].id;
})();
const saveDevices = () => { safeSet(DEV_KEY, JSON.stringify(devices)); safeSet(ACTIVE_KEY, activeId); };
saveDevices();

export const activeDevice = () => devices.find((d) => d.id === activeId);
export const base = () => activeDevice()?.url || '';
export function getToken() { return activeDevice()?.token || ''; }
export function setToken(t) { const d = activeDevice(); if (d) { d.token = t || ''; saveDevices(); } }
export function withToken(url) {
  return base() + url + (url.includes('?') ? '&' : '?') + 'token=' + encodeURIComponent(getToken());
}
export function rememberInfo(info) {
  const d = activeDevice();
  if (!d) return;
  Object.assign(d, { name: info.name || info.hostname, os: info.os_type, deviceId: info.device_id });
  // 同一台电脑可能被以不同地址添加过，合并掉重复项
  for (let i = devices.length - 1; i >= 0; i--) {
    const o = devices[i];
    if (o !== d && o.deviceId && o.deviceId === d.deviceId && o.url !== '') devices.splice(i, 1);
  }
  saveDevices();
}
export function addDevice({ url, token, name, os, deviceId }) {
  let d = devices.find((x) => (deviceId && x.deviceId === deviceId) || (url && x.url === url));
  if (d) Object.assign(d, { token, name, os, deviceId, url: d.url === '' ? '' : url });
  else devices.push(d = { id: Math.random().toString(36).slice(2, 10), url, token, name, os, deviceId });
  saveDevices();
  return d.id;
}
export function removeDevice(id) {
  const i = devices.findIndex((d) => d.id === id);
  if (i >= 0 && devices[i].url !== '') devices.splice(i, 1);
  if (activeId === id) activeId = devices[0].id;
  saveDevices();
}
export function renameDevice(id, name) {
  const d = devices.find((x) => x.id === id);
  if (d) { d.alias = name; saveDevices(); }
}
export const deviceName = (d) => d.alias || d.name || (d.url ? d.url.replace(/^https?:\/\//, '') : '这台电脑');
export function switchDevice(id) {
  activeId = id;
  saveDevices();
  location.reload();
}

/** “192.168.1.5” → “http://192.168.1.5:8765” */
export function normalizeUrl(input) {
  let s = input.trim().replace(/\/+$/, '');
  if (!s) return '';
  if (!/^https?:\/\//i.test(s)) s = `http://${s}`;
  const u = new URL(s);
  if (!u.port) u.port = '8765';
  return u.origin;
}

async function rawFetch(url, opts = {}, timeout = 6000) {
  const ctrl = new AbortController();
  const timer = setTimeout(() => ctrl.abort(), timeout);
  try { return await fetch(url, { ...opts, signal: opts.signal || ctrl.signal }); } finally { clearTimeout(timer); }
}

export async function pingDevice(d, timeout = 1800) {
  try {
    const res = await rawFetch(`${d.url}/api/ping`, {}, timeout);
    return res.ok ? await res.json() : null;
  } catch { return null; }
}

/** 用 PIN 登录另一台电脑，返回 { token, info } */
export async function loginDevice(url, pin) {
  let res;
  try {
    res = await rawFetch(`${url}/api/login`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ pin }) });
  } catch { throw new ApiError('连接不上这台电脑，请检查地址和防火墙', 0); }
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new ApiError(data.error || '登录失败', res.status);
  const info = await (await rawFetch(`${url}/api/info`, { headers: { 'X-Token': data.token } })).json();
  return { token: data.token, info };
}

export class ApiError extends Error {
  constructor(message, status) { super(message); this.status = status; }
}

export async function api(path, { method, body, raw, signal, timeout = 20000 } = {}) {
  const headers = { 'X-Token': getToken() };
  let payload = body;
  if (body !== undefined && !(body instanceof FormData)) {
    headers['Content-Type'] = 'application/json';
    payload = JSON.stringify(body);
  }
  let res;
  try {
    res = await rawFetch(base() + path, { method: method || (body !== undefined ? 'POST' : 'GET'), headers, body: payload, signal }, timeout);
  } catch (e) {
    if (e.name === 'AbortError' && signal?.aborted) throw e;
    throw new ApiError('无法连接电脑，请检查服务端是否运行', 0);
  }
  if (res.status === 401) bus.emit('unauthorized');
  if (!res.ok) {
    let msg = res.statusText;
    try { msg = (await res.json()).error || msg; } catch { /* 非 JSON */ }
    throw new ApiError(msg, res.status);
  }
  return raw ? res : res.json();
}

export const run = (actions) => api('/api/run', { body: { actions } });

// ---- WebSocket ----
let sock = null;
let retry = 0;
let wanted = false;
export const ws = {
  connected: false,
  connect() {
    wanted = true;
    if (sock && sock.readyState <= 1) return;
    const origin = base() ? base().replace(/^http/, 'ws') : `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}`;
    sock = new WebSocket(`${origin}/ws?token=${encodeURIComponent(getToken())}`);
    sock.onopen = () => { retry = 0; ws.connected = true; bus.emit('ws', true); };
    sock.onclose = () => {
      ws.connected = false; bus.emit('ws', false);
      if (wanted) setTimeout(() => ws.connect(), Math.min(5000, 400 * 2 ** retry++));
    };
    sock.onerror = () => sock.close();
    sock.onmessage = (ev) => {
      let msg;
      try { msg = JSON.parse(ev.data); } catch { return; }
      if (msg.t === 'stats') bus.emit('stats', msg.d);
      else if (msg.t === 'err') bus.emit('ws-error', msg.msg);
    };
  },
  close() { wanted = false; sock?.close(); },
  send(obj) {
    if (sock && sock.readyState === 1) { sock.send(JSON.stringify(obj)); return true; }
    return false;
  },
};

// iPad 从后台回到前台时立即重连
document.addEventListener('visibilitychange', () => {
  if (!document.hidden && wanted && (!sock || sock.readyState > 1)) { retry = 0; ws.connect(); }
});
