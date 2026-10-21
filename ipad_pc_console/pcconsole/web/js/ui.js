// 通用界面工具：DOM 构建、提示、弹窗、分段控件、格式化
import { icon } from './icons.js';

export function h(tag, attrs = {}, ...children) {
  const el = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === 'class') el.className = v;
    else if (k === 'style' && typeof v === 'object') {
      for (const [p, val] of Object.entries(v)) {
        if (p.startsWith('--')) el.style.setProperty(p, val); else el.style[p] = val;
      }
    } else if (k.startsWith('on') && typeof v === 'function') el.addEventListener(k.slice(2), v);
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

/** 带图标的按钮内容：h('button', {}, ...label('gear', '设置')) */
export const label = (name, text) => [icon(name), text ? h('span', {}, text) : null];

const TOAST_ICON = { ok: 'check', error: 'xcircle', '': 'info' };
export function toast(msg, kind = '') {
  const el = h('div', { class: `toast ${kind}` }, icon(TOAST_ICON[kind] || 'info'), h('span', {}, msg));
  const box = document.getElementById('toasts');
  box.append(el);
  while (box.children.length > 3) box.firstChild.remove();
  setTimeout(() => { el.classList.add('out'); setTimeout(() => el.remove(), 350); }, kind === 'error' ? 3600 : 2000);
}

export const showError = (e) => toast(e?.message || String(e), 'error');

/** 通用弹窗。type: 'dialog'（默认）| 'alert'（居中小提示框）| 'sheet'（底部滑出面板）
 *  buttons: [{label, kind, value, left, onClick}]。
 *  返回 Promise（resolve 为所点按钮的 value，点遮罩为 null），并带有 close(value) 方法。 */
export function modal({ title, body, buttons = [{ label: '关闭', value: null }], wide = false, type = 'dialog' }) {
  let close;
  const promise = new Promise((resolve) => {
    let mask, done = false;
    close = (v) => {
      if (done) return;
      done = true;
      mask.classList.add('closing');
      setTimeout(() => mask.remove(), type === 'sheet' ? 300 : 250);
      resolve(v);
    };
    const footer = h('footer', {}, buttons.map((b) => h('button', {
      class: `btn ${b.kind || ''} ${b.left ? 'left' : ''}`,
      onclick: async () => {
        if (b.onClick && (await b.onClick()) === false) return;
        close(b.value);
      },
    }, b.label)));
    const box = h('div', { class: `modal ${wide ? 'wide' : ''} ${type}` },
      title ? h('header', {}, title) : null,
      h('div', { class: 'body' }, body),
      buttons.length ? footer : null);
    mask = h('div', { class: `modal-mask ${type === 'sheet' ? 'sheet-mask' : ''}`, onclick: (e) => { if (e.target === mask) close(null); } }, box);
    document.body.append(mask);
  });
  promise.close = close;
  return promise;
}

export function confirmDialog(title, message, { okText = '确定', danger = false } = {}) {
  return modal({
    title, type: 'alert', body: h('div', { class: 'muted', style: { whiteSpace: 'pre-line' } }, message),
    buttons: [{ label: '取消', value: false }, { label: okText, kind: danger ? 'danger' : 'primary', value: true }],
  });
}

const OK = Symbol('ok');
export function promptDialog(title, value = '', placeholder = '', { type = 'text' } = {}) {
  const input = h('input', { class: 'input', value, placeholder, type, autocapitalize: 'off', autocorrect: 'off' });
  if (type === 'tel') input.inputMode = 'numeric';
  const p = modal({
    title, type: 'alert', body: input,
    buttons: [{ label: '取消', value: null }, { label: '确定', kind: 'primary', value: OK }],
  });
  input.addEventListener('keydown', (e) => { if (e.key === 'Enter') p.close(OK); });
  setTimeout(() => { input.focus(); input.select(); }, 80);
  return p.then((v) => (v === OK ? input.value : null));
}

/** 操作菜单（底部面板）。options: [{label, icon, value, danger}]，返回所选 value，取消为 null。 */
export function sheet(title, options) {
  let p;
  const body = options.map((o) => h('button', {
    class: `btn ${o.danger ? 'danger' : ''}`,
    onclick: () => p.close(o.value),
  }, o.icon ? icon(o.icon) : null, o.label));
  p = modal({ title, body, buttons: [], type: 'sheet' });
  return p;
}

/** 分段控件：items [{value, label, icon, onClick}]；滑块在选项之间以弹簧动画移动。 */
export function segmented(items, active, onChange) {
  const thumb = h('i', { class: 'thumb' });
  const el = h('div', { class: 'seg' }, thumb);
  let current = active;
  const place = (animate) => {
    const btn = el.querySelector('button.on');
    if (!btn) { thumb.style.width = '0'; return; }
    if (!animate) thumb.style.transition = 'none';
    thumb.style.width = `${btn.offsetWidth}px`;
    thumb.style.transform = `translateX(${btn.offsetLeft}px)`;
    if (!animate) { void thumb.offsetWidth; thumb.style.transition = ''; }
  };
  const render = (list) => {
    el.querySelectorAll('button').forEach((b) => b.remove());
    for (const it of list) {
      el.append(h('button', {
        class: it.value === current ? 'on' : '',
        'data-v': String(it.value),
        onclick: () => (it.onClick ? it.onClick() : el.set(it.value, true)),
      }, it.icon ? icon(it.icon) : null, it.label));
    }
    requestAnimationFrame(() => place(false));
  };
  el.set = (value, fire = false) => {
    current = value;
    el.querySelectorAll('button').forEach((b) => b.classList.toggle('on', b.dataset.v === String(value)));
    place(true);
    if (fire) onChange?.(value);
  };
  el.update = (list, value = current) => { current = value; render(list); };
  render(items);
  if (window.ResizeObserver) new ResizeObserver(() => place(false)).observe(el);
  return el;
}

export function emptyState(iconName, text) {
  return h('div', { class: 'empty' }, icon(iconName), h('div', {}, text));
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

/** 控制中心风格的粗滑条。el.set(value) 更新显示，onInput(value) 在拖动时触发。 */
export function bigSlider(iconName, onInput, { suffix = '%' } = {}) {
  const fill = h('div', { class: 'fill' });
  const text = h('span', {}, '--');
  const ic = h('span', {}, icon(iconName));
  const el = h('div', { class: 'big-slider' }, fill, h('div', { class: 'lbl' }, ic, text));
  let value = 0;
  const paint = () => { fill.style.width = `${value}%`; text.textContent = `${Math.round(value)}${suffix}`; };
  const fromEvent = (e) => {
    const r = el.getBoundingClientRect();
    return Math.max(0, Math.min(100, ((e.clientX - r.left) / r.width) * 100));
  };
  el.addEventListener('pointerdown', (e) => {
    el.setPointerCapture(e.pointerId); el.classList.add('dragging');
    value = fromEvent(e); paint(); onInput(Math.round(value));
  });
  el.addEventListener('pointermove', (e) => {
    if (!el.classList.contains('dragging')) return;
    value = fromEvent(e); paint(); onInput(Math.round(value));
  });
  const end = () => el.classList.remove('dragging');
  el.addEventListener('pointerup', end);
  el.addEventListener('pointercancel', end);
  el.set = (v) => { if (!el.classList.contains('dragging')) { value = v ?? 0; paint(); } };
  el.setIcon = (name) => ic.replaceChildren(icon(name));
  el.setDisabled = (d) => el.classList.toggle('disabled', d);
  return el;
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

const PALETTE = ['#0a84ff', '#32ade6', '#30b0c7', '#30d158', '#ff9f0a', '#ff453a', '#ff375f', '#bf5af2', '#5e5ce6', '#ac8e68'];
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
