// 远程屏幕：实时画面 + 直接点触操作电脑（点击、右键、拖拽、滚动、双指缩放）
import { api, prefs, savePrefs, withToken, ws } from './api.js';
import { icon } from './icons.js';
import { enterFullscreen, exitFullscreen, isFullscreen, isStandalone } from './display.js';
import { h, showError, toast } from './ui.js';
import { keyboardPanel, scrollBy } from './touchpad.js';

let stage, img, fpsEl, msgEl, monSel, ctrlBtn, kbBox;
let running = false, visible = false, monIndex = 0, control = true;
let mon = { left: 0, top: 0, width: 1920, height: 1080 };
let view = { s: 1, x: 0, y: 0 };
let frames = 0, fpsT = performance.now(), lastUrl = null, controller = null;

function applyView() {
  img.style.transform = `translate(${view.x}px, ${view.y}px) scale(${view.s})`;
}
function resetView() {
  // 带弹簧回弹地恢复到适应屏幕
  img.classList.add('ease');
  view = { s: 1, x: 0, y: 0 };
  applyView();
  setTimeout(() => img.classList.remove('ease'), 520);
}

async function loop() {
  if (running) return;
  running = true;
  while (visible) {
    const t0 = performance.now();
    try {
      controller = new AbortController();
      const width = Math.round(Math.min(prefs.screenWidth, Math.max(800, stage.clientWidth * devicePixelRatio * Math.max(1, view.s))));
      const res = await api(`/api/screen/frame?mon=${monIndex}&w=${width}&q=${prefs.screenQuality}&cursor=${prefs.screenCursor ? 1 : 0}`,
        { raw: true, signal: controller.signal });
      mon = {
        left: Number(res.headers.get('X-Mon-Left')), top: Number(res.headers.get('X-Mon-Top')),
        width: Number(res.headers.get('X-Mon-Width')), height: Number(res.headers.get('X-Mon-Height')),
      };
      const blob = await res.blob();
      const url = URL.createObjectURL(blob);
      await new Promise((resolve) => { img.onload = resolve; img.onerror = resolve; img.src = url; });
      if (lastUrl) URL.revokeObjectURL(lastUrl);
      lastUrl = url;
      msgEl.classList.add('hidden');
      frames++;
      const now = performance.now();
      if (now - fpsT > 1000) { fpsEl.textContent = `${frames} fps · ${Math.round(blob.size / 1024)} KB`; frames = 0; fpsT = now; }
    } catch (e) {
      if (e.name === 'AbortError') break;
      msgEl.textContent = e.message; msgEl.classList.remove('hidden');
      await new Promise((r) => setTimeout(r, 1500));
    }
    const wait = 1000 / prefs.screenFps - (performance.now() - t0);
    if (wait > 0) await new Promise((r) => setTimeout(r, wait));
  }
  running = false;
}

/** 屏幕坐标（iPad）→ 电脑虚拟桌面坐标 */
function toPc(cx, cy) {
  const r = img.getBoundingClientRect();
  if (!r.width) return null;
  const fx = (cx - r.left) / r.width, fy = (cy - r.top) / r.height;
  if (fx < 0 || fx > 1 || fy < 0 || fy > 1) return null;
  return { x: Math.round(mon.left + fx * mon.width), y: Math.round(mon.top + fy * mon.height) };
}
function moveTo(cx, cy) {
  const p = toPc(cx, cy);
  if (p) ws.send({ t: 'abs', x: p.x, y: p.y });
  return p;
}

function bindGestures() {
  const pts = new Map();
  let mode = null, t0 = 0, start = null, longTimer = null, pinch = null, maxN = 0;

  stage.addEventListener('touchstart', (e) => {
    e.preventDefault();
    for (const t of e.changedTouches) pts.set(t.identifier, { x: t.clientX, y: t.clientY });
    maxN = Math.max(maxN, pts.size);
    if (pts.size === 1 && !mode) {
      const t = e.changedTouches[0];
      t0 = performance.now(); start = { x: t.clientX, y: t.clientY }; mode = 'tap';
      if (control) {
        longTimer = setTimeout(() => {
          if (mode === 'tap') { mode = 'done'; moveTo(start.x, start.y); ws.send({ t: 'b', b: 'right', a: 'click' }); toast('右键'); }
        }, 550);
      }
    } else if (pts.size === 2) {
      clearTimeout(longTimer);
      if (mode === 'drag') ws.send({ t: 'b', b: 'left', a: 'up' });
      const [a, b] = [...pts.values()];
      pinch = { d: Math.hypot(a.x - b.x, a.y - b.y), cx: (a.x + b.x) / 2, cy: (a.y + b.y) / 2, s: view.s, x: view.x, y: view.y, decided: null };
      mode = 'two';
    }
  }, { passive: false });

  stage.addEventListener('touchmove', (e) => {
    e.preventDefault();
    const prev = new Map([...pts].map(([k, v]) => [k, { ...v }]));
    for (const t of e.changedTouches) if (pts.has(t.identifier)) pts.set(t.identifier, { x: t.clientX, y: t.clientY });
    if (mode === 'tap' || mode === 'drag') {
      const p = [...pts.values()][0];
      if (mode === 'tap' && Math.hypot(p.x - start.x, p.y - start.y) > 10) {
        clearTimeout(longTimer);
        if (!control || view.s > 1.01) { mode = 'pan'; }
        else { mode = 'drag'; moveTo(start.x, start.y); ws.send({ t: 'b', b: 'left', a: 'down' }); }
      }
      if (mode === 'drag') moveTo(p.x, p.y);
    }
    if (mode === 'pan') {
      const id = [...pts.keys()][0];
      const p = pts.get(id), q = prev.get(id);
      view.x += p.x - q.x; view.y += p.y - q.y; applyView();
    }
    if (mode === 'two' && pts.size === 2) {
      const [a, b] = [...pts.values()];
      const d = Math.hypot(a.x - b.x, a.y - b.y);
      const cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2;
      if (!pinch.decided) {
        if (Math.abs(d - pinch.d) > 24) pinch.decided = 'zoom';
        else if (Math.hypot(cx - pinch.cx, cy - pinch.cy) > 12) pinch.decided = control ? 'scroll' : 'zoom';
      }
      if (pinch.decided === 'zoom') {
        // 以双指中心为锚点缩放，并跟随平移
        const s = Math.min(6, Math.max(1, pinch.s * d / pinch.d));
        const r = stage.getBoundingClientRect();
        const ox = pinch.cx - r.left, oy = pinch.cy - r.top;
        const base = img.offsetLeft, baseY = img.offsetTop;
        const lx = (ox - base - pinch.x) / pinch.s, ly = (oy - baseY - pinch.y) / pinch.s;
        view.s = s;
        view.x = (cx - r.left) - base - lx * s;
        view.y = (cy - r.top) - baseY - ly * s;
        if (s === 1) { view.x = 0; view.y = 0; }
        applyView();
      } else if (pinch.decided === 'scroll') {
        const ids = [...pts.keys()];
        const dy = ids.reduce((acc, id) => acc + (pts.get(id).y - (prev.get(id)?.y ?? pts.get(id).y)), 0) / 2;
        const dx = ids.reduce((acc, id) => acc + (pts.get(id).x - (prev.get(id)?.x ?? pts.get(id).x)), 0) / 2;
        moveTo(cx, cy);
        scrollBy(dx, dy);
      }
    }
  }, { passive: false });

  const end = (e) => {
    e.preventDefault();
    for (const t of e.changedTouches) pts.delete(t.identifier);
    if (pts.size) return;
    clearTimeout(longTimer);
    if (mode === 'tap' && control && performance.now() - t0 < 500) {
      if (moveTo(start.x, start.y)) ws.send({ t: 'b', b: 'left', a: 'click' });
    } else if (mode === 'drag') {
      ws.send({ t: 'b', b: 'left', a: 'up' });
    } else if (mode === 'two' && !pinch?.decided && maxN === 2 && control) {
      // 双指轻点 = 在该处右键
      const { cx, cy } = pinch;
      if (moveTo(cx, cy)) ws.send({ t: 'b', b: 'right', a: 'click' });
    }
    mode = null; pinch = null; maxN = 0;
  };
  stage.addEventListener('touchend', end, { passive: false });
  stage.addEventListener('touchcancel', end, { passive: false });

  // 电脑浏览器调试：鼠标点击
  stage.addEventListener('click', (e) => {
    if (e.pointerType === 'touch' || !control) return;
    if (moveTo(e.clientX, e.clientY)) ws.send({ t: 'b', b: 'left', a: 'click' });
  });
}

async function loadMonitors() {
  try {
    const { monitors } = await api('/api/screen/monitors');
    monSel.replaceChildren(...monitors.map((m, i) => h('option', { value: i, selected: i === monIndex },
      `${m.primary ? '主屏' : `屏幕 ${i + 1}`} ${m.width}×${m.height}`)));
  } catch (e) { showError(e); }
}

function immersive(on) {
  document.body.classList.toggle('immersive', on);
  if (on && !isFullscreen() && !isStandalone()) enterFullscreen();
  else if (!on && isFullscreen() && !prefs.autoFullscreen) exitFullscreen();
}

function select(options, key, onChange) {
  const sel = h('select', { class: 'input sm' },
    options.map(([v, label]) => h('option', { value: v, selected: prefs[key] === v }, label)));
  sel.onchange = () => { prefs[key] = Number(sel.value); savePrefs(); onChange?.(); };
  return sel;
}

export default {
  id: 'screen', title: '远程屏幕', short: '屏幕', icon: 'display', fill: true,
  mount(el) {
    img = h('img', { alt: '' });
    fpsEl = h('div', { class: 'fps' }, '');
    msgEl = h('div', { class: 'msg' }, '正在连接…');
    stage = h('div', { class: 'scr-stage' }, img, fpsEl, msgEl,
      h('button', { class: 'btn sm scr-exit', onclick: () => immersive(false) }, icon('shrink'), '退出全屏'));
    monSel = h('select', { class: 'input sm', onchange: () => { monIndex = Number(monSel.value); resetView(); } });
    const paintCtrl = () => { ctrlBtn.classList.toggle('on', control); ctrlBtn.replaceChildren(icon(control ? 'hand' : 'eye'), control ? '触控操作' : '仅观看'); };
    ctrlBtn = h('button', { class: 'btn sm on', onclick: () => { control = !control; paintCtrl(); } });
    paintCtrl();
    kbBox = h('div', { class: 'hidden' }, keyboardPanel());
    const tools = h('div', { class: 'scr-tools' },
      monSel, ctrlBtn,
      select([[40, '画质 低'], [60, '画质 中'], [80, '画质 高'], [92, '画质 极高']], 'screenQuality'),
      select([[5, '5 fps'], [12, '12 fps'], [20, '20 fps'], [30, '30 fps']], 'screenFps'),
      select([[1280, '1280 宽'], [1600, '1600 宽'], [1920, '1920 宽'], [2560, '2560 宽'], [3840, '原始']], 'screenWidth'),
      h('button', { class: 'btn sm', onclick: resetView }, icon('fullscreen'), '适应'),
      h('button', { class: 'btn sm', onclick: () => kbBox.classList.toggle('hidden') }, icon('keyboard'), '键盘'),
      h('button', { class: 'btn sm', onclick: () => window.open(withToken('/api/screen/frame?w=0&q=92&mon=' + monIndex), '_blank') }, icon('camera'), '截图'),
      h('button', { class: 'btn sm', onclick: () => immersive(true) }, icon('fullscreen'), '全屏'));
    el.append(tools, stage, kbBox);
    bindGestures();
  },
  show() {
    visible = true;
    loadMonitors();
    loop();
  },
  hide() {
    visible = false;
    controller?.abort();
    immersive(false);
  },
};
