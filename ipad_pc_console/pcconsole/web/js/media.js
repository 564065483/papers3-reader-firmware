// 媒体：播放控制、音量与亮度（控制中心风格滑条）、播放器常用快捷键
import { api, isMac, run, state, ws } from './api.js';
import { icon } from './icons.js';
import { bigSlider, h, showError, throttle } from './ui.js';

let vol, muteBtn, bri, briCard, hint, macBri;

function paintVolume(s) {
  if (!s || s.level === null || s.level === undefined) {
    vol.set(0);
    vol.setDisabled(true);
    hint.textContent = '未安装 pycaw，只能按步进调节音量（电脑端执行 pip install pycaw 后可精确调节）。';
    return;
  }
  hint.textContent = '';
  vol.setDisabled(false);
  vol.set(s.level);
  vol.setIcon(s.muted || s.level === 0 ? 'speaker.slash' : 'speaker');
  muteBtn.classList.toggle('on', !!s.muted);
}

async function refresh() {
  try { paintVolume(await api('/api/volume')); } catch (e) { if (e.status !== 501) showError(e); }
  macBri.classList.toggle('hidden', !isMac());
  if (isMac()) { briCard.classList.add('hidden'); return; }
  try {
    const { level } = await api('/api/brightness');
    briCard.classList.toggle('hidden', level === null);
    if (level !== null) bri.set(level);
  } catch { briCard.classList.add('hidden'); }
}

const act = (value) => run([{ type: 'media', value }]).catch(showError);
const volStep = (step) => api('/api/volume', { body: { step } }).then(paintVolume).catch(showError);
const key = (k) => ws.send({ t: 'k', k });

export default {
  id: 'media', title: '媒体与音量', short: '媒体', icon: 'music',
  mount(el) {
    const setVol = throttle((v) => api('/api/volume', { body: { level: v } }).then(paintVolume).catch(showError), 90);
    vol = bigSlider('speaker', setVol);
    hint = h('div', { class: 'hint', style: { marginTop: '8px' } });
    muteBtn = h('button', { class: 'btn icon', onclick: () => api('/api/volume', { body: { mute: null } }).then(paintVolume).catch(showError) }, icon('speaker.slash'));

    const setBri = throttle((v) => api('/api/brightness', { body: { level: v } }).catch(showError), 300);
    bri = bigSlider('sun', setBri);
    briCard = h('div', { class: 'card hidden' }, h('h3', {}, icon('sun'), '屏幕亮度'), bri);
    macBri = h('div', { class: 'row hidden', style: { marginTop: '12px' } },
      h('button', { class: 'btn grow', onclick: () => key('brightnessdown') }, icon('sun'), '调暗'),
      h('button', { class: 'btn grow', onclick: () => key('brightnessup') }, icon('sun'), '调亮'));

    const player = [
      ['space', 'playpause', '播放/暂停'], ['left', 'backward', '后退'], ['right', 'forward', '前进'],
      ['up', 'speaker.plus', '音量 +'], ['down', 'speaker.minus', '音量 −'], ['f', 'fullscreen', '网页全屏'],
      ['esc', 'shrink', '退出全屏'], ['m', 'speaker.slash', '网页静音'], ['j', 'undo', '后退 10 秒'],
      ['l', 'redo', '前进 10 秒'], ['shift+n', 'chevron.right', '下一个视频'], ['c', 'bubble', '字幕'],
    ];
    el.append(h('div', { class: 'cards' },
      h('div', { class: 'card' }, h('h3', {}, icon('music'), '正在播放'),
        h('div', { class: 'now' },
          h('button', { class: 'round', onclick: () => act('prev') }, icon('backward')),
          h('button', { class: 'round big', onclick: () => act('playpause') }, icon('playpause')),
          h('button', { class: 'round', onclick: () => act('next') }, icon('forward'))),
        h('div', { class: 'hint', style: { textAlign: 'center' } }, '控制电脑上正在播放的音乐或视频（系统媒体键）')),
      h('div', { class: 'card' }, h('h3', {}, icon('speaker'), '系统音量'),
        h('div', { class: 'row' }, vol, muteBtn),
        h('div', { class: 'row wrap', style: { marginTop: '12px' } },
          h('button', { class: 'btn grow', onclick: () => volStep(-10) }, icon('speaker.minus'), '−10'),
          h('button', { class: 'btn grow', onclick: () => volStep(10) }, icon('speaker.plus'), '+10')),
        macBri, hint),
      briCard,
      h('div', { class: 'card', style: { gridColumn: '1 / -1' } }, h('h3', {}, icon('film'), '播放器快捷键（发送到当前窗口）'),
        h('div', { class: 'keygrid' }, player.map(([k, ic, desc]) =>
          h('button', { class: 'btn blue', onclick: () => key(k) }, icon(ic), desc))))));
  },
  show() { if (state.info) refresh(); },
};
