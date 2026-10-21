// 应用与窗口：启动已安装应用、管理正在运行的窗口
import { api, run } from './api.js';
import { colorFor, confirmDialog, h, showError, toast } from './ui.js';
import { pinToDashboard } from './dashboard.js';

let root, mode = 'apps', apps = [], query = '', listEl, segEl, timer = null;

async function loadApps(refresh = false) {
  listEl.replaceChildren(h('div', { class: 'empty' }, '正在读取开始菜单中的应用…'));
  try {
    apps = (await api(`/api/apps${refresh ? '?refresh=1' : ''}`)).apps;
    renderApps();
  } catch (e) {
    listEl.replaceChildren(h('div', { class: 'empty' }, e.message));
  }
}

function initials(name) {
  const clean = name.replace(/[^\p{L}\p{N}]/gu, '');
  return (/^[\x00-\x7f]/.test(clean) ? clean.slice(0, 2) : clean.slice(0, 1)).toUpperCase() || '?';
}

function renderApps() {
  const q = query.trim().toLowerCase();
  const shown = q ? apps.filter((a) => a.name.toLowerCase().includes(q)) : apps;
  if (!shown.length) { listEl.replaceChildren(h('div', { class: 'empty' }, '没有匹配的应用')); return; }
  listEl.replaceChildren(h('div', { class: 'app-grid' }, shown.map((a) => h('div', {
    class: 'app-cell',
    onclick: async () => {
      try { await run([{ type: 'launch', value: a.target }]); toast(`已启动 ${a.name}`, 'ok'); } catch (e) { showError(e); }
    },
  },
  h('button', { class: 'pin', title: '固定到控制台', onclick: (e) => { e.stopPropagation(); pinToDashboard({ label: a.name, icon: '🚀', color: 'indigo', actions: [{ type: 'launch', value: a.target }] }); } }, '📌'),
  h('div', { class: 'avatar', style: { background: colorFor(a.name) } }, initials(a.name)),
  h('div', { class: 'nm' }, a.name)))));
}

async function loadWindows() {
  try {
    const { windows } = await api('/api/windows');
    if (mode !== 'windows') return;
    if (!windows.length) { listEl.replaceChildren(h('div', { class: 'empty' }, '没有打开的窗口')); return; }
    const op = async (w, o) => {
      try { await api('/api/windows', { body: { hwnd: w.hwnd, op: o } }); setTimeout(loadWindows, 300); } catch (e) { showError(e); }
    };
    listEl.replaceChildren(h('div', { class: 'list' }, windows.map((w) => h('div', { class: `item ${w.foreground ? 'fg' : ''}` },
      h('div', { class: 'avatar', style: { background: colorFor(w.process || w.title), width: '44px', height: '44px', fontSize: '17px' } }, initials(w.process || w.title)),
      h('div', { class: 'grow' }, h('div', { class: 't ellipsis' }, w.title),
        h('div', { class: 'small muted ellipsis' }, `${w.process} · PID ${w.pid}${w.minimized ? ' · 已最小化' : ''}${w.foreground ? ' · 当前窗口' : ''}`)),
      h('button', { class: 'btn sm primary', onclick: () => op(w, 'activate') }, '切换'),
      h('button', { class: 'btn sm', onclick: () => op(w, 'minimize') }, '—'),
      h('button', { class: 'btn sm', onclick: () => op(w, w.maximized ? 'restore' : 'maximize') }, w.maximized ? '❐' : '☐'),
      h('button', {
        class: 'btn sm', onclick: async () => {
          if (await confirmDialog('关闭窗口', `关闭「${w.title}」？未保存的内容程序会提示保存。`, { okText: '关闭' })) op(w, 'close');
        },
      }, '✕')))));
  } catch (e) {
    listEl.replaceChildren(h('div', { class: 'empty' }, e.message));
  }
}

function render() {
  segEl.querySelectorAll('button').forEach((b) => b.classList.toggle('on', b.dataset.m === mode));
  clearInterval(timer);
  root.querySelector('.apps-tools').classList.toggle('hidden', mode !== 'apps');
  if (mode === 'apps') { if (apps.length) renderApps(); else loadApps(); }
  else { loadWindows(); timer = setInterval(loadWindows, 3000); }
}

export default {
  id: 'apps', title: '应用与窗口', short: '应用', icon: '🚀',
  mount(el) {
    root = el;
    segEl = h('div', { class: 'seg' },
      h('button', { 'data-m': 'apps', onclick: () => { mode = 'apps'; render(); } }, '📦 已安装应用'),
      h('button', { 'data-m': 'windows', onclick: () => { mode = 'windows'; render(); } }, '🪟 运行中的窗口'));
    const search = h('input', { class: 'input', placeholder: '🔍 搜索应用', style: { maxWidth: '360px' }, oninput: () => { query = search.value; renderApps(); } });
    listEl = h('div');
    el.append(h('div', { class: 'toolbar' }, segEl, h('div', { class: 'grow' }),
      h('div', { class: 'row apps-tools' }, search, h('button', { class: 'btn', onclick: () => loadApps(true) }, '🔄'))), listEl);
  },
  show() { render(); },
  hide() { clearInterval(timer); },
};
