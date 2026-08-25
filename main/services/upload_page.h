#pragma once

namespace papers3 {

inline constexpr char kUploadPageV2[] = R"HTML(<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>Paper S3 文件管理</title><style>
*{box-sizing:border-box}body{margin:0;background:#f1f1ee;color:#171717;font-family:system-ui,-apple-system,sans-serif}.wrap{max-width:680px;margin:auto;padding:24px 18px 60px}.head{display:flex;align-items:end;justify-content:space-between;margin:12px 4px 24px}.head h1{margin:0;font-size:32px}.head small,.muted{color:#6d6d68}.card{background:#fff;border:1px solid #d2d2cc;border-radius:26px;padding:20px;margin:14px 0}.upload{display:flex;align-items:center;gap:14px;border:1.5px dashed #777;border-radius:999px;padding:15px 20px;cursor:pointer}.upload svg{width:28px;fill:none;stroke:currentColor;stroke-width:1.8}.upload strong{display:block}.upload input{display:none}.types{font-size:12px;color:#6d6d68;margin-top:3px}progress{appearance:none;width:100%;height:7px;margin-top:18px}.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:14px 2px;border-bottom:1px solid #e2e2dd}.row:last-child{border:0}.name{overflow-wrap:anywhere}.btn{border:1px solid #222;border-radius:999px;padding:9px 15px;background:#171717;color:#fff;font-weight:650}.danger{background:#fff;color:#171717}@media(prefers-color-scheme:dark){body{background:#111;color:#f5f5f2}.card{background:#1e1e1e;border-color:#444}.muted,.head small,.types{color:#aaa}.row{border-color:#3a3a3a}.danger{background:#1e1e1e;color:#fff;border-color:#777}}
</style></head><body><main class="wrap"><header class="head"><div><small>PAPER S3</small><h1>文件管理</h1></div><span id="state" class="muted">设备已连接</span></header>
<section class="card"><label class="upload"><svg viewBox="0 0 24 24"><path d="M12 16V4m0 0L7.5 8.5M12 4l4.5 4.5M5 14v4a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-4"/></svg><span><strong>选择文件上传</strong><span class="types">EPUB / TXT / JPG / PNG / VLW / BIN</span></span><input id="file" type="file" accept=".epub,.txt,.jpg,.jpeg,.png,.vlw,.bin"></label><div id="filename" class="muted" style="margin-top:14px">尚未选择文件</div><progress id="progress" value="0" max="100"></progress><div id="status" class="muted"></div></section>
<section class="card"><h2>设备中的图书</h2><div id="files">正在读取…</div></section></main><script>
const f=document.querySelector('#file'),s=document.querySelector('#status'),p=document.querySelector('#progress'),n=document.querySelector('#filename');
const esc=x=>x.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function refresh(){try{const r=await fetch('/files');const a=await r.json();document.querySelector('#files').innerHTML=a.length?a.map(x=>`<div class="row"><span class="name"><b>${esc(x.name)}</b><br><small class="muted">${Math.ceil(x.size/1024)} KB</small></span><button class="btn danger" onclick="del('${encodeURIComponent(x.name)}')">删除</button></div>`).join(''):'<span class="muted">暂无图书</span>'}catch(e){s.textContent='读取失败，请重新连接设备'}}
async function del(x){if(!confirm('确认删除这个文件？'))return;const r=await fetch('/delete?name='+x,{method:'POST'});s.textContent=r.ok?'已删除':'删除失败';refresh()}
f.onchange=()=>{const x=f.files[0];if(!x)return;n.textContent=x.name;s.textContent='正在上传…';p.value=0;const q=new XMLHttpRequest;q.open('POST','/upload?name='+encodeURIComponent(x.name));q.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded/e.total*100};q.onload=()=>{s.textContent=q.status===200?'上传完成，可在设备中使用':'上传失败：'+q.responseText;if(q.status===200)refresh()};q.onerror=()=>s.textContent='网络中断';q.send(x)};refresh();
</script></body></html>)HTML";

}  // namespace papers3
