// 控制台：可自定义的按钮面板（多页面、宏、编辑器）
import { api, run, state } from './api.js';
import { confirmDialog, h, modal, onLongPress, promptDialog, sheet, showError, toast } from './ui.js';

export const COLORS = {
  blue: '#3b82f6', cyan: '#06b6d4', teal: '#14b8a6', green: '#22c55e', yellow: '#eab308',
  orange: '#f97316', red: '#ef4444', pink: '#ec4899', purple: '#a855f7', indigo: '#6366f1',
  gray: '#64748b', brown: '#a0663a',
};

const EMOJIS = ('💻 🌐 📝 🧮 ⌨️ 📁 📂 📥 🗂️ ⚙️ 📊 ✂️ 🖥️ 🪟 🔒 🎵 🎬 🎮 📷 🖼️ 📄 📋 📌 💾 🔍 ➕ ✖️ ♻️ 🔄 🔀 ❌ ⬆️ ⬇️ ⬅️ ➡️ ' +
  '▶️ ⏸️ ⏭️ ⏮️ 🔊 🔇 💡 🌙 ⏻ 🚀 ⭐ ❤️ 🔥 ✅ 💬 📧 📅 🧰 🛠️ 🧪 🐍 🟦 🟩 🎨 📚 🎧 🕹️ 🏠 💼 🧹 🤫 👋 ✍️ 🗑️ 🔗 📦 🖨️').split(' ');

export const SYSTEM_OPTIONS = [
  ['lock', '锁定电脑'], ['sleep', '睡眠'], ['hibernate', '休眠'], ['shutdown', '关机'], ['restart', '重启'],
  ['logoff', '注销'], ['cancel_shutdown', '取消关机/重启'], ['monitor_off', '关闭显示器'],
  ['show_desktop', '显示桌面'], ['task_view', '任务视图'], ['task_manager', '任务管理器'],
  ['explorer', '文件资源管理器'], ['settings', 'Windows 设置'], ['run_dialog', '运行对话框'],
  ['snip', '截图工具'], ['action_center', '通知中心'], ['emoji_panel', '表情面板'],
  ['switch_window', '切换到上一个窗口'], ['close_window', '关闭当前窗口'], ['minimize_all', '最小化全部窗口'],
  ['desktop_left', '左侧虚拟桌面'], ['desktop_right', '右侧虚拟桌面'], ['new_desktop', '新建虚拟桌面'],
];
export const MEDIA_OPTIONS = [
  ['playpause', '播放/暂停'], ['next', '下一首'], ['prev', '上一首'], ['stop', '停止'],
  ['volup', '音量 +'], ['voldown', '音量 −'], ['mute', '静音切换'],
];

export const ACTION_TYPES = {
  launch: { label: '启动程序', ph: '程序名或完整路径，如 notepad.exe、C:\\Program Files\\...\\app.exe', args: true },
  open: { label: '打开文件/文件夹', ph: '如 D:\\资料 或 %USERPROFILE%\\Desktop\\报告.docx' },
  url: { label: '打开网址', ph: 'https://www.bilibili.com' },
  hotkey: { label: '快捷键', ph: '如 ctrl+c、win+shift+s、f5、alt+tab' },
  text: { label: '输入文字', ph: '逐字输入到当前窗口，支持中文和换行', multi: true },
  paste: { label: '粘贴文字', ph: '写入剪贴板后 Ctrl+V，适合长文本', multi: true },
  command: { label: '执行命令', ph: '后台静默执行，如 ipconfig /flushdns' },
  media: { label: '媒体/音量', options: MEDIA_OPTIONS },
  system: { label: '系统操作', options: SYSTEM_OPTIONS },
  wait: { label: '等待（毫秒）', ph: '500' },
};

let root, pages = [], pageIdx = 0, editing = false;

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
    tileEl?.classList.add('fail');
    showError(e);
  }
}

function tile(btn, index) {
  const el = h('button', { class: 'tile', style: { '--c': COLORS[btn.color] || btn.color || COLORS.blue } },
    h('span', { class: 'ic' }, btn.icon || '⭐'),
    h('span', { class: 'lb' }, btn.label || '未命名'),
    h('div', { class: 'edit-bar' },
      h('span', { onclick: (e) => { e.stopPropagation(); move(index, -1); } }, '◀'),
      h('span', {}, '✎'),
      h('span', { onclick: (e) => { e.stopPropagation(); move(index, 1); } }, '▶')));
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
  render();
  save();
}

async function pageMenu(i) {
  const choice = await sheet(`页面「${pages[i].name}」`, [
    { label: '✏️ 重命名', value: 'rename' },
    { label: '◀ 向左移动', value: 'left' },
    { label: '▶ 向右移动', value: 'right' },
    { label: '🗑️ 删除页面', value: 'delete', danger: true },
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

function render() {
  if (!root) return;
  pageIdx = Math.min(pageIdx, Math.max(0, pages.length - 1));
  const page = pages[pageIdx];
  const seg = h('div', { class: 'seg' }, pages.map((p, i) => h('button', {
    class: i === pageIdx ? 'on' : '',
    onclick: () => { if (editing && i === pageIdx) pageMenu(i); else { pageIdx = i; render(); } },
  }, p.name + (editing && i === pageIdx ? ' ⚙︎' : ''))),
  editing ? h('button', {
    onclick: async () => {
      const name = await promptDialog('新页面名称', '新页面');
      if (!name) return;
      pages.push({ id: uid(), name, buttons: [] });
      pageIdx = pages.length - 1;
      render(); save();
    },
  }, '＋ 页面') : null);

  const head = h('div', { class: 'dash-head' }, seg, h('div', { class: 'grow' }),
    h('button', { class: `btn ${editing ? 'on' : ''}`, onclick: () => { editing = !editing; render(); } },
      editing ? '✓ 完成编辑' : '✎ 编辑'));

  const grid = h('div', { class: `tiles ${editing ? 'editing' : ''}` });
  if (page) {
    page.buttons.forEach((b, i) => grid.append(tile(b, i)));
    grid.append(h('button', { class: 'tile add', onclick: () => editButton(-1) },
      h('span', { class: 'ic' }, '＋'), h('span', { class: 'lb' }, '添加按钮')));
  } else {
    grid.append(h('div', { class: 'empty' }, '暂无页面'));
  }
  const tip = h('div', { class: 'hint', style: { marginTop: '18px' } },
    editing ? '点按钮修改，◀ ▶ 调整顺序；点当前页名称可重命名、移动或删除页面。'
      : '提示：长按任意按钮可直接编辑；左右滑动切换页面。');
  root.replaceChildren(head, grid, tip);
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
      if (next >= 0 && next < pages.length) { pageIdx = next; render(); }
    }
    x0 = null;
  });
}

// ---------------- 按钮编辑器 ----------------
function stepRow(step, list, rerender) {
  const def = ACTION_TYPES[step.type] || ACTION_TYPES.launch;
  const typeSel = h('select', {
    class: 'input',
    onchange: () => {
      step.type = typeSel.value;
      const d = ACTION_TYPES[step.type];
      step.value = d.options ? d.options[0][0] : '';
      rerender();
    },
  }, Object.entries(ACTION_TYPES).map(([k, d]) => h('option', { value: k, selected: k === step.type }, d.label)));

  let valueEl;
  if (def.options) {
    valueEl = h('select', { class: 'input', onchange: () => { step.value = valueEl.value; } },
      def.options.map(([k, label]) => h('option', { value: k, selected: k === step.value }, label)));
    if (!def.options.some(([k]) => k === step.value)) step.value = def.options[0][0];
  } else if (def.multi) {
    valueEl = h('textarea', { class: 'input', placeholder: def.ph, style: { minHeight: '70px' }, oninput: () => { step.value = valueEl.value; } });
    valueEl.value = step.value || '';
  } else {
    valueEl = h('input', { class: 'input', placeholder: def.ph, value: step.value || '', autocapitalize: 'off', autocorrect: 'off', spellcheck: false, oninput: () => { step.value = valueEl.value; } });
  }
  const i = list.indexOf(step);
  const tools = h('div', { class: 'step-tools' },
    h('button', { class: 'btn sm icon', onclick: () => { if (i > 0) { [list[i - 1], list[i]] = [list[i], list[i - 1]]; rerender(); } } }, '↑'),
    h('button', { class: 'btn sm icon', onclick: () => { list.splice(i, 1); rerender(); } }, '✕'));
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
  const nameIn = h('input', { class: 'input', value: btn.label || '', placeholder: '按钮名称', oninput: () => { btn.label = nameIn.value; } });
  const iconIn = h('input', { class: 'input', value: btn.icon || '', placeholder: '图标（emoji 或 1–2 个字）', style: { width: '140px', fontSize: '22px', textAlign: 'center' }, oninput: () => { btn.icon = iconIn.value; } });
  const emojis = h('div', { class: 'emojis' }, EMOJIS.map((e) => h('button', { onclick: () => { btn.icon = e; iconIn.value = e; } }, e)));
  const colors = h('div', { class: 'colors' });
  const paintColors = () => colors.replaceChildren(...Object.entries(COLORS).map(([k, c]) => h('button', {
    class: btn.color === k ? 'on' : '', style: { background: c }, onclick: () => { btn.color = k; paintColors(); },
  })));
  paintColors();
  const steps = h('div', { class: 'list' });
  const rerender = () => {
    steps.replaceChildren(...btn.actions.map((s) => stepRow(s, btn.actions, rerender)));
    if (!btn.actions.length) steps.append(h('div', { class: 'hint' }, '还没有动作，点下方“添加步骤”。多个步骤会按顺序执行，可组成宏。'));
  };
  rerender();

  const body = h('div', {},
    h('div', { class: 'field' }, h('label', {}, '名称与图标'), h('div', { class: 'row' }, nameIn, iconIn)),
    h('div', { class: 'field' }, emojis),
    h('div', { class: 'field' }, h('label', {}, '颜色'), colors),
    h('div', { class: 'field' }, h('label', {}, '动作步骤'), steps,
      h('div', { class: 'row' },
        h('button', { class: 'btn sm', onclick: () => { btn.actions.push({ type: 'launch', value: '' }); rerender(); } }, '＋ 添加步骤'),
        h('button', { class: 'btn sm', onclick: () => { btn.actions.push({ type: 'wait', value: '500' }); rerender(); } }, '＋ 等待'))),
    h('div', { class: 'hint' }, '按键名称：ctrl shift alt win enter esc tab space backspace delete home end pageup pagedown ' +
      'up down left right f1–f24 printscreen volumeup volumedown playpause 及字母数字。'));

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
  const original = isNew ? { id: uid(), label: '', icon: '⭐', color: 'blue', actions: [{ type: 'launch', value: '' }] } : page.buttons[index];
  const result = await buttonEditor(original, { allowDelete: !isNew, title: isNew ? '添加按钮' : '编辑按钮' });
  if (!result) return;
  if (result === 'delete') {
    if (!(await confirmDialog('删除按钮', `确定删除「${original.label}」？`, { danger: true, okText: '删除' }))) return;
    page.buttons.splice(index, 1);
  } else if (isNew) page.buttons.push(result);
  else page.buttons[index] = result;
  render();
  save();
}

/** 供“应用”“文件”页把条目固定到控制台 */
export async function pinToDashboard(button) {
  if (!pages.length) await load();
  const choice = pages.length > 1
    ? await sheet('添加到哪个页面？', pages.map((p, i) => ({ label: p.name, value: i })))
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
  id: 'dashboard', title: '控制台', icon: '🎛️',
  mount(el) { root = el; bindSwipe(el); load(); },
};
