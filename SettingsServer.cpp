#include "SettingsServer.h"

#include <Arduino.h>
#include <WiFi.h>
#include <cstddef>
#include <cstdlib>
#include <cstring>

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
body:not(.adv) .adv-only{display:none}body.adv .simple-only{display:none}
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
.apitest{float:right;font-size:11px;font-weight:400;margin-left:8px;display:inline-flex;align-items:center;gap:4px;cursor:pointer;color:#555}
.apitest input{width:auto;margin:0}
.apierr{font-size:12px;color:#c33;margin-top:8px;white-space:pre-line}
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
  <button class="tab adv-only" onclick="go(4)">Font Testing</button>
  <span id="sleepTimer" class="sleepclk"></span>
</div>

<div id="p0" class="pane on">
  <p class="note" style="margin-top:8px">Check the API Test box to have a provider's token tested when you press Save Settings.</p>
  <details><summary id="s_claude">Claude OAuth<button id="clr_claude" class="clrbtn" onclick="clearProv(event,1)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_claude" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cl_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cl_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Expires At (epoch)<input type="text" id="cl_exp" inputmode="numeric" spellcheck="false" oninput="updExp()" placeholder="from .credentials.json expiresAt, e.g. 1781422972619"></label>
    <div id="cl_exp_h" class="note" style="margin-top:2px"></div>
    <label>Subscription<select id="cl_sub">
      <option value="free">Free</option>
      <option value="pro">Pro</option>
      <option value="max">Max</option>
    </select></label>
  </div></details>
  <details><summary id="s_claudeplat">Claude Platform (Admin Key)<button id="clr_claudeplat" class="clrbtn" onclick="clearProv(event,7)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_claudeplat" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
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
  <details><summary id="s_codex">Codex OAuth<button id="clr_codex" class="clrbtn" onclick="clearProv(event,2)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_codex" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cx_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cx_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Account ID<input type="text" id="cx_aid"></label>
    <label>Last Refresh<input type="datetime-local" id="cx_lr"></label>
  </div></details>
  <details><summary id="s_copilot">GitHub Copilot PAT<button id="clr_copilot" class="clrbtn" onclick="clearProv(event,3)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_copilot" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
    <label>Personal Access Token<textarea class="secret" id="co_pat" rows="2" spellcheck="false"></textarea></label>
    <p class="note">github.com/settings/tokens &#8594; Classic &#8594; needs "copilot" scope</p>
  </div></details>
  <details><summary id="s_minimax">MiniMax<button id="clr_minimax" class="clrbtn" onclick="clearProv(event,4)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_minimax" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
    <label>API Key<textarea class="secret" id="mm_key" rows="2" spellcheck="false"></textarea></label>
    <label>Region<select id="mm_reg"><option value="0">International (api.minimax.io)</option><option value="1">China (api.minimaxi.com)</option></select></label>
  </div></details>
  <details><summary id="s_kimi">Kimi<button id="clr_kimi" class="clrbtn" onclick="clearProv(event,5)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_kimi" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
    <label>Auth Token (browser cookie kimi-auth)<textarea class="secret" id="ki_tok" rows="2" spellcheck="false"></textarea></label>
    <p class="note">Extract from www.kimi.com DevTools. No refresh &#8212; re-enter when expired.</p>
  </div></details>
  <details><summary id="s_zai">Zai / Zhipu<button id="clr_zai" class="clrbtn" onclick="clearProv(event,6)">Clear Token</button><label class="apitest" onclick="event.stopPropagation()"><input type="checkbox" id="t_zai" onclick="event.stopPropagation()">API Test</label></summary><div class="inner">
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
  <label class="chkrow" style="margin-top:0"><input type="checkbox" id="adv" onchange="toggleAdv()"><span>Advanced (show all options)</span></label>
  <label style="margin-top:12px">Timezone
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
  <label class="adv-only" style="margin-top:12px">Battery full (mV)<input type="number" id="batt_full" min="3500" max="5000" step="10" placeholder="4200"></label>
  <label class="chkrow" style="margin-top:14px">
    <input type="checkbox" id="dark">
    <span>Dark mode (screen)</span>
  </label>
  <label style="margin-top:14px">Device font<select id="ui_font">
      <option value="0">Arimo (Arial)</option>
      <option value="1">DejaVu Sans</option>
      <option value="2">Atkinson Hyperlegible</option>
      <option value="3">B612</option>
      <option value="4">Lexend</option>
      <option value="5">Hack (mono)</option>
      <option value="6">JetBrains Mono</option>
      <option value="7">Carlito</option>
      <option value="8">Roboto</option>
      <option value="9">Open Sans</option>
      <option value="10">Noto Sans</option>
      <option value="11">Source Sans 3</option>
      <option value="12">IBM Plex Sans</option>
      <option value="13">Fira Sans</option>
    </select></label>
  <label class="chkrow simple-only" style="margin-top:10px">
    <input type="checkbox" id="ui_hardblack" onchange="syncHB()">
    <span>Hard Black (Crisp, no antialias)</span>
  </label>
  <label class="chkrow adv-only" style="margin-top:10px">
    <input type="checkbox" id="ui_aa" onchange="syncAA()">
    <span>Smooth text (grayscale anti-aliasing) &#8212; off = crisp 1-bit</span>
  </label>
  <label class="chkrow adv-only" style="margin-top:10px">
    <input type="checkbox" id="ui_smcrisp">
    <span>Crisp small text (baked bitmap) &#8212; off = smooth vector. Big numbers/titles unaffected.</span>
  </label>
  <label class="adv-only" style="margin-top:10px">Text sharpness (smooth mode): <strong id="sharp_lbl">50</strong>
    <input type="range" id="ui_sharp" min="0" max="100" step="5" oninput="document.getElementById('sharp_lbl').textContent=this.value" style="width:100%;margin-top:4px">
  </label>
  <label class="adv-only" style="margin-top:10px">Text weight &#8212; small-text darkness (smooth mode): <strong id="weight_lbl">45</strong>
    <input type="range" id="ui_weight" min="0" max="100" step="5" oninput="document.getElementById('weight_lbl').textContent=this.value" style="width:100%;margin-top:4px">
  </label>
  <p class="note adv-only" style="margin-top:4px">Higher = sharper/heavier edges, lower = softer. Text weight darkens thin small text (0 = off). Font / smoothing / sharpness / weight changes repaint on Save (no reboot).</p>
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

<div id="p4" class="pane">
  <label class="chkrow"><input type="checkbox" id="ft_on" onchange="ftOn()"><span>Font Testing on Device Screen &#8212; not saved; off after reboot</span></label>
  <div id="ft_sub" style="display:none">
  <label class="chkrow" style="margin-top:10px"><input type="checkbox" id="ft_allview" onchange="ftAllView()"><span>All Font View (every font, one line) &#8212; applies on Save</span></label>
  <label id="ft_fontrow" style="margin-top:14px">Test font<select id="ft_font" onchange="ftSendLive()">
      <option value="0">Arimo (Arial)</option>
      <option value="1">DejaVu Sans</option>
      <option value="2">Atkinson Hyperlegible</option>
      <option value="3">B612</option>
      <option value="4">Lexend</option>
      <option value="5">Hack (mono)</option>
      <option value="6">JetBrains Mono</option>
      <option value="7">Carlito</option>
      <option value="8">Roboto</option>
      <option value="9">Open Sans</option>
      <option value="10">Noto Sans</option>
      <option value="11">Source Sans 3</option>
      <option value="12">IBM Plex Sans</option>
      <option value="13">Fira Sans</option>
    </select></label>
  <div id="ft_crisprow" style="display:none"><label class="chkrow" style="margin-top:10px"><input type="checkbox" id="ft_crisp"><span>Crisp text (hard black, no anti-alias) &#8212; applies on Save</span></label></div>
  <label class="chkrow" style="margin-top:10px"><input type="checkbox" id="ft_dark"><span>Dark mode (test screen) &#8212; applies on Save</span></label>
  <div class="row" style="margin-top:12px">
    <button class="btn" onclick="ftPage(-1)">&#9664; Previous Page</button>
    <button class="btn" onclick="ftPage(1)">Next Page &#9654;</button>
  </div>
  <p class="note" id="ft_now" style="margin-top:8px">Off</p>
  </div>
</div>

<div class="row">
  <button class="btn save" onclick="doSave()">Save Settings</button>
  <span id="msg"></span>
</div>
<div id="apierr" class="apierr"></div>
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
// CSRF guard: tag every state-changing (non-GET) request with a custom header.
// A cross-origin page cannot set this header without a preflight the device
// rejects, so drive-by POSTs (and the text/plain body trick) are blocked.
const _f=window.fetch;
window.fetch=(u,o)=>{o=o||{};if((o.method||'GET').toUpperCase()!=='GET'){o.headers=Object.assign({},o.headers||{},{'X-UM-CSRF':'1'});}return _f(u,o);};
const NPANE=5;
const FT_SIZES=[6,7,8,10,12,14,16,18,24,30,34];
let ftSizeIdx=0;
let curTab=0;
function go(n){
  curTab=n;
  for(let i=0;i<NPANE;i++){
    document.getElementById('p'+i).classList.toggle('on',i===n);
    document.querySelectorAll('.tab')[i].classList.toggle('on',i===n);
  }
  if(n===3)loadSt();
}
function toggleAdv(){
  const on=document.getElementById('adv').checked;
  document.body.classList.toggle('adv',on);
  try{localStorage.setItem('um_adv',on?'1':'0');}catch(e){}
}
// Hard Black and Smooth Text are the same ui_aa bool (opposite polarity); keep in sync.
function syncHB(){document.getElementById('ui_aa').checked=!document.getElementById('ui_hardblack').checked;}
function syncAA(){document.getElementById('ui_hardblack').checked=!document.getElementById('ui_aa').checked;}
function ftLabel(){
  const on=document.getElementById('ft_on').checked;
  const all=document.getElementById('ft_allview').checked;
  const sel=document.getElementById('ft_font');
  const what=all?'All Fonts':(sel?sel.selectedOptions[0].text:'');
  document.getElementById('ft_now').textContent=on
    ?('On — '+what+'  '+FT_SIZES[ftSizeIdx]+'px  (Dark + All Font View apply on Save)')
    :'Off';
}
function ftOn(){
  document.getElementById('ft_sub').style.display=document.getElementById('ft_on').checked?'block':'none';
  ftSendLive();
}
function ftAllView(){
  const all=document.getElementById('ft_allview').checked;
  document.getElementById('ft_fontrow').style.display=all?'none':'block';
  document.getElementById('ft_crisprow').style.display=all?'block':'none';
  if(all)ftSizeIdx=FT_SIZES.indexOf(14);   // All Font View starts at 14px (applies on Save)
  ftLabel();
}
function ftPage(d){
  if(!document.getElementById('ft_on').checked)return;
  ftSizeIdx=Math.max(0,Math.min(FT_SIZES.length-1,ftSizeIdx+d));
  ftSendLive();
}
async function ftSendLive(){   // size/font/on apply immediately (dark + view do NOT)
  const on=document.getElementById('ft_on').checked?1:0;
  const font=parseInt(document.getElementById('ft_font').value);
  ftLabel();
  const u='/api/fonttest?on='+on+'&font='+font+'&size_idx='+ftSizeIdx;
  try{await fetch(u,{method:'POST'});}catch(e){}
}
async function ftSendSave(){   // Save Settings applies dark + All Font View
  const font=parseInt(document.getElementById('ft_font').value);
  const dark=document.getElementById('ft_dark').checked?1:0;
  const all=document.getElementById('ft_allview').checked?1:0;
  const crisp=document.getElementById('ft_crisp').checked?1:0;
  const u='/api/fonttest?on=1&font='+font+'&size_idx='+ftSizeIdx+'&dark='+dark+'&all='+all+'&crisp='+crisp;
  try{await fetch(u,{method:'POST'});}catch(e){}
  ftLabel();
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
// Claude Expires At: paste raw epoch (ms 13-digit or seconds 10-digit, auto-detected
// like the firmware's >1e11 test); show local date/time + relative below the field.
function updExp(){
  const el=document.getElementById('cl_exp'), out=document.getElementById('cl_exp_h');
  if(!el||!out) return;
  const raw=(el.value||'').replace(/[^0-9]/g,''); const n=parseInt(raw,10);
  if(!n){ out.textContent=''; return; }
  const ms = n>=1e11 ? n : n*1000;
  const d=new Date(ms);
  const when=d.toLocaleString(undefined,{weekday:'short',year:'numeric',month:'short',
    day:'numeric',hour:'2-digit',minute:'2-digit',second:'2-digit'});
  const diff=ms-Date.now();
  if(diff<=0){ out.innerHTML=when+' — <span style="color:#c33">EXPIRED</span>'; return; }
  const m=Math.floor(diff/60000), h=Math.floor(m/60), dd=Math.floor(h/24);
  const rel = dd>0 ? `in ${dd}d ${h%24}h` : h>0 ? `in ${h}h ${m%60}m` : `in ${m}m`;
  out.textContent=`${when} — ${rel}`;
}
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
  const ce=document.getElementById('cl_exp');if(ce){ce.value=(c.cl_exp&&c.cl_exp!=='0')?c.cl_exp:'';updExp();}
  const cx=document.getElementById('cx_lr');if(cx)cx.value=isoToLocal(c.cx_lr??'');
  const mm=document.getElementById('mm_reg');if(mm)mm.value=String(c.mm_reg??0);
  const rs=document.getElementById('ref_sec');if(rs){rs.value=c.ref_sec??300;updRef(rs.value);}
  const bf=document.getElementById('batt_full');if(bf)bf.value=c.batt_full??4200;
  const ds=document.getElementById('deep_sleep');if(ds)ds.checked=!!c.deep_sleep;
  const dk=document.getElementById('dark');if(dk)dk.checked=!!c.dark;
  const uf=document.getElementById('ui_font');if(uf)uf.value=String(c.ui_font??0);
  const ftf=document.getElementById('ft_font');if(ftf)ftf.value=String(c.ui_font??4);
  const ua=document.getElementById('ui_aa');if(ua)ua.checked=(c.ui_aa!==false);
  const hb=document.getElementById('ui_hardblack');if(hb)hb.checked=(c.ui_aa===false);
  const usc=document.getElementById('ui_smcrisp');if(usc)usc.checked=(c.ui_smcrisp!==false);
  const us=document.getElementById('ui_sharp');if(us){us.value=String(c.ui_sharp??50);document.getElementById('sharp_lbl').textContent=us.value;}
  const uw=document.getElementById('ui_weight');if(uw){uw.value=String(c.ui_weight??45);document.getElementById('weight_lbl').textContent=uw.value;}
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
  d.cl_exp=((document.getElementById('cl_exp')?.value)||'').replace(/[^0-9]/g,'')||'0';
  d.cx_lr=localToIso(document.getElementById('cx_lr')?.value);
  d.mm_reg=parseInt(document.getElementById('mm_reg')?.value??'0');
  d.ref_sec=parseInt(document.getElementById('ref_sec').value);
  d.deep_sleep=document.getElementById('deep_sleep').checked;
  d.dark=document.getElementById('dark').checked;
  d.ui_font=parseInt(document.getElementById('ui_font').value);
  d.ui_aa=document.getElementById('ui_aa').checked;
  d.ui_smcrisp=document.getElementById('ui_smcrisp').checked;
  d.ui_sharp=parseInt(document.getElementById('ui_sharp').value);
  d.ui_weight=parseInt(document.getElementById('ui_weight').value);
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
  // Opt-in token testing: only providers whose "API Test" box is checked are
  // tested (bit n = provider n). Remember which, so applyCred only surfaces
  // their errors. Clear any stale error before this save.
  let mask=0; testedProvs=[];
  for(const p in TEST_IDS){const cb=document.getElementById(TEST_IDS[p]);
    if(cb&&cb.checked){mask|=(1<<PROV_ID[p]);testedProvs.push(p);}}
  document.getElementById('apierr').textContent='';
  const payload=collect(); payload.test_mask=mask;
  try{
    const r=await fetch('/api/settings',{method:'POST',
      headers:{'Content-Type':'application/json'},
      body:JSON.stringify(payload)});
    if(r.ok){
      // API Test is a per-save action, not a saved setting — uncheck the boxes.
      for(const p in TEST_IDS){const cb=document.getElementById(TEST_IDS[p]);if(cb)cb.checked=false;}
      if(curTab===0&&mask){setMsg('Saved! Testing tokens…','green');pollCredFor(30000);}
      else setMsg('Saved!','green');
      pollSleep();   // the save extended the window; refresh the countdown now
      if(document.getElementById('ft_on').checked)ftSendSave();  // apply Dark + All Font View
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
    const dd=Math.floor(up/86400),h=Math.floor((up%86400)/3600),m=Math.floor((up%3600)/60),s=up%60;
    const upStr=(dd>0?dd+'d ':'')+h+'h '+m+'m '+s+'s';
    const str=(r)=>(r==null||r===0)?'?':(r>=-60?'High':(r>=-72?'Med':'Low'));
    let battStr='?';
    if(d.batt!=null&&d.batt>=0){
      battStr=d.batt+'% | '+(d.batt_mv>=0?d.batt_mv:'?')+'mv';
      if(d.batt_days!=null&&d.batt_days>=0)
        battStr+=' | Est. '+Math.floor(d.batt_days/24)+' days '+(d.batt_days%24)+' hours on battery';
      else if(d.batt_days==-3)
        battStr+=' | Charging';
      else if(d.batt_days==-2)
        battStr+=' | Calibrating...';
      else if(d.batt_days<=-10){
        var ph=-d.batt_days-10;   // learned placeholder -> italic
        battStr+=' | <i>Est. '+Math.floor(ph/24)+' days '+(ph%24)+' hours on battery</i>';
      }
    }
    const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
    const rows=[
      ['IP Address',esc(d.ip??'?')],
      ['WiFi',esc(d.ssid??'?')+' | '+str(d.rssi)],   // SSID is attacker-controllable
      ['Battery',battStr],   // battStr has intentional <i> markup, composed from numbers
      ['Uptime',upStr],
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
const PROV_ID={claude:1,codex:2,copilot:3,minimax:4,kimi:5,zai:6,claudeplat:7};
const TEST_IDS={claude:'t_claude',codex:'t_codex',copilot:'t_copilot',minimax:'t_minimax',kimi:'t_kimi',zai:'t_zai',claudeplat:'t_claudeplat'};
let testedProvs=[];   // keys checked at last Save -> only surface their errors
// 'set' = configured but untested (white, but Clear Token still shows).
const CRED_BG={ok:'#d6f5d6',fail:'#f8d2d2',none:'',set:'',testing:''};
const HDR_BG={ok:'#8fdcb4',fail:'#f2aac0',none:'',set:'',testing:''};
const SUMMARY={claude:'s_claude',claudeplat:'s_claudeplat',codex:'s_codex',
  copilot:'s_copilot',minimax:'s_minimax',kimi:'s_kimi',zai:'s_zai'};
function applyCred(st){
  for(const p in PROV_FIELDS){
    const k=st[p]??'none';
    PROV_FIELDS[p].forEach(id=>{const el=document.getElementById(id);if(el)el.style.background=CRED_BG[k]??'';});
    const sm=document.getElementById(SUMMARY[p]);if(sm)sm.style.background=HDR_BG[k]??'';
    // Clear Token whenever a token is stored (ok/fail/set); hidden only when none.
    const btn=document.getElementById('clr_'+p);if(btn)btn.style.display=(k==='ok'||k==='fail'||k==='set')?'inline-block':'none';
  }
  // Surface API-test failures only for the providers checked at the last Save.
  const lines=[];
  testedProvs.forEach(p=>{if((st[p]??'none')==='fail')lines.push('API Test failed. Provider response: '+(st[p+'_err']||''));});
  const box=document.getElementById('apierr');if(box)box.textContent=lines.join('\n');
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
  wMsg.textContent=ASLEEP_MSG;wBox.classList.add('on');
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
(function(){try{var a=localStorage.getItem('um_adv')==='1';var c=document.getElementById('adv');if(c)c.checked=a;document.body.classList.toggle('adv',a);}catch(e){}})();
setInterval(pollSleep,5000);pollSleep();
</script>
<div style="margin-top:18px;padding-top:10px;border-top:1px solid #ccc;text-align:center;font-size:12px;color:#888">
  Initial main branch by <a href="https://github.com/limengdu" target="_blank" rel="noopener">limengdu</a>
  (<a href="https://github.com/limengdu/ePaper_vibe_coding_ai_usage_track" target="_blank" rel="noopener">ePaper_vibe_coding_ai_usage_track</a>).
  Special thanks to <a href="https://github.com/tddworks/ClaudeBar" target="_blank" rel="noopener">ClaudeBar</a>.
</div>
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

// Heap buffer for the POST /api/settings body. Allocated with malloc so the
// AsyncWebServerRequest destructor's free(_tempObject) on an aborted upload is
// correct — a `new String` would mismatch free() and leak its internal buffer.
struct UmReqBody { uint32_t len; uint32_t cap; char data[1]; };

// CSRF gate for state-changing routes: the SPA tags every non-GET request with
// X-UM-CSRF. A cross-origin page cannot set a custom header without a preflight
// the device never answers, so a drive-by browser POST (and the text/plain body
// trick that evades the application/json preflight) is rejected here. Returns
// true (and sends 403) when the header is absent.
static bool csrfReject(AsyncWebServerRequest* req) {
  if (req->hasHeader("X-UM-CSRF")) return false;
  req->send(403, "application/json", "{\"ok\":false,\"error\":\"csrf\"}");
  return true;
}

void SettingsServer::begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)(),
                           std::function<void()> onSaved,
                           std::function<String()> credJson,
                           std::function<void()> onKeepAlive,
                           std::function<void()> onSleepNow,
                           std::function<int()> sleepInSec,
                           std::function<int()> bootId,
                           int (*battMv)(),
                           std::function<int()> battDays,
                           std::function<void(int,int,int,int,int,int)> onFontTest,
                           std::function<long()> uptime) {
  cfg_ = cfg;
  uptime_ = uptime;
  battPct_ = battPct;
  battMv_ = battMv;
  battDays_ = battDays;
  onSaved_ = onSaved;
  credJson_ = credJson;
  onKeepAlive_ = onKeepAlive;
  onSleepNow_ = onSleepNow;
  sleepInSec_ = sleepInSec;
  bootId_ = bootId;
  onFontTest_ = onFontTest;

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
      UmReqBody* body = reinterpret_cast<UmReqBody*>(req->_tempObject);
      if (csrfReject(req)) { free(body); req->_tempObject = nullptr; return; }
      if (!body) {  // oversize/aborted body was dropped by the upload handler
        sendNoCache(req, 413, "application/json", "{\"ok\":false,\"error\":\"too_large\"}");
        return;
      }
      body->data[body->len] = '\0';
      if (cfg_->fromJson(String(body->data))) {
        cfg_->save();
        sysLog("[web] save -> extend awake");
        if (onSaved_)    onSaved_();      // apply changes + repaint (async-safe flag)
        if (onKeepAlive_) onKeepAlive_(); // a save is a user action -> extend NOW
        sendNoCache(req, 200, "application/json", "{\"ok\":true}");
      } else {
        sendNoCache(req, 400, "application/json", "{\"ok\":false,\"error\":\"parse\"}");
      }
      free(body); req->_tempObject = nullptr;
    },
    nullptr,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len,
       size_t index, size_t total) {
      static const size_t kMaxBody = 8192;   // settings JSON is well under this
      if (index == 0) {
        if (req->_tempObject) { free(req->_tempObject); req->_tempObject = nullptr; }
        if (total > kMaxBody) return;        // reject up front; body stays null
        const size_t cap = total > 0 ? total : 512;
        UmReqBody* b = (UmReqBody*)malloc(offsetof(UmReqBody, data) + cap + 1);
        if (!b) return;
        b->len = 0; b->cap = (uint32_t)cap;
        req->_tempObject = b;
      }
      UmReqBody* b = reinterpret_cast<UmReqBody*>(req->_tempObject);
      if (!b) return;                        // already rejected / OOM
      if (b->len + len > b->cap || b->len + len > kMaxBody) {  // overflow / chunked-no-length
        free(b); req->_tempObject = nullptr;
        return;
      }
      memcpy(b->data + b->len, data, len);
      b->len += (uint32_t)len;
    }
  );

  // GET /api/credstatus → per-provider credential test status
  server->on("/api/credstatus", HTTP_GET, [this](AsyncWebServerRequest* req) {
    sendNoCache(req, 200, "application/json", credJson_ ? credJson_() : "{}");
  });

  // POST /api/keepalive → user chose Continue Session: keep awake 2 minutes
  server->on("/api/keepalive", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (csrfReject(req)) return;
    sysLog("[web] keepalive (Continue Session)");
    if (onKeepAlive_) onKeepAlive_();
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // POST /api/sleepnow → user chose Sleep: enter deep sleep now
  server->on("/api/sleepnow", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (csrfReject(req)) return;
    if (onSleepNow_) onSleepNow_();
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // POST /api/fonttest?on=&font=&dark=&size_idx= → runtime font-test (NOT saved)
  server->on("/api/fonttest", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (csrfReject(req)) return;
    auto qp = [&](const char* k, int def) -> int {
      return req->hasParam(k) ? req->getParam(k)->value().toInt() : def;
    };
    if (onFontTest_) onFontTest_(qp("on", 0), qp("font", -1), qp("dark", -1),
                                 qp("all", -1), qp("crisp", -1), qp("size_idx", 0));
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
  });

  // POST /api/clearprovider?prov=N → wipe that provider's credentials
  server->on("/api/clearprovider", HTTP_POST, [this](AsyncWebServerRequest* req) {
    if (csrfReject(req)) return;
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
    long up = uptime_ ? uptime_() : (long)(millis() / 1000UL);   // since cold boot
    const int batt = battPct_ ? battPct_() : -1;       // reads ADC, also sets mV
    const int battMv = battMv_ ? battMv_() : -1;
    const int sleepIn = sleepInSec_ ? sleepInSec_() : -1;  // passive: does NOT extend
    const int boot = bootId_ ? bootId_() : 0;
    const int rssi = WiFi.isConnected() ? WiFi.RSSI() : 0;
    const int battDays = battDays_ ? battDays_() : -1;   // est hrs; -1=n/a -2=calibrating -3=charging
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
    if (csrfReject(req)) return;
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
  });

  // POST /api/wifi-reset → erase stored credentials + restart
  server->on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    if (csrfReject(req)) return;
    sendNoCache(req, 200, "application/json", "{\"ok\":true}");
    delay(200);
    WiFi.disconnect(true, true);  // wifioff=true, eraseap=true
    delay(100);
    ESP.restart();
  });

  sysLog("[settings] routes registered");
}

}  // namespace usage_monitor
