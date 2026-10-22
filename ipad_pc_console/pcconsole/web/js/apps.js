// 应用与窗口：秒开的应用列表（本地缓存 + 后台刷新）、最近使用、窗口切换器
import { activeDevice, api, isMac, run } from './api.js';
import { appAvatar } from './appicons.js';
import { icon } from './icons.js';
import { emptyState, h, onLongPress, segmented, showError, toast } from './ui.js';
import { pinToDashboard } from './dashboard.js';
import { appName, closeWindow, fetchWindows, keyedUpdate, windowMenu, windowOp } from './running.js';

let mode = 'apps', apps = [], query = '', listEl, timer = null, toolsEl, winGrid = null, refreshing = false;

const devKey = (name) => `pcc.${name}.${activeDevice()?.deviceId || activeDevice()?.id || 'local'}`;
function readLS(name, fallback) {
  try { return JSON.parse(localStorage.getItem(devKey(name))) ?? fallback; } catch { return fallback; }
}
function writeLS(name, value) {
  try { localStorage.setItem(devKey(name), JSON.stringify(value)); } catch { /* 存储已满或隐私模式 */ }
}

// ---------------- 已安装应用 ----------------
async function loadApps(force = false) {
  if (!apps.length) apps = readLS('apps', []);
  if (apps.length) renderApps();
  else listEl.replaceChildren(emptyState('app', '正在读取电脑上的应用，第一次需要几秒钟…'));
  if (refreshing) return;
  refreshing = true;
  toolsEl.querySelector('.refresh')?.classList.add('spin');
  try {
    const fresh = (await api(`/api/apps${force ? '?refresh=1' : ''}`, { timeout: 60000 })).apps;
    const changed = JSON.stringify(fresh) !== JSON.stringify(apps);
    apps = fresh;
    writeLS('apps', apps);
    if (changed && mode === 'apps') renderApps();
    if (force) toast(`已刷新，共 ${apps.length} 个应用`, 'ok');
  } catch (e) {
    if (!apps.length) listEl.replaceChildren(emptyState('xcircle', e.message)); else showError(e);
  } finally {
    refreshing = false;
    toolsEl.querySelector('.refresh')?.classList.remove('spin');
  }
}

async function launch(a, cell) {
  cell?.classList.remove('launch'); void cell?.offsetWidth; cell?.classList.add('launch');
  const recent = readLS('recent', []).filter((r) => r.target !== a.target);
  recent.unshift({ name: a.name, target: a.target });
  writeLS('recent', recent.slice(0, 10));
  try {
    await run([{ type: 'launch', value: a.target }]);
    toast(`已打开 ${a.name}`, 'ok');
  } catch (e) { showError(e); }
}

function appCell(a, i, animate) {
  const cell = h('div', { class: 'app-cell', style: { '--i': animate ? i : 0 } },
    h('button', {
      class: 'pin', title: '固定到控制台',
      onclick: (e) => { e.stopPropagation(); pinToDashboard({ label: a.name, icon: 'sf:app', color: 'indigo', actions: [{ type: 'launch', value: a.target }] }); },
    }, icon('pin')),
    appAvatar(a.name, a.target),
    h('div', { class: 'nm' }, a.name));
  cell.addEventListener('click', () => launch(a, cell));
  return cell;
}

function renderApps() {
  const q = query.trim().toLowerCase();
  const shown = q ? apps.filter((a) => a.name.toLowerCase().includes(q)) : apps;
  const parts = [];
  if (!q) {
    const known = new Set(apps.map((a) => a.target));
    const recent = readLS('recent', []).filter((r) => known.has(r.target)).slice(0, 8);
    if (recent.length) {
      parts.push(h('div', { class: 'section-title' }, '最近使用'),
        h('div', { class: 'app-grid' }, recent.map((a, i) => appCell(a, i, false))),
        h('div', { class: 'section-title' }, `全部应用（${apps.length}）`));
    }
  }
  if (!shown.length) parts.push(emptyState('search', '没有匹配的应用'));
  else parts.push(h('div', { class: 'app-grid list-anim' }, shown.map((a, i) => appCell(a, i, i < 24))));
  listEl.replaceChildren(...parts);
}

// ---------------- 窗口切换器 ----------------
function winCard(w) {
  const card = h('div', { class: 'win-card' },
    h('button', { class: 'win-close', title: isMac() ? '退出' : '关闭', onclick: (e) => { e.stopPropagation(); closeWindow(card._w).then(loadWindows); } }, icon('xmark')),
    h('button', { class: 'win-more', title: '更多', onclick: (e) => { e.stopPropagation(); windowMenu(card._w).then(loadWindows); } }, icon('ellipsis')),
    appAvatar(appName(w), w.icon_path, 64),
    h('div', { class: 'win-app' }), h('div', { class: 'win-title' }), h('div', { class: 'win-state' }));
  const longPressed = onLongPress(card, () => windowMenu(card._w).then(loadWindows));
  card.addEventListener('click', async () => {
    if (longPressed()) return;
    card.classList.remove('launch'); void card.offsetWidth; card.classList.add('launch');
    await windowOp(card._w, 'activate');
    loadWindows(true);
  });
  updateCard(card, w);
  return card;
}

function updateCard(card, w) {
  card._w = w;
  card.classList.toggle('fg', !!w.foreground);
  card.classList.toggle('min', !!w.minimized);
  card.querySelector('.win-app').textContent = appName(w);
  card.querySelector('.win-title').textContent = w.title;
  card.querySelector('.win-state').textContent = w.foreground ? '当前窗口' : w.minimized ? (isMac() ? '已隐藏' : '已最小化') : '';
}

async function loadWindows(force = false) {
  try {
    const windows = await fetchWindows(force === true);
    if (mode !== 'windows') return;
    if (!windows.length) { winGrid = null; listEl.replaceChildren(emptyState('macwindow', isMac() ? '没有正在运行的应用' : '没有打开的窗口')); return; }
    if (!winGrid || !winGrid.isConnected) {
      winGrid = h('div', { class: 'win-grid' });
      listEl.replaceChildren(h('div', { class: 'hint', style: { margin: '0 4px 12px' } }, '轻点卡片切换到该窗口 · 长按或点 ⋯ 进行更多操作'), winGrid);
    }
    keyedUpdate(winGrid, windows, (w) => w.hwnd, winCard, updateCard);
  } catch (e) {
    winGrid = null;
    listEl.replaceChildren(emptyState('xcircle', e.message));
  }
}

function render() {
  clearInterval(timer);
  toolsEl.classList.toggle('hidden', mode !== 'apps');
  if (mode === 'apps') loadApps();
  else { winGrid = null; loadWindows(true); timer = setInterval(loadWindows, 2500); }
}

export default {
  id: 'apps', title: '应用与窗口', short: '应用', icon: 'app',
  mount(el) {
    const segEl = segmented([
      { value: 'apps', label: '应用', icon: 'app' },
      { value: 'windows', label: isMac() ? '正在运行' : '打开的窗口', icon: 'macwindow' },
    ], mode, (m) => { mode = m; render(); });
    const search = h('input', { class: 'input', type: 'search', placeholder: '搜索应用', style: { maxWidth: '320px', borderRadius: '999px' }, oninput: () => { query = search.value; renderApps(); } });
    listEl = h('div');
    toolsEl = h('div', { class: 'row' }, search, h('button', { class: 'btn icon refresh', title: '重新读取应用列表', onclick: () => loadApps(true) }, icon('refresh')));
    el.append(h('div', { class: 'toolbar' }, segEl, h('div', { class: 'grow' }), toolsEl), listEl);
  },
  show() { render(); },
  hide() { clearInterval(timer); },
};

export { appAvatar };
