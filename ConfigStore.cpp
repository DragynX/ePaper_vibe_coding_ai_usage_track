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
  doc["cl_at"]      = cl_at_;
  doc["cl_rt"]      = cl_rt_;
  doc["cl_exp"]     = cl_exp_;
  doc["cl_sub"]     = cl_sub_;
  doc["cx_at"]      = cx_at_;
  doc["cx_rt"]      = cx_rt_;
  doc["cx_aid"]     = cx_aid_;
  doc["cx_lr"]      = cx_lr_;
  doc["co_pat"]     = co_pat_;
  doc["mm_key"]     = mm_key_;
  doc["mm_reg"]     = mm_reg_;
  doc["ki_tok"]     = ki_tok_;
  doc["za_key"]     = za_key_;
  doc["za_ep"]      = za_ep_;
  doc["cp_key"]     = cp_key_;
  doc["cp_org"]     = cp_org_;
  doc["ls_url"]     = ls_url_;
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigStore::fromJson(const String& json) {
  JsonDocument doc;
  if (deserializeJson(doc, json)) return false;
  leftProv_  = doc["left_prov"]  | leftProv_;
  rightProv_ = doc["right_prov"] | rightProv_;
  if (!doc["tz"].isNull())  tz_  = doc["tz"].as<String>();
  setRefreshSec(doc["ref_sec"] | refreshSec_);
  deepSleep_ = doc["deep_sleep"] | deepSleep_;
  if (!doc["cl_at"].isNull())  cl_at_  = doc["cl_at"].as<String>();
  if (!doc["cl_rt"].isNull())  cl_rt_  = doc["cl_rt"].as<String>();
  if (!doc["cl_exp"].isNull()) cl_exp_ = doc["cl_exp"].as<String>();
  if (!doc["cl_sub"].isNull()) cl_sub_ = doc["cl_sub"].as<String>();
  if (!doc["cx_at"].isNull())  cx_at_  = doc["cx_at"].as<String>();
  if (!doc["cx_rt"].isNull())  cx_rt_  = doc["cx_rt"].as<String>();
  if (!doc["cx_aid"].isNull()) cx_aid_ = doc["cx_aid"].as<String>();
  if (!doc["cx_lr"].isNull())  cx_lr_  = doc["cx_lr"].as<String>();
  if (!doc["co_pat"].isNull()) co_pat_ = doc["co_pat"].as<String>();
  if (!doc["mm_key"].isNull()) mm_key_ = doc["mm_key"].as<String>();
  mm_reg_ = doc["mm_reg"] | mm_reg_;
  if (!doc["ki_tok"].isNull()) ki_tok_ = doc["ki_tok"].as<String>();
  if (!doc["za_key"].isNull()) za_key_ = doc["za_key"].as<String>();
  if (!doc["za_ep"].isNull())  za_ep_  = doc["za_ep"].as<String>();
  if (!doc["cp_key"].isNull()) cp_key_ = doc["cp_key"].as<String>();
  if (!doc["cp_org"].isNull()) cp_org_ = doc["cp_org"].as<String>();
  if (!doc["ls_url"].isNull()) ls_url_ = doc["ls_url"].as<String>();
  return true;
}

}  // namespace usage_monitor
