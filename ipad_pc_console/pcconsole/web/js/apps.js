// 应用与窗口：启动已安装应用（显示真实图标）、管理正在运行的窗口 / 应用
import { api, isMac, run, withToken } from './api.js';
import { icon } from './icons.js';
import { colorFor, confirmDialog, emptyState, h, segmented, showError, toast } from './ui.js';
import { pinToDashboard } from './dashboard.js';

let root, mode = 'apps', apps = [], query = '', listEl, segEl, timer = null, toolsEl;

function initials(name) {
  const clean = name.replace(/[^\p{L}\p{N}]/gu, '');
  return (/^[\x00-\x7f]/.test(clean) ? clean.slice(0, 2) : clean.slice(0, 1)).toUpperCase() || '?';
}

/** 应用图标：优先显示电脑上的真实图标，读取失败时显示彩色首字母 */
export function appAvatar(name, target, size = 58) {
  const el = h('div', { class: 'avatar', style: { background: colorFor(name), width: `${size}px`, height: `${size}px`, fontSize: `${Math.round(size * .38)}px` } }, initials(name));
  if (target) {
    const img = h('img', { alt: '', loading: 'lazy', decoding: 'async' });
    img.onload = () => el.classList.add('has-img');
    img.onerror = () => img.remove();
    img.src = withToken(`/api/icon?size=${Math.round(size * 2)}&path=${encodeURIComponent(target)}`);
    el.append(img);
  }
  return el;
}

async function loadApps(refresh = false) {
  listEl.replaceChildren(emptyState('app', '正在读取电脑上的应用…'));
  try {
    apps = (await api(`/api/apps${refresh ? '?refresh=1' : ''}`, { timeout: 40000 })).apps;
    renderApps();
  } catch (e) {
    listEl.replaceChildren(emptyState('xcircle', e.message));
  }
}

function renderApps() {
  const q = query.trim().toLowerCase();
  const shown = q ? apps.filter((a) => a.name.toLowerCase().includes(q)) : apps;
  if (!shown.length) { listEl.replaceChildren(emptyState('search', '没有匹配的应用')); return; }
  listEl.replaceChildren(h('div', { class: 'app-grid list-anim' }, shown.map((a, i) => h('div', {
    class: 'app-cell', style: { '--i': i },
    onclick: async () => {
      try { await run([{ type: 'launch', value: a.target }]); toast(`已打开 ${a.name}`, 'ok'); } catch (e) { showError(e); }
    },
  },
  h('button', {
    class: 'pin', title: '固定到控制台',
    onclick: (e) => { e.stopPropagation(); pinToDashboard({ label: a.name, icon: 'sf:app', color: 'indigo', actions: [{ type: 'launch', value: a.target }] }); },
  }, icon('pin')),
  appAvatar(a.name, a.target),
  h('div', { class: 'nm' }, a.name)))));
}

async function loadWindows() {
  try {
    const { windows } = await api('/api/windows');
    if (mode !== 'windows') return;
    if (!windows.length) { listEl.replaceChildren(emptyState('macwindow', isMac() ? '没有正在运行的应用' : '没有打开的窗口')); return; }
    const mac = isMac();
    const op = async (w, o) => {
      try { await api('/api/windows', { body: { hwnd: w.hwnd, op: o } }); setTimeout(loadWindows, 300); } catch (e) { showError(e); }
    };
    const fresh = !listEl.querySelector('.list');
    listEl.replaceChildren(h('div', { class: `list ${fresh ? 'list-anim' : ''}` }, windows.map((w, i) => h('div', { class: `item ${w.foreground ? 'fg' : ''}`, style: { '--i': i } },
      appAvatar(w.process || w.title, w.icon_path, 44),
      h('div', { class: 'grow' }, h('div', { class: 't ellipsis' }, w.title),
        h('div', { class: 'small muted ellipsis' }, [w.process, `PID ${w.pid}`, w.minimized ? (mac ? '已隐藏' : '已最小化') : '', w.foreground ? '当前' : ''].filter(Boolean).join(' · '))),
      h('button', { class: 'btn sm primary', onclick: () => op(w, 'activate') }, '切换'),
      h('button', { class: 'btn sm icon', title: mac ? '隐藏' : '最小化', onclick: () => op(w, 'minimize') }, icon(mac ? 'eye.slash' : 'minus')),
      h('button', { class: 'btn sm icon', title: mac ? '全屏' : '最大化', onclick: () => op(w, w.maximized ? 'restore' : 'maximize') }, icon(w.maximized ? 'shrink' : 'fullscreen')),
      h('button', {
        class: 'btn sm icon tinted red', title: mac ? '退出' : '关闭',
        onclick: async () => {
          const what = mac ? `退出「${w.process}」` : `关闭「${w.title}」`;
          if (await confirmDialog(mac ? '退出应用' : '关闭窗口', `${what}？未保存的内容程序会提示保存。`, { okText: mac ? '退出' : '关闭', danger: true })) op(w, 'close');
        },
      }, icon('xmark'))))));
  } catch (e) {
    listEl.replaceChildren(emptyState('xcircle', e.message));
  }
}

function render() {
  clearInterval(timer);
  toolsEl.classList.toggle('hidden', mode !== 'apps');
  if (mode === 'apps') { if (apps.length) renderApps(); else loadApps(); }
  else { listEl.replaceChildren(); loadWindows(); timer = setInterval(loadWindows, 3000); }
}

export default {
  id: 'apps', title: '应用与窗口', short: '应用', icon: 'app',
  mount(el) {
    root = el;
    segEl = segmented([
      { value: 'apps', label: '已安装应用', icon: 'app' },
      { value: 'windows', label: isMac() ? '正在运行' : '打开的窗口', icon: 'macwindow' },
    ], mode, (m) => { mode = m; render(); });
    const search = h('input', { class: 'input', placeholder: '搜索应用', style: { maxWidth: '320px', borderRadius: '999px' }, oninput: () => { query = search.value; renderApps(); } });
    listEl = h('div');
    toolsEl = h('div', { class: 'row' }, search, h('button', { class: 'btn icon', onclick: () => loadApps(true) }, icon('refresh')));
    el.append(h('div', { class: 'toolbar' }, segEl, h('div', { class: 'grow' }, ''), toolsEl), listEl);
  },
  show() { render(); },
  hide() { clearInterval(timer); },
};
