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
</div>

<div id="p0" class="pane on">
  <details><summary id="s_claude">Claude OAuth</summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cl_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cl_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Expires At<input type="datetime-local" id="cl_exp"></label>
    <label>Subscription<select id="cl_sub">
      <option value="free">Free</option>
      <option value="pro">Pro</option>
      <option value="max">Max</option>
    </select></label>
  </div></details>
  <details><summary id="s_claudeplat">Claude Platform (Admin Key)</summary><div class="inner">
    <label>Admin API Key<textarea class="secret" id="cp_key" rows="2" spellcheck="false"></textarea></label>
    <label>Org ID (optional)<input type="text" id="cp_org" spellcheck="false"></label>
    <p class="note">From console.anthropic.com &#8594; API Keys &#8594; Admin Key. Shows 7-day token totals.</p>
  </div></details>
  <details><summary id="s_codex">Codex OAuth</summary><div class="inner">
    <label>Access Token<textarea class="secret" id="cx_at" rows="2" spellcheck="false"></textarea></label>
    <label>Refresh Token<textarea class="secret" id="cx_rt" rows="2" spellcheck="false"></textarea></label>
    <label>Account ID<input type="text" id="cx_aid"></label>
    <label>Last Refresh<input type="datetime-local" id="cx_lr"></label>
  </div></details>
  <details><summary id="s_copilot">GitHub Copilot PAT</summary><div class="inner">
    <label>Personal Access Token<textarea class="secret" id="co_pat" rows="2" spellcheck="false"></textarea></label>
    <p class="note">github.com/settings/tokens &#8594; Classic &#8594; needs "copilot" scope</p>
  </div></details>
  <details><summary id="s_minimax">MiniMax</summary><div class="inner">
    <label>API Key<textarea class="secret" id="mm_key" rows="2" spellcheck="false"></textarea></label>
    <label>Region<select id="mm_reg"><option value="0">International (api.minimax.io)</option><option value="1">China (api.minimaxi.com)</option></select></label>
  </div></details>
  <details><summary id="s_kimi">Kimi</summary><div class="inner">
    <label>Auth Token (browser cookie kimi-auth)<textarea class="secret" id="ki_tok" rows="2" spellcheck="false"></textarea></label>
    <p class="note">Extract from www.kimi.com DevTools. No refresh &#8212; re-enter when expired.</p>
  </div></details>
  <details><summary id="s_zai">Zai / Zhipu</summary><div class="inner">
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
      <option value="2">Codex</option>
      <option value="3">Copilot</option>
      <option value="4">MiniMax</option>
      <option value="5">Kimi</option>
      <option value="6">Zai</option>
      <option value="7">Claude Platform</option>
    </select>
  </label>
  <label style="margin-top:14px">RIGHT Column Provider
    <select id="right_prov">
      <option value="0">&#8212; None &#8212;</option>
      <option value="1">Claude OAuth</option>
      <option value="2">Codex</option>
      <option value="3">Copilot</option>
      <option value="4">MiniMax</option>
      <option value="5">Kimi</option>
      <option value="6">Zai</option>
      <option value="7">Claude Platform</option>
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
  <label class="chkrow" style="margin-top:14px">
    <input type="checkbox" id="deep_sleep">
    <span>Enable Deep Sleep between fetches</span>
  </label>
  <label class="chkrow" style="margin-top:10px">
    <input type="checkbox" id="dark">
    <span>Dark mode (screen)</span>
  </label>
  <label class="chkrow" style="margin-top:10px">
    <input type="checkbox" id="secure">
    <span>Secure Tokens (hide saved tokens; reveal only what you type this session)</span>
  </label>
  <p class="note" style="margin-top:4px">When on: settings page is only available for 5 min after power-on/reset. Device sleeps between fetches.</p>
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

<script>
const NPANE=4;
function go(n){
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
               'za_key','za_ep','cp_key','cp_org','ls_url'];
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
  const ds=document.getElementById('deep_sleep');if(ds)ds.checked=!!c.deep_sleep;
  const dk=document.getElementById('dark');if(dk)dk.checked=!!c.dark;
  const se=document.getElementById('secure');if(se)se.checked=!!c.secure;
  const lp=document.getElementById('left_prov');if(lp)lp.value=String(c.left_prov??0);
  const rp=document.getElementById('right_prov');if(rp)rp.value=String(c.right_prov??0);
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
  d.secure=document.getElementById('secure').checked;
  d.left_prov=parseInt(document.getElementById('left_prov').value);
  d.right_prov=parseInt(document.getElementById('right_prov').value);
  const tzSel=document.getElementById('tz_sel');
  d.tz=tzSel&&tzSel.value==='custom'?(document.getElementById('tz_custom')?.value??'UTC0'):(tzSel?.value??'UTC0');
  return d;
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
    if(r.ok){setMsg('Saved! Testing tokens…','green');pollCredFor(30000);}
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
    const rows=[
      ['IP Address',d.ip??'?'],
      ['WiFi SSID',d.ssid??'?'],
      ['Battery',(d.batt!=null&&d.batt>=0)?d.batt+'%':'?'],
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
  }
}
async function pollCred(){try{applyCred(await fetch('/api/credstatus',{cache:'no-store'}).then(r=>r.json()));}catch(e){}}
function pollCredFor(ms){const end=Date.now()+ms;const t=setInterval(()=>{pollCred();if(Date.now()>end)clearInterval(t);},2000);}
fetch('/api/settings').then(r=>r.json()).then(populate).catch(console.error);
pollCred();
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

void SettingsServer::begin(AsyncWebServer* server, ConfigStore* cfg, int (*battPct)(),
                           std::function<void()> onSaved,
                           std::function<String()> credJson) {
  cfg_ = cfg;
  battPct_ = battPct;
  onSaved_ = onSaved;
  credJson_ = credJson;

  // GET / → settings page
  server->on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", kHtml);
  });

  // GET /api/settings → JSON
  server->on("/api/settings", HTTP_GET, [this](AsyncWebServerRequest* req) {
    req->send(200, "application/json", cfg_->toJson());
  });

  // POST /api/settings → update + save
  server->on("/api/settings", HTTP_POST,
    [this](AsyncWebServerRequest* req) {
      String* body = reinterpret_cast<String*>(req->_tempObject);
      if (!body) {  // oversize/aborted body was dropped by the upload handler
        req->send(413, "application/json", "{\"ok\":false,\"error\":\"too_large\"}");
        return;
      }
      if (cfg_->fromJson(*body)) {
        cfg_->save();
        if (onSaved_) onSaved_();   // e.g. apply dark mode + repaint (async-safe flag)
        req->send(200, "application/json", "{\"ok\":true}");
      } else {
        req->send(400, "application/json", "{\"ok\":false,\"error\":\"parse\"}");
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
    req->send(200, "application/json", credJson_ ? credJson_() : "{}");
  });

  // GET /api/status → live info
  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
    String ip   = jsonEscape(WiFi.localIP().toString());
    String ssid = jsonEscape(WiFi.SSID());
    unsigned long up = millis() / 1000UL;
    const int batt = battPct_ ? battPct_() : -1;
    String json = "{\"ip\":\"" + ip + "\","
                  "\"ssid\":\"" + ssid + "\","
                  "\"batt\":" + String(batt) + ","
                  "\"uptime_sec\":" + String(up) + ","
                  "\"left_prov\":"  + String(cfg_->leftProvider()) + ","
                  "\"right_prov\":" + String(cfg_->rightProvider()) + "}";
    req->send(200, "application/json", json);
  });

  // POST /api/restart
  server->on("/api/restart", HTTP_POST, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true}");
    delay(200);
    ESP.restart();
  });

  // POST /api/wifi-reset → erase stored credentials + restart
  server->on("/api/wifi-reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"ok\":true}");
    delay(200);
    WiFi.disconnect(true, true);  // wifioff=true, eraseap=true
    delay(100);
    ESP.restart();
  });

  sysLog("[settings] routes registered");
}

}  // namespace usage_monitor
