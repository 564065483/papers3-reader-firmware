// 触控板 + 键盘：手势映射为鼠标操作，实时把 iPad 输入（含中文输入法、外接键盘）发送到电脑
import { api, prefs, savePrefs, ws } from './api.js';
import { h, modal, paintRange, showError, switchEl, toast } from './ui.js';

// ---------------- 鼠标移动 / 滚动合帧发送 ----------------
const pend = { mx: 0, my: 0, sx: 0, sy: 0 };
let raf = 0;
function flush() {
  raf = 0;
  const mx = Math.trunc(pend.mx), my = Math.trunc(pend.my);
  if (mx || my) { ws.send({ t: 'm', x: mx, y: my }); pend.mx -= mx; pend.my -= my; }
  const sx = Math.trunc(pend.sx), sy = Math.trunc(pend.sy);
  if (sx || sy) { ws.send({ t: 's', x: sx, y: sy }); pend.sx -= sx; pend.sy -= sy; }
}
const schedule = () => { if (!raf) raf = requestAnimationFrame(flush); };

function moveBy(dx, dy) {
  const mag = Math.hypot(dx, dy);
  const f = prefs.sensitivity * (1 + prefs.accel * Math.min(mag, 30) / 10);
  pend.mx += dx * f; pend.my += dy * f;
  schedule();
}

export function scrollBy(dx, dy) {
  // 滚轮单位：120 = 一格；自然滚动与 iPad / Mac 触控板方向一致
  const k = prefs.scrollSpeed * 3 * (prefs.naturalScroll ? 1 : -1);
  pend.sy += dy * k; pend.sx += -dx * k;
  schedule();
}

const click = (b = 'left', a = 'click') => ws.send({ t: 'b', b, a });

function ripple(pad, x, y) {
  const r = pad.getBoundingClientRect();
  const dot = h('i', { class: 'ripple', style: { left: `${x - r.left}px`, top: `${y - r.top}px` } });
  pad.append(dot);
  setTimeout(() => dot.remove(), 500);
}

// ---------------- 触控板手势 ----------------
function bindPad(pad) {
  const pts = new Map();
  let maxN = 0, t0 = 0, moved = false, dragging = false, longTimer = null;
  let acc = { x: 0, y: 0 }, vel = { x: 0, y: 0 }, inertia = 0, lastXY = null;

  const stopInertia = () => { cancelAnimationFrame(inertia); inertia = 0; };

  pad.addEventListener('touchstart', (e) => {
    e.preventDefault();
    stopInertia();
    for (const t of e.changedTouches) pts.set(t.identifier, { x: t.clientX, y: t.clientY });
    if (maxN === 0) {
      t0 = performance.now(); moved = false; acc = { x: 0, y: 0 }; vel = { x: 0, y: 0 };
      const t = e.changedTouches[0];
      lastXY = { x: t.clientX, y: t.clientY };
      // 长按不动 → 按住左键拖拽（选择文字、拖动窗口）
      longTimer = setTimeout(() => {
        if (!moved && pts.size === 1) {
          dragging = true; click('left', 'down'); pad.classList.add('dragging');
          ripple(pad, lastXY.x, lastXY.y);
        }
      }, 450);
    } else clearTimeout(longTimer);
    maxN = Math.max(maxN, pts.size);
  }, { passive: false });

  pad.addEventListener('touchmove', (e) => {
    e.preventDefault();
    let sx = 0, sy = 0, n = 0;
    for (const t of e.changedTouches) {
      const p = pts.get(t.identifier);
      if (!p) continue;
      sx += t.clientX - p.x; sy += t.clientY - p.y;
      p.x = t.clientX; p.y = t.clientY; n++;
    }
    if (!n) return;
    const dx = sx / n, dy = sy / n;
    acc.x += dx; acc.y += dy;
    if (!moved && Math.hypot(acc.x, acc.y) > 8) { moved = true; clearTimeout(longTimer); }
    if (maxN === 1) moveBy(dx, dy);
    else if (maxN === 2 && pts.size === 2) {
      scrollBy(dx, dy);
      vel = { x: vel.x * 0.5 + dx * 0.5, y: vel.y * 0.5 + dy * 0.5 };
    }
  }, { passive: false });

  const end = (e) => {
    e.preventDefault();
    for (const t of e.changedTouches) pts.delete(t.identifier);
    if (pts.size) return;
    clearTimeout(longTimer);
    const dt = performance.now() - t0;
    const t = e.changedTouches[0];
    if (dragging) {
      dragging = false; click('left', 'up'); pad.classList.remove('dragging');
    } else if (!moved && dt < 320) {
      click(maxN === 1 ? 'left' : maxN === 2 ? 'right' : 'middle');
      ripple(pad, t.clientX, t.clientY);
    } else if (maxN === 2 && moved && Math.hypot(vel.x, vel.y) > 2) {
      // 双指滚动惯性
      const step = () => {
        vel.x *= 0.94; vel.y *= 0.94;
        if (Math.hypot(vel.x, vel.y) < 0.4) { inertia = 0; return; }
        scrollBy(vel.x, vel.y);
        inertia = requestAnimationFrame(step);
      };
      inertia = requestAnimationFrame(step);
    } else if (maxN >= 3 && moved) {
      // 三指轻扫：上=任务视图，下=显示桌面，左/右=切换窗口
      const { x, y } = acc;
      let combo = null;
      if (Math.abs(y) > Math.abs(x) && Math.abs(y) > 60) combo = y < 0 ? 'win+tab' : 'win+d';
      else if (Math.abs(x) > 60) combo = x < 0 ? 'alt+shift+tab' : 'alt+tab';
      if (combo) { ws.send({ t: 'k', k: combo }); toast(`三指手势：${combo}`); }
    }
    maxN = 0;
  };
  pad.addEventListener('touchend', end, { passive: false });
  pad.addEventListener('touchcancel', end, { passive: false });

  // 用电脑浏览器调试时支持鼠标拖动
  let mouseDown = null;
  pad.addEventListener('mousedown', (e) => { mouseDown = { x: e.clientX, y: e.clientY, moved: false }; });
  window.addEventListener('mousemove', (e) => {
    if (!mouseDown) return;
    const dx = e.clientX - mouseDown.x, dy = e.clientY - mouseDown.y;
    if (Math.abs(dx) + Math.abs(dy) > 0) { mouseDown.moved = true; moveBy(dx, dy); }
    mouseDown.x = e.clientX; mouseDown.y = e.clientY;
  });
  window.addEventListener('mouseup', () => { if (mouseDown && !mouseDown.moved) click('left'); mouseDown = null; });
  pad.addEventListener('wheel', (e) => { e.preventDefault(); scrollBy(-e.deltaX / 3, -e.deltaY / 3); }, { passive: false });
}

function bindScrollStrip(strip) {
  let last = null;
  strip.addEventListener('touchstart', (e) => { e.preventDefault(); last = e.touches[0].clientY; }, { passive: false });
  strip.addEventListener('touchmove', (e) => {
    e.preventDefault();
    const y = e.touches[0].clientY;
    scrollBy(0, (y - last) * 1.5);
    last = y;
  }, { passive: false });
}

function holdButton(label, button) {
  const el = h('button', {}, label);
  el.addEventListener('pointerdown', (e) => { e.preventDefault(); el.setPointerCapture(e.pointerId); el.classList.add('down'); click(button, 'down'); });
  const up = () => { if (el.classList.contains('down')) { el.classList.remove('down'); click(button, 'up'); } };
  el.addEventListener('pointerup', up);
  el.addEventListener('pointercancel', up);
  el.addEventListener('contextmenu', (e) => e.preventDefault());
  return el;
}

// ---------------- 键盘 ----------------
const KEYMAP = {
  Backspace: 'backspace', Enter: 'enter', Tab: 'tab', Escape: 'esc', Delete: 'delete',
  ArrowLeft: 'left', ArrowRight: 'right', ArrowUp: 'up', ArrowDown: 'down',
  Home: 'home', End: 'end', PageUp: 'pageup', PageDown: 'pagedown', ' ': 'space',
};
const CODEMAP = {
  Minus: '-', Equal: '=', BracketLeft: '[', BracketRight: ']', Backslash: '\\', Semicolon: ';',
  Quote: "'", Comma: ',', Period: '.', Slash: '/', Backquote: '`', Space: 'space',
};
const SPECIAL = [['esc', 'Esc'], ['tab', 'Tab'], ['backspace', '⌫'], ['delete', 'Del'], ['enter', '↵ 回车'],
  ['left', '←'], ['up', '↑'], ['down', '↓'], ['right', '→'], ['home', 'Home'], ['end', 'End'],
  ['pageup', 'PgUp'], ['pagedown', 'PgDn'], ['space', '空格'], ['printscreen', 'PrtSc'], ['apps', '菜单键']];

export function keyboardPanel() {
  const sticky = { ctrl: false, shift: false, alt: false, win: false };
  const modBtns = {};
  const anySticky = () => Object.values(sticky).some(Boolean);
  const paintMods = () => Object.entries(modBtns).forEach(([k, b]) => b.classList.toggle('on', sticky[k]));
  const clearMods = () => { Object.keys(sticky).forEach((k) => { sticky[k] = false; }); paintMods(); };

  const sendKey = (key, extra = []) => {
    const mods = [...new Set([...extra, ...Object.keys(sticky).filter((k) => sticky[k])])];
    const combo = [...mods.filter((m) => m !== key), key].join('+');
    if (!ws.send({ t: 'k', k: combo })) toast('未连接到电脑', 'error');
    clearMods();
  };

  for (const [k, label] of [['ctrl', 'Ctrl'], ['shift', 'Shift'], ['alt', 'Alt'], ['win', '⊞ Win']]) {
    modBtns[k] = h('button', {
      class: 'btn sm',
      onclick: () => { sticky[k] = !sticky[k]; paintMods(); },
      ondblclick: () => { if (k === 'win') { clearMods(); ws.send({ t: 'k', k: 'win' }); } },
    }, label);
  }
  const fkeys = h('div', { class: 'keys hidden' }, Array.from({ length: 12 }, (_, i) =>
    h('button', { class: 'btn sm', onclick: () => sendKey(`f${i + 1}`) }, `F${i + 1}`)));

  const input = h('input', {
    class: 'input kb-input', placeholder: '⌨️ 点这里调出键盘，输入内容实时发送到电脑（支持中文输入法 / 外接键盘）',
    autocomplete: 'off', autocorrect: 'off', autocapitalize: 'off', spellcheck: false, enterkeyhint: 'send',
  });
  let composing = false;
  const flushInput = () => {
    const v = input.value;
    if (!v) return;
    input.value = '';
    if (anySticky()) { for (const ch of v) sendKey(ch.toLowerCase()); return; }
    if (!ws.send({ t: 'txt', v })) toast('未连接到电脑', 'error');
  };
  input.addEventListener('compositionstart', () => { composing = true; });
  input.addEventListener('compositionend', () => { composing = false; setTimeout(flushInput, 0); });
  input.addEventListener('input', (e) => { if (!composing && !e.isComposing) flushInput(); });
  input.addEventListener('keydown', (e) => {
    if (composing || e.isComposing || e.keyCode === 229) return;
    const hasMod = e.ctrlKey || e.altKey || e.metaKey || anySticky();
    let key = KEYMAP[e.key] || (/^F\d{1,2}$/.test(e.key) ? e.key.toLowerCase() : null);
    if (!key && hasMod) {
      if (e.code.startsWith('Key')) key = e.code.slice(3).toLowerCase();
      else if (e.code.startsWith('Digit')) key = e.code.slice(5);
      else key = CODEMAP[e.code] || null;
    }
    if (!key || (key === 'space' && !hasMod)) return; // 普通字符走 input 事件
    e.preventDefault();
    const mods = [];
    if (e.ctrlKey) mods.push('ctrl');
    if (e.metaKey) mods.push(prefs.cmdAsCtrl ? 'ctrl' : 'win');
    if (e.altKey) mods.push('alt');
    if (e.shiftKey) mods.push('shift');
    sendKey(key, mods);
  });

  const sendTextDialog = () => {
    const ta = h('textarea', { class: 'input', placeholder: '输入或粘贴一段文字…', style: { minHeight: '200px' } });
    setTimeout(() => ta.focus(), 50);
    modal({
      title: '发送文字到电脑', body: ta,
      buttons: [
        { label: '取消', value: null },
        { label: '粘贴到电脑', onClick: async () => { try { await api('/api/clipboard', { body: { text: ta.value, paste: true } }); toast('已粘贴', 'ok'); } catch (e) { showError(e); return false; } } },
        { label: '逐字输入', kind: 'primary', onClick: () => { if (!ws.send({ t: 'txt', v: ta.value })) { toast('未连接到电脑', 'error'); return false; } } },
      ],
    });
  };

  return h('div', { class: 'kb' },
    h('div', { class: 'keys' }, Object.values(modBtns),
      SPECIAL.map(([k, label]) => h('button', { class: 'btn sm', onclick: () => sendKey(k) }, label)),
      h('button', { class: 'btn sm', onclick: () => fkeys.classList.toggle('hidden') }, 'F1–F12'),
      h('button', { class: 'btn sm', onclick: sendTextDialog }, '📝 发送整段文字')),
    fkeys,
    input);
}

// ---------------- 页面 ----------------
function slider(label, key, min, max, step) {
  const r = h('input', { type: 'range', min, max, step, value: prefs[key] });
  const val = h('span', { class: 'small muted', style: { minWidth: '34px' } }, String(prefs[key]));
  r.addEventListener('input', () => { prefs[key] = Number(r.value); val.textContent = r.value; paintRange(r); savePrefs(); });
  paintRange(r);
  return h('label', { class: 'row small', style: { minWidth: '220px' } }, h('span', { class: 'muted' }, label), r, val);
}

export default {
  id: 'touchpad', title: '触控板与键盘', short: '触控板', icon: '🖱️', fill: true,
  mount(el) {
    const pad = h('div', { class: 'tp-pad' },
      h('div', { class: 'tip' }, '单指移动 · 轻点=左键 · 双指轻点=右键 · 双指滑动=滚动 · 长按=拖拽 · 三指上/下/左/右=任务视图/桌面/切换窗口'));
    const strip = h('div', { class: 'tp-scroll' }, '⇅');
    const lockBtn = h('button', { class: 'btn sm' }, '🔒 拖拽锁定');
    let locked = false;
    const setLock = (on) => {
      locked = on;
      lockBtn.classList.toggle('on', on);
      click('left', on ? 'down' : 'up');
    };
    lockBtn.onclick = () => setLock(!locked);
    bindPad(pad);
    bindScrollStrip(strip);
    const tools = h('div', { class: 'tp-tools' },
      slider('灵敏度', 'sensitivity', 0.4, 4, 0.1),
      slider('滚动', 'scrollSpeed', 0.2, 3, 0.1),
      h('label', { class: 'row small' }, h('span', { class: 'muted' }, '自然滚动'),
        switchEl(prefs.naturalScroll, (v) => { prefs.naturalScroll = v; savePrefs(); })),
      lockBtn,
      h('button', { class: 'btn sm', onclick: () => click('left', 'double') }, '双击'));
    const btns = h('div', { class: 'tp-btns' }, holdButton('左键', 'left'), holdButton('中键', 'middle'), holdButton('右键', 'right'));
    el.append(h('div', { class: 'tp' }, tools, pad, strip, btns), keyboardPanel());
  },
};
