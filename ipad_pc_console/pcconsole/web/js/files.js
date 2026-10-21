// 文件：浏览电脑磁盘、在电脑上打开、iPad 预览/下载、上传、重命名、删除到回收站 / 废纸篓
import { api, base, getToken, isMac, withToken } from './api.js';
import { icon } from './icons.js';
import { confirmDialog, emptyState, fmtBytes, fmtTime, h, promptDialog, sheet, showError, toast } from './ui.js';
import { pinToDashboard } from './dashboard.js';

let listEl, crumbsEl, placesEl, progressEl, cwd = '', parent = null, showHidden = false, loaded = false;

const KINDS = {
  photo: ['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'heic', 'svg', 'ico', 'tif', 'tiff'],
  film: ['mp4', 'mkv', 'avi', 'mov', 'wmv', 'flv', 'webm', 'm4v'],
  music: ['mp3', 'flac', 'wav', 'aac', 'm4a', 'ogg', 'wma'],
  cube: ['zip', 'rar', '7z', 'tar', 'gz', 'xz', 'dmg', 'iso'],
  app: ['exe', 'msi', 'app', 'pkg', 'lnk'],
  code: ['py', 'js', 'ts', 'html', 'css', 'json', 'c', 'cpp', 'h', 'java', 'go', 'rs', 'sh', 'bat', 'ps1', 'xml', 'yaml', 'yml'],
};
const THUMB = new Set(['jpg', 'jpeg', 'png', 'gif', 'bmp', 'webp', 'tif', 'tiff']);
const PREVIEWABLE = new Set([...KINDS.photo, ...KINDS.film, ...KINDS.music, 'pdf', 'txt', 'md', 'log', 'json', 'html', 'csv']);
const NATIVE_ICON = new Set(['exe', 'lnk', 'app', 'msi', 'pdf', 'doc', 'docx', 'xls', 'xlsx', 'ppt', 'pptx', 'pages', 'numbers', 'key', 'heic', ...KINDS.film]);

const ext = (name) => (name.includes('.') ? name.split('.').pop().toLowerCase() : '');
function symbolFor(e) {
  if (e.dir) return e.total !== undefined ? 'disk' : 'folder';
  const x = ext(e.name);
  for (const [k, list] of Object.entries(KINDS)) if (list.includes(x)) return k;
  return 'doc';
}

/** 图片显示缩略图；程序、文档、视频显示电脑上的真实图标；其余用线性图标 */
function fileIcon(e) {
  const box = h('div', { class: 'ic' }, icon(symbolFor(e)));
  const x = ext(e.name);
  if (!e.dir && (THUMB.has(x) || NATIVE_ICON.has(x))) {
    const img = h('img', { alt: '', loading: 'lazy', class: THUMB.has(x) ? 'thumb' : '' });
    img.onload = () => box.replaceChildren(img);
    img.src = withToken(`/api/icon?size=96&path=${encodeURIComponent(e.path)}`);
  }
  return box;
}

async function open(path) {
  listEl.replaceChildren(emptyState('folder', '加载中…'));
  try {
    const r = await api(`/api/files?path=${encodeURIComponent(path)}&hidden=${showHidden ? 1 : 0}`);
    cwd = r.path; parent = r.parent;
    renderCrumbs();
    renderList(r.entries);
  } catch (e) {
    listEl.replaceChildren(emptyState('xcircle', e.message));
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
  const items = [h('button', { onclick: () => open('') }, isMac() ? '磁盘' : '此电脑')];
  for (const [text, p] of parts) items.push(icon('chevron.right'), h('button', { onclick: () => open(p) }, text));
  crumbsEl.replaceChildren(...items);
  crumbsEl.scrollLeft = crumbsEl.scrollWidth;
}

function renderList(entries) {
  if (!entries.length) { listEl.replaceChildren(emptyState('folder', '空文件夹')); return; }
  listEl.replaceChildren(h('div', { class: 'list list-anim' }, entries.map((e, i) => h('div', {
    class: 'item tap', style: { '--i': i }, onclick: () => (e.dir ? open(e.path) : fileMenu(e)),
  },
  fileIcon(e),
  h('div', { class: 'grow' }, h('div', { class: 't ellipsis' }, e.name),
    h('div', { class: 'small muted' }, e.total ? `可用 ${fmtBytes(e.free)} · 共 ${fmtBytes(e.total)}`
      : e.mtime ? `${fmtTime(e.mtime)}${e.dir ? '' : ` · ${fmtBytes(e.size)}`}` : '')),
  e.dir ? icon('chevron.right') : null,
  h('button', { class: 'btn sm icon ghost', onclick: (ev) => { ev.stopPropagation(); fileMenu(e); } }, icon('ellipsis'))))));
}

async function fileOp(op, path, extra = {}) {
  return api('/api/files/op', { body: { op, path, ...extra } });
}

async function fileMenu(e) {
  const mac = isMac();
  const x = ext(e.name);
  const options = [];
  if (e.dir) options.push({ label: '进入文件夹', icon: 'folder', value: 'enter' });
  options.push({ label: e.dir ? '在电脑上打开文件夹' : '在电脑上打开', icon: 'display', value: 'open' },
    { label: mac ? '在访达中显示' : '在资源管理器中显示', icon: 'search', value: 'reveal' });
  if (!e.dir) {
    if (PREVIEWABLE.has(x)) options.push({ label: '在 iPad 上预览', icon: 'eye', value: 'preview' });
    options.push({ label: '下载到 iPad', icon: 'download', value: 'download' });
  }
  options.push({ label: '固定到控制台', icon: 'pin', value: 'pin' }, { label: '重命名', icon: 'pencil', value: 'rename' },
    { label: mac ? '移到废纸篓' : '删除（移到回收站）', icon: 'trash', value: 'delete', danger: true });
  const choice = await sheet(e.name, options);
  try {
    if (choice === 'enter') open(e.path);
    else if (choice === 'open') { await fileOp('open', e.path); toast('已在电脑上打开', 'ok'); }
    else if (choice === 'reveal') await fileOp('reveal', e.path);
    else if (choice === 'preview') window.open(withToken(`/api/files/download?inline=1&path=${encodeURIComponent(e.path)}`), '_blank');
    else if (choice === 'download') window.open(withToken(`/api/files/download?path=${encodeURIComponent(e.path)}`), '_blank');
    else if (choice === 'pin') pinToDashboard({ label: e.name.replace(/\.[^.]+$/, '') || e.name, icon: `sf:${symbolFor(e)}`, color: e.dir ? 'teal' : 'blue', actions: [{ type: 'open', value: e.path }] });
    else if (choice === 'rename') {
      const name = await promptDialog('重命名', e.name);
      if (name && name !== e.name) { await fileOp('rename', e.path, { name }); open(cwd); }
    } else if (choice === 'delete') {
      if (await confirmDialog('删除', `把「${e.name}」${mac ? '移到废纸篓' : '移到回收站'}？`, { danger: true, okText: '删除' })) {
        await fileOp('delete', e.path); toast('已删除，可在电脑上还原', 'ok'); open(cwd);
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
    xhr.open('POST', `${base()}/api/files/upload?dir=${encodeURIComponent(cwd)}`);
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
      h('div', { class: 'section-title' }, '个人收藏'),
      ...places.map((p) => h('button', { class: 'btn', onclick: () => open(p.path) }, icon(p.icon), p.label)),
      h('div', { class: 'section-title' }, '位置'),
      ...drives.map((d) => h('button', { class: 'btn', onclick: () => open(d.path) }, icon('disk'), d.name)));
  } catch (e) { showError(e); }
}

export default {
  id: 'files', title: '文件', icon: 'folder',
  mount(el) {
    crumbsEl = h('div', { class: 'crumbs' });
    placesEl = h('div', { class: 'places glass' });
    listEl = h('div');
    progressEl = h('div', { class: 'progress hidden', style: { marginBottom: '10px' } }, h('i'));
    const hiddenBtn = h('button', { class: 'btn sm icon', title: '显示隐藏文件', onclick: () => { showHidden = !showHidden; hiddenBtn.classList.toggle('on', showHidden); open(cwd); } }, icon('eye'));
    el.append(
      h('div', { class: 'toolbar' },
        h('button', { class: 'btn sm icon', title: '上一级', onclick: () => parent !== null && open(parent) }, icon('chevron.left')),
        crumbsEl,
        h('button', { class: 'btn sm icon', title: '刷新', onclick: () => open(cwd) }, icon('refresh')),
        h('button', { class: 'btn sm tinted', onclick: upload }, icon('upload'), '上传'),
        h('button', {
          class: 'btn sm icon', title: '新建文件夹', onclick: async () => {
            if (!cwd) return toast('请先进入一个文件夹', 'error');
            const name = await promptDialog('新建文件夹', '新建文件夹');
            if (name) { try { await fileOp('mkdir', cwd, { name }); open(cwd); } catch (e) { showError(e); } }
          },
        }, icon('plus')),
        h('button', { class: 'btn sm icon', title: '在电脑上打开当前文件夹', onclick: () => cwd && fileOp('open', cwd).catch(showError) }, icon('display')),
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
