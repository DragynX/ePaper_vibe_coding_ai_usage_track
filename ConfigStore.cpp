#include "ConfigStore.h"

#include <ArduinoJson.h>
#include <Preferences.h>

#include "AppLog.h"

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
  ls_url_ = p.getString("ls_url", "");
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
  p.putString("ls_url",    ls_url_);
  p.end();
  sysLog("[cfg] saved");
}

String ConfigStore::toJson() const {
  JsonDocument doc;
  doc["left_prov"]  = leftProv_;
  doc["right_prov"] = rightProv_;
  doc["tz"]         = tz_;
  doc["ref_sec"]    = refreshSec_;
  doc["deep_sleep"] = deepSleep_;
  // Secret fields are never echoed back: the GET response is reachable by any
  // LAN client. Emit "" plus a <key>_set flag so the UI can show saved state.
  doc["cl_at"]      = "";
  doc["cl_at_set"]  = cl_at_.length()  > 0;
  doc["cl_rt"]      = "";
  doc["cl_rt_set"]  = cl_rt_.length()  > 0;
  doc["cl_exp"]     = cl_exp_;
  doc["cl_sub"]     = cl_sub_;
  doc["cx_at"]      = "";
  doc["cx_at_set"]  = cx_at_.length()  > 0;
  doc["cx_rt"]      = "";
  doc["cx_rt_set"]  = cx_rt_.length()  > 0;
  doc["cx_aid"]     = cx_aid_;
  doc["cx_lr"]      = cx_lr_;
  doc["co_pat"]     = "";
  doc["co_pat_set"] = co_pat_.length() > 0;
  doc["mm_key"]     = "";
  doc["mm_key_set"] = mm_key_.length() > 0;
  doc["mm_reg"]     = mm_reg_;
  doc["ki_tok"]     = "";
  doc["ki_tok_set"] = ki_tok_.length() > 0;
  doc["za_key"]     = "";
  doc["za_key_set"] = za_key_.length() > 0;
  doc["za_ep"]      = za_ep_;
  doc["cp_key"]     = "";
  doc["cp_key_set"] = cp_key_.length() > 0;
  doc["cp_org"]     = cp_org_;
  doc["ls_url"]     = ls_url_;
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigStore::fromJson(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  // Secret fields use keep-if-blank: a missing or empty value preserves the
  // stored secret (the UI never receives it back, so a plain save must not
  // wipe it). Non-secret fields keep the missing-only guard.
  auto setSecret = [&](const char* key, String& dst) {
    if (!doc[key].isNull()) {
      const String v = doc[key].as<String>();
      if (v.length() > 0) dst = v;
    }
  };
  leftProv_  = doc["left_prov"]  | leftProv_;
  rightProv_ = doc["right_prov"] | rightProv_;
  if (!doc["tz"].isNull())  tz_  = doc["tz"].as<String>();
  setRefreshSec(doc["ref_sec"] | refreshSec_);
  deepSleep_ = doc["deep_sleep"] | deepSleep_;
  setSecret("cl_at", cl_at_);
  setSecret("cl_rt", cl_rt_);
  if (!doc["cl_exp"].isNull()) cl_exp_ = doc["cl_exp"].as<String>();
  if (!doc["cl_sub"].isNull()) cl_sub_ = doc["cl_sub"].as<String>();
  setSecret("cx_at", cx_at_);
  setSecret("cx_rt", cx_rt_);
  if (!doc["cx_aid"].isNull()) cx_aid_ = doc["cx_aid"].as<String>();
  if (!doc["cx_lr"].isNull())  cx_lr_  = doc["cx_lr"].as<String>();
  setSecret("co_pat", co_pat_);
  setSecret("mm_key", mm_key_);
  mm_reg_ = doc["mm_reg"] | mm_reg_;
  setSecret("ki_tok", ki_tok_);
  setSecret("za_key", za_key_);
  if (!doc["za_ep"].isNull())  za_ep_  = doc["za_ep"].as<String>();
  setSecret("cp_key", cp_key_);
  if (!doc["cp_org"].isNull()) cp_org_ = doc["cp_org"].as<String>();
  if (!doc["ls_url"].isNull()) ls_url_ = doc["ls_url"].as<String>();
  return true;
}

}  // namespace usage_monitor
