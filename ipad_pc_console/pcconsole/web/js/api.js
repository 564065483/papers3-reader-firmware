// 与电脑端通信：HTTP 接口、WebSocket 实时通道、本地偏好设置

const PREF_KEY = 'pcc.prefs';
const TOKEN_KEY = 'pcc.token';

const DEFAULT_PREFS = {
  theme: 'dark',          // dark / light / auto
  tile: 'm',              // s / m / l
  sensitivity: 1.6,       // 触控板灵敏度
  accel: 0.6,             // 指针加速
  scrollSpeed: 1.0,
  naturalScroll: true,
  cmdAsCtrl: true,        // iPad 外接键盘的 ⌘ 当作 Ctrl
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

export const prefs = { ...DEFAULT_PREFS, ...JSON.parse(safeGet(PREF_KEY) || '{}') };
export function savePrefs() { safeSet(PREF_KEY, JSON.stringify(prefs)); }

export function applyTheme() {
  document.documentElement.dataset.theme = prefs.theme;
  document.documentElement.dataset.tile = prefs.tile;
}

/** 全局共享状态：info 为电脑基本信息，pages 为控制台按钮配置 */
export const state = { info: null, pages: null };

// ---- 事件总线 ----
const listeners = {};
export const bus = {
  on(type, fn) { (listeners[type] ||= new Set()).add(fn); return () => listeners[type].delete(fn); },
  emit(type, data) { listeners[type]?.forEach((fn) => fn(data)); },
};

// ---- 令牌 ----
let token = safeGet(TOKEN_KEY) || '';
export function getToken() { return token; }
export function setToken(t) { token = t || ''; safeSet(TOKEN_KEY, t || null); }
export function withToken(url) {
  return url + (url.includes('?') ? '&' : '?') + 'token=' + encodeURIComponent(token);
}

export class ApiError extends Error {
  constructor(message, status) { super(message); this.status = status; }
}

export async function api(path, { method, body, raw, signal } = {}) {
  const headers = { 'X-Token': token };
  let payload = body;
  if (body !== undefined && !(body instanceof FormData)) {
    headers['Content-Type'] = 'application/json';
    payload = JSON.stringify(body);
  }
  let res;
  try {
    res = await fetch(path, { method: method || (body !== undefined ? 'POST' : 'GET'), headers, body: payload, signal });
  } catch (e) {
    if (e.name === 'AbortError') throw e;
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
    const proto = location.protocol === 'https:' ? 'wss' : 'ws';
    sock = new WebSocket(`${proto}://${location.host}/ws?token=${encodeURIComponent(token)}`);
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
