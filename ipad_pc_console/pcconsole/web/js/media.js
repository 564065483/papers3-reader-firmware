// 媒体：播放控制、精确音量、屏幕亮度、播放器常用快捷键
import { api, run, state, ws } from './api.js';
import { h, paintRange, showError, throttle } from './ui.js';

let volRange, volNum, muteBtn, briRange, briNum, briCard, hint;

function paintVolume(s) {
  if (!s || s.level === null || s.level === undefined) {
    volNum.textContent = '--';
    hint.textContent = '未安装 pycaw，音量只能按步进调节（电脑端执行 pip install pycaw 后可精确调节）。';
    volRange.disabled = true;
    return;
  }
  hint.textContent = '';
  volRange.disabled = false;
  volRange.value = s.level;
  paintRange(volRange);
  volNum.textContent = s.muted ? '🔇' : `${s.level}`;
  muteBtn.classList.toggle('on', !!s.muted);
}

async function refresh() {
  try { paintVolume(await api('/api/volume')); } catch (e) { if (e.status !== 501) showError(e); }
  try {
    const { level } = await api('/api/brightness');
    briCard.classList.toggle('hidden', level === null);
    if (level !== null) { briRange.value = level; briNum.textContent = level; paintRange(briRange); }
  } catch { briCard.classList.add('hidden'); }
}

const act = (value) => run([{ type: 'media', value }]).catch(showError);
const volStep = (step) => api('/api/volume', { body: { step } }).then(paintVolume).catch(showError);
const key = (k) => ws.send({ t: 'k', k });

export default {
  id: 'media', title: '媒体与音量', short: '媒体', icon: '🎵',
  mount(el) {
    volRange = h('input', { type: 'range', min: 0, max: 100, step: 1, value: 50 });
    volNum = h('div', { class: 'vol-num' }, '--');
    hint = h('div', { class: 'hint' });
    const setVol = throttle((v) => api('/api/volume', { body: { level: v } }).then(paintVolume).catch(showError), 90);
    volRange.addEventListener('input', () => { paintRange(volRange); volNum.textContent = volRange.value; setVol(Number(volRange.value)); });
    muteBtn = h('button', { class: 'btn', onclick: () => api('/api/volume', { body: { mute: null } }).then(paintVolume).catch(showError) }, '🔇 静音');

    briRange = h('input', { type: 'range', min: 0, max: 100, step: 1, value: 50 });
    briNum = h('div', { class: 'vol-num' }, '--');
    const setBri = throttle((v) => api('/api/brightness', { body: { level: v } }).catch(showError), 300);
    briRange.addEventListener('input', () => { paintRange(briRange); briNum.textContent = briRange.value; setBri(Number(briRange.value)); });
    briCard = h('div', { class: 'card hidden' }, h('h3', {}, '☀️ 屏幕亮度'), h('div', { class: 'row' }, briNum, briRange));

    const player = [
      ['space', '␣', '播放/暂停'], ['left', '←', '后退'], ['right', '→', '前进'], ['up', '↑', '音量+'], ['down', '↓', '音量−'],
      ['f', 'F', '全屏(网页)'], ['f11', 'F11', '全屏'], ['esc', 'Esc', '退出全屏'], ['m', 'M', '静音(网页)'],
      ['shift+p', '⇧P', '上一个视频'], ['shift+n', '⇧N', '下一个视频'], ['j', 'J', '后退10秒'], ['l', 'L', '前进10秒'],
      ['k', 'K', '暂停(YouTube)'], ['c', 'C', '字幕(YouTube)'],
    ];
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, '▶️ 播放控制'),
        h('div', { class: 'media-main' },
          h('button', { class: 'round', onclick: () => act('prev') }, '⏮'),
          h('button', { class: 'round big', onclick: () => act('playpause') }, '⏯'),
          h('button', { class: 'round', onclick: () => act('next') }, '⏭')),
        h('div', { class: 'row', style: { justifyContent: 'center', marginTop: '10px' } },
          h('button', { class: 'btn', onclick: () => act('stop') }, '⏹ 停止'))),
      h('div', { class: 'card' }, h('h3', {}, '🔊 系统音量'),
        h('div', { class: 'row' }, volNum, volRange),
        h('div', { class: 'row wrap', style: { marginTop: '10px' } },
          h('button', { class: 'btn', onclick: () => volStep(-10) }, '−10'),
          h('button', { class: 'btn', onclick: () => volStep(-2) }, '−'),
          h('button', { class: 'btn', onclick: () => volStep(2) }, '＋'),
          h('button', { class: 'btn', onclick: () => volStep(10) }, '+10'),
          muteBtn),
        hint),
      briCard,
      h('div', { class: 'card', style: { gridColumn: '1 / -1' } }, h('h3', {}, '🎬 播放器快捷键（发送到当前窗口）'),
        h('div', { class: 'keygrid' }, player.map(([k, label, desc]) =>
          h('button', { class: 'btn', onclick: () => key(k) }, h('b', {}, label), desc))))));
  },
  show() { if (state.info) refresh(); },
};
