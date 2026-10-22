// 线性图标库（24×24，风格参考 SF Symbols：圆角端点、1.8 线宽）
const dot = (x, y) => `<path d="M${x} ${y}h.01" stroke-width="2.6"/>`;
const fill = 'fill="currentColor" stroke="none"';

export const ICONS = {
  // 导航
  grid: '<rect x="3.5" y="3.5" width="7" height="7" rx="2"/><rect x="13.5" y="3.5" width="7" height="7" rx="2"/><rect x="3.5" y="13.5" width="7" height="7" rx="2"/><rect x="13.5" y="13.5" width="7" height="7" rx="2"/>',
  cursor: '<path d="M5.5 3.5v15.2l4.1-3.9 2.7 5.7 2.6-1.2-2.7-5.6h5.6z"/>',
  touchpad: '<rect x="3" y="4" width="18" height="16" rx="3"/><path d="M3 15h18M12 15v5"/>',
  display: '<rect x="2.5" y="4" width="19" height="12.5" rx="2"/><path d="M8.5 20.5h7M12 16.5v4"/>',
  'display.slash': '<rect x="2.5" y="4" width="19" height="12.5" rx="2"/><path d="M8.5 20.5h7M12 16.5v4M4 2.5l16 16"/>',
  laptop: '<rect x="4.5" y="5" width="15" height="10.5" rx="1.6"/><path d="M2.5 19h19"/>',
  desktop: '<rect x="2.5" y="3.5" width="19" height="13" rx="2"/><path d="M9 20.5h6M10.5 16.5l-.5 4M13.5 16.5l.5 4"/>',
  music: '<path d="M9 18V5.5l11-2v12.5"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/>',
  app: '<rect x="3.5" y="3.5" width="7" height="7" rx="2"/><rect x="13.5" y="3.5" width="7" height="7" rx="2"/><rect x="3.5" y="13.5" width="7" height="7" rx="2"/><circle cx="17" cy="17" r="3.5"/>',
  folder: '<path d="M3 7.5a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
  pulse: '<path d="M2.5 12h4l2.5-6 4 12 2.5-6h6"/>',
  gear: '<path d="M9.67 5.19L10.82 4.90L10.69 2.79L13.31 2.79L13.18 4.90L15.17 5.53L16.19 6.14L17.58 4.56L19.44 6.42L17.86 7.81L18.81 9.67L19.10 10.82L21.21 10.69L21.21 13.31L19.10 13.18L18.47 15.17L17.86 16.19L19.44 17.58L17.58 19.44L16.19 17.86L14.33 18.81L13.18 19.10L13.31 21.21L10.69 21.21L10.82 19.10L8.83 18.47L7.81 17.86L6.42 19.44L4.56 17.58L6.14 16.19L5.19 14.33L4.90 13.18L2.79 13.31L2.79 10.69L4.90 10.82L5.53 8.83L6.14 7.81L4.56 6.42L6.42 4.56L7.81 6.14Z"/><circle cx="12" cy="12" r="3.2"/>',
  sliders: '<path d="M4 7h9M17 7h3M4 17h3M11 17h9"/><circle cx="15" cy="7" r="2"/><circle cx="9" cy="17" r="2"/>',

  // 常用
  globe: '<circle cx="12" cy="12" r="9"/><path d="M3 12h18M12 3c2.5 2.6 3.8 5.6 3.8 9s-1.3 6.4-3.8 9c-2.5-2.6-3.8-5.6-3.8-9S9.5 5.6 12 3z"/>',
  compass: '<circle cx="12" cy="12" r="9"/><path d="M15.8 8.2l-2.3 5.3-5.3 2.3 2.3-5.3z"/>',
  terminal: '<rect x="3" y="4.5" width="18" height="15" rx="2.5"/><path d="M7 9.5l3 2.5-3 2.5M12.5 15h4.5"/>',
  code: '<path d="M8.5 7L3.5 12l5 5M15.5 7l5 5-5 5M13.5 4.5l-3 15"/>',
  calculator: `<rect x="5" y="2.8" width="14" height="18.4" rx="2.5"/><rect x="8" y="5.8" width="8" height="3.5" rx="1"/>${dot(8.5, 13)}${dot(12, 13)}${dot(15.5, 13)}${dot(8.5, 16.8)}${dot(12, 16.8)}${dot(15.5, 16.8)}`,
  notes: '<path d="M11 4.5H6.5a2 2 0 0 0-2 2v11a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2V13"/><path d="M17.8 3.8l2.4 2.4-7.6 7.6-3.1.7.7-3.1z"/>',
  pencil: '<path d="M15.5 4.5l4 4L9 19H5v-4z"/><path d="M13.5 6.5l4 4"/>',
  doc: '<path d="M6.5 3h7l4.5 4.5V19a2 2 0 0 1-2 2h-9.5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z"/><path d="M13.5 3v4.5H18M8.5 12h7M8.5 15.5h7"/>',
  chart: '<path d="M4 20h16"/><rect x="5.5" y="11" width="3" height="6.5" rx="1"/><rect x="10.5" y="6.5" width="3" height="11" rx="1"/><rect x="15.5" y="3.5" width="3" height="14" rx="1"/>',
  scissors: '<circle cx="6" cy="6.5" r="2.5"/><circle cx="6" cy="17.5" r="2.5"/><path d="M8 8l12 10M8 16L20 6"/>',
  taskview: '<rect x="3" y="7" width="13" height="11" rx="2"/><path d="M8 4h11a2 2 0 0 1 2 2v9"/>',
  macwindow: `<rect x="3" y="4.5" width="18" height="15" rx="2.5"/><path d="M3 9h18"/>${dot(6, 6.8)}${dot(8.3, 6.8)}${dot(10.6, 6.8)}`,
  lock: '<rect x="5" y="10.5" width="14" height="10" rx="2.5"/><path d="M8 10.5V7.5a4 4 0 0 1 8 0v3"/>',
  download: '<circle cx="12" cy="12" r="9"/><path d="M12 7.5v8M8.5 12.5l3.5 3.5 3.5-3.5"/>',
  upload: '<circle cx="12" cy="12" r="9"/><path d="M12 16.5v-8M8.5 11.5L12 8l3.5 3.5"/>',
  tray: '<path d="M3.5 14.5V18a2 2 0 0 0 2 2h13a2 2 0 0 0 2-2v-3.5"/><path d="M12 3.5v11M7.5 10l4.5 4.5 4.5-4.5"/>',
  photo: '<rect x="3" y="4.5" width="18" height="15" rx="2.5"/><circle cx="8.5" cy="9.5" r="1.8"/><path d="M21 16l-5-5-8.5 8.5"/>',
  film: '<rect x="3" y="4.5" width="18" height="15" rx="2.5"/><path d="M7.5 4.5v15M16.5 4.5v15M3 9h4.5M3 15h4.5M16.5 9H21M16.5 15H21"/>',
  gamepad: `<path d="M7 8h10a4.5 4.5 0 0 1 4.4 5.4l-.8 3.9a2.2 2.2 0 0 1-3.9.9L15 16H9l-1.7 2.2a2.2 2.2 0 0 1-3.9-.9l-.8-3.9A4.5 4.5 0 0 1 7 8z"/><path d="M8 10.5v3M6.5 12h3"/>${dot(15.5, 11)}${dot(17.5, 13)}`,
  camera: '<path d="M4 8a2 2 0 0 1 2-2h2l1.5-2h5L16 6h2a2 2 0 0 1 2 2v9.5a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2z"/><circle cx="12" cy="12.5" r="3.5"/>',
  mail: '<rect x="3" y="5" width="18" height="14" rx="2.5"/><path d="M3.5 6.5l8.5 6.5 8.5-6.5"/>',
  calendar: '<rect x="3.5" y="5" width="17" height="15.5" rx="2.5"/><path d="M3.5 10h17M8 3v4M16 3v4"/>',
  search: '<circle cx="10.5" cy="10.5" r="6.5"/><path d="M15.5 15.5l5 5"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  minus: '<path d="M5 12h14"/>',
  'plus.square': '<rect x="3.5" y="3.5" width="17" height="17" rx="4.5"/><path d="M12 8v8M8 12h8"/>',
  dock: '<rect x="3" y="4" width="18" height="16" rx="3"/><path d="M6.5 16.5h11" stroke-width="3"/>',
  xmark: '<path d="M6 6l12 12M18 6L6 18"/>',
  xcircle: '<circle cx="12" cy="12" r="9"/><path d="M9 9l6 6M15 9l-6 6"/>',
  check: '<path d="M5 12.5l4.5 4.5L19 7.5"/>',
  refresh: '<path d="M19.5 12a7.5 7.5 0 1 1-2.2-5.3"/><path d="M19.5 4v4.5H15"/>',
  undo: '<path d="M9 14.5L4.5 10 9 5.5"/><path d="M4.5 10H15a5 5 0 0 1 0 10h-3"/>',
  redo: '<path d="M15 14.5l4.5-4.5L15 5.5"/><path d="M19.5 10H9a5 5 0 0 0 0 10h3"/>',
  copy: '<rect x="8" y="8" width="12" height="12" rx="2.5"/><path d="M16 8V6a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v8a2 2 0 0 0 2 2h2"/>',
  clipboard: '<rect x="5" y="4.5" width="14" height="17" rx="2.5"/><rect x="8.5" y="2.8" width="7" height="3.6" rx="1.2"/>',
  selection: '<rect x="4" y="4" width="16" height="16" rx="2.5" stroke-dasharray="3.2 3"/>',
  save: '<path d="M8 9.5H6.5a2 2 0 0 0-2 2V19a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2v-7.5a2 2 0 0 0-2-2H16"/><path d="M12 3v11M8.5 10.5L12 14l3.5-3.5"/>',
  share: '<path d="M8 9.5H6.5a2 2 0 0 0-2 2V19a2 2 0 0 0 2 2h11a2 2 0 0 0 2-2v-7.5a2 2 0 0 0-2-2H16"/><path d="M12 14V3M8.5 6.5L12 3l3.5 3.5"/>',
  power: '<path d="M12 3v8"/><path d="M7 6.2a7.5 7.5 0 1 0 10 0"/>',
  restart: '<path d="M4.5 12a7.5 7.5 0 1 0 2.2-5.3"/><path d="M4.5 4v4.5H9"/>',
  moon: '<path d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5z"/>',
  zzz: '<path d="M4 6h5l-5 6h5M13 10h4l-4 5h4M17 3.5h3.5l-3.5 4h3.5"/>',
  sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2.5v2M12 19.5v2M2.5 12h2M19.5 12h2M5.3 5.3l1.4 1.4M17.3 17.3l1.4 1.4M5.3 18.7l1.4-1.4M17.3 6.7l1.4-1.4"/>',
  logout: '<path d="M14 4H6.5A1.5 1.5 0 0 0 5 5.5v13A1.5 1.5 0 0 0 6.5 20H14"/><path d="M10 12h10.5M17 8.5l3.5 3.5-3.5 3.5"/>',
  star: '<path d="M12 3.6l2.41 5.68 6.15.54-4.66 4.05 1.39 6.01L12 16.7l-5.29 3.18 1.39-6.01-4.66-4.05 6.15-.54z"/>',
  heart: '<path d="M12 20s-7.5-4.6-7.5-10.2A4.3 4.3 0 0 1 12 7a4.3 4.3 0 0 1 7.5 2.8C19.5 15.4 12 20 12 20z"/>',
  bolt: '<path d="M13.5 2.5L5 13.5h6l-1 8 8.5-11h-6z"/>',
  flame: '<path d="M12 21a6.5 6.5 0 0 0 6.5-6.5c0-4-3-6-4-10-2 1.5-3 3.5-3 5.5-1-.7-1.6-1.8-1.8-3C7.3 8.8 5.5 11.3 5.5 14.5A6.5 6.5 0 0 0 12 21z"/>',
  sparkles: '<path d="M11 3l1.8 5.2L18 10l-5.2 1.8L11 17l-1.8-5.2L4 10l5.2-1.8z"/><path d="M18.5 14.5l.8 2 2 .8-2 .8-.8 2-.8-2-2-.8 2-.8z"/>',
  briefcase: '<rect x="3" y="7" width="18" height="13" rx="2.5"/><path d="M9 7V5.5A1.5 1.5 0 0 1 10.5 4h3A1.5 1.5 0 0 1 15 5.5V7M3 12.5h18"/>',
  house: '<path d="M4 10.5L12 4l8 6.5V19a1.5 1.5 0 0 1-1.5 1.5H15v-6H9v6H5.5A1.5 1.5 0 0 1 4 19z"/>',
  keyboard: `<rect x="2.5" y="6" width="19" height="12" rx="2.5"/>${dot(6, 9.5)}${dot(9.3, 9.5)}${dot(12.6, 9.5)}${dot(15.9, 9.5)}${dot(18.2, 9.5)}${dot(6.5, 12.3)}${dot(9.8, 12.3)}${dot(13, 12.3)}${dot(16.3, 12.3)}<path d="M8 15h8"/>`,
  command: '<path d="M9 9V6.5A2.5 2.5 0 1 0 6.5 9H9zm0 0h6m-6 0v6m6-6V6.5A2.5 2.5 0 1 1 17.5 9H15zm0 0v6m0 0h-6m6 0v2.5a2.5 2.5 0 1 0 2.5-2.5H15zm-6 0v2.5A2.5 2.5 0 1 1 6.5 15H9z"/>',
  mouse: '<rect x="6" y="3" width="12" height="18" rx="6"/><path d="M12 7v3.5"/>',
  trash: '<path d="M4 6.5h16M9.5 6.5v-2h5v2M6 6.5l1 13a1.5 1.5 0 0 0 1.5 1.4h7a1.5 1.5 0 0 0 1.5-1.4l1-13M10 10.5v6.5M14 10.5v6.5"/>',
  link: '<path d="M10 14a4.5 4.5 0 0 0 6.4 0l3-3a4.5 4.5 0 0 0-6.4-6.4l-1.2 1.2"/><path d="M14 10a4.5 4.5 0 0 0-6.4 0l-3 3a4.5 4.5 0 0 0 6.4 6.4l1.2-1.2"/>',
  printer: '<path d="M7 9V3.5h10V9"/><rect x="3" y="9" width="18" height="8.5" rx="2"/><rect x="7" y="14" width="10" height="6.5" rx="1"/>',
  wifi: `<path d="M2.5 9a14 14 0 0 1 19 0M5.5 12.3a9.5 9.5 0 0 1 13 0M8.6 15.5a5 5 0 0 1 6.8 0"/><path d="M12 19h.01" stroke-width="3"/>`,
  radar: '<circle cx="12" cy="12" r="1.8" fill="currentColor"/><path d="M8.2 8.2a5.4 5.4 0 0 0 0 7.6M15.8 8.2a5.4 5.4 0 0 1 0 7.6M5.3 5.3a9.5 9.5 0 0 0 0 13.4M18.7 5.3a9.5 9.5 0 0 1 0 13.4"/>',
  cpu: '<rect x="6" y="6" width="12" height="12" rx="2"/><rect x="9.5" y="9.5" width="5" height="5" rx="1"/><path d="M9 2.5V6M15 2.5V6M9 18v3.5M15 18v3.5M2.5 9H6M2.5 15H6M18 9h3.5M18 15h3.5"/>',
  memory: '<rect x="3" y="7" width="18" height="10" rx="1.5"/><path d="M7 10v4M11 10v4M15 10v4M6 17v3M10 17v3M14 17v3M18 17v3"/>',
  disk: `<rect x="3" y="7" width="18" height="10" rx="2.5"/><path d="M6.5 12h6"/>${dot(17, 12)}`,
  network: '<path d="M8 20V5M4.5 8.5L8 5l3.5 3.5M16 4v15M12.5 15.5L16 19l3.5-3.5"/>',
  battery: `<rect x="2.5" y="7" width="17" height="10" rx="2.5"/><path d="M21.5 10.5v3"/><rect x="5" y="9.5" width="9" height="5" rx="1" ${fill}/>`,
  bubble: '<path d="M4 11.5c0-4 3.6-7 8-7s8 3 8 7-3.6 7-8 7c-1 0-2-.2-2.9-.5L5 19.5l1.1-3.2C4.8 15 4 13.3 4 11.5z"/>',
  person: '<circle cx="12" cy="8" r="4"/><path d="M4.5 20.5a7.5 7.5 0 0 1 15 0"/>',
  cloud: '<path d="M7 19a4.5 4.5 0 0 1-.6-9A6 6 0 0 1 18 9.5a4.8 4.8 0 0 1-.5 9.5z"/>',
  cube: '<path d="M12 3l8 4.5v9L12 21l-8-4.5v-9z"/><path d="M4 7.5l8 4.5 8-4.5M12 12v9"/>',
  paintbrush: '<path d="M19.5 3.5c-3 1.6-7.6 6-9.3 8.6l1.7 1.7c2.6-1.7 7-6.3 8.6-9.3z"/><path d="M9.5 13.2c-2 0-3.5 1.3-3.5 3.3 0 1.4-1 2.3-2.5 2.5 1 1.3 2.6 2 4.2 2 2.6 0 4.3-1.8 3.5-4.1z"/>',
  book: '<path d="M12 6.5C10.3 5 7.8 4.5 4 4.5v14c3.8 0 6.3.5 8 2 1.7-1.5 4.2-2 8-2v-14c-3.8 0-6.3.5-8 2zM12 6.5v14"/>',
  headphones: '<path d="M4 15v-3a8 8 0 0 1 16 0v3"/><rect x="3.5" y="14" width="4" height="6.5" rx="1.5"/><rect x="16.5" y="14" width="4" height="6.5" rx="1.5"/>',
  mic: '<rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5.5 11a6.5 6.5 0 0 0 13 0M12 17.5V21"/>',
  video: '<rect x="3" y="6" width="13" height="12" rx="2.5"/><path d="M16 10.5l5-3v9l-5-3z"/>',
  paperplane: '<path d="M21 3L10 14M21 3l-6.5 18-4.5-7-7-4.5z"/>',
  bell: '<path d="M6 16.5V11a6 6 0 0 1 12 0v5.5l1.5 2h-15z"/><path d="M10 20.5a2 2 0 0 0 4 0"/>',
  smile: `<circle cx="12" cy="12" r="9"/><path d="M8.5 14.5a4.5 4.5 0 0 0 7 0"/>${dot(9, 9.5)}${dot(15, 9.5)}`,
  lightbulb: '<path d="M9 17.5h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.4.3.5.7.5 1.1v.5h6V15c0-.4.2-.8.5-1.1A6 6 0 0 0 12 3z"/>',
  clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3.5 2"/>',
  shuffle: '<path d="M4 8h15M15.5 4.5L19 8l-3.5 3.5M20 16H5M8.5 12.5L5 16l3.5 3.5"/>',
  fullscreen: '<path d="M14.5 4H20v5.5M20 4l-6 6M9.5 20H4v-5.5M4 20l6-6"/>',
  shrink: '<path d="M20 4l-6 6M14 5v5h5M4 20l6-6M10 19v-5H5"/>',
  'arrow.left': '<path d="M19 12H5M11 6l-6 6 6 6"/>',
  'arrow.right': '<path d="M5 12h14M13 6l6 6-6 6"/>',
  'arrow.up': '<path d="M12 19V5M6 11l6-6 6 6"/>',
  'arrow.down': '<path d="M12 5v14M6 13l6 6 6-6"/>',
  'chevron.left': '<path d="M15 5l-7 7 7 7"/>',
  'chevron.right': '<path d="M9 5l7 7-7 7"/>',
  'chevron.down': '<path d="M5 9l7 7 7-7"/>',
  'chevron.updown': '<path d="M8 9l4-4 4 4M8 15l4 4 4-4"/>',
  ellipsis: `${dot(6, 12)}${dot(12, 12)}${dot(18, 12)}`.replaceAll('2.6', '3'),
  pin: '<path d="M15 3.5l5.5 5.5-3 1.5-3.5 3.5.5 4-1.5 1.5-3.5-3.5L4.5 21 3 19.5l5-5-3.5-3.5L6 9.5l4 .5 3.5-3.5z"/>',
  eye: '<path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="3"/>',
  'eye.slash': `<path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z"/><circle cx="12" cy="12" r="3"/><path d="M4 4l16 16"/>`,
  list: `<path d="M8.5 6.5h12M8.5 12h12M8.5 17.5h12"/>${dot(4, 6.5)}${dot(4, 12)}${dot(4, 17.5)}`,
  hand: '<path d="M8 13V5.5a1.5 1.5 0 0 1 3 0V11M11 10V4a1.5 1.5 0 0 1 3 0v6M14 10V5.5a1.5 1.5 0 0 1 3 0V13c0 4.5-2.5 8-7 8-2.7 0-4.3-1.4-5.5-3.6L3 14.5a1.5 1.5 0 0 1 2.4-1.8L8 15.5"/>',
  play: `<path d="M7 4.8v14.4a.8.8 0 0 0 1.2.7l11.6-7.2a.8.8 0 0 0 0-1.4L8.2 4.1A.8.8 0 0 0 7 4.8z" ${fill}/>`,
  pause: `<rect x="6" y="4.5" width="4" height="15" rx="1.2" ${fill}/><rect x="14" y="4.5" width="4" height="15" rx="1.2" ${fill}/>`,
  playpause: `<path d="M3 6.2v11.6l8.5-5.8z" ${fill}/><rect x="14" y="6" width="2.8" height="12" rx="1" ${fill}/><rect x="18.6" y="6" width="2.8" height="12" rx="1" ${fill}/>`,
  forward: `<path d="M3 6.2v11.6l8-5.8zM11.5 6.2v11.6l8-5.8z" ${fill}/><rect x="19.4" y="6" width="2.2" height="12" rx="1" ${fill}/>`,
  backward: `<path d="M21 6.2v11.6l-8-5.8zM12.5 6.2v11.6l-8-5.8z" ${fill}/><rect x="2.4" y="6" width="2.2" height="12" rx="1" ${fill}/>`,
  stop: `<rect x="6" y="6" width="12" height="12" rx="2.5" ${fill}/>`,
  speaker: '<path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z"/><path d="M15.5 9a4.5 4.5 0 0 1 0 6M18 6.5a8 8 0 0 1 0 11"/>',
  'speaker.slash': '<path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z"/><path d="M16 9.5l5 5M21 9.5l-5 5"/>',
  'speaker.minus': '<path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z"/><path d="M16 12h5"/>',
  'speaker.plus': '<path d="M4 9.5h3.5L12 5.5v13l-4.5-4H4z"/><path d="M16 12h5M18.5 9.5v5"/>',
  character: '<path d="M5 19L12 4l7 15M7.8 13.5h8.4"/>',
  qr: `<rect x="3.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="13.5" y="3.5" width="7" height="7" rx="1.5"/><rect x="3.5" y="13.5" width="7" height="7" rx="1.5"/><path d="M14 14h2.5v2.5H14zM18 18h2.5v2.5H18zM18 14h2.5M14 18v2.5"/>`,
  shield: '<path d="M12 3l7.5 3v5.5c0 4.5-3.2 8.2-7.5 9.5-4.3-1.3-7.5-5-7.5-9.5V6z"/><path d="M9 12l2 2 4-4"/>',
  info: `<circle cx="12" cy="12" r="9"/><path d="M12 11v5.5"/>${dot(12, 7.8)}`,
  bag: '<path d="M5 8h14l-1 12.5H6z"/><path d="M9 8V6.5a3 3 0 0 1 6 0V8"/>',
  cart: '<path d="M3 4h2.5l2.2 11h10.6L20.5 7H7"/><circle cx="9" cy="19" r="1.5"/><circle cx="17" cy="19" r="1.5"/>',
  car: '<path d="M5 16.5V12l2-5h10l2 5v4.5"/><path d="M3.5 12h17v4.5h-17zM7 16.5V19M17 16.5V19"/>',
  leaf: '<path d="M5 19c0-8 5-13 15-14-1 10-6 15-14 15"/><path d="M5 19c3-4 6-7 10-9"/>',
  rocket: '<path d="M12 15l-3-3c1.5-5 4.5-8 10-8.5-.5 5.5-3.5 8.5-8.5 10z"/><path d="M9 12l-3.5.5L4 15l4 .5M12 15l-.5 3.5L9 20l-.5-4"/><circle cx="15" cy="9" r="1.5"/>',
  translate: '<path d="M4 5.5h9M8.5 3.5v2c0 4-2 7-5 8.5M6 9c1.2 2.2 3.2 3.8 5.5 4.5"/><path d="M12.5 20.5l4-9 4 9M13.8 17.5h5.4"/>',
};

const SVG_NS = 'http://www.w3.org/2000/svg';

export function svgMarkup(name, cls = '') {
  const body = ICONS[name] || ICONS.star;
  return `<svg class="sf ${cls}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${body}</svg>`;
}

/** 返回图标元素 */
export function icon(name, cls = '') {
  const tpl = document.createElement('template');
  tpl.innerHTML = svgMarkup(name, cls);
  return tpl.content.firstChild;
}

/** 按钮图标：“sf:名称”为矢量图标，其它内容当作 emoji / 文字显示 */
export function glyph(value, cls = '') {
  if (typeof value === 'string' && value.startsWith('sf:')) return icon(value.slice(3), cls);
  const span = document.createElement('span');
  span.className = `emoji ${cls}`;
  span.textContent = value || '★';
  return span;
}

export const ICON_NAMES = Object.keys(ICONS).filter((n) => !['ellipsis', 'chevron.left', 'chevron.right', 'chevron.down', 'chevron.updown'].includes(n));
export { SVG_NS };
