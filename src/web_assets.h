#pragma once
#include <Arduino.h>

// Single-page web UI, served from PROGMEM. All image processing (resize →
// grayscale → Floyd–Steinberg dither → 4-bit pack) happens in the browser, so
// the device only stores/blits ready framebuffers.
static const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>T5 E-Paper Frame</title>
<style>
:root{--bg:#11141a;--card:#1b2029;--ink:#e8ecf2;--mut:#8b95a7;--acc:#4f8cff;--line:#2a313d;}
*{box-sizing:border-box}
body{margin:0;font:15px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--ink)}
header{padding:14px 18px;border-bottom:1px solid var(--line);display:flex;align-items:center;gap:8px;flex-wrap:wrap}
header h1{font-size:17px;margin:0 8px 0 0;font-weight:600}
.pill{font-size:12px;color:var(--mut);background:var(--card);border:1px solid var(--line);border-radius:999px;padding:3px 10px}
main{max-width:880px;margin:0 auto;padding:18px}
nav{display:flex;gap:6px;margin-bottom:14px}
nav button{flex:1;padding:10px;background:var(--card);color:var(--ink);border:1px solid var(--line);border-radius:10px;cursor:pointer;font-size:14px}
nav button.active{border-color:var(--acc);color:#fff;box-shadow:inset 0 0 0 1px var(--acc)}
.seg{display:flex;gap:6px;margin-bottom:16px}
.seg button{flex:1;padding:11px;background:var(--card);border:1px solid var(--line);border-radius:10px;color:var(--ink);cursor:pointer;font-size:14px}
.seg button.on{border-color:var(--acc);background:rgba(79,140,255,.16);color:#fff;font-weight:600}
.card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:16px}
.card h2{margin:0 0 4px;font-size:15px}
.card .sub{color:var(--mut);font-size:12px;margin:0 0 12px}
label{display:block;font-size:13px;color:var(--mut);margin:10px 0 4px}
input,select{width:100%;padding:9px 10px;background:#0e1117;border:1px solid var(--line);border-radius:9px;color:var(--ink);font-size:14px}
.row{display:flex;gap:12px}.row>*{flex:1}
button.act{background:var(--acc);color:#fff;border:none;border-radius:9px;padding:10px 14px;cursor:pointer;font-size:14px;margin-top:12px}
button.act.alt{background:#2f3947}
button.ghost{background:transparent;border:1px solid var(--line);color:var(--ink);border-radius:8px;padding:6px 10px;cursor:pointer;font-size:13px}
button:disabled{opacity:.5;cursor:default}
canvas{width:100%;max-width:240px;border:1px solid var(--line);border-radius:8px;image-rendering:pixelated;background:#000;display:block;margin:10px auto 0}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;margin-top:10px}
.ph{background:#0e1117;border:1px solid var(--line);border-radius:9px;padding:10px;font-size:13px;word-break:break-all}
.ph.live{border-color:var(--acc);box-shadow:inset 0 0 0 1px var(--acc)}
.ph .tag{display:inline-block;font-size:11px;color:var(--acc);margin-bottom:4px}
.ph .b{display:flex;gap:6px;margin-top:8px}
.chips{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}
.chip{font-size:12px;padding:4px 9px;border:1px solid var(--line);border-radius:999px;background:#0e1117;color:var(--ink);cursor:pointer}
.chip:hover{border-color:var(--acc)}
.zrow{display:flex;gap:6px;margin-bottom:8px}.zrow input{flex:1;min-width:0}
.modebar{margin-bottom:14px}
.badge{display:inline-block;background:rgba(95,211,141,.15);border:1px solid #5fd38d;color:#5fd38d;border-radius:999px;padding:7px 14px;font-size:13px}
.tile{background:#0e1117;border:1px solid var(--line);border-radius:10px;padding:10px;margin-bottom:10px}
.tile .row{margin-bottom:6px}.tile .row:last-child{margin-bottom:0}
.muted{color:var(--mut);font-size:13px}
.ok{color:#5fd38d}.err{color:#ff6b6b}
.kv{display:flex;justify-content:space-between;border-top:1px solid var(--line);padding:7px 0;font-size:13px}
.kv:first-of-type{border-top:none}
.kv b{font-weight:600}
small{color:var(--mut)}
</style>
</head>
<body>
<header>
  <h1>🖼️ T5 Frame</h1>
  <span class="pill" id="pMode">—</span>
  <span class="pill" id="pIp">—</span>
  <span class="pill" id="pVer">—</span>
  <span class="pill" id="pFs">—</span>
</header>
<main>
  <nav>
    <button data-t="photos" class="active">Photos</button>
    <button data-t="metrics">Metrics</button>
    <button data-t="home">Home</button>
    <button data-t="settings">Settings</button>
  </nav>

  <!-- PHOTOS -->
  <section id="t-photos">
    <div class="modebar" id="mb0"></div>
    <div class="card">
      <h2>Upload a photo</h2>
      <p class="sub">Resized &amp; dithered in your browser to portrait 540×960, then sent to the frame.</p>
      <input type="file" id="file" accept="image/*">
      <div class="row">
        <div><label>Fit</label>
          <select id="fit"><option value="cover">Cover (crop)</option><option value="contain">Contain (fit)</option></select>
        </div>
        <div><label>Dither</label>
          <select id="dither"><option value="fs">Floyd–Steinberg</option><option value="none">None</option></select>
        </div>
      </div>
      <div class="row">
        <div><label>Brightness <span id="bv">0</span></label><input type="range" id="bright" min="-100" max="100" value="0"></div>
        <div><label>Contrast <span id="cv">1.0</span></label><input type="range" id="contrast" min="50" max="200" value="100"></div>
      </div>
      <canvas id="preview" width="270" height="480"></canvas>
      <button class="act" id="up" disabled style="width:100%">Upload to frame</button>
      <span id="upMsg" class="muted"></span>
    </div>
    <div class="card">
      <h2>Library <small id="cnt"></small></h2>
      <p class="sub" id="curState">—</p>
      <div class="row">
        <div><label>Slideshow interval (seconds)</label><input type="number" id="slide" min="5" value="600"></div>
        <div style="display:flex;align-items:flex-end"><button class="ghost" id="cycle" style="width:100%">↻ Cycle all photos</button></div>
      </div>
      <div class="grid" id="gallery"></div>
    </div>
  </section>

  <!-- METRICS -->
  <section id="t-metrics" hidden>
    <div class="modebar" id="mb1"></div>
    <div class="card">
      <h2>Location</h2>
      <p class="sub">City is detected automatically from the coordinates.</p>
      <div class="kv"><span>Detected city</span><b id="city">—</b></div>
      <div class="row">
        <div><label>Latitude</label><input id="lat" type="number" step="0.0001"></div>
        <div><label>Longitude</label><input id="lon" type="number" step="0.0001"></div>
      </div>
      <button class="ghost" id="geo">📍 Use my browser location</button>
    </div>
    <div class="card">
      <h2>News blocks</h2>
      <p class="sub">Two blocks, one rotating headline each (advances every refresh). Pick a type, or choose “Custom…” to use any RSS/Atom feed.</p>
      <label>Block 1 — type</label><select id="n1cat"></select>
      <div class="row">
        <div><label>Label (shown on screen)</label><input id="n1label"></div>
        <div><label>Feed URL</label><input id="n1url"></div>
      </div>
      <label style="margin-top:16px">Block 2 — type</label><select id="n2cat"></select>
      <div class="row">
        <div><label>Label (shown on screen)</label><input id="n2label"></div>
        <div><label>Feed URL</label><input id="n2url"></div>
      </div>
    </div>
    <div class="card">
      <h2>Timing</h2>
      <div class="row">
        <div><label>Refresh every (minutes)</label><input id="refresh" type="number" min="1" value="15"></div>
        <div><label>POSIX timezone</label><input id="tz"></div>
      </div>
      <div class="row">
        <button class="act" id="saveM" style="width:100%">Save</button>
        <button class="act alt" id="showM" style="width:100%">Save &amp; show now</button>
      </div>
      <span id="mMsg" class="muted"></span>
    </div>
  </section>

  <!-- HOME (HA metric tiles) -->
  <section id="t-home" hidden>
    <div class="modebar" id="mb2"></div>
    <div class="card">
      <h2>Home Assistant</h2>
      <p class="sub">The Home mode shows indoor zones read from HA's REST API. In HA: profile → Security → create a <b>Long-Lived Access Token</b>.</p>
      <label>HA base URL</label><input id="haUrl" placeholder="http://homeassistant.local:8123">
      <label>Long-lived token <span id="haTokState" class="muted"></span></label>
      <input id="haToken" type="password" placeholder="(unchanged)">
    </div>
    <div class="card">
      <h2>Tiles <small>(4)</small></h2>
      <p class="sub">Each tile shows one metric. Pick a <b>type</b> (sets the icon + unit), a label, and the HA <code>entity_id</code>. "2nd entity" is the humidity for climate types.</p>
      <div id="tiles"></div>
      <div class="row">
        <button class="act" id="saveHome" style="width:100%">Save</button>
        <button class="act alt" id="showHome" style="width:100%">Save &amp; show now</button>
      </div>
      <span id="hMsg" class="muted"></span>
    </div>
  </section>

  <!-- SETTINGS -->
  <section id="t-settings" hidden>
    <div class="card">
      <h2>Wi-Fi</h2>
      <div class="kv"><span>Connected to</span><b id="ssid">—</b></div>
      <label>Change network (SSID)</label><input id="nssid">
      <label>Password</label><input id="npass" type="password">
      <button class="act" id="saveWifi">Save &amp; reboot</button>
    </div>
    <div class="card">
      <h2>Device</h2>
      <div class="kv"><span>IP address</span><b id="sIp">—</b></div>
      <div class="kv"><span>Firmware</span><b id="sVer">—</b></div>
      <div class="kv"><span>Free memory</span><b id="sHeap">—</b></div>
      <div class="kv"><span>Storage</span><b id="sFs">—</b></div>
      <div style="display:flex;gap:8px;margin-top:12px;flex-wrap:wrap">
        <button class="ghost" id="shot">📷 Screenshot</button>
        <button class="ghost" id="refreshNow">↻ Repaint display</button>
        <button class="ghost" id="reboot">⏻ Reboot</button>
      </div>
      <canvas id="shotcv" width="270" height="480" hidden></canvas>
    </div>
    <div class="card">
      <h2>Firmware update (OTA)</h2>
      <p class="sub">Flash a new <code>.pio/build/T5-ePaper-S3/firmware.bin</code> over Wi-Fi — no cable.</p>
      <input type="file" id="fw" accept=".bin">
      <button class="act" id="doFw" disabled style="width:100%">Flash firmware</button>
      <span id="fwMsg" class="muted"></span>
    </div>
  </section>
</main>
<script>
const $=s=>document.querySelector(s);
let packed=null, status=null;
// Curated, region-agnostic feed catalog. {group,name,label,url}. Pick "Custom…"
// (url="") to enter any RSS/Atom feed (e.g. your own local paper).
const NEWS_PRESETS=[
  ['Tech','Ars Technica','Tech','https://feeds.arstechnica.com/arstechnica/index'],
  ['Tech','The Verge','Tech','https://www.theverge.com/rss/index.xml'],
  ['Tech','Hacker News','Tech','https://hnrss.org/frontpage'],
  ['Tech','TechCrunch','Tech','https://techcrunch.com/feed/'],
  ['Tech','Wired','Tech','https://www.wired.com/feed/rss'],
  ['World','BBC World','World','https://feeds.bbci.co.uk/news/world/rss.xml'],
  ['World','Al Jazeera','World','https://www.aljazeera.com/xml/rss/all.xml'],
  ['World','NPR News','News','https://feeds.npr.org/1001/rss.xml'],
  ['Business','BBC Business','Business','https://feeds.bbci.co.uk/news/business/rss.xml'],
  ['Business','CNBC','Business','https://www.cnbc.com/id/100003114/device/rss/rss.html'],
  ['Science','BBC Science','Science','https://feeds.bbci.co.uk/news/science_and_environment/rss.xml'],
  ['Science','ScienceDaily','Science','https://www.sciencedaily.com/rss/all.xml'],
  ['Sports','BBC Sport','Sport','https://feeds.bbci.co.uk/sport/rss.xml'],
  ['Sports','ESPN','Sport','https://www.espn.com/espn/rss/news'],
  ['Regional','NL Times (Netherlands)','Netherlands','https://nltimes.nl/rss.xml'],
  ['Regional','DutchNews (Netherlands)','Netherlands','https://www.dutchnews.nl/feed/'],
];
function buildCat(sel){
  let groups={};
  NEWS_PRESETS.forEach((p,i)=>{(groups[p[0]]=groups[p[0]]||[]).push(i);});
  let html='';
  for(const g in groups){ html+='<optgroup label="'+g+'">';
    groups[g].forEach(i=>html+='<option value="'+i+'">'+NEWS_PRESETS[i][1]+'</option>'); html+='</optgroup>'; }
  html+='<option value="custom">Custom…</option>';
  $(sel).innerHTML=html;
}
function wireCat(catSel,labEl,urlEl){
  $(catSel).onchange=()=>{const v=$(catSel).value; if(v!=='custom'){const p=NEWS_PRESETS[+v];$(labEl).value=p[2];$(urlEl).value=p[3];}};
}
function setCat(catSel,url){
  const i=NEWS_PRESETS.findIndex(p=>p[3]===url);
  $(catSel).value = i>=0 ? String(i) : 'custom';
}

// ---- tabs ----
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));
  b.classList.add('active');
  ['photos','metrics','home','settings'].forEach(t=>$('#t-'+t).hidden=(t!==b.dataset.t));
});
// ---- mode switch (per-tab banner) ----
function switchMode(m){post('/api/mode',{mode:m}).then(load);}
function updateModebars(){
  const m=status.settings.mode;
  [0,1,2].forEach(mode=>{const el=$('#mb'+mode); if(!el)return;
    el.innerHTML = (mode===m)
      ? '<span class="badge">● Active mode</span>'
      : '<button class="act" style="margin:0" onclick="switchMode('+mode+')">▶ Switch to this mode</button>';});
}
const TILE_TYPES=[
  ['room_living','Living room (climate)'],['room_bed','Bedroom (climate)'],
  ['room_down','Downstairs (climate)'],['room_up','Upstairs (climate)'],
  ['temperature','Temperature'],['humidity','Humidity'],
  ['storage','Storage (%)'],['storage_gb','Storage (GB)'],
  ['voltage','Voltage'],['power','Power'],['battery','Battery'],
  ['co2','CO₂'],['pressure','Pressure'],['custom','Custom'],
];

// ---- status ----
async function load(){
  status=await (await fetch('/api/status')).json();
  const s=status.settings;
  $('#pIp').textContent=status.ip; $('#sIp').textContent=status.ip;
  $('#pMode').textContent=s.mode==1?'📊 Metrics':s.mode==2?'🏠 Home':'🖼️ Photo';
  $('#pVer').textContent=status.version||'—';
  const fs=(status.fsUsed/1048576).toFixed(1)+'/'+(status.fsTotal/1048576).toFixed(1)+' MB';
  $('#pFs').textContent=fs; $('#sFs').textContent=fs;
  $('#sHeap').textContent=(status.freeHeap/1024|0)+' KB';
  $('#sVer').textContent=status.version||'—';
  $('#ssid').textContent=s.wifiSsid||'—';
  updateModebars();
  $('#slide').value=s.slideshowSec;
  $('#city').textContent=s.locationName||'—';
  $('#lat').value=s.lat; $('#lon').value=s.lon;
  buildCat('#n1cat'); buildCat('#n2cat');
  wireCat('#n1cat','#n1label','#n1url'); wireCat('#n2cat','#n2label','#n2url');
  $('#n1label').value=s.news1Label; $('#n1url').value=s.news1Url;
  $('#n2label').value=s.news2Label; $('#n2url').value=s.news2Url;
  setCat('#n1cat',s.news1Url); setCat('#n2cat',s.news2Url);
  $('#refresh').value=s.metricsRefresh; $('#tz').value=s.tz;
  $('#haUrl').value=s.haUrl||'';
  $('#haTokState').textContent=status.haTokenSet?'· set (blank = keep)':'· not set';
  buildTiles();
  renderGallery();
}
function buildTiles(){
  const t=(status.settings.tiles)||[];
  const esc=v=>(v||'').replace(/"/g,'&quot;');
  let h='';
  for(let i=0;i<4;i++){const tt=t[i]||{};
    const opts=TILE_TYPES.map(([k,n])=>`<option value="${k}" ${k===tt.type?'selected':''}>${n}</option>`).join('');
    h+=`<div class="tile">
      <div class="row"><select id="t${i}type">${opts}</select><input id="t${i}label" placeholder="Label" value="${esc(tt.label)}"></div>
      <div class="row"><input id="t${i}ent" placeholder="entity_id" value="${esc(tt.entity)}"><input id="t${i}ent2" placeholder="2nd entity (humidity)" value="${esc(tt.entity2)}"></div>
    </div>`;
  }
  $('#tiles').innerHTML=h;
}
function renderGallery(){
  const g=$('#gallery'); g.innerHTML='';
  const s=status.settings;
  $('#cnt').textContent='('+status.photos.length+')';
  $('#curState').textContent = s.mode==2 ? 'Home mode is active.'
    : s.mode==1 ? 'Metrics mode is active.'
    : s.pinnedPhoto ? ('Showing: '+s.pinnedPhoto+' (pinned)')
    : (status.photos.length ? ('Cycling all photos every '+s.slideshowSec+'s') : 'No photos yet — upload one above.');
  status.photos.forEach(p=>{
    const live = (p.name===status.current && s.mode==0);
    const d=document.createElement('div'); d.className='ph'+(live?' live':'');
    d.innerHTML=(live?'<span class="tag">● on screen</span>':'')+`<div>${p.name}</div><div class="b">
      <button class="ghost" data-show="${p.name}">Show</button>
      <button class="ghost" data-del="${p.name}">Delete</button></div>`;
    g.appendChild(d);
  });
  g.querySelectorAll('[data-show]').forEach(b=>b.onclick=()=>post('/api/photo/show',{name:b.dataset.show}).then(load));
  g.querySelectorAll('[data-del]').forEach(b=>b.onclick=()=>{if(confirm('Delete '+b.dataset.del+'?'))post('/api/photo/delete',{name:b.dataset.del}).then(load);});
}
function post(u,o){return fetch(u,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(o)});}

// ---- image processing ----
let imgEl=null;
$('#file').onchange=e=>{
  const f=e.target.files[0]; if(!f)return;
  const r=new FileReader();
  r.onload=()=>{const im=new Image();im.onload=()=>{imgEl=im;process();};im.src=r.result;};
  r.readAsDataURL(f);
};
['fit','dither','bright','contrast'].forEach(id=>$('#'+id).addEventListener('input',()=>{
  $('#bv').textContent=$('#bright').value;
  $('#cv').textContent=($('#contrast').value/100).toFixed(2);
  if(imgEl)process();
}));

// Logical portrait canvas (PW x PH) is rotated into the native panel
// framebuffer (NW x NH). Keep PORTRAIT_CW in sync with src/config.h.
const PW=540,PH=960,NW=960,NH=540,PORTRAIT_CW=1;
function process(){
  const c=document.createElement('canvas');c.width=PW;c.height=PH;
  const ctx=c.getContext('2d');ctx.fillStyle='#fff';ctx.fillRect(0,0,PW,PH);
  const iw=imgEl.width,ih=imgEl.height,ir=iw/ih,tr=PW/PH;
  let dw,dh,dx,dy;
  if($('#fit').value==='cover'){
    if(ir>tr){dh=PH;dw=PH*ir;}else{dw=PW;dh=PW/ir;}
  }else{
    if(ir>tr){dw=PW;dh=PW/ir;}else{dh=PH;dw=PH*ir;}
  }
  dx=(PW-dw)/2;dy=(PH-dh)/2;
  ctx.drawImage(imgEl,dx,dy,dw,dh);
  const d=ctx.getImageData(0,0,PW,PH).data;
  const br=+$('#bright').value, co=$('#contrast').value/100;
  const g=new Float32Array(PW*PH);
  for(let i=0;i<PW*PH;i++){
    let v=0.299*d[i*4]+0.587*d[i*4+1]+0.114*d[i*4+2];
    v=(v-128)*co+128+br;
    g[i]=v;
  }
  const q=new Uint8Array(PW*PH);
  const fs=$('#dither').value==='fs';
  for(let y=0;y<PH;y++)for(let x=0;x<PW;x++){
    const i=y*PW+x;
    let lvl=Math.round(g[i]/17); if(lvl<0)lvl=0; if(lvl>15)lvl=15;
    q[i]=lvl;
    if(fs){
      const err=g[i]-lvl*17;
      if(x+1<PW)g[i+1]+=err*7/16;
      if(y+1<PH){
        if(x>0)g[i+PW-1]+=err*3/16;
        g[i+PW]+=err*5/16;
        if(x+1<PW)g[i+PW+1]+=err*1/16;
      }
    }
  }
  // rotate logical portrait -> native panel framebuffer (matches Display::setPixel)
  packed=new Uint8Array(NW*NH/2);
  for(let ly=0;ly<PH;ly++)for(let lx=0;lx<PW;lx++){
    const v=q[ly*PW+lx];
    let PX,PY;
    if(PORTRAIT_CW){PX=ly;PY=NH-1-lx;}else{PX=NW-1-ly;PY=lx;}
    const idx=PY*(NW/2)+(PX>>1);
    if(PX&1)packed[idx]=(packed[idx]&0x0F)|(v<<4);else packed[idx]=(packed[idx]&0xF0)|v;
  }
  // portrait preview
  const pv=$('#preview'),pc=pv.getContext('2d');
  const tmp=document.createElement('canvas');tmp.width=PW;tmp.height=PH;
  const tc=tmp.getContext('2d'),id=tc.createImageData(PW,PH);
  for(let i=0;i<PW*PH;i++){const v=q[i]*17;id.data[i*4]=id.data[i*4+1]=id.data[i*4+2]=v;id.data[i*4+3]=255;}
  tc.putImageData(id,0,0);
  pc.clearRect(0,0,pv.width,pv.height);
  pc.drawImage(tmp,0,0,pv.width,pv.height);
  $('#up').disabled=false;
}

let curName='photo';
$('#file').addEventListener('change',e=>{const f=e.target.files[0];if(f)curName=f.name.replace(/\.[^.]+$/,'');});
$('#up').onclick=async()=>{
  if(!packed)return;
  $('#up').disabled=true;$('#upMsg').textContent='Uploading…';$('#upMsg').className='muted';
  const name=(curName||'photo').replace(/[^a-z0-9_-]/gi,'_').slice(0,40)+'.bin';
  const r=await fetch('/api/upload?name='+encodeURIComponent(name),{method:'POST',body:packed});
  if(r.ok){$('#upMsg').textContent='Uploaded ✓';$('#upMsg').className='ok';await load();}
  else{$('#upMsg').textContent='Failed';$('#upMsg').className='err';}
  $('#up').disabled=false;
};
$('#slide').addEventListener('change',()=>post('/api/settings',{slideshowSec:+$('#slide').value}));
$('#cycle').onclick=()=>post('/api/photo/cycle',{}).then(load);

// ---- metrics ----
$('#geo').onclick=()=>navigator.geolocation.getCurrentPosition(p=>{
  $('#lat').value=p.coords.latitude.toFixed(4);$('#lon').value=p.coords.longitude.toFixed(4);
});
async function saveMetrics(){
  await post('/api/settings',{lat:+$('#lat').value,lon:+$('#lon').value,
    news1Label:$('#n1label').value,news1Url:$('#n1url').value,
    news2Label:$('#n2label').value,news2Url:$('#n2url').value,
    metricsRefresh:+$('#refresh').value,tz:$('#tz').value});
}
$('#saveM').onclick=async()=>{await saveMetrics();$('#mMsg').textContent='Saved ✓';$('#mMsg').className='ok';load();};
$('#showM').onclick=async()=>{await saveMetrics();await post('/api/mode',{mode:1});$('#mMsg').textContent='Rendering on the frame…';$('#mMsg').className='ok';load();};

// ---- home (HA zones) ----
async function saveHome(){
  const tiles=[];
  for(let i=0;i<4;i++)tiles.push({type:$('#t'+i+'type').value,label:$('#t'+i+'label').value,
    entity:$('#t'+i+'ent').value,entity2:$('#t'+i+'ent2').value});
  const body={haUrl:$('#haUrl').value,tiles};
  if($('#haToken').value)body.haToken=$('#haToken').value;
  await post('/api/settings',body);
  $('#haToken').value='';
}
$('#saveHome').onclick=async()=>{await saveHome();$('#hMsg').textContent='Saved ✓';$('#hMsg').className='ok';load();};
$('#showHome').onclick=async()=>{await saveHome();await post('/api/mode',{mode:2});$('#hMsg').textContent='Rendering on the frame…';$('#hMsg').className='ok';load();};

// ---- settings ----
$('#refreshNow').onclick=()=>post('/api/refresh',{});
$('#reboot').onclick=()=>{if(confirm('Reboot device?'))post('/api/reboot',{});};
$('#shot').onclick=async()=>{
  const buf=new Uint8Array(await (await fetch('/api/fb')).arrayBuffer());
  if(buf.length<NW*NH/2){alert('No framebuffer available');return;}
  const cv=$('#shotcv'); cv.hidden=false; cv.width=PW; cv.height=PH;
  const ctx=cv.getContext('2d'), img=ctx.createImageData(PW,PH);
  for(let ly=0;ly<PH;ly++)for(let lx=0;lx<PW;lx++){
    let PX,PY; if(PORTRAIT_CW){PX=ly;PY=NH-1-lx;}else{PX=NW-1-ly;PY=lx;}
    const byte=buf[PY*(NW/2)+(PX>>1)];
    const v=((PX&1)?(byte>>4):(byte&0x0F))*17;
    const o=(ly*PW+lx)*4; img.data[o]=img.data[o+1]=img.data[o+2]=v; img.data[o+3]=255;
  }
  ctx.putImageData(img,0,0);
  cv.toBlob(b=>{const a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='t5-screenshot.png';a.click();});
};
let fwFile=null;
$('#fw').onchange=e=>{fwFile=e.target.files[0];$('#doFw').disabled=!fwFile;};
$('#doFw').onclick=async()=>{
  if(!fwFile||!confirm('Flash this firmware? The device will reboot.'))return;
  $('#doFw').disabled=true;$('#fwMsg').textContent='Uploading… do not power off.';$('#fwMsg').className='muted';
  try{
    const r=await fetch('/api/update',{method:'POST',body:fwFile});
    if(r.ok){$('#fwMsg').textContent='Update OK — rebooting. Reload in ~15s.';$('#fwMsg').className='ok';}
    else{$('#fwMsg').textContent='Update failed (bad image?).';$('#fwMsg').className='err';$('#doFw').disabled=false;}
  }catch(err){
    $('#fwMsg').textContent='Uploaded; device rebooting. Reload in ~15s.';$('#fwMsg').className='ok';
  }
};
$('#saveWifi').onclick=async()=>{
  if(!$('#nssid').value)return;
  await post('/api/wifi',{wifiSsid:$('#nssid').value,wifiPass:$('#npass').value});
  alert('Saved. Device will reboot and join the new network.');
};

load();
</script>
</body>
</html>)HTMLPAGE";

// Minimal page shown while the device is in Soft-AP onboarding mode.
static const char SETUP_HTML[] PROGMEM = R"SETUPPAGE(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>T5 Frame Setup</title>
<style>body{font:16px system-ui;background:#11141a;color:#e8ecf2;margin:0;padding:24px}
.card{max-width:420px;margin:6vh auto;background:#1b2029;border:1px solid #2a313d;border-radius:14px;padding:22px}
h1{font-size:19px}label{display:block;color:#8b95a7;font-size:13px;margin:14px 0 4px}
input{width:100%;padding:11px;background:#0e1117;border:1px solid #2a313d;border-radius:9px;color:#fff;font-size:15px}
button{margin-top:18px;width:100%;padding:12px;background:#4f8cff;border:none;border-radius:9px;color:#fff;font-size:15px}
</style></head><body>
<div class="card">
<h1>📶 Connect your T5 Frame</h1>
<p style="color:#8b95a7">Enter your Wi-Fi details. The device will reboot and join your network.</p>
<label>Wi-Fi network (SSID)</label><input id="s">
<label>Password</label><input id="p" type="password">
<button onclick="save()">Save &amp; connect</button>
<p id="m" style="color:#8b95a7"></p>
</div>
<script>
async function save(){
  document.getElementById('m').textContent='Saving…';
  await fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({wifiSsid:document.getElementById('s').value,wifiPass:document.getElementById('p').value})});
  document.getElementById('m').textContent='Rebooting — reconnect to your home Wi-Fi, then open http://t5frame.local';
}
</script>
</body></html>)SETUPPAGE";
