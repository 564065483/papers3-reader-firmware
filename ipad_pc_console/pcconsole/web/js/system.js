// 系统：电源、常用系统功能、剪贴板互传、截图、远程命令行
import { api, run, withToken } from './api.js';
import { confirmDialog, copyText, h, modal, sheet, showError, toast } from './ui.js';

const sys = (value, delay = 0) => run([{ type: 'system', value, delay }]);

async function power(value, label, timed) {
  let delay = 0;
  if (timed) {
    const choice = await sheet(`${label}`, [
      { label: '立即', value: 0 }, { label: '1 分钟后', value: 60 }, { label: '10 分钟后', value: 600 },
      { label: '30 分钟后', value: 1800 }, { label: '1 小时后', value: 3600 }, { label: '2 小时后', value: 7200 },
    ]);
    if (choice === null || choice === undefined) return;
    delay = choice;
  }
  if (!(await confirmDialog(label, delay ? `电脑将在 ${Math.round(delay / 60)} 分钟后${label}，可随时点“取消关机/重启”。` : `确定立即${label}？未保存的工作可能丢失。`, { danger: true, okText: label }))) return;
  try { await sys(value, delay); toast(delay ? `已计划${label}` : `正在${label}…`, 'ok'); } catch (e) { showError(e); }
}

function quick(icon, label, value) {
  return h('button', { class: 'btn', onclick: () => sys(value).then(() => toast(label, 'ok')).catch(showError) }, h('b', {}, icon), label);
}

function clipboardCard() {
  const ta = h('textarea', { class: 'input', placeholder: '在这里输入或粘贴文字，可发送到电脑剪贴板；也可以读取电脑剪贴板内容到这里' });
  return h('div', { class: 'card' }, h('h3', {}, '📋 剪贴板互传'), ta,
    h('div', { class: 'row wrap', style: { marginTop: '10px' } },
      h('button', { class: 'btn', onclick: async () => { try { ta.value = (await api('/api/clipboard')).text; toast('已读取电脑剪贴板', 'ok'); } catch (e) { showError(e); } } }, '⬇️ 读取电脑剪贴板'),
      h('button', { class: 'btn', onclick: async () => { try { await api('/api/clipboard', { body: { text: ta.value } }); toast('已复制到电脑', 'ok'); } catch (e) { showError(e); } } }, '⬆️ 复制到电脑'),
      h('button', { class: 'btn', onclick: async () => { try { await api('/api/clipboard', { body: { text: ta.value, paste: true } }); toast('已粘贴到电脑当前窗口', 'ok'); } catch (e) { showError(e); } } }, '📌 发送并粘贴'),
      h('button', { class: 'btn', onclick: async () => toast((await copyText(ta.value)) ? '已复制到 iPad 剪贴板' : '复制失败，请手动长按选择', 'ok') }, '📱 复制到 iPad')));
}

function terminalCard() {
  const out = h('div', { class: 'term' }, h('span', { class: 'muted' }, '远程命令行：命令在电脑上执行，输出显示在这里。支持 cd 切换目录。\n'));
  const shellSel = h('select', { class: 'input', style: { width: '150px' } }, h('option', { value: 'cmd' }, 'CMD'), h('option', { value: 'powershell' }, 'PowerShell'));
  const input = h('input', { class: 'input', placeholder: '输入命令后回车，例如 ipconfig', autocapitalize: 'off', autocorrect: 'off', spellcheck: false });
  const cwdEl = h('div', { class: 'small muted ellipsis' }, '');
  let cwd = '', history = [], hi = 0, busy = false;
  const print = (text, cls = '') => { out.append(h('span', { class: cls }, text)); out.scrollTop = out.scrollHeight; };
  const exec = async (cmd) => {
    if (!cmd.trim() || busy) return;
    if (cmd.trim() === 'cls' || cmd.trim() === 'clear') { out.replaceChildren(); return; }
    history.push(cmd); hi = history.length; busy = true;
    print(`\n${cwd || '~'}> ${cmd}\n`, 'cmd');
    try {
      const r = await api('/api/command', { body: { cmd, shell: shellSel.value, cwd } });
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
  const chips = ['ipconfig', 'systeminfo', 'tasklist', 'dir', 'ping -n 4 www.baidu.com', 'netstat -an', 'whoami', 'wmic cpu get name'];
  return h('div', { class: 'card', style: { gridColumn: '1 / -1' } }, h('h3', {}, '⌨️ 远程命令行'),
    h('div', { class: 'row wrap', style: { marginBottom: '10px' } }, chips.map((c) => h('button', { class: 'btn sm', onclick: () => exec(c) }, c))),
    out, cwdEl,
    h('div', { class: 'row', style: { marginTop: '10px' } }, shellSel, input, h('button', { class: 'btn primary', onclick: () => { exec(input.value); input.value = ''; } }, '运行')));
}

function screenshot() {
  const url = withToken(`/api/screen/frame?w=0&q=90&t=${Date.now()}`);
  modal({
    title: '电脑截图', wide: true,
    body: h('img', { src: url, style: { width: '100%', borderRadius: '12px' } }),
    buttons: [{ label: '在新窗口打开（可长按保存）', onClick: () => { window.open(url, '_blank'); return false; } }, { label: '关闭', kind: 'primary', value: null }],
  });
}

export default {
  id: 'system', title: '系统', icon: '⚙️',
  mount(el) {
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, '⏻ 电源'),
        h('div', { class: 'keygrid' },
          h('button', { class: 'btn', onclick: () => sys('lock').then(() => toast('已锁定', 'ok')).catch(showError) }, h('b', {}, '🔒'), '锁定'),
          h('button', { class: 'btn', onclick: () => power('sleep', '睡眠') }, h('b', {}, '🌙'), '睡眠'),
          h('button', { class: 'btn', onclick: () => power('hibernate', '休眠') }, h('b', {}, '💤'), '休眠'),
          h('button', { class: 'btn', onclick: () => power('logoff', '注销') }, h('b', {}, '🚪'), '注销'),
          h('button', { class: 'btn', onclick: () => power('restart', '重启', true) }, h('b', {}, '🔄'), '重启'),
          h('button', { class: 'btn', onclick: () => power('shutdown', '关机', true) }, h('b', {}, '⏻'), '关机'),
          h('button', { class: 'btn', onclick: () => sys('cancel_shutdown').then(() => toast('已取消计划的关机/重启', 'ok')).catch(showError) }, h('b', {}, '✋'), '取消关机'),
          h('button', { class: 'btn', onclick: () => sys('monitor_off').catch(showError) }, h('b', {}, '🌑'), '关闭显示器'))),
      h('div', { class: 'card' }, h('h3', {}, '🧰 系统功能'),
        h('div', { class: 'keygrid' },
          quick('🖥️', '显示桌面', 'show_desktop'), quick('🪟', '任务视图', 'task_view'),
          quick('📊', '任务管理器', 'task_manager'), quick('📁', '资源管理器', 'explorer'),
          quick('⚙️', '设置', 'settings'), quick('▶️', '运行', 'run_dialog'),
          quick('✂️', '截图工具', 'snip'), quick('🔔', '通知中心', 'action_center'),
          quick('😀', '表情面板', 'emoji_panel'), quick('⬇️', '最小化全部', 'minimize_all'),
          quick('⬅️', '左侧桌面', 'desktop_left'), quick('➡️', '右侧桌面', 'desktop_right'),
          quick('➕', '新建桌面', 'new_desktop'), quick('❌', '关闭窗口', 'close_window'),
          h('button', { class: 'btn', onclick: screenshot }, h('b', {}, '📸'), '查看截图'))),
      clipboardCard(),
      terminalCard()));
  },
};
