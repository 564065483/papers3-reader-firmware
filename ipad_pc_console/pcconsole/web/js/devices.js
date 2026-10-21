// 多台电脑：已添加的电脑、局域网自动发现、手动添加、切换
import {
  activeDevice, addDevice, api, deviceName, devices, loginDevice, normalizeUrl, pingDevice,
  removeDevice, renameDevice, state, switchDevice,
} from './api.js';
import { icon } from './icons.js';
import { h, modal, promptDialog, sheet, showError, toast } from './ui.js';

export const osIcon = (os) => (os === 'mac' ? 'laptop' : 'desktop');
export const osName = (os) => ({ mac: 'macOS', windows: 'Windows', linux: 'Linux' }[os] || '');

function row(d, statusEl, onMenu) {
  const active = d.id === activeDevice()?.id;
  return h('div', {
    class: `item tap dev-row ${active ? 'active' : ''}`,
    onclick: () => { if (!active) switchDevice(d.id); },
  },
  h('div', { class: 'ic' }, icon(osIcon(d.os))),
  h('div', { class: 'grow' },
    h('div', { class: 't ellipsis' }, deviceName(d)),
    h('div', { class: 'small muted row', style: { gap: '6px' } }, statusEl,
      h('span', { class: 'ellipsis' }, [osName(d.os), d.url ? d.url.replace(/^https?:\/\//, '') : '当前网页所在电脑'].filter(Boolean).join(' · ')))),
  active ? icon('check', 'check') : null,
  h('button', { class: 'btn sm icon ghost', onclick: (e) => { e.stopPropagation(); onMenu(d); } }, icon('ellipsis')));
}

async function connectNew(url, presetName, close) {
  const pin = await promptDialog(`输入「${presetName || url}」上显示的 PIN 码`, '', '6 位数字', { type: 'tel' });
  if (!pin) return;
  try {
    const { token, info } = await loginDevice(url, pin.trim());
    const id = addDevice({ url, token, name: info.name || info.hostname, os: info.os_type, deviceId: info.device_id });
    toast(`已添加 ${info.name || info.hostname}`, 'ok');
    close();
    setTimeout(() => switchDevice(id), 350);
  } catch (e) { showError(e); }
}

export function openSwitcher() {
  const saved = h('div', { class: 'list list-anim' });
  const nearby = h('div', { class: 'list list-anim' }, h('div', { class: 'hint', style: { padding: '6px 4px' } }, '正在搜索局域网中的电脑…'));
  const addr = h('input', { class: 'input', placeholder: '电脑地址，例如 192.168.1.20', autocapitalize: 'off', autocorrect: 'off', inputMode: 'url' });
  let p;
  const close = () => p.close(null);

  const menu = async (d) => {
    const choice = await sheet(deviceName(d), [
      { label: '重命名', icon: 'pencil', value: 'rename' },
      d.url ? { label: '移除这台电脑', icon: 'trash', value: 'remove', danger: true } : null,
    ].filter(Boolean));
    if (choice === 'rename') {
      const name = await promptDialog('显示名称', deviceName(d));
      if (name) { renameDevice(d.id, name); paintSaved(); }
    } else if (choice === 'remove') {
      const wasActive = d.id === activeDevice()?.id;
      removeDevice(d.id);
      if (wasActive) location.reload(); else paintSaved();
    }
  };

  const paintSaved = () => {
    saved.replaceChildren(...devices.map((d, i) => {
      const dot = h('i', { class: 'dot wait' });
      pingDevice(d).then((r) => { dot.className = `dot ${r ? 'on' : ''}`; });
      const el = row(d, dot, menu);
      el.style.setProperty('--i', i);
      return el;
    }));
  };
  paintSaved();

  api('/api/peers').then(({ peers }) => {
    const known = new Set(devices.map((d) => d.deviceId).concat(state.info?.device_id));
    const fresh = peers.filter((p2) => !known.has(p2.id));
    if (!fresh.length) {
      nearby.replaceChildren(h('div', { class: 'hint', style: { padding: '6px 4px' } }, '没有发现新的电脑。请在其它电脑上也运行控制台服务端，并连接同一个 Wi‑Fi。'));
      return;
    }
    nearby.replaceChildren(...fresh.map((p2) => h('div', {
      class: 'item tap dev-row',
      onclick: () => connectNew(`http://${p2.address}:${p2.port}`, p2.name, close),
    }, h('div', { class: 'ic' }, icon(osIcon(p2.os))),
    h('div', { class: 'grow' }, h('div', { class: 't' }, p2.name), h('div', { class: 'small muted' }, `${osName(p2.os)} · ${p2.address}`)),
    h('span', { class: 'btn sm tinted' }, '添加'))));
  }).catch(() => nearby.replaceChildren(h('div', { class: 'hint' }, '当前电脑未连接，无法搜索')));

  const body = h('div', {},
    h('div', { class: 'section-title' }, '我的电脑'), saved,
    h('div', { class: 'section-title row', style: { gap: '6px' } }, icon('radar'), '附近的电脑'), nearby,
    h('div', { class: 'section-title' }, '手动添加'),
    h('div', { class: 'row' }, addr, h('button', {
      class: 'btn primary', onclick: () => {
        let url;
        try { url = normalizeUrl(addr.value); } catch { url = ''; }
        if (!url) return toast('请输入电脑的 IP 地址', 'error');
        connectNew(url, addr.value.trim(), close);
      },
    }, '连接')),
    h('div', { class: 'hint', style: { marginTop: '10px' } },
      '在每台 Windows 或 Mac 上运行控制台服务端即可。地址和 PIN 码显示在电脑端的窗口或配对页面中。'));
  p = modal({ title: '切换电脑', body, buttons: [{ label: '完成', kind: 'primary', value: null }] });
}
