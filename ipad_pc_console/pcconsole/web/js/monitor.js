// 性能监控：CPU / 内存 / 网络 / 磁盘 / 电池，以及进程管理
import { api, bus } from './api.js';
import { icon } from './icons.js';
import { confirmDialog, fmtBytes, fmtDuration, fmtRate, h, sheet, showError, toast } from './ui.js';

const HISTORY = 60;
const hist = { cpu: [], mem: [], down: [], up: [] };
let els = {}, visible = false, procTimer = null, procs = [], sortKey = 'cpu', filter = '';

function push(arr, v) { arr.push(v); if (arr.length > HISTORY) arr.shift(); }

function spark(canvas, series, colors, maxFixed) {
  const dpr = devicePixelRatio || 1;
  const w = canvas.clientWidth, hgt = canvas.clientHeight;
  if (!w) return;
  if (canvas.width !== w * dpr) { canvas.width = w * dpr; canvas.height = hgt * dpr; }
  const ctx = canvas.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, hgt);
  const max = maxFixed || Math.max(1, ...series.flat());
  series.forEach((data, i) => {
    if (data.length < 2) return;
    ctx.beginPath();
    data.forEach((v, j) => {
      const x = (j / (HISTORY - 1)) * w, y = hgt - (v / max) * (hgt - 4) - 2;
      j ? ctx.lineTo(x, y) : ctx.moveTo(x, y);
    });
    ctx.strokeStyle = colors[i]; ctx.lineWidth = 2.2; ctx.lineJoin = 'round'; ctx.stroke();
    ctx.lineTo(((data.length - 1) / (HISTORY - 1)) * w, hgt); ctx.lineTo(0, hgt); ctx.closePath();
    ctx.fillStyle = colors[i] + '22'; ctx.fill();
  });
}

const level = (p) => (p > 90 ? 'bad' : p > 70 ? 'warn' : '');
const bar = (p) => h('div', { class: 'bar' }, h('i', { class: level(p), style: { width: `${Math.min(100, p)}%` } }));

function onStats(s) {
  push(hist.cpu, s.cpu); push(hist.mem, s.mem.percent); push(hist.down, s.net.down); push(hist.up, s.net.up);
  if (!visible) return;
  els.cpu.textContent = `${Math.round(s.cpu)}%`;
  els.cpuSub.textContent = `${s.cpu_per.length} 线程${s.cpu_freq ? ` · ${(s.cpu_freq / 1000).toFixed(2)} GHz` : ''} · ${s.procs} 个进程`;
  els.cores.replaceChildren(...s.cpu_per.map((p) => h('i', { style: { height: `${Math.max(3, p)}%` } })));
  els.mem.textContent = `${Math.round(s.mem.percent)}%`;
  els.memSub.textContent = `${fmtBytes(s.mem.used)} / ${fmtBytes(s.mem.total)}${s.swap.total ? ` · 虚拟内存 ${Math.round(s.swap.percent)}%` : ''}`;
  els.down.textContent = fmtRate(s.net.down);
  els.up.textContent = fmtRate(s.net.up);
  els.netSub.textContent = `累计 ↓ ${fmtBytes(s.net.recv)} · ↑ ${fmtBytes(s.net.sent)}`;
  els.disks.replaceChildren(...s.disks.map((d) => h('div', { style: { marginBottom: '10px' } },
    h('div', { class: 'row small' }, h('b', {}, d.mount), h('span', { class: 'muted grow' }, d.fs),
      h('span', {}, `${fmtBytes(d.used)} / ${fmtBytes(d.total)}`)), bar(d.percent))));
  els.sys.replaceChildren(
    h('span', {}, '已运行'), h('span', {}, fmtDuration(s.uptime)),
    h('span', {}, '电池'), h('span', {}, s.battery ? `${s.battery.percent}% ${s.battery.plugged ? '（充电中）' : s.battery.secs ? `（剩余 ${fmtDuration(s.battery.secs)}）` : ''}` : '台式机 / 无电池'));
  spark(els.cpuC, [hist.cpu], ['#0a84ff'], 100);
  spark(els.memC, [hist.mem], ['#bf5af2'], 100);
  spark(els.netC, [hist.down, hist.up], ['#30d158', '#ff9f0a']);
}

async function loadProcs() {
  try {
    procs = (await api('/api/processes')).processes;
    renderProcs();
  } catch (e) { els.ptable.replaceChildren(h('tr', {}, h('td', {}, e.message))); }
}

function renderProcs() {
  const q = filter.toLowerCase();
  const list = procs.filter((p) => !q || p.name.toLowerCase().includes(q) || String(p.pid) === q)
    .sort((a, b) => (sortKey === 'name' ? a.name.localeCompare(b.name) : b[sortKey] - a[sortKey]))
    .slice(0, 150);
  els.ptable.replaceChildren(
    h('tr', {},
      h('th', { onclick: () => { sortKey = 'name'; renderProcs(); } }, `进程${sortKey === 'name' ? ' ▲' : ''}`),
      h('th', { class: 'n' }, 'PID'),
      h('th', { class: 'n', onclick: () => { sortKey = 'cpu'; renderProcs(); } }, `CPU${sortKey === 'cpu' ? ' ▼' : ''}`),
      h('th', { class: 'n', onclick: () => { sortKey = 'mem'; renderProcs(); } }, `内存${sortKey === 'mem' ? ' ▼' : ''}`),
      h('th', {}, '用户'), h('th', {})),
    ...list.map((p) => h('tr', {},
      h('td', {}, p.name), h('td', { class: 'n muted' }, p.pid), h('td', { class: 'n' }, `${p.cpu.toFixed(1)}%`),
      h('td', { class: 'n' }, fmtBytes(p.mem)), h('td', { class: 'muted small' }, p.user),
      h('td', {}, h('button', { class: 'btn sm tinted red', onclick: () => killMenu(p) }, '结束')))));
}

async function killMenu(p) {
  const choice = await sheet(`${p.name}（PID ${p.pid}）`, [
    { label: '结束进程', icon: 'xcircle', value: 'term' },
    { label: '强制结束', icon: 'bolt', value: 'kill', danger: true },
  ]);
  if (!choice) return;
  if (!(await confirmDialog('结束进程', `确定${choice === 'kill' ? '强制' : ''}结束 ${p.name}？未保存的数据可能丢失。`, { danger: true, okText: '结束' }))) return;
  try {
    await api('/api/processes/kill', { body: { pid: p.pid, force: choice === 'kill' } });
    toast('已结束', 'ok');
    setTimeout(loadProcs, 500);
  } catch (e) { showError(e); }
}

export default {
  id: 'monitor', title: '性能监控', short: '监控', icon: 'pulse',
  mount(el) {
    const c = () => h('canvas', { class: 'spark' });
    els = {
      cpu: h('b'), cpuSub: h('div', { class: 'small muted' }), cores: h('div', { class: 'cores' }), cpuC: c(),
      mem: h('b'), memSub: h('div', { class: 'small muted' }), memC: c(),
      down: h('b', { style: { color: '#30d158' } }), up: h('b', { style: { color: '#ff9f0a', fontSize: '22px' } }),
      netSub: h('div', { class: 'small muted' }), netC: c(),
      disks: h('div'), sys: h('div', { class: 'kv' }), ptable: h('table', { class: 'ptable' }),
    };
    const search = h('input', { class: 'input', placeholder: '搜索进程名或 PID', style: { maxWidth: '320px', borderRadius: '999px' }, oninput: () => { filter = search.value; renderProcs(); } });
    el.append(
      h('div', { class: 'cards' },
        h('div', { class: 'card' }, h('h3', {}, icon('cpu'), 'CPU'), h('div', { class: 'gauge' }, els.cpu), els.cpuSub, els.cpuC, els.cores),
        h('div', { class: 'card' }, h('h3', {}, icon('memory'), '内存'), h('div', { class: 'gauge' }, els.mem), els.memSub, els.memC),
        h('div', { class: 'card' }, h('h3', {}, icon('network'), '网络'), h('div', { class: 'gauge' }, '↓', els.down, '↑', els.up), els.netSub, els.netC),
        h('div', { class: 'card' }, h('h3', {}, icon('disk'), '磁盘'), els.disks),
        h('div', { class: 'card' }, h('h3', {}, icon('info'), '系统'), els.sys)),
      h('div', { class: 'card', style: { marginTop: '16px' } },
        h('div', { class: 'toolbar' }, h('h3', { style: { margin: 0 } }, icon('list'), '进程'), h('div', { class: 'grow' }), search,
          h('button', { class: 'btn sm icon', onclick: loadProcs }, icon('refresh'))),
        h('div', { style: { overflowX: 'auto' } }, els.ptable)));
    bus.on('stats', onStats);
  },
  show() {
    visible = true;
    api('/api/stats').then(onStats).catch(showError);
    loadProcs();
    procTimer = setInterval(loadProcs, 4000);
  },
  hide() { visible = false; clearInterval(procTimer); },
};
