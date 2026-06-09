#include "SettingsServer.h"

#include <Arduino.h>
#include <WiFi.h>

#include "ProjectConfig.h"

#include "AppLog.h"

namespace usage_monitor {

// ---------------------------------------------------------------------------
// Embedded settings page (single-file SPA, three tabs + status)
// ---------------------------------------------------------------------------
static const char kHtml[] = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UsageMonitor</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#f0f0f0;padding:12px;font-size:14px;max-width:640px;margin:0 auto}
h1{font-size:1.1em;margin-bottom:12px;color:#333}
.tabs{display:flex;gap:4px;margin-bottom:12px;flex-wrap:wrap}
.tab{padding:6px 14px;cursor:pointer;border-radius:4px;background:#ccc;border:none;font-size:13px;color:#333;font-family:inherit}
.tab.on{background:#444;color:#fff}
.pane{display:none}.pane.on{display:block}
label{display:block;margin-top:8px;font-size:12px;color:#555;font-weight:600}
input[type=text],input[type=number],select,textarea{width:100%;padding:5px 7px;border:1px solid #bbb;border-radius:3px;font-size:13px;background:#fff;font-family:inherit}
textarea{font-family:monospace;font-size:11px;height:72px;resize:vertical}
details{margin-top:10px;border:1px solid #ddd;border-radius:4px;background:#fff}
summary{padding:8px 10px;cursor:pointer;font-weight:600;font-size:13px;user-select:none}
.inner{padding:4px 10px 10px}
.row{display:flex;gap:8px;margin-top:16px;align-items:center}
.btn{padding:8px 18px;border:none;border-radius:4px;cursor:pointer;font-size:13px;font-weight:600;font-family:inherit}
.save{background:#2a7;color:#fff;flex:1}
.danger{background:#c33;color:#fff}
.info{background:#359;color:#fff}
#msg{font-size:12px;color:#888;margin-left:6px}
.srow{display:flex;justify-content:space-between;padding:4px 0;border-bottom:1px dotted #ddd;font-size:12px}
.srow:last-child{border:0}
.sbox{background:#fff;border:1px solid #ddd;border-radius:4px;padding:10px}
.note{font-size:11px;color:#888;margin-top:4px}
.chkrow{display:flex;align-items:center;gap:8px;margin-top:10px;cursor:pointer}
.chkrow input{width:auto}
.secret{font-family:monospace;font-size:11px;height:44px;-webkit-text-security:disc}
.secret:focus{-webkit-text-security:none}
.clrbtn{display:none;float:right;font-size:11px;padding:2px 8px;margin-left:8px;border:none;border-radius:3px;background:#c33;color:#fff;cursor:pointer;font-family:inherit}
.modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.55);z-index:99;align-items:center;justify-content:center}
.modal.on{display:flex}
.modal .box{background:#fff;border-radius:8px;padding:20px;max-width:300px;text-align:center;box-shadow:0 4px 20px rgba(0,0,0,.4)}
.modal .box p{margin-bottom:14px;font-size:14px;color:#333}
.modal .box .row{justify-content:center}
.sleepclk{margin-left:auto;align-self:center;font-size:12px;color:#888;font-variant-numeric:tabular-nums}
</style>
</head>
<body>
)rawhtml"
"<h1>&#9881; UsageMonitor Settings "
"<span style='font-size:0.75em;color:#999'>v" UM_VERSION "</span></h1>\n"
R"rawhtml(
<div class="tabs">
  <button class="tab on" onclick="go(0)">Credentials</button>
  <button class="tab" onclick="go(1)">Display</button>
  <button class="tab" onclick="go(2)">System</button>
  <button class="tab" onclick="go(3)">Status</button>
  <span id="sleepTimer" class="sleepclk"></span>
</div>

<div id="p0" class="pane on">
  <details><summary id="s_claude">Claude OAuth<button id="clr_claude" class="clrbtn" onclick="clearProv(event,1)">Clear Token</button></summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cl_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cl_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Expires At<input type="datetime-local" id="cl_exp"></label>
    <label>Subscription<select id="cl_sub">
      <option value="free">Free</option>
      <option value="pro">Pro</option>
      <option value="max">Max</option>
    </select></label>
  </div></details>
  <details><summary id="s_claudeplat">Claude Platform (Admin Key)<button id="clr_claudeplat" class="clrbtn" onclick="clearProv(event,7)">Clear Token</button></summary><div class="inner">
    <label>Admin API Key<textarea class="secret" id="cp_key" rows="2" spellcheck="false"></textarea></label>
    <label>Org ID (optional)<input type="text" id="cp_org" spellcheck="false"></label>
    <label>Device shows<span><label class="rad"><input type="radio" name="cp_mode" id="cp_mode_prepaid" value="prepaid" onchange="cpMode()"> Prepaid</label> <label class="rad"><input type="radio" name="cp_mode" id="cp_mode_spend" value="spend" onchange="cpMode()"> Spend</label></span></label>
    <div id="cp_prepaid_box">
    <label>Prepaid Amount ($)<input type="number" step="0.01" id="cp_prepaid" placeholder="e.g. 50.00"></label>
    </div>
    <div id="cp_spend_box">
    <label>Spend window<select id="cp_spendwin"><option value="7">7 day cost</option><option value="14">14 day cost</option><option value="30">30 day cost</option></select></label>
    </div>
    <p class="note">From console.anthropic.com &#8594; API Keys &#8594; Admin Key. <b>Prepaid</b>: device shows prepaid &#8722; 30-day cost remaining. <b>Spend</b>: device shows the chosen 7/14/30-day cost.</p>
  </div></details>
  <details><summary id="s_codex">Codex OAuth<button id="clr_codex" class="clrbtn" onclick="clearProv(event,2)">Clear Token</button></summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cx_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cx_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Account ID<input type="text" id="cx_aid"></label>
    <label>Last Refresh<input type="datetime-local" id="cx_lr"></label>
  </div></details>
  <details><summary id="s_copilot">GitHub Copilot PAT<button id="clr_copilot" class="clrbtn" onclick="clearProv(event,3)">Clear Token</button></summary><div class="inner">
    <label>Personal Access Token<textarea class="secret" id="co_pat" rows="2" spellcheck="false"></textarea></label>
    <p class="note">github.com/settings/tokens &#8594; Classic &#8594; needs "copilot" scope</p>
  </div></details>
  <details><summary id="s_minimax">MiniMax<button id="clr_minimax" class="clrbtn" onclick="clearProv(event,4)">Clear Token</button></summary><div class="inner">
    <label>API Key<textarea class="secret" id="mm_key" rows="2" spellcheck="false"></textarea></label>
    <label>Region<select id="mm_reg"><option value="0">International (api.minimax.io)</option><option value="1">China (api.minimaxi.com)</option></select></label>
  </div></details>
  <details><summary id="s_kimi">Kimi<button id="clr_kimi" class="clrbtn" onclick="clearProv(event,5)">Clear Token</button></summary><div class="inner">
    <label>Auth Token (browser cookie kimi-auth)<textarea class="secret" id="ki_tok" rows="2" spellcheck="false"></textarea></label>
    <p class="note">Extract from www.kimi.com DevTools. No refresh &#8212; re-enter when expired.</p>
  </div></details>
  <details><summary id="s_zai">Zai / Zhipu<button id="clr_zai" class="clrbtn" onclick="clearProv(event,6)">Clear Token</button></summary><div class="inner">
    <label>API Key<textarea class="secret" id="za_key" rows="2" spellcheck="false"></textarea></label>
    <label>Endpoint<input type="text" id="za_ep" placeholder="https://api.z.ai"></label>
  </div></details>
  <details><summary>Local Stats Server</summary><div class="inner">
    <label>Base URL (leave empty to disable)<input type="text" id="ls_url" placeholder="http://192.168.x.x:8787"></label>
  </div></details>
</div>

<div id="p1" class="pane">
  <label style="margin-top:0">LEFT Column Provider
    <select id="left_prov">
      <option value="0">&#8212; None &#8212;</option>
      <option value="1">Claude OAuth</option>
      <option value="7">Claude Platform</option>
      <option value="2">Codex</option>
      <option value="3">Copilot</option>
      <option value="5">Kimi</option>
      <option value="4">MiniMax</option>
      <option value="6">Zai</option>
    </select>
  </label>
  <label style="margin-top:14px">RIGHT Column Provider
    <select id="right_prov">
      <option value="0">&#8212; None &#8212;</option>
      <option value="1">Claude OAuth</option>
      <option value="7">Claude Platform</option>
      <option value="2">Codex</option>
      <option value="3">Copilot</option>
      <option value="5">Kimi</option>
      <option value="4">MiniMax</option>
      <option value="6">Zai</option>
    </select>
  </label>
</div>

<div id="p2" class="pane">
  <label style="margin-top:0">Timezone
    <select id="tz_sel" onchange="onTzSel(this.value)">
      <option value="UTC0">UTC</option>
      <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern</option>
      <option value="CST6CDT,M3.2.0,M11.1.0">US Central</option>
      <option value="MST7MDT,M3.2.0,M11.1.0">US Mountain</option>
      <option value="MST7">US Mountain (no DST / Arizona)</option>
      <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific</option>
      <option value="AKST9AKDT,M3.2.0,M11.1.0">US Alaska</option>
      <option value="HST10">US Hawaii</option>
      <option value="GMT0BST,M3.5.0/1,M10.5.0">UK / Ireland</option>
      <option value="CET-1CEST,M3.5.0,M10.5.0/3">Central Europe (Paris / Berlin / Rome)</option>
      <option value="EET-2EEST,M3.5.0/3,M10.5.0/4">Eastern Europe (Athens / Helsinki)</option>
      <option value="MSK-3">Moscow</option>
      <option value="GST-4">Gulf (Dubai / Abu Dhabi)</option>
      <option value="IST-5:30">India (IST)</option>
      <option value="BST-6">Bangladesh</option>
      <option value="ICT-7">Indochina (Bangkok / Hanoi)</option>
      <option value="CST-8">China / Singapore / Taiwan</option>
      <option value="JST-9">Japan</option>
      <option value="KST-9">Korea</option>
      <option value="AEST-10AEDT,M10.1.0,M4.1.0/3">Australia East (Sydney / Melbourne)</option>
      <option value="AWST-8">Australia West (Perth)</option>
      <option value="NZST-12NZDT,M9.5.0,M4.1.0/3">New Zealand</option>
      <option value="custom">Custom POSIX string&#8230;</option>
    </select>
    <input type="text" id="tz_custom" placeholder="e.g. EST5EDT,M3.2.0,M11.1.0" style="display:none;margin-top:4px">
  </label>
  <label style="margin-top:12px">Refresh Interval: <strong id="ref_lbl">5 min</strong>
    <input type="range" id="ref_sec" min="300" max="3600" step="60" oninput="updRef(this.value)" style="width:100%;margin-top:4px">
  </label>
  <label style="margin-top:12px">Battery full (mV)<input type="number" id="batt_full" min="3500" max="5000" step="10" placeholder="4200"></label>
  <label class="chkrow" style="margin-top:14px">
    <input type="checkbox" id="dark">
    <span>Dark mode (screen)</span>
  </label>
  <label style="margin-top:14px">Device font<select id="ui_font">
      <option value="0">Arimo (Arial)</option>
      <option value="1">Roboto</option>
      <option value="2">Open Sans</option>
      <option value="3">Noto Sans</option>
      <option value="4">Source Sans 3</option>
      <option value="5">IBM Plex Sans</option>
      <option value="6">Fira Sans</option>
      <option value="7">DejaVu Sans</option>
    </select></label>
  <p class="note" style="margin-top:4px">Changing the font repaints the screen on Save (no reboot).</p>
  <label class="chkrow" style="margin-top:10px">
    <input type="checkbox" id="secure">
    <span>Secure Tokens (hide saved tokens; reveal only what you type this session)</span>
  </label>
  <label class="chkrow" style="margin-top:10px">
    <input type="checkbox" id="deep_sleep">
    <span>Enable Deep Sleep between fetches</span>
  </label>
  <p class="note" style="margin-top:4px">Deep Sleep: settings page is only available for 5 min after power-on/reset. Device sleeps between fetches.</p>
  <div class="row" style="margin-top:20px">
    <button class="btn info" onclick="doRestart()">Restart Device</button>
    <button class="btn danger" onclick="doWifiReset()">Reset WiFi</button>
  </div>
</div>

<div id="p3" class="pane">
  <div class="sbox" id="st_box"><div class="srow"><span>Loading&#8230;</span></div></div>
</div>

<div class="row">
  <button class="btn save" onclick="doSave()">Save Settings</button>
  <span id="msg"></span>
</div>
<p class="note" style="text-align:center;margin-top:6px">Press Green button on device to wake up and access this page.</p>

<div id="sleepModal" class="modal"><div class="box">
  <p>Device will sleep in <strong id="sleepCd">30</strong>s.</p>
  <div class="row">
    <button class="btn save" onclick="keepAlive()">Continue Session</button>
    <button class="btn danger" onclick="sleepNow()">Sleep</button>
  </div>
</div></div>

<div id="wakeBox" class="modal"><div class="box" style="border:2px solid #2a7">
  <p id="wakeMsg" style="color:#176">Press Green button on device to wake up and access this page.</p>
  <div class="row"><button class="btn save" onclick="wakeOk()">OK</button></div>
</div></div>

<script>
const NPANE=4;
let curTab=0;
function go(n){
  curTab=n;
  for(let i=0;i<NPANE;i++){
    document.getElementById('p'+i).classList.toggle('on',i===n);
    document.querySelectorAll('.tab')[i].classList.toggle('on',i===n);
  }
  if(n===3)loadSt();
}
function updRef(v){
  document.getElementById('ref_lbl').textContent=Math.round(v/60)+' min';
}
function onTzSel(v){
  document.getElementById('tz_custom').style.display=v==='custom'?'block':'none';
}
const STR_IDS=['cl_at','cl_rt','cl_sub',
               'cx_at','cx_rt','cx_aid',
               'co_pat','mm_key','ki_tok',
               'za_key','za_ep','cp_key','cp_org','cp_prepaid','cp_spendwin','ls_url'];
// datetime-local <-> wire format (cl_exp: ms epoch string, cx_lr: ISO8601)
function msToLocal(ms){const n=parseInt(ms);if(!n)return'';
  const d=new Date(n);return new Date(n-d.getTimezoneOffset()*60000).toISOString().slice(0,16);}
function localToMs(v){return v?String(new Date(v).getTime()):'0';}
function isoToLocal(s){const t=Date.parse(s);return isNaN(t)?'':msToLocal(t);}
function localToIso(v){return v?new Date(v).toISOString():'0';}
const SECRET_IDS=['cl_at','cl_rt','cx_at','cx_rt','co_pat','mm_key','ki_tok','za_key','cp_key'];
function populate(c){
  STR_IDS.forEach(id=>{
    const el=document.getElementById(id);
    if(el)el.value=c[id]??'';
  });
  // Secrets: echoed (real value) only when Secure Tokens is off; otherwise blank
  // with a "saved" placeholder. Masking/reveal is handled by the .secret CSS.
  SECRET_IDS.forEach(id=>{
    const el=document.getElementById(id);
    if(el){el.value=c[id]??'';el.placeholder=c[id+'_set']?'saved — leave blank to keep':'not set';}
  });
  const sub=document.getElementById('cl_sub');
  if(sub){
    const v=String(c.cl_sub??'pro').toLowerCase();
    sub.value=['free','pro','max'].includes(v)?v:'pro';
  }
  const ce=document.getElementById('cl_exp');if(ce)ce.value=msToLocal(c.cl_exp);
  const cx=document.getElementById('cx_lr');if(cx)cx.value=isoToLocal(c.cx_lr??'');
  const mm=document.getElementById('mm_reg');if(mm)mm.value=String(c.mm_reg??0);
  const rs=document.getElementById('ref_sec');if(rs){rs.value=c.ref_sec??300;updRef(rs.value);}
  const bf=document.getElementById('batt_full');if(bf)bf.value=c.batt_full??4200;
  const ds=document.getElementById('deep_sleep');if(ds)ds.checked=!!c.deep_sleep;
  const dk=document.getElementById('dark');if(dk)dk.checked=!!c.dark;
  const uf=document.getElementById('ui_font');if(uf)uf.value=String(c.ui_font??0);
  const se=document.getElementById('secure');if(se)se.checked=!!c.secure;
  const lp=document.getElementById('left_prov');if(lp)lp.value=String(c.left_prov??0);
  const rp=document.getElementById('right_prov');if(rp)rp.value=String(c.right_prov??0);
  const cpm=String(c.cp_mode??'prepaid')==='spend'?'spend':'prepaid';
  const cpr=document.getElementById('cp_mode_'+cpm);if(cpr)cpr.checked=true;
  cpMode();
  const tzv=c.tz??'UTC0';
  const tzSel=document.getElementById('tz_sel');
  const tzCust=document.getElementById('tz_custom');
  let tzMatch=false;
  if(tzSel){for(let i=0;i<tzSel.options.length;i++){if(tzSel.options[i].value===tzv){tzSel.value=tzv;tzMatch=true;break;}}}
  if(!tzMatch&&tzSel){tzSel.value='custom';if(tzCust){tzCust.value=tzv;tzCust.style.display='block';}}
}
function collect(){
  const d={};
  STR_IDS.forEach(id=>{d[id]=document.getElementById(id)?.value??'';});
  d.cl_exp=localToMs(document.getElementById('cl_exp')?.value);
  d.cx_lr=localToIso(document.getElementById('cx_lr')?.value);
  d.mm_reg=parseInt(document.getElementById('mm_reg')?.value??'0');
  d.ref_sec=parseInt(document.getElementById('ref_sec').value);
  d.deep_sleep=document.getElementById('deep_sleep').checked;
  d.dark=document.getElementById('dark').checked;
  d.ui_font=parseInt(document.getElementById('ui_font').value);
  d.secure=document.getElementById('secure').checked;
  d.left_prov=parseInt(document.getElementById('left_prov').value);
  d.right_prov=parseInt(document.getElementById('right_prov').value);
  d.batt_full=parseInt(document.getElementById('batt_full')?.value||'4200');
  const cpr=document.querySelector('input[name=cp_mode]:checked');
  d.cp_mode=cpr?cpr.value:'prepaid';
  const tzSel=document.getElementById('tz_sel');
  d.tz=tzSel&&tzSel.value==='custom'?(document.getElementById('tz_custom')?.value??'UTC0'):(tzSel?.value??'UTC0');
  return d;
}
// Show the prepaid amount vs spend-window field based on the cp_mode radio.
function cpMode(){
  const spend=document.getElementById('cp_mode_spend')?.checked;
  const pb=document.getElementById('cp_prepaid_box');
  const sb=document.getElementById('cp_spend_box');
  if(pb)pb.style.display=spend?'none':'block';
  if(sb)sb.style.display=spend?'block':'none';
}
let msgTimer=null;
function setMsg(t,c){
  const m=document.getElementById('msg');m.textContent=t;m.style.color=c;
  if(msgTimer)clearTimeout(msgTimer);
  if(t)msgTimer=setTimeout(()=>{m.textContent='';},15000);
}
async function doSave(){
  setMsg('Saving…','#888');
  try{
    const r=await fetch('/api/settings',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(collect())});
    // Token tests only run when a credential changed (Credentials tab); only
    // show "Testing tokens" there.
    if(r.ok){
      if(curTab===0){setMsg('Saved! Testing tokens…','green');pollCredFor(30000);}
      else setMsg('Saved!','green');
      pollSleep();   // the save extended the window; refresh the countdown now
    }
    else setMsg('Error '+r.status,'red');
  }catch(e){setMsg('Failed','red');}
}
async function doRestart(){
  if(!confirm('Restart device?'))return;
  setMsg('Restarting…','#888');
  await fetch('/api/restart',{method:'POST'}).catch(()=>{});
  // Poll until the device is back, then reload the page.
  const deadline=Date.now()+90000;
  await new Promise(r=>setTimeout(r,4000));
  while(Date.now()<deadline){
    try{
      const r=await fetch('/api/status',{cache:'no-store'});
      if(r.ok){setMsg('Restarted','green');location.reload();return;}
    }catch(e){}
    await new Promise(r=>setTimeout(r,2000));
  }
  setMsg('Still offline — refresh manually','red');
}
async function doWifiReset(){
  if(!confirm('Clear WiFi credentials and restart?'))return;
  setMsg('Clearing WiFi…','#888');
  await fetch('/api/wifi-reset',{method:'POST'}).catch(()=>{});
}
const PNAMES=['None','Claude OAuth','Codex','Copilot','MiniMax','Kimi','Zai','Claude Platform'];
async function loadSt(){
  try{
    const d=await fetch('/api/status').then(r=>r.json());
    const up=d.uptime_sec|0;
    const h=Math.floor(up/3600),m=Math.floor((up%3600)/60),s=up%60;
    const str=(r)=>(r==null||r===0)?'?':(r>=-60?'High':(r>=-72?'Med':'Low'));
    let battStr='?';
    if(d.batt!=null&&d.batt>=0){
      battStr=d.batt+'% | '+(d.batt_mv>=0?d.batt_mv:'?')+'mv';
      if(d.batt_days!=null&&d.batt_days>=0)
        battStr+=' | Est. '+Math.floor(d.batt_days/24)+' days '+(d.batt_days%24)+' hours on battery';
    }
    const rows=[
      ['IP Address',d.ip??'?'],
      ['WiFi',(d.ssid??'?')+' | '+str(d.rssi)],
      ['Battery',battStr],
      ['Uptime',h+'h '+m+'m '+s+'s'],
      ['LEFT',PNAMES[d.left_prov??0]??'?'],
      ['RIGHT',PNAMES[d.right_prov??0]??'?'],
    ];
    document.getElementById('st_box').innerHTML=
      rows.map(([k,v])=>`<div class="srow"><span>${k}</span><strong>${v}</strong></div>`).join('');
  }catch(e){
    document.getElementById('st_box').innerHTML='<div class="srow"><span>Unavailable</span></div>';
  }
}
// Map each provider to its secret-field boxes; color them by test status.
const PROV_FIELDS={claude:['cl_at','cl_rt'],codex:['cx_at','cx_rt'],copilot:['co_pat'],
  minimax:['mm_key'],kimi:['ki_tok'],zai:['za_key'],claudeplat:['cp_key']};
const CRED_BG={ok:'#d6f5d6',fail:'#f8d2d2',none:'',testing:''};
const HDR_BG={ok:'#8fdcb4',fail:'#f2aac0',none:'',testing:''};
const SUMMARY={claude:'s_claude',claudeplat:'s_claudeplat',codex:'s_codex',
  copilot:'s_copilot',minimax:'s_minimax',kimi:'s_kimi',zai:'s_zai'};
function applyCred(st){
  for(const p in PROV_FIELDS){
    const k=st[p]??'none';
    PROV_FIELDS[p].forEach(id=>{const el=document.getElementById(id);if(el)el.style.background=CRED_BG[k]??'';});
    const sm=document.getElementById(SUMMARY[p]);if(sm)sm.style.background=HDR_BG[k]??'';
    // Clear Token only when a token exists (green/red); hidden when white.
    const btn=document.getElementById('clr_'+p);if(btn)btn.style.display=(k==='ok'||k==='fail')?'inline-block':'none';
  }
}
async function pollCred(){try{applyCred(await fetch('/api/credstatus',{cache:'no-store'}).then(r=>r.json()));}catch(e){}}
async function clearProv(ev,id){
  ev.stopPropagation();ev.preventDefault();   // don't toggle the <details>
  if(!confirm('Clear this provider’s token? It will stop being used.'))return;
  try{
    const r=await fetch('/api/clearprovider?prov='+id,{method:'POST'});
    if(r.ok){
      // re-pull settings so cleared fields blank out, then recolor white
      const c=await fetch('/api/settings',{cache:'no-store'}).then(x=>x.json());
      populate(c);await pollCred();setMsg('Token cleared','green');
    }else setMsg('Clear failed','red');
  }catch(e){setMsg('Failed','red');}
}
function pollCredFor(ms){const end=Date.now()+ms;const t=setInterval(()=>{pollCred();if(Date.now()>end)clearInterval(t);},2000);}
fetch('/api/settings',{cache:'no-store'}).then(r=>r.json()).then(populate).catch(console.error);
pollCred();

// Sleep-warning modal + countdown: passively poll sleep_in (does NOT keep the
// device awake); show a Continue/Sleep modal at <=30s and a live MM:SS timer.
const sModal=document.getElementById('sleepModal');
const sCd=document.getElementById('sleepCd');
const sTimer=document.getElementById('sleepTimer');
const wBox=document.getElementById('wakeBox');
const wMsg=document.getElementById('wakeMsg');
const WAKE_MSG='Press Green button on device to wake up and access this page.';
const ASLEEP_MSG='Device is asleep — press the Green button to wake.';
async function wakeOk(){
  // Verify the device is reachable; if it's asleep, tell the user it's not ready.
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    if(r.ok){wBox.classList.remove('on');resync();}
    else{wMsg.textContent='Device not ready';}
  }catch(e){wMsg.textContent='Device not ready';}
}
let sleepRemain=-1;       // seconds until sleep (-1 = deep sleep off)
let userSleeping=false;   // user pressed Sleep -> never re-show modal
function renderTimer(){
  if(sleepRemain<0){sTimer.textContent='';return;}
  const s=Math.max(0,sleepRemain|0);
  sTimer.textContent='Sleep in '+String((s/60)|0).padStart(2,'0')+':'+String(s%60).padStart(2,'0');
}
setInterval(()=>{
  if(sleepRemain>0)sleepRemain--;
  renderTimer();
  // At 00:01 (about to sleep) show the green "press button" box; hide the
  // Continue/Sleep modal. The box stays up while the device is asleep and is
  // cleared on the next wake (boot_id change).
  if(sleepRemain===1 && !userSleeping){
    sModal.classList.remove('on');
    wMsg.textContent=WAKE_MSG;
    wBox.classList.add('on');
  }
  // At 00:00 the device has slept: keep the box, switch to the asleep message.
  if(sleepRemain===0 && !userSleeping){
    sModal.classList.remove('on');
    wMsg.textContent=ASLEEP_MSG;
    wBox.classList.add('on');
  }
},1000);
async function keepAlive(){
  try{await fetch('/api/keepalive',{method:'POST'});}catch(e){}
  userSleeping=false;sModal.classList.remove('on');wBox.classList.remove('on');pollSleep();
}
async function sleepNow(){
  try{await fetch('/api/sleepnow',{method:'POST'});}catch(e){}
  userSleeping=true;sModal.classList.remove('on');sleepRemain=0;renderTimer();
  setMsg('Sleeping…','#888');
}
let wasOnline=true;   // device reachability; false while it's asleep/unreachable
let bootId=null;      // device session token; changes every wake (boot_id)
function resync(){     // pull fresh values after a wake / reconnect
  fetch('/api/settings',{cache:'no-store'}).then(r=>r.json()).then(populate).catch(()=>{});
  pollCred();
}
async function pollSleep(){
  if(userSleeping && wasOnline){sModal.classList.remove('on');sleepRemain=0;renderTimer();return;}
  try{
    const d=await fetch('/api/status',{cache:'no-store'}).then(r=>r.json());
    // boot_id changes on every device wake -> treat as a fresh session: clear
    // stale counters/cache, resync, and (if this tab is visible) keep it awake.
    const woke=(bootId!==null && d.boot_id!==bootId);
    if(woke || !wasOnline){
      wasOnline=true;userSleeping=false;wBox.classList.remove('on');resync();
      if(woke && document.visibilityState==='visible')keepAlive();  // auto 2-min
    }
    bootId=d.boot_id;
    const s=d.sleep_in;
    sleepRemain=(s==null)?-1:s;renderTimer();
    if(s!=null && s>=0 && s<=30){sCd.textContent=s;sModal.classList.add('on');}
    else sModal.classList.remove('on');
  }catch(e){
    wasOnline=false;   // unreachable -> asleep; timer holds at 00:00
    sleepRemain=0;renderTimer();sModal.classList.remove('on');
  }
}
setInterval(pollSleep,5000);pollSleep();
</script>
</body>
</html>
)rawhtml";

// ---------------------------------------------------------------------------

// Escape a string for safe inclusion inside a JSON string literal: backslash,
// double-quote, and control chars (< 0x20). SSID is user/AP-controlled.
static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 8);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (static_cast<uint8_t>(c) < 0x20) {
      char buf[7];
      snprintf(buf, sizeof(buf), "\\u%04x", c);
      out += buf;
    } else {
      out += c;
    }
  }
  return out;
}

// Send a response with caching fully disabled, so an open tab / bfcache / proxy
// never serves stale state (settings, status, credential colors all change live).
static void sendNoCache(AsyncWebServerRequest* req, int code, const char* type,
                        const String& body) {
  AsyncWebServerResponse* res = req->beginResponse(code, type, body);
  res->addHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  res->addHeader("Pragma", "no-cache");
  req->send(res);
}

void SettingsServer::begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)(),
                           std::function<void()> onSaved,
                           std::function<String()> credJson,
                           std::function<void()> onKeepAlive,
                           std::function<void()> onSleepNow,
                           std::function<int()> sleepInSec,
                           std::function<int()> bootId,
                           int (*battMv)(),
                           std::function<int()> battDays) {
  cfg_ = cfg;
  battPct_ = battPct;
  battMv_ = battMv;
  battDays_ = battDays;
  onSaved_ = onSaved;
  credJson_ = credJson;
  onKeepAlive_ = onKeepAlive;
  onSleepNow_ = onSleepNow;
  sleepInSec_ = sleepInSec;
  bootId_ = bootId;

  // GET / → settings page
  server->on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "text/html", kHtml);
  });

  // GET /api/settings → JSON
  server->on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "application/json", cfg_->toJson());
  });

  // POST /api/settings → update + save
  server->on("/api/settings", HTTP_POST,
    [this](AsyncWebServerRequest* req) {
      String* body = reinterpret_cast<String*>(req->_tempObject);
      if (!body) {  // oversize/aborted body was dropped by the upload handler
        sendNoCache(req, 413, "application/json", "{\"ok\":false,\"error\":\"too_large\"}");
        return;
      }
      if (cfg_->fromJson(*body)) {
        cfg_->save();
        sysLog("[web] save -> extend awake");
        if (onSaved_)    onSaved_();      // apply changes + repaint (async-safe flag)
        if (onKeepAlive_) onKeepAlive_(); // a save is a user action -> extend NOW
        sendNoCache(req, 200, "application/json", "{\"ok\":true}");
      } else {
        sendNoCache(req, 400, "application/json", "{\"ok\":false,\"error\":\"parse\"}");
      }
      delete body; req->_tempObject = nullptr;
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
       size_t index, size_t total) {
      static const size_t kMaxBody = 8192;   // settings JSON is well under this
      if (index == 0) {
        if (req->_tempObject) { delete reinterpret_cast<String*>(req->_tempObject);
                                req->_tempObject = nullptr; }
        if (total > kMaxBody) return;        // reject up front; body stays null
        String* b = new String();
        b->reserve(total > 0 ? total : 512);
        req->_tempObject = b;
      }
      String* b = reinterpret_cast<String*>(req->_tempObject);
      if (!b) return;                        // already rejected
      if (b->length() + len > kMaxBody) {    // chunked without Content-Length
        delete b; req->_tempObject = nullptr;
        return;
      }
      b->concat(reinterpret_cast<const char*>(data), len);
    }
  );

  // GET /api/credstatus → per-provider credential test status
  server->on("/api/credstatus", HTTP_GET, [this](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "application/json", credJson_ ? credJson_() : "{}");
  });

  // POST /api/keepalive → user chose Continue Session: keep awake 2 minutes
  server->on("/api/keepalive", HTTP_POST, [this](AsyncWebServerRequest* req) {
    sysLog("[web] keepalive (Continue Session)");
    if (onKeepAlive_) onKeepAlive_();
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // POST /api/sleepnow → user chose Sleep: enter deep sleep now
  server->on("/api/sleepnow", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (onSleepNow_) onSleepNow_();
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // POST /api/clearprovider?prov=N → wipe that provider's credentials
  server->on("/api/clearprovider", HTTP_POST, [this](AsyncWebServerRequest* req) {
    int prov = 0;
    if (req->hasParam("prov")) prov = req->getParam("prov")->value().toInt();
    if (prov < 1 || prov > 7) {
      sendNoCache(req, 400, "application/json", "{\"ok\":false,\"error\":\"prov\"}");
      return;
    }
    cfg_->clearProvider((uint8_t)prov);
    cfg_->save();
    sysLog("[web] clear provider %d -> extend awake", prov);
    if (onSaved_)     onSaved_();    // re-wire providers + repaint (clears cache, status)
    if (onKeepAlive_) onKeepAlive_();
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // GET /api/status → live info
  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    String ip   = jsonEscape(WiFi.localIP().toString());
    String ssid = jsonEscape(WiFi.SSID());
    unsigned long up = millis() / 1000UL;
    const int batt = battPct_ ? battPct_() : -1;       // reads ADC, also sets mV
    const int battMv = battMv_ ? battMv_() : -1;
    const int sleepIn = sleepInSec_ ? sleepInSec_() : -1;  // passive: does NOT extend
    const int boot = bootId_ ? bootId_() : 0;
    const int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    const int battDays = battDays_ ? battDays_() : -1;   // est hours on battery, -1 = n/a
    String json = "{\"ip\":\"" + ip + "\","
                  "\"ssid\":\"" + ssid + "\","
                  "\"rssi\":" + String(rssi) + ","
                  "\"batt\":" + String(batt) + ","
                  "\"batt_mv\":" + String(battMv) + ","
                  "\"batt_days\":" + String(battDays) + ","
                  "\"sleep_in\":" + String(sleepIn) + ","
                  "\"boot_id\":" + String(boot) + ","
                  "\"uptime_sec\":" + String(up) + ","
                  "\"left_prov\":"  + String(cfg_->leftProvider()) + ","
                  "\"right_prov\":" + String(cfg_->rightProvider()) + "}";
    sendNoCache(req, 200, "application/json", json);
  });

  // POST /api/restart
  server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
  });

  // POST /api/wifi-reset → erase stored credentials + restart
  server->on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
    delay(200);
    WiFi.disconnect(true, true);  // wifioff=true, eraseap=true
    delay(100);
    ESP.restart();
  });

  sysLog("[settings] routes registered");
}

}  // namespace usage_monitor
