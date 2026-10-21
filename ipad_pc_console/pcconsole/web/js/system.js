// 系统：电源、常用系统功能、剪贴板互传、截图、远程命令行（按 Windows / macOS 显示对应功能）
import { api, isMac, run, withToken } from './api.js';
import { icon } from './icons.js';
import { confirmDialog, copyText, h, modal, sheet, showError, toast } from './ui.js';

const sys = (value, delay = 0) => run([{ type: 'system', value, delay }]);

async function power(value, text, timed) {
  let delay = 0;
  if (timed) {
    const choice = await sheet(text, [
      { label: '立即', icon: 'bolt', value: 0 }, { label: '1 分钟后', icon: 'clock', value: 60 },
      { label: '10 分钟后', icon: 'clock', value: 600 }, { label: '30 分钟后', icon: 'clock', value: 1800 },
      { label: '1 小时后', icon: 'clock', value: 3600 }, { label: '2 小时后', icon: 'clock', value: 7200 },
    ]);
    if (choice === null || choice === undefined) return;
    delay = choice;
  }
  if (!(await confirmDialog(text, delay ? `电脑将在 ${Math.round(delay / 60)} 分钟后${text}，可随时点“取消关机”。` : `确定立即${text}？未保存的工作可能丢失。`, { danger: true, okText: text }))) return;
  try { await sys(value, delay); toast(delay ? `已计划${text}` : `正在${text}…`, 'ok'); } catch (e) { showError(e); }
}

function tileBtn(iconName, text, onclick, cls = 'blue') {
  return h('button', { class: `btn ${cls}`, onclick }, icon(iconName), text);
}
const quick = (iconName, text, value) => tileBtn(iconName, text, () => sys(value).then(() => toast(text, 'ok')).catch(showError));

function clipboardCard() {
  const ta = h('textarea', { class: 'input', placeholder: '在这里输入或粘贴文字，可发送到电脑剪贴板；也可以读取电脑剪贴板的内容' });
  const call = async (fn, msg) => { try { await fn(); toast(msg, 'ok'); } catch (e) { showError(e); } };
  return h('div', { class: 'card' }, h('h3', {}, icon('clipboard'), '剪贴板互传'), ta,
    h('div', { class: 'row wrap', style: { marginTop: '12px' } },
      h('button', { class: 'btn sm', onclick: () => call(async () => { ta.value = (await api('/api/clipboard')).text; }, '已读取电脑剪贴板') }, icon('download'), '读取电脑'),
      h('button', { class: 'btn sm', onclick: () => call(() => api('/api/clipboard', { body: { text: ta.value } }), '已复制到电脑') }, icon('upload'), '复制到电脑'),
      h('button', { class: 'btn sm tinted', onclick: () => call(() => api('/api/clipboard', { body: { text: ta.value, paste: true } }), '已粘贴到电脑当前窗口') }, icon('paperplane'), '发送并粘贴'),
      h('button', { class: 'btn sm', onclick: async () => toast((await copyText(ta.value)) ? '已复制到 iPad' : '复制失败，请手动长按选择', 'ok') }, icon('copy'), '复制到 iPad')));
}

function terminalCard() {
  const mac = isMac();
  const out = h('div', { class: 'term' }, h('span', { style: { opacity: '.6' } }, '命令在电脑上执行，输出显示在这里，支持 cd 切换目录。\n'));
  const shells = mac ? [['zsh', 'zsh'], ['bash', 'bash']] : [['cmd', 'CMD'], ['powershell', 'PowerShell']];
  const shellSel = h('select', { class: 'input sm' }, shells.map(([v, t]) => h('option', { value: v }, t)));
  const input = h('input', { class: 'input', placeholder: mac ? '输入命令后回车，例如 ls -la' : '输入命令后回车，例如 ipconfig', autocapitalize: 'off', autocorrect: 'off', spellcheck: false, style: { borderRadius: '999px' } });
  const cwdEl = h('div', { class: 'small muted ellipsis', style: { marginTop: '8px' } }, '');
  let cwd = '', history = [], hi = 0, busy = false;
  const print = (text, cls = '') => { out.append(h('span', { class: cls }, text)); out.scrollTop = out.scrollHeight; };
  const exec = async (cmd) => {
    if (!cmd.trim() || busy) return;
    if (cmd.trim() === 'cls' || cmd.trim() === 'clear') { out.replaceChildren(); return; }
    history.push(cmd); hi = history.length; busy = true;
    print(`\n${cwd || '~'} ❯ ${cmd}\n`, 'cmd');
    try {
      const r = await api('/api/command', { body: { cmd, shell: shellSel.value, cwd }, timeout: 120000 });
      if (r.stdout) print(r.stdout);
      if (r.stderr) print(r.stderr, 'err');
      if (r.code) print(`[退出码 ${r.code}]\n`, 'err');
      cwd = r.cwd; cwdEl.textContent = `当前目录：${cwd}`;
    } catch (e) { print(`${e.message}\n`, 'err'); }
    busy = false;
  };
  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') { exec(input.value); input.value = ''; }
    else if (e.key === 'ArrowUp' && hi > 0) { input.value = history[--hi]; e.preventDefault(); }
    else if (e.key === 'ArrowDown' && hi < history.length) { input.value = history[++hi] || ''; e.preventDefault(); }
  });
  const chips = mac
    ? ['ls -la', 'df -h', 'top -l 1 | head -12', 'ifconfig | grep inet', 'sw_vers', 'pmset -g batt', 'uptime', 'whoami']
    : ['ipconfig', 'systeminfo', 'tasklist', 'dir', 'ping -n 4 www.baidu.com', 'netstat -an', 'whoami', 'wmic cpu get name'];
  return h('div', { class: 'card', style: { gridColumn: '1 / -1' } }, h('h3', {}, icon('terminal'), '远程命令行'),
    h('div', { class: 'row wrap', style: { marginBottom: '12px' } }, chips.map((c) => h('button', { class: 'btn sm', onclick: () => exec(c) }, c))),
    out, cwdEl,
    h('div', { class: 'row', style: { marginTop: '10px' } }, shellSel, input, h('button', { class: 'btn primary', onclick: () => { exec(input.value); input.value = ''; } }, icon('play'), '运行')));
}

function screenshot() {
  const url = withToken(`/api/screen/frame?w=0&q=90&t=${Date.now()}`);
  modal({
    title: '电脑截图', wide: true,
    body: h('img', { src: url, style: { width: '100%', borderRadius: '16px' } }),
    buttons: [{ label: '在新窗口打开（可长按保存）', onClick: () => { window.open(url, '_blank'); return false; } }, { label: '完成', kind: 'primary', value: null }],
  });
}

export default {
  id: 'system', title: '系统', icon: 'gear',
  mount(el) {
    const mac = isMac();
    const powerBtns = [
      tileBtn('lock', mac ? '锁定屏幕' : '锁定', () => sys('lock').then(() => toast('已锁定', 'ok')).catch(showError)),
      tileBtn('moon', '睡眠', () => power('sleep', '睡眠'), 'orange'),
      mac ? null : tileBtn('zzz', '休眠', () => power('hibernate', '休眠'), 'orange'),
      tileBtn('logout', mac ? '退出登录' : '注销', () => power('logoff', mac ? '退出登录' : '注销')),
      tileBtn('restart', mac ? '重新启动' : '重启', () => power('restart', mac ? '重新启动' : '重启', true), 'orange'),
      tileBtn('power', '关机', () => power('shutdown', '关机', true), 'red'),
      tileBtn('xcircle', '取消关机', () => sys('cancel_shutdown').then(() => toast('已取消计划的关机/重启', 'ok')).catch(showError)),
      tileBtn('display.slash', '关闭显示器', () => sys('monitor_off').catch(showError)),
    ];
    const quickBtns = mac ? [
      quick('display', '显示桌面', 'show_desktop'), quick('taskview', '调度中心', 'task_view'),
      quick('macwindow', '应用窗口', 'app_windows'), quick('chart', '活动监视器', 'task_manager'),
      quick('folder', '访达', 'explorer'), quick('gear', '系统设置', 'settings'),
      quick('search', '聚焦搜索', 'spotlight'), quick('scissors', '截屏工具', 'snip'),
      quick('smile', '表情与符号', 'emoji_panel'), quick('shuffle', '切换应用', 'switch_window'),
      quick('arrow.left', '左侧桌面', 'desktop_left'), quick('arrow.right', '右侧桌面', 'desktop_right'),
      quick('xmark', '关闭窗口', 'close_window'), quick('xcircle', '退出应用', 'quit_app'),
      quick('bolt', '强制退出', 'force_quit'),
    ] : [
      quick('display', '显示桌面', 'show_desktop'), quick('taskview', '任务视图', 'task_view'),
      quick('chart', '任务管理器', 'task_manager'), quick('folder', '资源管理器', 'explorer'),
      quick('gear', '设置', 'settings'), quick('play', '运行', 'run_dialog'),
      quick('scissors', '截图工具', 'snip'), quick('bell', '通知中心', 'action_center'),
      quick('smile', '表情面板', 'emoji_panel'), quick('minus', '最小化全部', 'minimize_all'),
      quick('arrow.left', '左侧桌面', 'desktop_left'), quick('arrow.right', '右侧桌面', 'desktop_right'),
      quick('plus', '新建桌面', 'new_desktop'), quick('xmark', '关闭窗口', 'close_window'),
    ];
    quickBtns.push(tileBtn('camera', '查看截图', screenshot));
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, icon('power'), '电源'), h('div', { class: 'keygrid' }, powerBtns)),
      h('div', { class: 'card' }, h('h3', {}, icon('sliders'), '系统功能'), h('div', { class: 'keygrid' }, quickBtns)),
      clipboardCard(),
      terminalCard()));
  },
};
