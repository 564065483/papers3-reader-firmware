// 文件：浏览电脑磁盘、在电脑上打开、iPad 预览/下载、上传、重命名、删除到回收站
import { api, getToken, withToken } from './api.js';
import { confirmDialog, fmtBytes, fmtTime, h, promptDialog, sheet, showError, toast } from './ui.js';
import { pinToDashboard } from './dashboard.js';

let root, listEl, crumbsEl, placesEl, progressEl, cwd = '', parent = null, showHidden = false, loaded = false;

const ICONS = {
  image: ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'heic', 'svg', 'ico', 'tif', 'tiff'],
  video: ['mp4', 'mkv', 'avi', 'mov', 'wmv', 'flv', 'webm', 'm4v'],
  audio: ['mp3', 'flac', 'wav', 'aac', 'm4a', 'ogg', 'wma'],
  doc: ['doc', 'docx', 'rtf', 'odt'], sheet: ['xls', 'xlsx', 'csv', 'ods'], ppt: ['ppt', 'pptx', 'key'],
  pdf: ['pdf'], zip: ['zip', 'rar', '7z', 'tar', 'gz', 'xz'], exe: ['exe', 'msi', 'bat', 'cmd', 'ps1', 'lnk'],
  code: ['py', 'js', 'ts', 'html', 'css', 'json', 'c', 'cpp', 'h', 'java', 'go', 'rs', 'md', 'txt', 'log', 'ini', 'xml', 'yaml', 'yml'],
};
const EMOJI = { image: '🖼️', video: '🎬', audio: '🎵', doc: '📘', sheet: '📗', ppt: '📙', pdf: '📕', zip: '🗜️', exe: '⚙️', code: '📄' };
const PREVIEWABLE = new Set([...ICONS.image, ...ICONS.video, ...ICONS.audio, 'pdf', 'txt', 'md', 'log', 'json', 'html', 'csv']);

const ext = (name) => (name.includes('.') ? name.split('.').pop().toLowerCase() : '');
function iconFor(e) {
  if (e.dir) return e.path.length <= 3 ? '💽' : '📁';
  const x = ext(e.name);
  for (const [k, list] of Object.entries(ICONS)) if (list.includes(x)) return EMOJI[k];
  return '📄';
}

async function open(path) {
  listEl.replaceChildren(h('div', { class: 'empty' }, '加载中…'));
  try {
    const r = await api(`/api/files?path=${encodeURIComponent(path)}&hidden=${showHidden ? 1 : 0}`);
    cwd = r.path; parent = r.parent;
    renderCrumbs();
    renderList(r.entries);
  } catch (e) {
    listEl.replaceChildren(h('div', { class: 'empty' }, e.message));
  }
}

function renderCrumbs() {
  const parts = [];
  if (cwd) {
    const sep = cwd.includes('\\') ? '\\' : '/';
    const segs = cwd.split(sep).filter(Boolean);
    let acc = cwd.startsWith('/') ? '/' : '';
    segs.forEach((s, i) => {
      acc = i === 0 && sep === '\\' ? `${s}\\` : acc + (acc.endsWith(sep) ? '' : sep) + s;
      parts.push([s, acc]);
    });
  }
  crumbsEl.replaceChildren(h('button', { onclick: () => open('') }, '💻 此电脑'),
    ...parts.map(([label, p]) => h('button', { onclick: () => open(p) }, label)));
  crumbsEl.scrollLeft = crumbsEl.scrollWidth;
}

function renderList(entries) {
  if (!entries.length) { listEl.replaceChildren(h('div', { class: 'empty' }, '空文件夹')); return; }
  listEl.replaceChildren(h('div', { class: 'list' }, entries.map((e) => h('div', {
    class: 'item tap', onclick: () => (e.dir ? open(e.path) : fileMenu(e)),
  },
  h('div', { class: 'ic' }, iconFor(e)),
  h('div', { class: 'grow' }, h('div', { class: 't ellipsis' }, e.name),
    h('div', { class: 'small muted' }, e.total ? `可用 ${fmtBytes(e.free)} / 共 ${fmtBytes(e.total)}`
      : e.mtime ? `${fmtTime(e.mtime)}${e.dir ? '' : ` · ${fmtBytes(e.size)}`}` : '')),
  h('button', { class: 'btn sm ghost', onclick: (ev) => { ev.stopPropagation(); fileMenu(e); } }, '⋯')))));
}

async function fileOp(op, path, extra = {}) {
  return api('/api/files/op', { body: { op, path, ...extra } });
}

async function fileMenu(e) {
  const x = ext(e.name);
  const options = [{ label: e.dir ? '📂 在电脑上打开文件夹' : '🖥️ 在电脑上打开', value: 'open' },
    { label: '🔎 在资源管理器中显示', value: 'reveal' }];
  if (!e.dir) {
    if (PREVIEWABLE.has(x)) options.push({ label: '👁️ 在 iPad 上预览', value: 'preview' });
    options.push({ label: '⬇️ 下载到 iPad', value: 'download' });
  }
  if (e.dir) options.unshift({ label: '➡️ 进入文件夹', value: 'enter' });
  options.push({ label: '📌 固定到控制台', value: 'pin' }, { label: '✏️ 重命名', value: 'rename' },
    { label: '🗑️ 删除（移到回收站）', value: 'delete', danger: true });
  const choice = await sheet(e.name, options);
  try {
    if (choice === 'enter') open(e.path);
    else if (choice === 'open') { await fileOp('open', e.path); toast('已在电脑上打开', 'ok'); }
    else if (choice === 'reveal') await fileOp('reveal', e.path);
    else if (choice === 'preview') window.open(withToken(`/api/files/download?inline=1&path=${encodeURIComponent(e.path)}`), '_blank');
    else if (choice === 'download') window.open(withToken(`/api/files/download?path=${encodeURIComponent(e.path)}`), '_blank');
    else if (choice === 'pin') pinToDashboard({ label: e.name.replace(/\.[^.]+$/, '') || e.name, icon: iconFor(e), color: e.dir ? 'teal' : 'blue', actions: [{ type: 'open', value: e.path }] });
    else if (choice === 'rename') {
      const name = await promptDialog('重命名', e.name);
      if (name && name !== e.name) { await fileOp('rename', e.path, { name }); open(cwd); }
    } else if (choice === 'delete') {
      if (await confirmDialog('删除', `把「${e.name}」移到电脑的回收站？`, { danger: true, okText: '删除' })) {
        await fileOp('delete', e.path); toast('已移到回收站', 'ok'); open(cwd);
      }
    }
  } catch (err) { showError(err); }
}

function upload() {
  if (!cwd) return toast('请先进入一个文件夹', 'error');
  const input = h('input', { type: 'file', multiple: true });
  input.onchange = () => {
    if (!input.files.length) return;
    const form = new FormData();
    let total = 0;
    for (const f of input.files) { form.append('file', f, f.name); total += f.size; }
    const xhr = new XMLHttpRequest();
    xhr.open('POST', `/api/files/upload?dir=${encodeURIComponent(cwd)}`);
    xhr.setRequestHeader('X-Token', getToken());
    progressEl.classList.remove('hidden');
    const bar = progressEl.firstChild;
    xhr.upload.onprogress = (ev) => { bar.style.width = `${(ev.loaded / (ev.total || total)) * 100}%`; };
    xhr.onload = () => {
      progressEl.classList.add('hidden'); bar.style.width = '0';
      if (xhr.status === 200) { toast(`已上传 ${JSON.parse(xhr.responseText).saved.length} 个文件`, 'ok'); open(cwd); }
      else { try { toast(JSON.parse(xhr.responseText).error, 'error'); } catch { toast('上传失败', 'error'); } }
    };
    xhr.onerror = () => { progressEl.classList.add('hidden'); toast('上传失败', 'error'); };
    xhr.send(form);
  };
  input.click();
}

async function loadPlaces() {
  try {
    const { places, drives } = await api('/api/files/places');
    placesEl.replaceChildren(
      ...places.map((p) => h('button', { class: 'btn', onclick: () => open(p.path) }, `${p.icon} ${p.label}`)),
      ...drives.map((d) => h('button', { class: 'btn', onclick: () => open(d.path) }, `💽 ${d.name}`)));
  } catch (e) { showError(e); }
}

export default {
  id: 'files', title: '文件', icon: '📁',
  mount(el) {
    root = el;
    crumbsEl = h('div', { class: 'crumbs' });
    placesEl = h('div', { class: 'places' });
    listEl = h('div');
    progressEl = h('div', { class: 'progress hidden', style: { marginBottom: '10px' } }, h('i'));
    const hiddenBtn = h('button', { class: 'btn sm', onclick: () => { showHidden = !showHidden; hiddenBtn.classList.toggle('on', showHidden); open(cwd); } }, '隐藏文件');
    el.append(
      h('div', { class: 'toolbar' },
        h('button', { class: 'btn sm', onclick: () => parent !== null && open(parent) }, '⬆ 上级'),
        crumbsEl,
        h('button', { class: 'btn sm', onclick: () => open(cwd) }, '🔄'),
        h('button', { class: 'btn sm', onclick: upload }, '⬆️ 上传'),
        h('button', {
          class: 'btn sm', onclick: async () => {
            if (!cwd) return toast('请先进入一个文件夹', 'error');
            const name = await promptDialog('新建文件夹', '新建文件夹');
            if (name) { try { await fileOp('mkdir', cwd, { name }); open(cwd); } catch (e) { showError(e); } }
          },
        }, '＋ 文件夹'),
        h('button', { class: 'btn sm', onclick: () => cwd && fileOp('open', cwd).catch(showError) }, '🖥️ 电脑上打开'),
        hiddenBtn),
      progressEl,
      h('div', { class: 'files' }, placesEl, listEl));
  },
  show() {
    if (loaded) return;
    loaded = true;
    loadPlaces();
    open('');
  },
};
