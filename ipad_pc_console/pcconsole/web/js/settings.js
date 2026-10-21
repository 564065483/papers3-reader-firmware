// 设置：操作手感、外观、按钮配置导入导出、安全
import { api, applyTheme, bus, prefs, savePrefs, setToken, state, ws } from './api.js';
import { confirmDialog, copyText, fmtBytes, h, modal, paintRange, promptDialog, showError, switchEl, toast } from './ui.js';
import { reloadPages } from './dashboard.js';

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
  const el = h('div', { class: 'seg' });
  const paint = () => el.replaceChildren(...options.map(([v, label]) => h('button', {
    class: prefs[key] === v ? 'on' : '', onclick: () => { prefs[key] = v; savePrefs(); paint(); after?.(); },
  }, label)));
  paint();
  return el;
}

async function exportConfig() {
  const { pages } = await api('/api/pages');
  const text = JSON.stringify(pages, null, 2);
  const ta = h('textarea', { class: 'input', style: { minHeight: '360px' } });
  ta.value = text;
  modal({
    title: '导出按钮配置', wide: true, body: ta,
    buttons: [{ label: '复制', onClick: async () => { toast((await copyText(ta.value)) ? '已复制' : '复制失败', 'ok'); return false; } }, { label: '关闭', kind: 'primary', value: null }],
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
    const yes = (b) => (b ? '✅ 可用' : '❌ 不可用');
    box.replaceChildren(
      h('span', {}, '电脑名称'), h('span', {}, i.hostname),
      h('span', {}, '用户'), h('span', {}, i.user),
      h('span', {}, '系统'), h('span', {}, i.os),
      h('span', {}, '处理器'), h('span', {}, `${i.cpu}（${i.cores} 线程）`),
      h('span', {}, '内存'), h('span', {}, fmtBytes(i.memory)),
      h('span', {}, '访问地址'), h('span', {}, i.addresses.map((a) => `http://${a}:${i.port}`).join('\n')),
      h('span', {}, 'Windows 控制'), h('span', {}, yes(i.windows)),
      h('span', {}, '精确音量'), h('span', {}, i.precise_volume ? '✅ 可用' : '❌ 未安装 pycaw（仅步进）'),
      h('span', {}, '服务端版本'), h('span', {}, i.version));
  };
  paint(state.info);
  bus.on('info', paint);
  return h('div', { class: 'card' }, h('h3', {}, 'ℹ️ 关于这台电脑'), box);
}

export default {
  id: 'settings', title: '设置', icon: '🔧',
  mount(el) {
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, '🖱️ 触控板'),
        row('指针灵敏度', null, range('sensitivity', 0.4, 4, 0.1)),
        row('指针加速', '快速滑动时移动更远', range('accel', 0, 2, 0.1)),
        row('滚动速度', null, range('scrollSpeed', 0.2, 3, 0.1)),
        row('自然滚动', '与 iPad 一致：手指向上，内容向上', toggle('naturalScroll')),
        row('⌘ 当作 Ctrl', '外接键盘时 ⌘C/⌘V 对应电脑 Ctrl+C/Ctrl+V', toggle('cmdAsCtrl'))),
      h('div', { class: 'card' }, h('h3', {}, '🎨 外观'),
        row('主题', null, seg('theme', [['dark', '深色'], ['light', '浅色'], ['auto', '跟随系统']], applyTheme)),
        row('按钮大小', '控制台按钮尺寸', seg('tile', [['s', '小'], ['m', '中'], ['l', '大']], applyTheme)),
        row('远程画面显示鼠标', null, toggle('screenCursor'))),
      h('div', { class: 'card' }, h('h3', {}, '🎛️ 控制台按钮'),
        h('div', { class: 'row wrap' },
          h('button', { class: 'btn', onclick: () => exportConfig().catch(showError) }, '📤 导出配置'),
          h('button', { class: 'btn', onclick: importConfig }, '📥 导入配置'),
          h('button', {
            class: 'btn danger', onclick: async () => {
              if (!(await confirmDialog('恢复默认按钮', '将删除所有自定义按钮并恢复默认布局。', { danger: true, okText: '恢复' }))) return;
              try { await api('/api/pages/reset', { body: {} }); await reloadPages(); toast('已恢复默认', 'ok'); } catch (e) { showError(e); }
            },
          }, '♻️ 恢复默认')),
        h('div', { class: 'hint', style: { marginTop: '10px' } }, '按钮配置保存在电脑上（%APPDATA%\\iPadPCConsole\\config.json），换 iPad 登录也能看到。')),
      h('div', { class: 'card' }, h('h3', {}, '🔐 安全'),
        row('修改 PIN 码', '用于新设备登录', h('button', {
          class: 'btn sm', onclick: async () => {
            const pin = await promptDialog('新的 PIN 码（4–12 位数字）', '', '例如 246810');
            if (!pin) return;
            try { await api('/api/pin', { body: { pin } }); toast('PIN 已修改', 'ok'); } catch (e) { showError(e); }
          },
        }, '修改')),
        row('注销所有设备', '重置访问令牌，所有已登录的设备都需要重新输入 PIN', h('button', {
          class: 'btn sm danger', onclick: async () => {
            if (!(await confirmDialog('注销所有设备', '包括当前这台 iPad。', { danger: true, okText: '注销' }))) return;
            try { await api('/api/token/reset', { body: {} }); } catch (e) { showError(e); }
            ws.close(); setToken(''); location.reload();
          },
        }, '注销')),
        row('退出登录', '仅退出这台 iPad', h('button', { class: 'btn sm', onclick: () => { ws.close(); setToken(''); location.reload(); } }, '退出'))),
      aboutCard(),
      h('div', { class: 'card' }, h('h3', {}, '📱 添加到主屏幕'),
        h('div', { class: 'muted', style: { lineHeight: 1.8 } },
          '在 Safari 中点击地址栏右侧的“分享”按钮 → “添加到主屏幕”，之后从主屏幕图标打开即可全屏使用，就像原生 App 一样。',
          h('br'), '建议在 iPad“设置 → 显示与亮度 → 自动锁定”中延长时间，或在使用时接上电源。'))));
  },
};
