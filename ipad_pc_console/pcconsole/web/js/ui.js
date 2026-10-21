// 通用界面工具：DOM 构建、提示、弹窗、格式化

export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'class') el.className = v;
    else if (k === 'style' && typeof v === 'object') {
      for (const [p, val] of Object.entries(v)) {
        if (p.startsWith('--')) el.style.setProperty(p, val); else el.style[p] = val;
      }
    }
    else if (k.startsWith('on') && typeof v === 'function') el.addEventListener(k.slice(2), v);
    else if (k === 'html') el.innerHTML = v;
    else if (k in el && typeof v !== 'string') el[k] = v;
    else el.setAttribute(k, v === true ? '' : v);
  }
  for (const c of children.flat(Infinity)) {
    if (c === null || c === undefined || c === false) continue;
    el.append(c instanceof Node ? c : document.createTextNode(String(c)));
  }
  return el;
}

export function toast(msg, kind = '') {
  const el = h('div', { class: `toast ${kind}` }, msg);
  document.getElementById('toasts').append(el);
  setTimeout(() => el.remove(), kind === 'error' ? 3800 : 2200);
}

export const showError = (e) => toast(e?.message || String(e), 'error');

/** 通用弹窗。buttons: [{label, kind, value, left, onClick}]。
 *  返回 Promise（resolve 为所点按钮的 value，点遮罩为 null），并带有 close(value) 方法。 */
export function modal({ title, body, buttons = [{ label: '关闭', value: null }], wide = false, cls = '' }) {
  let close;
  const promise = new Promise((resolve) => {
    let mask;
    close = (v) => { mask.remove(); resolve(v); };
    const footer = h('footer', {}, buttons.map((b) => h('button', {
      class: `btn ${b.kind || ''} ${b.left ? 'left' : ''}`,
      onclick: async () => {
        if (b.onClick && (await b.onClick()) === false) return;
        close(b.value);
      },
    }, b.label)));
    const box = h('div', { class: `modal ${wide ? 'wide' : ''} ${cls}` },
      title ? h('header', {}, title) : null,
      h('div', { class: 'body' }, body),
      buttons.length ? footer : null);
    mask = h('div', { class: 'modal-mask', onclick: (e) => { if (e.target === mask) close(null); } }, box);
    document.body.append(mask);
  });
  promise.close = close;
  return promise;
}

export function confirmDialog(title, message, { okText = '确定', danger = false } = {}) {
  return modal({
    title, body: h('div', { class: 'muted', style: { whiteSpace: 'pre-line' } }, message),
    buttons: [{ label: '取消', value: false }, { label: okText, kind: danger ? 'danger' : 'primary', value: true }],
  });
}

const OK = Symbol('ok');
export function promptDialog(title, value = '', placeholder = '') {
  const input = h('input', { class: 'input', value, placeholder });
  const p = modal({
    title, body: input,
    buttons: [{ label: '取消', value: null }, { label: '确定', kind: 'primary', value: OK }],
  });
  input.addEventListener('keydown', (e) => { if (e.key === 'Enter') p.close(OK); });
  setTimeout(() => { input.focus(); input.select(); }, 50);
  return p.then((v) => (v === OK ? input.value : null));
}

/** 操作菜单。options: [{label, value, danger}]，返回所选 value，取消为 null。 */
export function sheet(title, options) {
  let p;
  const body = options.map((o) => h('button', {
    class: `btn ${o.danger ? 'danger' : ''}`,
    onclick: () => p.close(o.value),
  }, o.label));
  p = modal({ title, body, buttons: [], cls: 'sheet' });
  return p;
}

export function fmtBytes(n, digits = 1) {
  if (n === null || n === undefined || isNaN(n)) return '--';
  const units = ['B', 'KB', 'MB', 'GB', 'TB'];
  let i = 0;
  while (n >= 1024 && i < units.length - 1) { n /= 1024; i++; }
  return `${n.toFixed(i === 0 ? 0 : digits)} ${units[i]}`;
}
export const fmtRate = (n) => `${fmtBytes(n)}/s`;
export function fmtDuration(sec) {
  sec = Math.floor(sec);
  const d = Math.floor(sec / 86400), hr = Math.floor((sec % 86400) / 3600), m = Math.floor((sec % 3600) / 60);
  return d ? `${d}天 ${hr}小时` : hr ? `${hr}小时 ${m}分` : `${m}分钟`;
}
export function fmtTime(ts) {
  const d = new Date(ts * 1000);
  const p = (x) => String(x).padStart(2, '0');
  return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
}

export function debounce(fn, ms) {
  let t;
  return (...a) => { clearTimeout(t); t = setTimeout(() => fn(...a), ms); };
}
export function throttle(fn, ms) {
  let last = 0, timer = null, pending;
  return (...a) => {
    pending = a;
    const now = Date.now();
    if (now - last >= ms) { last = now; fn(...a); }
    else if (!timer) timer = setTimeout(() => { timer = null; last = Date.now(); fn(...pending); }, ms - (now - last));
  };
}

/** 滑块填充色 */
export function paintRange(input) {
  const p = ((input.value - input.min) / (input.max - input.min)) * 100;
  input.style.setProperty('--p', `${p}%`);
}

export function switchEl(checked, onchange) {
  const input = h('input', { type: 'checkbox', checked, onchange: () => onchange(input.checked) });
  return h('label', { class: 'switch' }, input, h('span'));
}

/** 长按（触摸 / 鼠标通用）。返回值表示是否触发了长按，供 click 判断。 */
export function onLongPress(el, fn, ms = 550) {
  let timer = null, fired = false, x0 = 0, y0 = 0;
  el.addEventListener('pointerdown', (e) => {
    fired = false; x0 = e.clientX; y0 = e.clientY;
    timer = setTimeout(() => { fired = true; fn(e); }, ms);
  });
  el.addEventListener('pointermove', (e) => {
    if (timer && Math.hypot(e.clientX - x0, e.clientY - y0) > 10) { clearTimeout(timer); timer = null; }
  });
  const cancel = () => { clearTimeout(timer); timer = null; };
  el.addEventListener('pointerup', cancel);
  el.addEventListener('pointercancel', cancel);
  el.addEventListener('contextmenu', (e) => e.preventDefault());
  return () => fired;
}

const PALETTE = ['#3b82f6', '#06b6d4', '#14b8a6', '#22c55e', '#eab308', '#f97316', '#ef4444', '#ec4899', '#a855f7', '#6366f1'];
export function colorFor(text) {
  let hsh = 0;
  for (const ch of text) hsh = (hsh * 31 + ch.codePointAt(0)) >>> 0;
  return PALETTE[hsh % PALETTE.length];
}

/** 兼容非 HTTPS 环境的复制到 iPad 剪贴板 */
export async function copyText(text) {
  try {
    if (navigator.clipboard && window.isSecureContext) { await navigator.clipboard.writeText(text); return true; }
  } catch { /* 回退 */ }
  const ta = h('textarea', { style: { position: 'fixed', top: '-1000px', opacity: '0' } });
  ta.value = text;
  document.body.append(ta);
  ta.focus(); ta.select();
  ta.setSelectionRange(0, text.length);
  let ok = false;
  try { ok = document.execCommand('copy'); } catch { ok = false; }
  ta.remove();
  return ok;
}
