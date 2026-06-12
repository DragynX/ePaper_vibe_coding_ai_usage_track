#include "ConfigStore.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>

#include "AppLog.h"
#include "ProviderSelect.h"

namespace usage_monitor {

static const char kNs[] = "umcfg";

void ConfigStore::begin() { load(); }

void ConfigStore::load() {
  Preferences p;
  if (!p.begin(kNs, true)) return;
  leftProv_   = p.getUChar("left_prov", 0);
  rightProv_  = p.getUChar("right_prov", 0);
  tz_         = p.getString("tz", "UTC0");
  refreshSec_ = p.getUInt("ref_sec", 300);
  deepSleep_  = p.getBool("deep_sleep", false);
  dark_       = p.getBool("dark", false);
  secure_     = p.getBool("secure", false);
  ui_font_    = p.getInt("ui_font", 4);
  ui_aa_      = p.getBool("ui_aa", false);
  ui_sharp_   = p.getInt("ui_sharp", 50);
  ui_weight_  = p.getInt("ui_weight", 45);
  ui_small_crisp_ = p.getBool("ui_smcrisp", true);
  battFull_   = (uint16_t)p.getUShort("batt_full", 4200);
  cl_at_  = p.getString("cl_at",  "");
  cl_rt_  = p.getString("cl_rt",  "");
  cl_exp_ = p.getString("cl_exp", "0");
  cl_sub_ = p.getString("cl_sub", "pro");
  cx_at_  = p.getString("cx_at",  "");
  cx_rt_  = p.getString("cx_rt",  "");
  cx_aid_ = p.getString("cx_aid", "");
  cx_lr_  = p.getString("cx_lr",  "0");
  co_pat_ = p.getString("co_pat", "");
  mm_key_ = p.getString("mm_key", "");
  mm_reg_ = p.getInt("mm_reg", 0);
  ki_tok_ = p.getString("ki_tok", "");
  za_key_ = p.getString("za_key", "");
  za_ep_  = p.getString("za_ep",  "https://api.z.ai");
  cp_key_ = p.getString("cp_key", "");
  cp_org_ = p.getString("cp_org", "");
  cp_prepaid_ = p.getString("cp_prepaid", "");
  cp_topup_   = p.getString("cp_topup", "");
  cp_mode_     = p.getString("cp_mode", "prepaid");
  cp_spendwin_ = p.getString("cp_spendwin", "30");
  ls_url_ = p.getString("ls_url", "");
  if (p.getBytes("batt_est", &battEst_, sizeof(battEst_)) != sizeof(battEst_)
      || battEst_.version != 1) {
    initBattEst();
  }
  p.end();
  sysLog("[cfg] loaded left=%d right=%d ref=%us", (int)leftProv_, (int)rightProv_, (unsigned)refreshSec_);
}

void ConfigStore::save() {
  Preferences p;
  if (!p.begin(kNs, false)) { sysLog("[cfg] NVS open failed"); return; }
  p.putUChar("left_prov",  leftProv_);
  p.putUChar("right_prov", rightProv_);
  p.putString("tz",        tz_);
  p.putUInt("ref_sec",     refreshSec_);
  p.putBool("deep_sleep",  deepSleep_);
  p.putBool("dark",        dark_);
  p.putBool("secure",      secure_);
  p.putInt("ui_font",      ui_font_);
  p.putBool("ui_aa",       ui_aa_);
  p.putInt("ui_sharp",     ui_sharp_);
  p.putInt("ui_weight",    ui_weight_);
  p.putBool("ui_smcrisp",  ui_small_crisp_);
  p.putUShort("batt_full", battFull_);
  p.putString("cl_at",     cl_at_);
  p.putString("cl_rt",     cl_rt_);
  p.putString("cl_exp",    cl_exp_);
  p.putString("cl_sub",    cl_sub_);
  p.putString("cx_at",     cx_at_);
  p.putString("cx_rt",     cx_rt_);
  p.putString("cx_aid",    cx_aid_);
  p.putString("cx_lr",     cx_lr_);
  p.putString("co_pat",    co_pat_);
  p.putString("mm_key",    mm_key_);
  p.putInt("mm_reg",       mm_reg_);
  p.putString("ki_tok",    ki_tok_);
  p.putString("za_key",    za_key_);
  p.putString("za_ep",     za_ep_);
  p.putString("cp_key",    cp_key_);
  p.putString("cp_org",    cp_org_);
  p.putString("cp_prepaid", cp_prepaid_);
  p.putString("cp_topup",   cp_topup_);
  p.putString("cp_mode",     cp_mode_);
  p.putString("cp_spendwin", cp_spendwin_);
  p.putString("ls_url",    ls_url_);
  p.end();
  sysLog("[cfg] saved");
}

// --- Learned battery runtime estimates ------------------------------------
static const uint8_t kBattSoc[ConfigStore::kBattBuckets] =
    {99, 95, 90, 80, 70, 60, 50, 40, 30, 20, 10};

void ConfigStore::initBattEst() {
  battEst_.version = 1;
  for (int i = 0; i < kBattBuckets; ++i) {
    battEst_.deep[i]  = -1.0f;
    battEst_.awake[i] = -1.0f;
  }
  battEst_.lastDeep = battEst_.lastAwake = -1.0f;
}

int ConfigStore::battBucketIndex(int soc) {
  int best = 0, bestd = 1000;
  for (int i = 0; i < kBattBuckets; ++i) {
    const int d = abs((int)kBattSoc[i] - soc);
    if (d < bestd) { bestd = d; best = i; }
  }
  return best;
}

float ConfigStore::battEstLookup(bool deep, int soc) const {
  const float* tbl = deep ? battEst_.deep : battEst_.awake;
  const int idx = battBucketIndex(soc);
  for (int r = 0; r < kBattBuckets; ++r) {        // search outward from the bucket
    if (idx - r >= 0 && tbl[idx - r] >= 0.0f) return tbl[idx - r];
    if (idx + r < kBattBuckets && tbl[idx + r] >= 0.0f) return tbl[idx + r];
  }
  return deep ? battEst_.lastDeep : battEst_.lastAwake;   // carry last, or -1
}

void ConfigStore::battEstRecord(bool deep, int soc, float days) {
  if (days < 0.0f) return;
  float* tbl  = deep ? battEst_.deep : battEst_.awake;
  float& last = deep ? battEst_.lastDeep : battEst_.lastAwake;
  float& slot = tbl[battBucketIndex(soc)];
  bool changed = false;
  // 1-day deadband: the estimate jitters by whole days early in a discharge, and
  // the UI rounds to days/hours anyway — a tight band would rewrite the NVS blob
  // almost every wake (flash wear + radio-on commit time) for no visible change.
  if (slot < 0.0f || fabsf(slot - days) >= 1.0f) { slot = days; changed = true; }
  if (last < 0.0f || fabsf(last - days) >= 1.0f) { last = days; changed = true; }
  if (changed) saveBattEst();                     // wear guard: only on real change
}

void ConfigStore::saveBattEst() {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putBytes("batt_est", &battEst_, sizeof(battEst_));
  p.end();
}

String ConfigStore::toJson() const {
  JsonDocument doc;
  doc["left_prov"]  = leftProv_;
  doc["right_prov"] = rightProv_;
  doc["tz"]         = tz_;
  doc["ref_sec"]    = refreshSec_;
  doc["deep_sleep"] = deepSleep_;
  doc["dark"]       = dark_;
  doc["secure"]     = secure_;
  doc["ui_font"]    = ui_font_;
  doc["ui_aa"]      = ui_aa_;
  doc["ui_sharp"]   = ui_sharp_;
  doc["ui_weight"]  = ui_weight_;
  doc["ui_smcrisp"] = ui_small_crisp_;
  doc["batt_full"]  = battFull_;
  // Secret fields are NEVER echoed back — a saved key is shown blank with a
  // "saved" placeholder (the <key>_set flag drives it). They never cross the LAN.
  auto secret = [&](const char* key, const String& v) {
    doc[key] = String("");
    doc[String(key) + "_set"] = v.length() > 0;
  };
  secret("cl_at", cl_at_);
  secret("cl_rt", cl_rt_);
  doc["cl_exp"]     = cl_exp_;
  doc["cl_sub"]     = cl_sub_;
  secret("cx_at", cx_at_);
  secret("cx_rt", cx_rt_);
  doc["cx_aid"]     = cx_aid_;
  doc["cx_lr"]      = cx_lr_;
  secret("co_pat", co_pat_);
  secret("mm_key", mm_key_);
  doc["mm_reg"]     = mm_reg_;
  secret("ki_tok", ki_tok_);
  secret("za_key", za_key_);
  doc["za_ep"]      = za_ep_;
  secret("cp_key", cp_key_);
  doc["cp_org"]     = cp_org_;
  doc["cp_prepaid"] = cp_prepaid_;
  doc["cp_topup"]   = cp_topup_;
  doc["cp_mode"]     = cp_mode_;
  doc["cp_spendwin"] = cp_spendwin_;
  doc["ls_url"]     = ls_url_;
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigStore::fromJson(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  pendingTestMask_ = 0;
  // Secret fields use keep-if-blank: a missing or empty value preserves the
  // stored secret (under Secure Tokens the UI never gets it back, so a plain
  // save must not wipe it). Token testing is no longer auto-triggered by a
  // secret change — it is opt-in via the "API Test" checkbox (test_mask below).
  auto setSecret = [&](const char* key, String& dst, uint8_t /*prov*/) {
    if (doc[key].isNull()) return;
    const String v = doc[key].as<String>();
    if (v.length() == 0) return;
    if (v != dst) dst = v;
  };
  leftProv_  = doc["left_prov"]  | leftProv_;
  rightProv_ = doc["right_prov"] | rightProv_;
  if (!doc["tz"].isNull())  tz_  = doc["tz"].as<String>();
  setRefreshSec(doc["ref_sec"] | refreshSec_);
  deepSleep_ = doc["deep_sleep"] | deepSleep_;
  dark_      = doc["dark"] | dark_;
  secure_    = doc["secure"] | secure_;
  ui_font_   = doc["ui_font"] | ui_font_;
  ui_aa_     = doc["ui_aa"] | ui_aa_;
  ui_sharp_  = doc["ui_sharp"] | ui_sharp_;
  ui_weight_ = doc["ui_weight"] | ui_weight_;
  ui_small_crisp_ = doc["ui_smcrisp"] | ui_small_crisp_;
  battFull_  = doc["batt_full"] | battFull_;
  setSecret("cl_at", cl_at_, UM_PROV_CLAUDE);
  setSecret("cl_rt", cl_rt_, UM_PROV_CLAUDE);
  if (!doc["cl_exp"].isNull()) cl_exp_ = doc["cl_exp"].as<String>();
  if (!doc["cl_sub"].isNull()) cl_sub_ = doc["cl_sub"].as<String>();
  setSecret("cx_at", cx_at_, UM_PROV_CODEX);
  setSecret("cx_rt", cx_rt_, UM_PROV_CODEX);
  if (!doc["cx_aid"].isNull()) cx_aid_ = doc["cx_aid"].as<String>();
  if (!doc["cx_lr"].isNull())  cx_lr_  = doc["cx_lr"].as<String>();
  setSecret("co_pat", co_pat_, UM_PROV_COPILOT);
  setSecret("mm_key", mm_key_, UM_PROV_MINIMAX);
  mm_reg_ = doc["mm_reg"] | mm_reg_;
  setSecret("ki_tok", ki_tok_, UM_PROV_KIMI);
  setSecret("za_key", za_key_, UM_PROV_ZAI);
  if (!doc["za_ep"].isNull()) {
    // The Zai bearer key is sent to this endpoint, so only accept https:// URLs —
    // reject http/attacker hosts that would exfiltrate the key (keep the old value).
    const String ep = doc["za_ep"].as<String>();
    if (ep.startsWith("https://")) za_ep_ = ep;
  }
  setSecret("cp_key", cp_key_, UM_PROV_CLAUDEPLAT);
  if (!doc["cp_org"].isNull()) cp_org_ = doc["cp_org"].as<String>();
  if (!doc["cp_prepaid"].isNull()) cp_prepaid_ = doc["cp_prepaid"].as<String>();
  if (!doc["cp_mode"].isNull())     cp_mode_     = doc["cp_mode"].as<String>();
  if (!doc["cp_spendwin"].isNull()) cp_spendwin_ = doc["cp_spendwin"].as<String>();
  if (!doc["ls_url"].isNull()) ls_url_ = doc["ls_url"].as<String>();
  // Opt-in token testing: the UI sends test_mask (bit n = provider n's "API Test"
  // box was checked at Save). Secrets above are already stored, so isConfigured()
  // reflects the new tokens when runPendingTokenTests() consumes this mask.
  uint32_t tm = doc["test_mask"] | 0;
  pendingTestMask_ = (uint8_t)(tm & 0xFE);   // bits 1..7 only (bit 0 unused)
  return true;
}

void ConfigStore::clearProvider(uint8_t prov) {
  switch (prov) {
    case UM_PROV_CLAUDE:     cl_at_ = ""; cl_rt_ = ""; cl_exp_ = "0"; break;
    case UM_PROV_CODEX:      cx_at_ = ""; cx_rt_ = ""; cx_aid_ = ""; cx_lr_ = "0"; break;
    case UM_PROV_COPILOT:    co_pat_ = ""; break;
    case UM_PROV_MINIMAX:    mm_key_ = ""; break;
    case UM_PROV_KIMI:       ki_tok_ = ""; break;
    case UM_PROV_ZAI:        za_key_ = ""; break;
    case UM_PROV_CLAUDEPLAT: cp_key_ = ""; cp_org_ = ""; cp_prepaid_ = ""; cp_topup_ = ""; break;
    default: break;
  }
}

}  // namespace usage_monitor
