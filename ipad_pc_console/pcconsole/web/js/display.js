// 全屏与“添加到主屏幕”：在 Safari 里隐藏地址栏，只显示控制台
import { activeDevice, getToken, prefs } from './api.js';
import { icon } from './icons.js';
import { h, modal, toast } from './ui.js';

/** 是否以主屏幕 App（独立窗口）方式运行 */
export const isStandalone = () => window.navigator.standalone === true
  || matchMedia('(display-mode: standalone)').matches || matchMedia('(display-mode: fullscreen)').matches;

const root = () => document.documentElement;
export const canFullscreen = () => !!(root().requestFullscreen || root().webkitRequestFullscreen);
export const isFullscreen = () => !!(document.fullscreenElement || document.webkitFullscreenElement);

export function enterFullscreen() {
  const el = root();
  try {
    const p = (el.requestFullscreen || el.webkitRequestFullscreen)?.call(el);
    p?.catch?.(() => {});
  } catch { /* 浏览器不支持 */ }
}
export function exitFullscreen() {
  try { (document.exitFullscreen || document.webkitExitFullscreen)?.call(document)?.catch?.(() => {}); } catch { /* 忽略 */ }
}
export function toggleFullscreen() {
  if (!canFullscreen()) { showInstallGuide(); return; }
  if (isFullscreen()) exitFullscreen(); else enterFullscreen();
}

/** 让“添加到主屏幕”后的 App 自动带上登录令牌，从主屏幕打开无需再输 PIN */
export function updateManifestLink() {
  const link = document.querySelector('link[rel="manifest"]');
  if (!link) return;
  const d = activeDevice();
  link.href = d && d.url === '' && getToken() ? `/manifest.webmanifest?token=${encodeURIComponent(getToken())}` : '/manifest.webmanifest';
}

export function showInstallGuide() {
  const step = (n, iconName, title, text) => h('div', { class: 'guide-step' },
    h('div', { class: 'guide-num' }, String(n)),
    h('div', { class: 'guide-ic' }, icon(iconName)),
    h('div', { class: 'grow' }, h('div', { class: 't' }, title), h('div', { class: 'small muted' }, text)));
  modal({
    title: '只显示控制台（隐藏网址栏）',
    body: h('div', {},
      h('div', { class: 'section-title' }, '方法一：添加到主屏幕（推荐，永久有效）'),
      h('div', { class: 'list' },
        step(1, 'share', '点 Safari 的「分享」按钮', '在地址栏右侧，或屏幕右上角的方框加箭头图标'),
        step(2, 'plus.square', '选择「添加到主屏幕」', '名称可以改成你喜欢的，比如「电脑控制台」'),
        step(3, 'app', '以后从主屏幕图标打开', '会像原生 App 一样全屏显示，没有网址栏，还能在多任务里单独切换')),
      h('div', { class: 'section-title' }, '方法二：临时全屏'),
      h('div', { class: 'list' },
        step(1, 'fullscreen', '点右上角的全屏按钮', canFullscreen()
          ? '立即进入全屏；从屏幕左上角点 × 或向下轻扫可退出。Safari 全屏时弹出键盘可能会自动退出全屏'
          : '当前浏览器不支持网页全屏，请使用方法一'))),
    buttons: canFullscreen()
      ? [{ label: '稍后', value: null }, { label: '现在全屏', kind: 'primary', onClick: () => { enterFullscreen(); } }]
      : [{ label: '知道了', kind: 'primary', value: null }],
  });
}

/** 在 Safari 中打开时，顶部显示一次提示条 */
export function installBanner() {
  if (isStandalone()) return null;
  let dismissed = false;
  try { dismissed = sessionStorage.getItem('pcc.banner') === '1' || localStorage.getItem('pcc.banner.never') === '1'; } catch { /* 忽略 */ }
  if (dismissed) return null;
  const el = h('div', { class: 'banner glass' },
    icon('fullscreen'),
    h('div', { class: 'grow' }, h('b', {}, '想要只显示控制台？'), h('span', { class: 'muted' }, ' 添加到主屏幕后像 App 一样全屏打开，没有网址栏。')),
    canFullscreen() ? h('button', { class: 'btn sm', onclick: () => { enterFullscreen(); close(); } }, '临时全屏') : null,
    h('button', { class: 'btn sm primary', onclick: () => { showInstallGuide(); close(); } }, '怎么设置'),
    h('button', { class: 'btn sm icon ghost', title: '不再提示', onclick: () => { try { localStorage.setItem('pcc.banner.never', '1'); } catch { /* 忽略 */ } close(); } }, icon('xmark')));
  function close() {
    try { sessionStorage.setItem('pcc.banner', '1'); } catch { /* 忽略 */ }
    el.classList.add('out');
    setTimeout(() => el.remove(), 350);
  }
  return el;
}

/** 设置中开启“自动全屏”后，在 Safari 里第一次轻点页面时进入全屏（浏览器要求全屏必须由用户操作触发） */
export function bindAutoFullscreen() {
  if (isStandalone() || !canFullscreen()) return;
  const handler = () => {
    if (prefs.autoFullscreen && !isFullscreen()) enterFullscreen();
  };
  document.addEventListener('touchend', handler, { passive: true });
  document.addEventListener('click', handler);
}

export function fullscreenButton() {
  if (isStandalone()) return null;
  const btn = h('button', { class: 'chip icon-chip', title: '全屏', onclick: toggleFullscreen });
  const paint = () => btn.replaceChildren(icon(isFullscreen() ? 'shrink' : 'fullscreen'));
  paint();
  document.addEventListener('fullscreenchange', paint);
  document.addEventListener('webkitfullscreenchange', paint);
  return btn;
}

