// 正在运行的窗口 / 应用：共享的数据获取、切换操作，以及控制台底部的“程序坞”
import { api, isMac } from './api.js';
import { appAvatar } from './appicons.js';
import { icon } from './icons.js';
import { confirmDialog, h, onLongPress, sheet, showError, toast } from './ui.js';

let last = { t: 0, data: [] };
let pending = null;

/** 获取窗口列表；1 秒内的重复请求直接复用结果 */
export function fetchWindows(force = false) {
  if (!force && Date.now() - last.t < 1000) return Promise.resolve(last.data);
  if (pending) return pending;
  pending = api('/api/windows').then(({ windows }) => {
    last = { t: Date.now(), data: windows };
    return windows;
  }).finally(() => { pending = null; });
  return pending;
}

export const appName = (w) => (w.process || w.title || '').replace(/\.exe$/i, '');

export async function windowOp(w, op, quiet = false) {
  try {
    await api('/api/windows', { body: { hwnd: w.hwnd, op } });
    if (!quiet && op === 'activate') toast(`已切换到 ${appName(w)}`, 'ok');
    last.t = 0;
  } catch (e) { showError(e); }
}

export async function closeWindow(w) {
  const mac = isMac();
  const what = mac ? `退出「${appName(w)}」` : `关闭「${w.title}」`;
  if (await confirmDialog(mac ? '退出应用' : '关闭窗口', `${what}？未保存的内容程序会提示保存。`, { okText: mac ? '退出' : '关闭', danger: true })) {
    await windowOp(w, 'close', true);
  }
}

export async function windowMenu(w) {
  const mac = isMac();
  const choice = await sheet(w.title, [
    { label: '切换到这个窗口', icon: 'macwindow', value: 'activate' },
    { label: mac ? '隐藏' : '最小化', icon: mac ? 'eye.slash' : 'minus', value: 'minimize' },
    { label: w.maximized ? '还原' : (mac ? '全屏' : '最大化'), icon: w.maximized ? 'shrink' : 'fullscreen', value: w.maximized ? 'restore' : 'maximize' },
    { label: mac ? '退出应用' : '关闭窗口', icon: 'xmark', value: 'close', danger: true },
  ]);
  if (choice === 'close') await closeWindow(w);
  else if (choice) await windowOp(w, choice);
}

/**
 * 按 key 增量更新列表：已存在的元素原地更新，不会每次刷新都整片重绘（避免闪烁和卡顿）。
 * make(item) 创建元素，update(el, item) 更新元素。
 */
export function keyedUpdate(container, items, key, make, update) {
  const existing = new Map([...container.children].filter((c) => c.dataset.key).map((c) => [c.dataset.key, c]));
  let prev = null;
  for (const item of items) {
    const k = String(key(item));
    let el = existing.get(k);
    if (el) { existing.delete(k); update(el, item); }
    else { el = make(item); el.dataset.key = k; }
    const want = prev ? prev.nextSibling : container.firstChild;
    if (el !== want) container.insertBefore(el, want);
    prev = el;
  }
  for (const el of existing.values()) el.remove();
}

/** 控制台底部的程序坞：显示正在运行的窗口，轻点即可切换，长按更多操作 */
export function createDock() {
  const list = h('div', { class: 'dock-items' });
  const el = h('div', { class: 'dock glass hidden' }, h('div', { class: 'dock-title' }, icon('dock'), isMac() ? '正在运行' : '已打开'), list);
  let timer = null;

  const make = (w) => {
    const item = h('button', { class: 'dock-item' }, appAvatar(appName(w), w.icon_path, 46), h('span', { class: 'dock-name' }));
    item._w = w;
    const longPressed = onLongPress(item, () => windowMenu(item._w));
    item.addEventListener('click', () => { if (!longPressed()) windowOp(item._w, 'activate'); });
    update(item, w);
    return item;
  };
  const update = (item, w) => {
    item._w = w;
    item.classList.toggle('active', !!w.foreground);
    item.querySelector('.dock-name').textContent = appName(w);
    item.title = w.title;
  };
  const refresh = async () => {
    try {
      const wins = await fetchWindows();
      el.classList.toggle('hidden', !wins.length);
      keyedUpdate(list, wins, (w) => w.hwnd, make, update);
    } catch { el.classList.add('hidden'); }
  };
  el.start = () => { refresh(); clearInterval(timer); timer = setInterval(refresh, 4000); };
  el.stop = () => clearInterval(timer);
  return el;
}
