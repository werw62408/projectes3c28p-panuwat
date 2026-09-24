#pragma once
#include <Arduino.h>
// หน้าเว็บในตัวบอร์ด — ธีมขาวดำ สีมีเฉพาะกิจกรรม (ทำงานออฟไลน์ ไม่โหลดอะไรจากเน็ต)
const char INDEX_HTML[] PROGMEM = R"HTMLPAGE(<!DOCTYPE html>
<html lang="th"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>สมุดติ๊ก</title>
<style>
:root{--bg:#f2f2f2;--card:#fff;--ink:#111;--soft:#6e6e6e;--line:#d0d0d0}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--ink);font-family:system-ui,-apple-system,"Sukhumvit Set","Noto Sans Thai",sans-serif}
header{background:var(--ink);color:#fff;padding:12px 16px;display:flex;align-items:center;gap:10px;position:sticky;top:0;z-index:5}
header h1{font-size:19px;margin:0;flex:1;letter-spacing:.5px}
.chip{background:#fff;color:var(--ink);border:none;border-radius:20px;padding:5px 14px;font-weight:700;font-size:14px}
.warn{background:#fff;border:2px solid var(--ink);margin:10px 12px 0;padding:10px 12px;border-radius:12px;font-size:14px}
nav{display:flex;gap:6px;padding:10px 12px;overflow-x:auto;position:sticky;top:50px;background:var(--bg);z-index:4}
nav button{flex:0 0 auto;border:1px solid var(--line);background:var(--card);color:var(--soft);border-radius:18px;padding:7px 14px;font-size:14px}
nav button.on{background:var(--ink);color:#fff;border-color:var(--ink)}
main{max-width:520px;margin:0 auto;padding:0 12px 40px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:9px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:11px;position:relative}
.act{border-left:6px solid var(--c);cursor:pointer;user-select:none;transition:transform .08s}
.act:active{transform:scale(.97)}
.act.good{background:color-mix(in srgb,var(--c) 22%,#fff);border-color:var(--c);border-left-color:var(--c)}
.act.limit{box-shadow:0 0 0 2px var(--c) inset}
.act.over{background:var(--ink);color:#fff;border-color:var(--ink)}
.act.over .sub,.act.over .unit{color:#bbb}.act.over .n{color:#fff}
.act.od{box-shadow:0 0 0 3px var(--ink) inset}
.name{font-weight:700;font-size:14px}
.n{font-size:28px;font-weight:800;color:var(--c);line-height:1.1;margin-top:4px}
.unit{font-size:12px;color:var(--soft)}
.sub{font-size:11.5px;color:var(--soft);margin-top:3px}
.bar{height:3px;background:var(--line);border-radius:2px;margin-top:7px;overflow:hidden}.bar i{display:block;height:100%;background:var(--c)}
.row{display:flex;gap:6px;margin-top:8px}
.btn{border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:10px;padding:6px 10px;font-size:13px}
.btn.dark{background:var(--ink);color:#fff;border-color:var(--ink)}
.act.over .btn{background:#333;color:#fff;border-color:#555}
.sec{font-size:12px;letter-spacing:1.5px;color:var(--soft);margin:14px 4px 8px}
.stat{margin-bottom:10px;border-left:6px solid var(--c)}
.nums{display:flex;gap:16px;margin-top:6px}.nums b{display:block;font-size:20px;color:var(--c)}.nums span{font-size:11px;color:var(--soft)}
svg{width:100%;display:block}
table.heat{border-collapse:separate;border-spacing:1px;width:100%;table-layout:fixed}
table.heat td{height:22px;border-radius:3px}table.heat td.l{width:70px;font-size:12px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;padding-right:4px}
.hrs{display:flex;font-size:10px;color:var(--soft);margin-left:71px}.hrs span{flex:1}
.ed{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin-top:8px}
.ed label{font-size:11px;color:var(--soft);display:flex;flex-direction:column;gap:2px}
input,select{font:inherit;font-size:14px;padding:7px;border:1px solid var(--line);border-radius:8px;background:#fff;color:var(--ink);width:100%}
input[type=color]{padding:2px;height:36px}
.tools{display:flex;gap:6px;justify-content:flex-end}
.muted{color:var(--soft);font-size:13px}
.toast{position:fixed;left:50%;bottom:20px;transform:translateX(-50%);background:var(--ink);color:#fff;padding:9px 16px;border-radius:20px;font-size:14px;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
</style></head><body>
<header><h1>สมุดติ๊ก</h1><span id="clk" class="muted" style="color:#bbb"></span><button class="chip" id="place"></button></header>
<div class="warn" id="tw" hidden>เวลาในบอร์ดยังไม่ตรง — กำลังตั้งตามมือถือให้อัตโนมัติ</div>
<nav id="nav"></nav>
<main id="main"></main>
<div class="toast" id="toast"></div>
<script>
const $=s=>document.querySelector(s);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=v=>Math.abs(v-Math.round(v))<0.05?String(Math.round(v)):v.toFixed(1);
const DOW=['อา','จ','อ','พ','พฤ','ศ','ส'];
let S=null,tab='today',range=7,ACTS=null,timeSent=false;
const TABS=[['today','วันนี้'],['stats','สถิติ'],['heat','ช่วงเวลา'],['acts','กิจกรรม'],['media','รูป/คลิป'],['set','ตั้งค่า']];
function toast(t){const e=$('#toast');e.textContent=t;e.classList.add('show');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('show'),1400)}
async function api(path,body,json){
  const o=body===undefined?{}:{method:'POST',headers:{'Content-Type':json?'application/json':'application/x-www-form-urlencoded'},body:json?JSON.stringify(body):new URLSearchParams(body)};
  const r=await fetch(path,o);if(!r.ok)throw new Error(await r.text());return r.json();
}
function ago(t){if(!t)return'ยังไม่มีบันทึก';let d=Math.max(0,Date.now()/1000-t);if(d<60)return'เมื่อกี้';let m=Math.floor(d/60);if(m<60)return m+' นาทีที่แล้ว';let h=Math.floor(m/60);m%=60;if(h<24)return h+' ชม. '+(m?m+' นาที':'')+'ที่แล้ว';return Math.floor(h/24)+' วันที่แล้ว'}
function renderNav(){$('#nav').innerHTML=TABS.map(([k,l])=>`<button class="${k==tab?'on':''}" onclick="go('${k}')">${l}</button>`).join('')}
function go(k){tab=k;renderNav();render()}
async function refresh(){
  S=await api('/api/state');
  if(S.approx&&!timeSent){timeSent=true;S=await api('/api/time',{t:Math.floor(Date.now()/1000)})}
  $('#tw').hidden=!S.approx;
  $('#place').textContent=(S.place=='U'?'มอ':'บ้าน')+(S.auto?' · A':'');
  $('#clk').textContent=new Date(S.now*1000).toLocaleTimeString('th-TH',{hour:'2-digit',minute:'2-digit'});
  if(tab=='today')renderToday();
}
$('#place').onclick=async()=>{S=await api('/api/place',{p:S.place=='U'?'H':'U'});refresh()};
function render(){({today:renderToday,stats:renderStats,heat:renderHeat,acts:renderActs,media:renderMedia,set:renderSet})[tab]()}
function cls(a){let c='card act';if(a.state==2)c+=' good';if(a.state==3)c+=' limit';if(a.state==4)c+=' over';if(a.overdue)c+=' od';return c}
function renderToday(){
  if(!S)return;
  $('#main').innerHTML=`<div class="sec">แตะการ์ด = บันทึก · กดค้าง/ปุ่ม “ใส่ค่า” = ใส่ตัวเลขเอง</div><div class="grid">`+S.acts.map(a=>{
    const unit=a.unit.length>0, m=unit?a.sum:a.count;
    const goal=a.goal>0&&a.type?(a.type==1?'/ ':'ไม่เกิน ')+fmt(a.goal):'';
    const pct=a.goal>0&&a.type?Math.min(100,m/a.goal*100):0;
    return `<div class="${cls(a)}" style="--c:${a.color}" data-id="${a.id}">
      <div class="name">${esc(a.name)}</div>
      <div class="n">${fmt(m)} <span class="unit">${esc(goal)} ${esc(a.unit)}</span></div>
      <div class="sub">${a.overdue?'! เกินเวลา · ':''}${ago(a.last)}</div>
      ${pct||goal?`<div class="bar"><i style="width:${pct}%"></i></div>`:''}
      <div class="row"><button class="btn" data-undo="${a.id}" ${a.count?'':'disabled'}>ยกเลิก</button><button class="btn" data-val="${a.id}">ใส่ค่า</button></div>
    </div>`}).join('')+`</div>`;
  document.querySelectorAll('.act').forEach(el=>{
    let t;el.onpointerdown=()=>{t=setTimeout(()=>{t=null;askVal(el.dataset.id)},600)};
    el.onpointerup=e=>{if(t){clearTimeout(t);if(!e.target.closest('button'))logIt(el.dataset.id)}};
    el.onpointerleave=()=>clearTimeout(t);
  });
  document.querySelectorAll('[data-undo]').forEach(b=>b.onclick=async e=>{e.stopPropagation();S=await api('/api/undo',{id:b.dataset.undo});toast('ยกเลิกแล้ว');renderToday()});
  document.querySelectorAll('[data-val]').forEach(b=>b.onclick=e=>{e.stopPropagation();askVal(b.dataset.val)});
}
async function logIt(id,v){const b={id};if(v!==undefined)b.v=v;S=await api('/api/log',b);const a=S.acts.find(x=>x.id==id);toast('บันทึก '+(a?a.name:''));renderToday()}
function askVal(id){const a=S.acts.find(x=>x.id==id);const v=prompt('ใส่ค่า: '+a.name+(a.unit?' ('+a.unit+')':''),a.step);if(v===null)return;const n=parseFloat(v);if(!(n>0))return toast('ค่าไม่ถูกต้อง');logIt(id,n)}
function rangeBtns(){return `<div class="tools" style="margin-top:6px">${[7,30].map(d=>`<button class="btn ${d==range?'dark':''}" onclick="range=${d};render()">${d} วัน</button>`).join('')}</div>`}
async function renderStats(){
  $('#main').innerHTML='<p class="muted">กำลังโหลด…</p>';
  const [st]=await Promise.all([api('/api/stats?days='+range),S?0:refresh()]);
  const byId=Object.fromEntries(st.acts.map(a=>[a.id,a]));
  $('#main').innerHTML=rangeBtns()+S.acts.map(a=>{
    const s=byId[a.id];if(!s)return'';
    const unit=a.unit.length>0, arr=unit?s.daily:s.dcount, tot=unit?s.total:s.totalCount;
    const sum=arr.reduce((x,y)=>x+y,0), mx=Math.max(1,a.goal||0,...arr), n=arr.length, bw=300/n;
    const bars=arr.map((v,i)=>{const h=v/mx*70;const over=a.type==2&&a.goal>0&&v>a.goal;return `<rect x="${i*bw+bw*.15}" y="${80-h}" width="${bw*.7}" height="${Math.max(h,.5)}" rx="2" fill="${over?'#111':a.color}" opacity="${i==n-1||over?1:.55}"/>`}).join('');
    const gl=a.goal>0&&a.type?`<line x1="0" x2="300" y1="${80-a.goal/mx*70}" y2="${80-a.goal/mx*70}" stroke="#111" stroke-dasharray="4 4"/>`:'';
    const lbl=n<=7?st.days.map((d,i)=>`<text x="${i*bw+bw/2}" y="95" font-size="10" text-anchor="middle" fill="#6e6e6e">${DOW[new Date(d+'T12:00:00').getDay()]}</text>`).join(''):'';
    const gap=s.gapMin?(s.gapMin>=60?Math.floor(s.gapMin/60)+' ชม. ':'')+Math.round(s.gapMin%60)+' นาที':'–';
    return `<div class="card stat" style="--c:${a.color}"><div class="name">${esc(a.name)}</div>
      <div class="nums"><div><b>${fmt(arr[n-1])}</b><span>วันนี้</span></div><div><b>${fmt(sum)}</b><span>${range} วัน</span></div><div><b>${fmt(sum/n)}</b><span>เฉลี่ย/วัน</span></div><div><b>${fmt(tot)}</b><span>ทั้งหมด</span></div></div>
      <svg viewBox="0 0 300 100">${bars}${gl}${lbl}</svg><div class="muted">ห่างกันเฉลี่ย ${gap}${a.unit?' · หน่วย '+esc(a.unit):''}</div></div>`}).join('');
}
async function renderHeat(){
  $('#main').innerHTML='<p class="muted">กำลังโหลด…</p>';
  const [st]=await Promise.all([api('/api/stats?days='+range),S?0:refresh()]);
  const byId=Object.fromEntries(st.acts.map(a=>[a.id,a]));
  const mix=(c,t)=>`color-mix(in srgb,${c} ${Math.round(t*100)}%,#fff)`;
  $('#main').innerHTML=rangeBtns()+`<div class="sec">จำนวนครั้งแยกตามชั่วโมง (${range} วันล่าสุด)</div><div class="card">
    <div class="hrs">${[0,3,6,9,12,15,18,21].map(h=>`<span>${h}</span>`).join('')}</div><table class="heat">`+
    S.acts.map(a=>{const h=(byId[a.id]||{hours:[]}).hours;const mx=Math.max(1,...h);
      return `<tr><td class="l">${esc(a.name)}</td>`+h.map((c,i)=>`<td title="${i}:00 · ${c} ครั้ง" style="background:${c?mix(a.color,.25+.75*c/mx):'#e6e6e6'}"></td>`).join('')+'</tr>'}).join('')+
    `</table></div><p class="muted">ช่องเข้ม = ทำบ่อยในชั่วโมงนั้น</p>`;
}
async function renderActs(){
  if(!ACTS)ACTS=await api('/api/acts');
  const typeSel=(v)=>`<select data-k="type">${[['0','ไม่มีเป้า'],['1','อย่างน้อย'],['2','ไม่เกิน']].map(([k,l])=>`<option value="${k}" ${v==k?'selected':''}>${l}</option>`).join('')}</select>`;
  $('#main').innerHTML=`<div class="sec">แก้ไขกิจกรรม (สูงสุด 12) · หน่วยเว้นว่าง = นับครั้ง</div>`+ACTS.map((a,i)=>`
    <div class="card stat" style="--c:${a.color}" data-i="${i}">
      <div style="display:flex;gap:6px;align-items:center"><input data-k="name" value="${esc(a.name)}" placeholder="ชื่อกิจกรรม">
      <button class="btn" onclick="mv(${i},-1)">↑</button><button class="btn" onclick="mv(${i},1)">↓</button><button class="btn" onclick="del(${i})">ลบ</button></div>
      <div class="ed">
        <label>สี<input type="color" data-k="color" value="${a.color}"></label>
        <label>หน่วย (เว้นว่าง = นับครั้ง, หรือใส่ เช่น บาท)<input data-k="unit" value="${esc(a.unit)}"></label>
        <label>ค่าต่อการแตะ 1 ครั้ง<input type="number" step="any" data-k="step" value="${a.step}"></label>
        <label>ประเภทเป้า${typeSel(a.type)}</label>
        <label>เป้า/ลิมิตต่อวัน<input type="number" step="any" data-k="goal" value="${a.goal}"></label>
        <label>เตือนถ้าไม่ได้ทำเกิน (นาที, 0=ไม่เตือน)<input type="number" data-k="remind" value="${a.remind}"></label>
      </div></div>`).join('')+`
    <div class="tools"><button class="btn" onclick="addAct()">+ เพิ่มกิจกรรม</button><button class="btn dark" onclick="saveActs()">บันทึกลงบอร์ด</button></div>
    <div class="sec">ส่งออกข้อมูล (CSV เปิดใน Excel / Google Sheets ได้)</div>
    <div class="tools" style="justify-content:flex-start"><a class="btn" href="/export.csv?days=7">7 วัน</a><a class="btn" href="/export.csv?days=30">30 วัน</a><a class="btn dark" href="/export.csv?days=0">ทั้งหมด</a></div>`;
  document.querySelectorAll('[data-i] [data-k]').forEach(el=>el.oninput=()=>{const i=+el.closest('[data-i]').dataset.i,k=el.dataset.k;
    ACTS[i][k]=['step','goal','remind','type'].includes(k)?+el.value:el.value;if(k=='color')el.closest('[data-i]').style.setProperty('--c',el.value)});
}
function mv(i,d){const j=i+d;if(j<0||j>=ACTS.length)return;[ACTS[i],ACTS[j]]=[ACTS[j],ACTS[i]];renderActs()}
function del(i){if(confirm('ลบ "'+ACTS[i].name+'"? (ประวัติเดิมยังอยู่ในไฟล์ CSV)')){ACTS.splice(i,1);renderActs()}}
function addAct(){if(ACTS.length>=12)return toast('ครบ 12 กิจกรรมแล้ว');const cs=['#2F8F82','#3E6B99','#A9822B','#B5586F','#6B7F3E','#7A6C9E','#C0612B','#2B7BC0'];ACTS.push({id:'',name:'กิจกรรมใหม่',unit:'',color:cs[ACTS.length%cs.length],step:1,goal:0,type:0,remind:0});renderActs()}
async function saveActs(){try{ACTS=await api('/api/acts',ACTS,true);toast('บันทึกแล้ว');S=null;await refresh();renderActs()}catch(e){toast('บันทึกไม่สำเร็จ')}}
async function renderSet(){
  const w=await api('/api/wifi');
  $('#main').innerHTML=`<div class="sec">รหัส Wi-Fi ของตัวบอร์ด (SomudTick)</div><div class="card">
    <div class="ed" style="grid-template-columns:1fr"><label>รหัสใหม่ อย่างน้อย 8 ตัว (เปลี่ยนแล้วมือถือจะหลุด ต้องต่อใหม่ด้วยรหัสใหม่)<input id="appass" value="${esc(w.appass||'')}"></label></div></div>
    <div class="sec">Wi-Fi บ้าน (ใช้ซิงก์เวลาจากเน็ต)</div><div class="card">
    <div class="ed" style="grid-template-columns:1fr"><label>ชื่อ Wi-Fi<input id="ssid" value="${esc(w.ssid)}"></label><label>รหัสผ่าน<input id="pass" type="password" placeholder="${w.ssid?'******** (ไม่เปลี่ยนให้เว้นไว้)':''}"></label></div>
    <p class="muted">สถานะ: ${w.sta?'เชื่อมแล้ว '+w.ip:'ยังไม่ได้เชื่อม'}</p></div>
    <div class="sec">สลับบ้าน/มอ อัตโนมัติ (ดูจากชื่อ Wi-Fi ที่เจอรอบตัว ไม่ต้องเชื่อมต่อ)</div><div class="card">
    <div class="ed"><label>ชื่อ Wi-Fi ที่บ้าน<input id="home" value="${esc(w.home)}"></label><label>ชื่อ Wi-Fi ที่มอ<input id="uni" value="${esc(w.uni)}"></label></div>
    <label class="muted" style="display:flex;gap:8px;align-items:center;margin-top:8px"><input type="checkbox" id="auto" style="width:auto" ${w.auto?'checked':''}> เปิดสลับอัตโนมัติ</label></div>
    <div class="tools" style="margin-top:10px"><button class="btn dark" onclick="saveWifi()">บันทึก</button></div>
    <div class="sec">เวลา</div><div class="card"><p class="muted" style="margin:0 0 8px">เวลาบอร์ด: ${S?new Date(S.now*1000).toLocaleString('th-TH'):''} ${S&&S.approx?'(ยังไม่ตรง)':''}</p><button class="btn" onclick="syncTime()">ตั้งเวลาตามมือถือ</button></div>`;
}
async function saveWifi(){const p=$('#pass').value;const ap=$('#appass').value.trim();if(ap&&ap.length<8)return toast('รหัสบอร์ดต้องมีอย่างน้อย 8 ตัว');const b={ssid:$('#ssid').value,home:$('#home').value,uni:$('#uni').value,auto:$('#auto').checked?1:0,appass:ap};b.pass=p||'********';await api('/api/wifi',b);toast('บันทึกแล้ว');setTimeout(refresh,1500)}
async function syncTime(){S=await api('/api/time',{t:Math.floor(Date.now()/1000)});toast('ตั้งเวลาแล้ว');renderSet()}
// ---------- photos & videos: convert on the phone, send to the board's SD card ----------
function safeName(n,prefix){let b=n.replace(/\.[^.]+$/,'').replace(/[^A-Za-z0-9_-]+/g,'_').replace(/^_+|_+$/g,'').slice(0,40);if(b.replace(/_/g,'').length<2)b=prefix+'_'+Date.now().toString(36);return b}
function fitBox(w,h){const W0=w>h?320:240,H0=w>h?240:320;const k=Math.min(W0/w,H0/h,1);return [Math.max(2,Math.round(w*k)),Math.max(2,Math.round(h*k))]}
function upload(blob,dir,name,onp){return new Promise((ok,bad)=>{const x=new XMLHttpRequest();x.open('POST','/upload?dir='+encodeURIComponent(dir));
  x.upload.onprogress=e=>{if(e.lengthComputable&&onp)onp(e.loaded/e.total)};x.onload=()=>x.status==200?ok():bad(new Error(x.responseText||'upload failed'));
  x.onerror=()=>bad(new Error('ส่งไม่สำเร็จ (Wi-Fi หลุด?)'));const f=new FormData();f.append('f',blob,name);x.send(f)})}
function setStatus(t){const e=$('#mst');if(e)e.textContent=t}
async function sendPhotos(files){
  for(let i=0;i<files.length;i++){const f=files[i];setStatus(`รูป ${i+1}/${files.length}: กำลังย่อ...`);
    try{const url=URL.createObjectURL(f);const img=new Image();await new Promise((ok,bad)=>{img.onload=ok;img.onerror=()=>bad(new Error('เปิดรูปไม่ได้'));img.src=url});
      const [w,h]=fitBox(img.naturalWidth,img.naturalHeight);const c=document.createElement('canvas');c.width=w;c.height=h;c.getContext('2d').drawImage(img,0,0,w,h);URL.revokeObjectURL(url);
      const b=await new Promise(r=>c.toBlob(r,'image/jpeg',0.85));
      await upload(b,upDir('i'),safeName(f.name,'photo')+'.jpg',p=>setStatus(`รูป ${i+1}/${files.length}: กำลังส่ง ${Math.round(p*100)}%`));
    }catch(e){toast(f.name+': '+e.message)}}
  setStatus('ส่งรูปเสร็จแล้ว');loadSd();
}
async function makePcm(file){
  const AC=window.AudioContext||window.webkitAudioContext,OAC=window.OfflineAudioContext||window.webkitOfflineAudioContext;
  const ab=await file.arrayBuffer();const ac=new AC();
  const buf=await new Promise((ok,bad)=>{const p=ac.decodeAudioData(ab,ok,bad);if(p&&p.then)p.then(ok,bad)});
  const off=new OAC(1,Math.ceil(buf.duration*16000),16000);const s=off.createBufferSource();s.buffer=buf;s.connect(off.destination);s.start(0);
  const r=await off.startRendering();const d=r.getChannelData(0);const out=new Int16Array(d.length);
  for(let i=0;i<d.length;i++){const v=Math.max(-1,Math.min(1,d[i]));out[i]=v<0?v*32768:v*32767}
  try{ac.close()}catch(e){}
  return new Blob([out.buffer]);
}
async function sendVideo(file){
  const name=safeName(file.name,'video');
  setStatus('กำลังเปิดคลิป...');
  const v=document.createElement('video');v.muted=true;v.playsInline=true;v.setAttribute('playsinline','');v.preload='auto';v.src=URL.createObjectURL(file);
  await new Promise((ok,bad)=>{v.onloadedmetadata=ok;v.onerror=()=>bad(new Error('เปิดคลิปไม่ได้'))});
  try{await v.play();v.pause()}catch(e){}
  const dur=v.duration;if(!(dur>0))throw new Error('อ่านความยาวคลิปไม่ได้');
  if(dur>180&&!confirm('คลิปยาว '+Math.round(dur/60)+' นาที จะใช้เวลาแปลงนาน ทำต่อไหม?'))return;
  const [w,h]=fitBox(v.videoWidth,v.videoHeight);const c=document.createElement('canvas');c.width=w;c.height=h;const g=c.getContext('2d');
  const fps=15,n=Math.max(1,Math.floor(dur*fps)),parts=[];
  for(let i=0;i<n;i++){
    await new Promise(ok=>{v.onseeked=ok;v.currentTime=Math.min(dur-0.01,i/fps)});
    g.drawImage(v,0,0,w,h);parts.push(await new Promise(r=>c.toBlob(r,'image/jpeg',0.6)));
    if(i%5==0)setStatus(`แปลงภาพ ${Math.round(i*100/n)}% (อย่าปิดหน้านี้)`);
  }
  URL.revokeObjectURL(v.src);
  const mj=new Blob(parts);
  setStatus('กำลังแปลงเสียง...');
  let pcm=null;try{pcm=await makePcm(file)}catch(e){toast('คลิปนี้แปลงเสียงไม่ได้ จะส่งแค่ภาพ')}
  await upload(mj,upDir('v'),name+'.mjpeg',p=>setStatus(`ส่งภาพ ${Math.round(p*100)}% (${(mj.size/1048576).toFixed(1)} MB)`));
  if(pcm)await upload(pcm,upDir('v'),name+'.pcm',p=>setStatus(`ส่งเสียง ${Math.round(p*100)}%`));
  setStatus('ส่งคลิปเสร็จแล้ว เปิดดูที่บอร์ด: Apps > Files > '+upDir('v'));loadSd();
}
// ---------- file manager (same tools as the board screen) ----------
let cwd='/',SDI=[],SDD=null,CR=[],UP='/';
const TRASH='/.trash';
const ERR={nocard:'ไม่มีการ์ด SD ในบอร์ด',badname:'ชื่อใช้ได้แค่ A-Z, 0-9, ช่องว่าง, - และ _',exists:'มีชื่อนี้อยู่แล้ว',notempty:'โฟลเดอร์ยังมีไฟล์อยู่ ย้ายหรือลบไฟล์ข้างในออกก่อน',same:'อยู่ในโฟลเดอร์นี้อยู่แล้ว',inside:'ย้ายโฟลเดอร์เข้าไปในตัวเองไม่ได้',missing:'ไม่เจอไฟล์นี้แล้ว',fail:'ทำไม่สำเร็จ'};
function upDir(k){if(cwd==TRASH)return k=='v'?'/videos':'/photos';if(cwd=='/')return k=='v'?'/videos':'/photos';return cwd}
const join=(d,n)=>d=='/'?'/'+n:d+'/'+n;
const kb=s=>s>=1048576?(s/1048576).toFixed(1)+' MB':Math.ceil(s/1024)+' KB';
async function sdOp(b,okMsg){try{await api('/api/sd/op',b);toast(okMsg)}catch(e){let c='fail';try{c=JSON.parse(e.message).err}catch(_){}toast(ERR[c]||ERR.fail)}loadSd()}
function openDir(d){cwd=d;loadSd()}
async function loadSd(){
  const box=$('#sdl');if(!box)return;
  const ud=$('#updir');if(ud)ud.textContent=cwd=='/'||cwd==TRASH?'/photos (รูป) และ /videos (คลิป)':cwd;
  box.innerHTML='<p class="muted">กำลังอ่านการ์ด...</p>';
  try{const d=await api('/api/sd?dir='+encodeURIComponent(cwd));SDD=d;
    if(!d.ok){box.innerHTML='<p class="muted">ไม่เจอการ์ด SD ในบอร์ด ใส่การ์ด (FAT32) แล้วกดรีเฟรช</p>';return}
    cwd=d.dir;const tr=cwd==TRASH;
    $('#sdinfo').textContent=`ใช้ไป ${(d.used/1048576).toFixed(0)} / ${(d.total/1048576).toFixed(0)} MB`;
    SDI=d.items.sort((a,b)=>(a.t=='d')!=(b.t=='d')?(a.t=='d'?-1:1):(a.show||a.n).toLowerCase()<(b.show||b.n).toLowerCase()?-1:1);
    // path bar
    CR=['/'];let crumbs='<a href="#" onclick="openDir(CR[0]);return false">การ์ด SD</a>';
    if(tr)crumbs+=' / ถังขยะ';else{let acc='';cwd.split('/').filter(Boolean).forEach(p=>{acc+='/'+p;CR.push(acc);crumbs+=` / <a href="#" onclick="openDir(CR[${CR.length-1}]);return false">${esc(p)}</a>`})}
    let tools='';UP=tr?'/':(cwd.replace(/\/[^/]*$/,'')||'/');
    if(cwd!='/')tools+=`<button class="btn" onclick="openDir(UP)">⬆ ขึ้น</button>`;
    if(tr){if(SDI.length)tools+=`<button class="btn" style="color:#c33" onclick="emptyTrash()">ล้างถังขยะ</button>`}
    else{tools+=`<button class="btn" onclick="newFolder()">+ โฟลเดอร์ใหม่</button>`;if(d.trash)tools+=`<button class="btn" onclick="openDir(TRASH)">🗑 ถังขยะ (${d.trash})</button>`}
    const row=(f,i)=>{
      const icon=f.t=='d'?'📁':f.t=='v'?'🎬':'🖼';
      const name=esc(f.show||f.n), size=f.t=='d'?'':`<span class="muted"> ${kb(f.s)}</span>`;
      const from=tr?`<div class="muted" style="font-size:11px">มาจาก ${esc(f.from)}</div>`:'';
      const btns=tr?`<button class="btn" onclick="restoreIt(${i})">กู้คืน</button><button class="btn" style="color:#c33" onclick="purgeIt(${i})">ลบถาวร</button>`
        :`<button class="btn" onclick="renameIt(${i})">เปลี่ยนชื่อ</button><button class="btn" onclick="moveIt(${i})">ย้าย</button><button class="btn" style="color:#c33" onclick="trashIt(${i})">ลบ</button>`;
      const open=f.t=='d'?`onclick="openDir(join(cwd,SDI[${i}].n))" style="cursor:pointer;font-weight:700"`:'';
      return `<div style="border-top:1px solid var(--line);padding:8px 0" id="r${i}"><div ${open} style="font-size:14px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">${icon} ${name}${size}${f.t=='d'?' ›':''}</div>${from}<div class="row" style="margin-top:6px">${btns}</div><div id="mv${i}"></div></div>`};
    box.innerHTML=`<div class="sec">${tr?'ถังขยะ (ลบแล้วยังกู้คืนได้)':'ไฟล์ในการ์ด'}</div><div class="card"><div style="font-size:14px">${crumbs}</div><div class="row" style="flex-wrap:wrap">${tools}</div>
      <div style="margin-top:8px">${SDI.map(row).join('')||'<p class="muted">'+(tr?'ถังขยะว่าง':'ยังไม่มีไฟล์ในโฟลเดอร์นี้')+'</p>'}</div></div>`;
  }catch(e){box.innerHTML='<p class="muted">อ่านการ์ดไม่ได้</p>'}
}
function baseNoExt(n){const i=n.lastIndexOf('.');return i>0?n.slice(0,i):n}
function newFolder(){const n=prompt('ชื่อโฟลเดอร์ใหม่ (A-Z, 0-9, ช่องว่าง, - หรือ _)');if(!n)return;sdOp({op:'mkdir',path:cwd,name:n},'สร้างโฟลเดอร์แล้ว')}
function renameIt(i){const f=SDI[i];const n=prompt('ชื่อใหม่'+(f.t=='d'?'':' (ไม่ต้องใส่ .jpg / .mjpeg)'),f.t=='d'?f.n:baseNoExt(f.n));if(!n||n==f.n)return;sdOp({op:'rename',path:join(cwd,f.n),name:n},'เปลี่ยนชื่อแล้ว')}
function trashIt(i){const f=SDI[i];if(!confirm(f.t=='d'?'ลบโฟลเดอร์ "'+f.n+'"? (ลบได้เฉพาะโฟลเดอร์ว่าง)':'ย้าย "'+f.n+'" ไปถังขยะ? (กู้คืนได้)'))return;sdOp({op:'trash',path:join(cwd,f.n)},f.t=='d'?'ลบโฟลเดอร์แล้ว':'ย้ายไปถังขยะแล้ว')}
function restoreIt(i){sdOp({op:'restore',name:SDI[i].n},'กู้คืนไปที่ '+SDI[i].from+' แล้ว')}
function purgeIt(i){if(!confirm('ลบ "'+SDI[i].show+'" ถาวร? กู้คืนไม่ได้แล้วนะ'))return;sdOp({op:'purge',name:SDI[i].n},'ลบถาวรแล้ว')}
function emptyTrash(){if(!confirm('ล้างถังขยะ ลบ '+SDD.trash+' ไฟล์ถาวร? กู้คืนไม่ได้แล้วนะ'))return;sdOp({op:'empty'},'ล้างถังขยะแล้ว').then(()=>openDir('/'))}
async function moveIt(i){
  const f=SDI[i],src=join(cwd,f.n),box=$('#mv'+i);
  box.innerHTML='<p class="muted">กำลังโหลดรายชื่อโฟลเดอร์...</p>';
  let dirs=await api('/api/sd/dirs');
  dirs=dirs.filter(d=>d!=cwd&&!(f.t=='d'&&(d==src||d.startsWith(src+'/'))));
  if(!dirs.length){box.innerHTML='<p class="muted">ยังไม่มีโฟลเดอร์อื่น สร้างโฟลเดอร์ใหม่ก่อน</p>';return}
  box.innerHTML=`<div class="row" style="align-items:center"><span class="muted" style="white-space:nowrap">ย้ายไป</span><select id="mvs${i}">${dirs.map(d=>`<option value="${esc(d)}">${d=='/'?'การ์ด SD (นอกสุด)':esc(d)}</option>`).join('')}</select>
    <button class="btn dark" onclick="sdOp({op:'move',path:join(cwd,SDI[${i}].n),dest:$('#mvs${i}').value},'ย้ายแล้ว')">ย้าย</button><button class="btn" onclick="$('#mv${i}').innerHTML=''">ยกเลิก</button></div>`;
}
function renderMedia(){
  $('#main').innerHTML=`<div class="sec">ส่งรูป/คลิปจากมือถือเข้าบอร์ด (ลงการ์ด SD)</div>
  <div class="card"><p class="muted" style="margin:0 0 8px">มือถือจะย่อรูปและแปลงคลิปให้พอดีจอบอร์ดเอง ไม่ต้องใช้คอม ระหว่างแปลงคลิปอย่าปิดหรือสลับหน้า</p>
   <p class="muted" style="margin:0 0 8px">ส่งเข้าโฟลเดอร์: <b id="updir"></b> (เปิดโฟลเดอร์อื่นด้านล่างก่อน ถ้าอยากส่งเข้าที่อื่น)</p>
   <div class="row"><label class="btn dark">เลือกรูป<input type="file" accept="image/*" multiple hidden onchange="sendPhotos(this.files)"></label>
   <label class="btn dark">เลือกคลิป<input type="file" accept="video/*" hidden onchange="sendVideo(this.files[0]).catch(e=>{setStatus('ไม่สำเร็จ: '+e.message)})"></label>
   <button class="btn" onclick="loadSd()">รีเฟรช</button></div>
   <p id="mst" style="margin:10px 0 0;font-weight:600"></p><p id="sdinfo" class="muted" style="margin:4px 0 0"></p></div><div id="sdl"></div>`;
  loadSd();
}

renderNav();refresh().then(render).catch(e=>{$('#main').innerHTML='<p>เชื่อมต่อบอร์ดไม่ได้</p>'});
setInterval(()=>{if(tab=='today')refresh()},15000);
</script></body></html>)HTMLPAGE";
