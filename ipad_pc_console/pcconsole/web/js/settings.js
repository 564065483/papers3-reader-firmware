// 设置：操作手感、外观、电脑管理、按钮配置导入导出、安全
import { activeDevice, api, applyTheme, bus, isMac, prefs, renameDevice, savePrefs, setToken, state, ws } from './api.js';
import { icon } from './icons.js';
import { confirmDialog, copyText, fmtBytes, h, modal, paintRange, promptDialog, segmented, showError, switchEl, toast } from './ui.js';
import { reloadPages } from './dashboard.js';
import { openSwitcher, osName } from './devices.js';
import { canFullscreen, isStandalone, showInstallGuide } from './display.js';

function row(title, desc, control) {
  return h('div', { class: 'setting' }, h('div', { class: 'grow' }, h('div', {}, title), desc ? h('div', { class: 'small muted' }, desc) : null), control);
}
function range(key, min, max, step) {
  const r = h('input', { type: 'range', min, max, step, value: prefs[key] });
  const v = h('span', { class: 'small muted', style: { width: '36px', textAlign: 'right' } }, String(prefs[key]));
  r.addEventListener('input', () => { prefs[key] = Number(r.value); v.textContent = r.value; paintRange(r); savePrefs(); });
  paintRange(r);
  return h('div', { class: 'row' }, r, v);
}
function toggle(key, after) { return switchEl(prefs[key], (val) => { prefs[key] = val; savePrefs(); after?.(); }); }
function seg(key, options, after) {
  return segmented(options.map(([value, label]) => ({ value, label })), prefs[key], (v) => { prefs[key] = v; savePrefs(); after?.(); });
}

async function exportConfig() {
  const { pages } = await api('/api/pages');
  const ta = h('textarea', { class: 'input', style: { minHeight: '360px' } });
  ta.value = JSON.stringify(pages, null, 2);
  modal({
    title: '导出按钮配置', wide: true, body: ta,
    buttons: [{ label: '复制', onClick: async () => { toast((await copyText(ta.value)) ? '已复制' : '复制失败', 'ok'); return false; } }, { label: '完成', kind: 'primary', value: null }],
  });
}

async function importConfig() {
  const ta = h('textarea', { class: 'input', style: { minHeight: '360px' }, placeholder: '粘贴导出的 JSON 配置…' });
  const ok = await modal({ title: '导入按钮配置（会覆盖现有按钮）', wide: true, body: ta, buttons: [{ label: '取消', value: false }, { label: '导入', kind: 'primary', value: true }] });
  if (!ok) return;
  try {
    const pages = JSON.parse(ta.value);
    await api('/api/pages', { method: 'PUT', body: { pages } });
    await reloadPages();
    toast('导入成功', 'ok');
  } catch (e) { showError(e instanceof SyntaxError ? new Error('JSON 格式不正确') : e); }
}

function aboutCard() {
  const box = h('div', { class: 'kv' });
  const paint = (i) => {
    if (!i) return;
    box.replaceChildren(
      h('span', {}, '电脑名称'), h('span', {}, i.name || i.hostname),
      h('span', {}, '系统'), h('span', {}, `${osName(i.os_type)} · ${i.os}`),
      h('span', {}, '用户'), h('span', {}, i.user),
      h('span', {}, '处理器'), h('span', {}, `${i.cpu}（${i.cores} 线程）`),
      h('span', {}, '内存'), h('span', {}, fmtBytes(i.memory)),
      h('span', {}, '访问地址'), h('span', {}, i.addresses.map((a) => `http://${a}:${i.port}`).join('\n')),
      h('span', {}, '键鼠控制'), h('span', {}, i.controllable ? '可用' : '当前系统不支持'),
      h('span', {}, '精确音量'), h('span', {}, i.precise_volume ? '可用' : '未安装 pycaw（仅步进调节）'),
      h('span', {}, '服务端版本'), h('span', {}, i.version));
  };
  paint(state.info);
  bus.on('info', paint);
  return h('div', { class: 'card' }, h('h3', {}, icon('info'), '关于这台电脑'), box);
}

export default {
  id: 'settings', title: '设置', icon: 'sliders',
  mount(el) {
    const mac = isMac();
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, icon(mac ? 'laptop' : 'desktop'), '电脑'),
        row('切换或添加电脑', '同时控制家里的 Windows 和 Mac', h('button', { class: 'btn sm tinted', onclick: openSwitcher }, icon('shuffle'), '管理')),
        row('这台电脑的名称', '显示在切换列表和局域网发现中', h('button', {
          class: 'btn sm', onclick: async () => {
            const name = await promptDialog('电脑名称', state.info?.name || '');
            if (!name) return;
            try {
              await api('/api/name', { body: { name } });
              renameDevice(activeDevice().id, '');
              location.reload();
            } catch (e) { showError(e); }
          },
        }, '重命名'))),
      h('div', { class: 'card' }, h('h3', {}, icon('touchpad'), '触控板'),
        row('指针灵敏度', null, range('sensitivity', 0.4, 4, 0.1)),
        row('指针加速', '快速滑动时移动更远', range('accel', 0, 2, 0.1)),
        row('滚动速度', null, range('scrollSpeed', 0.2, 3, 0.1)),
        row('自然滚动', '与 iPad 一致：手指向上，内容向上', toggle('naturalScroll')),
        mac ? null : row('⌘ 当作 Ctrl', '外接键盘时 ⌘C / ⌘V 对应 Windows 的 Ctrl+C / Ctrl+V', toggle('cmdAsCtrl'))),
      h('div', { class: 'card' }, h('h3', {}, icon('paintbrush'), '外观'),
        row('主题', null, seg('theme', [['dark', '深色'], ['light', '浅色'], ['auto', '自动']], applyTheme)),
        row('按钮大小', null, seg('tile', [['s', '小'], ['m', '中'], ['l', '大']], applyTheme)),
        row('远程画面显示鼠标', null, toggle('screenCursor'))),
      h('div', { class: 'card' }, h('h3', {}, icon('grid'), '控制台按钮'),
        h('div', { class: 'row wrap' },
          h('button', { class: 'btn sm', onclick: () => exportConfig().catch(showError) }, icon('share'), '导出'),
          h('button', { class: 'btn sm', onclick: importConfig }, icon('save'), '导入'),
          h('button', {
            class: 'btn sm tinted red', onclick: async () => {
              if (!(await confirmDialog('恢复默认按钮', '将删除所有自定义按钮并恢复默认布局。', { danger: true, okText: '恢复' }))) return;
              try { await api('/api/pages/reset', { body: {} }); await reloadPages(); toast('已恢复默认', 'ok'); } catch (e) { showError(e); }
            },
          }, icon('refresh'), '恢复默认')),
        h('div', { class: 'hint', style: { marginTop: '12px' } }, '每台电脑的按钮分别保存在那台电脑上，切换电脑后显示的是它自己的按钮。')),
      h('div', { class: 'card' }, h('h3', {}, icon('shield'), '安全'),
        row('修改 PIN 码', '用于新设备登录', h('button', {
          class: 'btn sm', onclick: async () => {
            const pin = await promptDialog('新的 PIN 码（4–12 位数字）', '', '例如 246810', { type: 'tel' });
            if (!pin) return;
            try { await api('/api/pin', { body: { pin } }); toast('PIN 已修改', 'ok'); } catch (e) { showError(e); }
          },
        }, '修改')),
        row('注销所有设备', '重置访问令牌，所有设备都需要重新输入 PIN', h('button', {
          class: 'btn sm tinted red', onclick: async () => {
            if (!(await confirmDialog('注销所有设备', '包括当前这台 iPad。', { danger: true, okText: '注销' }))) return;
            try { await api('/api/token/reset', { body: {} }); } catch (e) { showError(e); }
            ws.close(); setToken(''); location.reload();
          },
        }, '注销')),
        row('退出登录', '仅退出这台 iPad', h('button', { class: 'btn sm', onclick: () => { ws.close(); setToken(''); location.reload(); } }, '退出'))),
      aboutCard(),
      h('div', { class: 'card' }, h('h3', {}, icon('fullscreen'), '全屏显示'),
        isStandalone()
          ? h('div', { class: 'muted' }, '已经以主屏幕 App 方式全屏运行。')
          : [
            row('只显示控制台', '添加到主屏幕后像 App 一样全屏打开，没有网址栏', h('button', { class: 'btn sm tinted', onclick: showInstallGuide }, '设置方法')),
            canFullscreen() ? row('轻点自动全屏', '在 Safari 中打开时，第一次轻点页面就进入全屏', toggle('autoFullscreen')) : null,
          ],
        h('div', { class: 'hint', style: { marginTop: '10px' } }, '长时间使用建议接上电源，并在 iPad“设置 → 显示与亮度 → 自动锁定”中延长时间。'))));
  },
};
