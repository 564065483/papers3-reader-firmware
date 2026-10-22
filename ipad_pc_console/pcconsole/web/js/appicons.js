// 应用图标加载器：只加载屏幕上可见的图标，合并成批量请求，并限制同时进行的请求数，
// 保证点按钮、切换窗口等操作始终有空闲连接，不会被几百个图标请求卡住。
import { api } from './api.js';
import { colorFor, h } from './ui.js';

const cache = new Map();     // `${size}|${path}` → data URI（null 表示无图标）
const waiting = new Map();   // key → [回调]
const queue = new Map();     // key → { path, size }
let inflight = 0, timer = null;
const BATCH = 24, MAX_INFLIGHT = 2;

function schedule() {
  if (!timer) timer = setTimeout(flush, 40);
}

function flush() {
  timer = null;
  while (queue.size && inflight < MAX_INFLIGHT) {
    const first = queue.values().next().value;
    const batch = [];
    for (const [key, item] of queue) {
      if (item.size !== first.size) continue;
      batch.push([key, item.path]);
      queue.delete(key);
      if (batch.length >= BATCH) break;
    }
    inflight++;
    api('/api/icons', { body: { size: first.size, paths: batch.map(([, p]) => p) }, timeout: 30000 })
      .then(({ icons }) => batch.forEach(([key, p]) => settle(key, icons[p] || null)))
      .catch(() => batch.forEach(([key]) => settle(key, null)))
      .finally(() => { inflight--; if (queue.size) schedule(); });
  }
}

function settle(key, uri) {
  cache.set(key, uri);
  for (const cb of waiting.get(key) || []) cb(uri);
  waiting.delete(key);
}

export function loadIcon(path, size, cb) {
  const key = `${size}|${path}`;
  if (cache.has(key)) { cb(cache.get(key)); return; }
  if (!waiting.has(key)) waiting.set(key, []);
  waiting.get(key).push(cb);
  if (!queue.has(key)) { queue.set(key, { path, size }); schedule(); }
}

const io = 'IntersectionObserver' in window ? new IntersectionObserver((entries) => {
  for (const e of entries) {
    if (!e.isIntersecting) continue;
    io.unobserve(e.target);
    e.target._load?.();
  }
}, { rootMargin: '400px 0px' }) : null;

function initials(name) {
  const clean = name.replace(/[^\p{L}\p{N}]/gu, '');
  return (/^[\x00-\x7f]/.test(clean) ? clean.slice(0, 2) : clean.slice(0, 1)).toUpperCase() || '?';
}

/** 应用图标：先显示彩色首字母，进入可视区域后再换成电脑上的真实图标 */
export function appAvatar(name, path, size = 58) {
  const el = h('div', { class: 'avatar', style: { background: colorFor(name || '?'), width: `${size}px`, height: `${size}px`, fontSize: `${Math.round(size * 0.38)}px` } }, h('span', {}, initials(name || '?')));
  if (!path) return el;
  const px = Math.min(256, Math.round(size * 2));
  const apply = (uri) => {
    if (!uri) return;
    const img = h('img', { alt: '', src: uri });
    el.append(img);
    el.classList.add('has-img');
  };
  const key = `${px}|${path}`;
  if (cache.has(key)) apply(cache.get(key));
  else {
    el._load = () => loadIcon(path, px, apply);
    if (io) io.observe(el); else el._load();
  }
  return el;
}
