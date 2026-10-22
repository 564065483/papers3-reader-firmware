// 控制台：可自定义的按钮面板（多页面、宏、编辑器）
import { api, isMac, run, state } from './api.js';
import { ICON_NAMES, glyph, icon } from './icons.js';
import { confirmDialog, h, modal, onLongPress, promptDialog, segmented, sheet, showError, toast } from './ui.js';
import { createDock } from './running.js';

// 苹果系统色
export const COLORS = {
  blue: '#0a84ff', cyan: '#32ade6', teal: '#30b0c7', green: '#30d158', yellow: '#f5b400',
  orange: '#ff9f0a', red: '#ff453a', pink: '#ff375f', purple: '#bf5af2', indigo: '#5e5ce6',
  gray: '#8e8e93', brown: '#ac8e68',
};

const SYSTEM_WIN = [
  ['lock', '锁定电脑'], ['sleep', '睡眠'], ['hibernate', '休眠'], ['shutdown', '关机'], ['restart', '重启'],
  ['logoff', '注销'], ['cancel_shutdown', '取消关机/重启'], ['monitor_off', '关闭显示器'],
  ['show_desktop', '显示桌面'], ['task_view', '任务视图'], ['task_manager', '任务管理器'],
  ['explorer', '文件资源管理器'], ['settings', 'Windows 设置'], ['run_dialog', '运行对话框'],
  ['snip', '截图工具'], ['action_center', '通知中心'], ['emoji_panel', '表情面板'],
  ['switch_window', '切换到上一个窗口'], ['close_window', '关闭当前窗口'], ['minimize_all', '最小化全部窗口'],
  ['desktop_left', '左侧虚拟桌面'], ['desktop_right', '右侧虚拟桌面'], ['new_desktop', '新建虚拟桌面'],
];
const SYSTEM_MAC = [
  ['lock', '锁定屏幕'], ['sleep', '睡眠'], ['shutdown', '关机'], ['restart', '重新启动'], ['logoff', '退出登录'],
  ['cancel_shutdown', '取消关机/重启'], ['monitor_off', '关闭显示器'], ['show_desktop', '显示桌面'],
  ['task_view', '调度中心'], ['app_windows', '应用程序窗口'], ['task_manager', '活动监视器'], ['explorer', '访达'],
  ['settings', '系统设置'], ['spotlight', '聚焦搜索'], ['snip', '截屏工具'], ['emoji_panel', '表情与符号'],
  ['switch_window', '切换应用'], ['close_window', '关闭窗口'], ['quit_app', '退出应用'], ['force_quit', '强制退出'],
  ['minimize_all', '最小化全部窗口'], ['desktop_left', '左侧桌面'], ['desktop_right', '右侧桌面'],
];
export const systemOptions = () => (isMac() ? SYSTEM_MAC : SYSTEM_WIN);
export const MEDIA_OPTIONS = [
  ['playpause', '播放/暂停'], ['next', '下一首'], ['prev', '上一首'], ['stop', '停止'],
  ['volup', '音量 +'], ['voldown', '音量 −'], ['mute', '静音切换'],
];

function actionTypes() {
  const mac = isMac();
  return {
    launch: { label: '打开应用', ph: mac ? '应用名或路径，如 Safari、/Applications/微信.app' : '程序名或完整路径，如 notepad.exe、C:\\Program Files\\...\\app.exe', args: true },
    open: { label: '打开文件/文件夹', ph: mac ? '如 ~/Documents/报告.pdf' : '如 D:\\资料 或 %USERPROFILE%\\Desktop\\报告.docx' },
    url: { label: '打开网址', ph: 'https://www.bilibili.com' },
    hotkey: { label: '快捷键', ph: mac ? '如 cmd+c、cmd+shift+4、ctrl+up' : '如 ctrl+c、win+shift+s、f5、alt+tab' },
    text: { label: '输入文字', ph: '逐字输入到当前窗口，支持中文和换行', multi: true },
    paste: { label: '粘贴文字', ph: '写入剪贴板后粘贴，适合长文本', multi: true },
    command: { label: '执行命令', ph: mac ? '后台静默执行，如 open -a Music' : '后台静默执行，如 ipconfig /flushdns' },
    media: { label: '媒体/音量', options: MEDIA_OPTIONS },
    system: { label: '系统操作', options: systemOptions() },
    wait: { label: '等待（毫秒）', ph: '500' },
  };
}

let root, grid, head, seg, dock, pages = [], pageIdx = 0, editing = false;

const uid = () => Math.random().toString(36).slice(2, 12);

async function load() {
  try {
    pages = (await api('/api/pages')).pages;
    state.pages = pages;
  } catch (e) {
    showError(e);
    pages = [];
  }
  render();
}

async function save() {
  try {
    pages = (await api('/api/pages', { method: 'PUT', body: { pages } })).pages;
    state.pages = pages;
  } catch (e) { showError(e); }
}

export async function runButton(btn, tileEl) {
  tileEl?.classList.remove('ok', 'fail');
  tileEl?.classList.add('busy');
  try {
    await run(btn.actions);
    if (tileEl) { tileEl.classList.remove('busy'); void tileEl.offsetWidth; tileEl.classList.add('ok'); }
  } catch (e) {
    tileEl?.classList.remove('busy');
    if (tileEl) { void tileEl.offsetWidth; tileEl.classList.add('fail'); }
    showError(e);
  }
}

export function tileEl(btn, { index = 0, preview = false } = {}) {
  const el = h(preview ? 'div' : 'button', {
    class: `tile ${preview ? 'preview-tile' : ''}`,
    style: { '--c': COLORS[btn.color] || btn.color || COLORS.blue, '--i': index },
  },
  h('span', { class: 'glyph' }, glyph(btn.icon || 'sf:star')),
  h('span', { class: 'lb' }, btn.label || '未命名'));
  if (preview) return el;
  el.append(
    h('span', { class: 'edit-badge' }, icon('pencil')),
    h('span', { class: 'move' },
      h('span', { onclick: (e) => { e.stopPropagation(); move(index, -1); } }, icon('chevron.left')),
      h('span', { onclick: (e) => { e.stopPropagation(); move(index, 1); } }, icon('chevron.right'))));
  const longPressed = onLongPress(el, () => editButton(index));
  el.addEventListener('click', () => {
    if (longPressed()) return;
    if (editing) editButton(index); else runButton(btn, el);
  });
  return el;
}

function move(index, delta) {
  const list = pages[pageIdx].buttons;
  const j = index + delta;
  if (j < 0 || j >= list.length) return;
  [list[index], list[j]] = [list[j], list[index]];
  renderGrid(false);
  save();
}

async function pageMenu(i) {
  const choice = await sheet(`页面「${pages[i].name}」`, [
    { label: '重命名', icon: 'pencil', value: 'rename' },
    { label: '向左移动', icon: 'arrow.left', value: 'left' },
    { label: '向右移动', icon: 'arrow.right', value: 'right' },
    { label: '删除页面', icon: 'trash', value: 'delete', danger: true },
  ]);
  if (choice === 'rename') {
    const name = await promptDialog('页面名称', pages[i].name);
    if (name) pages[i].name = name;
  } else if (choice === 'left' || choice === 'right') {
    const j = i + (choice === 'left' ? -1 : 1);
    if (j < 0 || j >= pages.length) return;
    [pages[i], pages[j]] = [pages[j], pages[i]];
    pageIdx = j;
  } else if (choice === 'delete') {
    if (pages.length <= 1) return toast('至少保留一个页面');
    if (!(await confirmDialog('删除页面', `确定删除「${pages[i].name}」及其中 ${pages[i].buttons.length} 个按钮？`, { danger: true, okText: '删除' }))) return;
    pages.splice(i, 1);
    pageIdx = Math.max(0, i - 1);
  } else return;
  render();
  save();
}

function segItems() {
  const items = pages.map((p, i) => ({
    value: i, label: p.name, icon: editing && i === pageIdx ? 'gear' : null,
    onClick: () => {
      if (editing && i === pageIdx) return pageMenu(i);
      pageIdx = i;
      seg.set(i);
      if (editing) seg.update(segItems(), i);
      renderGrid(true);
    },
  }));
  if (editing) {
    items.push({
      value: -1, label: '页面', icon: 'plus',
      onClick: async () => {
        const name = await promptDialog('新页面名称', '新页面');
        if (!name) return;
        pages.push({ id: uid(), name, buttons: [] });
        pageIdx = pages.length - 1;
        render(); save();
      },
    });
  }
  return items;
}

function renderGrid(animate = true) {
  const page = pages[pageIdx];
  grid.className = `tiles ${editing ? 'editing' : ''}`;
  if (!page) { grid.replaceChildren(h('div', { class: 'empty' }, '暂无页面')); return; }
  const tiles = page.buttons.map((b, i) => tileEl(b, { index: animate ? i : 0 }));
  if (!animate) tiles.forEach((t) => { t.style.animation = editing ? '' : 'none'; });
  tiles.push(h('button', { class: 'tile add', style: { '--i': page.buttons.length }, onclick: () => editButton(-1) },
    icon('plus'), h('span', { class: 'lb' }, '添加按钮')));
  grid.replaceChildren(...tiles);
}

function render() {
  if (!root) return;
  pageIdx = Math.min(pageIdx, Math.max(0, pages.length - 1));
  seg = segmented(segItems(), pageIdx);
  const editBtn = h('button', { class: `btn ${editing ? 'primary' : ''}`, onclick: () => { editing = !editing; render(); } },
    icon(editing ? 'check' : 'pencil'), editing ? '完成' : '编辑');
  head.replaceChildren(seg, h('div', { class: 'grow' }), editBtn);
  renderGrid(true);
  root.querySelector('.hint').textContent = editing
    ? '轻点按钮进行修改，‹ › 调整顺序；轻点当前页面名称可以重命名、移动或删除页面。'
    : '长按任意按钮可直接编辑 · 左右轻扫切换页面 · 底部程序坞轻点即可切换窗口';
}

// 左右滑动切换页面
function bindSwipe(el) {
  let x0 = null, y0 = 0;
  el.addEventListener('touchstart', (e) => {
    if (e.touches.length === 1) { x0 = e.touches[0].clientX; y0 = e.touches[0].clientY; } else x0 = null;
  }, { passive: true });
  el.addEventListener('touchend', (e) => {
    if (x0 === null || editing) return;
    const t = e.changedTouches[0];
    const dx = t.clientX - x0, dy = t.clientY - y0;
    if (Math.abs(dx) > 90 && Math.abs(dy) < 50) {
      const next = pageIdx + (dx < 0 ? 1 : -1);
      if (next >= 0 && next < pages.length) { pageIdx = next; seg.set(next); renderGrid(true); }
    }
    x0 = null;
  });
}

// ---------------- 按钮编辑器 ----------------
function stepRow(step, list, rerender, types) {
  const def = types[step.type] || types.launch;
  const typeSel = h('select', {
    class: 'input',
    onchange: () => {
      step.type = typeSel.value;
      const d = types[step.type];
      step.value = d.options ? d.options[0][0] : '';
      rerender();
    },
  }, Object.entries(types).map(([k, d]) => h('option', { value: k, selected: k === step.type }, d.label)));

  let valueEl;
  if (def.options) {
    if (!def.options.some(([k]) => k === step.value)) step.value = def.options[0][0];
    valueEl = h('select', { class: 'input', onchange: () => { step.value = valueEl.value; rerender(); } },
      def.options.map(([k, text]) => h('option', { value: k, selected: k === step.value }, text)));
  } else if (def.multi) {
    valueEl = h('textarea', { class: 'input', placeholder: def.ph, style: { minHeight: '70px' }, oninput: () => { step.value = valueEl.value; } });
    valueEl.value = step.value || '';
  } else {
    valueEl = h('input', { class: 'input', placeholder: def.ph, value: step.value || '', autocapitalize: 'off', autocorrect: 'off', spellcheck: false, oninput: () => { step.value = valueEl.value; } });
  }
  const i = list.indexOf(step);
  const tools = h('div', { class: 'step-tools' },
    h('button', { class: 'btn sm icon', onclick: () => { if (i > 0) { [list[i - 1], list[i]] = [list[i], list[i - 1]]; rerender(); } } }, icon('arrow.up')),
    h('button', { class: 'btn sm icon', onclick: () => { list.splice(i, 1); rerender(); } }, icon('xmark')));
  const row = h('div', { class: 'step' }, typeSel, valueEl, tools);
  if (def.args) {
    const args = h('input', { class: 'input extra', placeholder: '启动参数（可选）', value: step.args || '', autocapitalize: 'off', oninput: () => { step.args = args.value; } });
    row.append(h('div'), args);
  }
  if (step.type === 'system' && (step.value === 'shutdown' || step.value === 'restart')) {
    const delay = h('input', { class: 'input extra', type: 'number', placeholder: '延迟秒数（0 = 立即）', value: step.delay || '', oninput: () => { step.delay = Number(delay.value) || 0; } });
    row.append(h('div'), delay);
  }
  return row;
}

export function buttonEditor(original, { allowDelete = false, title = '编辑按钮' } = {}) {
  const btn = JSON.parse(JSON.stringify(original));
  btn.actions ||= [];
  const types = actionTypes();
  const previewBox = h('div');
  const repaintPreview = () => previewBox.replaceChildren(tileEl(btn, { preview: true }));
  const nameIn = h('input', { class: 'input', value: btn.label || '', placeholder: '按钮名称', oninput: () => { btn.label = nameIn.value; repaintPreview(); } });
  const emojiIn = h('input', {
    class: 'input', placeholder: '或输入 emoji / 文字作为图标', value: btn.icon?.startsWith('sf:') ? '' : (btn.icon || ''),
    oninput: () => { if (emojiIn.value.trim()) { btn.icon = emojiIn.value.trim(); paintIcons(); repaintPreview(); } },
  });
  const iconGrid = h('div', { class: 'icon-grid' });
  const paintIcons = () => iconGrid.replaceChildren(...ICON_NAMES.map((n) => h('button', {
    class: btn.icon === `sf:${n}` ? 'on' : '', title: n,
    onclick: () => { btn.icon = `sf:${n}`; emojiIn.value = ''; paintIcons(); repaintPreview(); },
  }, icon(n))));
  paintIcons();
  const colors = h('div', { class: 'colors' });
  const paintColors = () => colors.replaceChildren(...Object.entries(COLORS).map(([k, c]) => h('button', {
    class: btn.color === k ? 'on' : '', style: { background: c }, onclick: () => { btn.color = k; paintColors(); repaintPreview(); },
  })));
  paintColors();
  repaintPreview();

  const steps = h('div', { class: 'list' });
  const rerender = () => {
    steps.replaceChildren(...btn.actions.map((s) => stepRow(s, btn.actions, rerender, types)));
    if (!btn.actions.length) steps.append(h('div', { class: 'hint' }, '还没有动作，点下方“添加步骤”。多个步骤会按顺序执行，可组成宏。'));
  };
  rerender();

  const keyHint = isMac()
    ? '按键名称：cmd option ctrl shift enter esc tab space backspace delete home end pageup pagedown up down left right f1–f20 及字母数字。'
    : '按键名称：ctrl shift alt win enter esc tab space backspace delete home end pageup pagedown up down left right f1–f24 printscreen 及字母数字。';
  const body = h('div', {},
    h('div', { class: 'editor-top' }, previewBox,
      h('div', { class: 'grow', style: { display: 'flex', flexDirection: 'column', gap: '12px' } },
        h('div', { class: 'field', style: { margin: 0 } }, h('label', {}, '名称'), nameIn),
        h('div', { class: 'field', style: { margin: 0 } }, h('label', {}, '颜色'), colors))),
    h('div', { class: 'field' }, h('label', {}, '图标'), iconGrid, emojiIn),
    h('div', { class: 'field' }, h('label', {}, '动作步骤'), steps,
      h('div', { class: 'row' },
        h('button', { class: 'btn sm tinted', onclick: () => { btn.actions.push({ type: 'launch', value: '' }); rerender(); } }, icon('plus'), '添加步骤'),
        h('button', { class: 'btn sm', onclick: () => { btn.actions.push({ type: 'wait', value: '500' }); rerender(); } }, icon('clock'), '等待'))),
    h('div', { class: 'hint' }, keyHint));

  const buttons = [];
  if (allowDelete) buttons.push({ label: '删除', kind: 'danger', left: true, value: 'delete' });
  buttons.push({ label: '试运行', onClick: () => { if (btn.actions.length) runButton(btn); return false; } });
  buttons.push({ label: '取消', value: null });
  buttons.push({
    label: '保存', kind: 'primary', value: 'save',
    onClick: () => {
      if (!btn.label?.trim()) { toast('请填写按钮名称', 'error'); return false; }
      if (!btn.actions.length) { toast('至少需要一个动作', 'error'); return false; }
      return true;
    },
  });
  return modal({ title, body, buttons, wide: true }).then((r) => (r === 'save' ? btn : r));
}

async function editButton(index) {
  const page = pages[pageIdx];
  if (!page) return;
  const isNew = index < 0;
  const original = isNew ? { id: uid(), label: '', icon: 'sf:star', color: 'blue', actions: [{ type: 'launch', value: '' }] } : page.buttons[index];
  const result = await buttonEditor(original, { allowDelete: !isNew, title: isNew ? '添加按钮' : '编辑按钮' });
  if (!result) return;
  if (result === 'delete') {
    if (!(await confirmDialog('删除按钮', `确定删除「${original.label}」？`, { danger: true, okText: '删除' }))) return;
    page.buttons.splice(index, 1);
  } else if (isNew) page.buttons.push(result);
  else page.buttons[index] = result;
  renderGrid(false);
  save();
}

/** 供“应用”“文件”页把条目固定到控制台 */
export async function pinToDashboard(button) {
  if (!pages.length) await load();
  const choice = pages.length > 1
    ? await sheet('添加到哪个页面？', pages.map((p, i) => ({ label: p.name, icon: 'grid', value: i })))
    : 0;
  if (choice === null || choice === undefined) return;
  const result = await buttonEditor({ id: uid(), color: 'blue', ...button }, { title: '添加到控制台' });
  if (!result || result === 'delete') return;
  pages[choice].buttons.push(result);
  pageIdx = choice;
  if (root) render();
  await save();
  toast('已添加到控制台', 'ok');
}

export async function reloadPages() { await load(); }

export default {
  id: 'dashboard', title: '控制台', icon: 'grid',
  mount(el) {
    root = el;
    head = h('div', { class: 'dash-head' });
    grid = h('div', { class: 'tiles' });
    dock = createDock();
    el.append(head, grid, h('div', { class: 'hint', style: { marginTop: '18px', textAlign: 'center' } }), dock);
    bindSwipe(el);
    load();
  },
  show() { dock?.start(); },
  hide() { dock?.stop(); },
};
