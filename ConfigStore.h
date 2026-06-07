#pragma once
#include <Arduino.h>

namespace usage_monitor {

class ConfigStore {
 public:
  void begin();
  void load();
  void save();

  uint8_t leftProvider() const  { return leftProv_; }
  uint8_t rightProvider() const { return rightProv_; }
  const String& tz() const      { return tz_; }
  uint32_t refreshSec() const   { return refreshSec_; }
  bool deepSleepEnabled() const { return deepSleep_; }
  bool darkMode() const         { return dark_; }
  bool secureTokens() const     { return secure_; }

  // Bitmask of providers whose secret changed in the last fromJson() (bit n =
  // provider id n). Consumed by the token-test pass, then cleared.
  uint8_t pendingTestMask() const { return pendingTestMask_; }
  void    clearPendingTest()      { pendingTestMask_ = 0; }

  const String& claudeAt() const       { return cl_at_; }
  const String& claudeRt() const       { return cl_rt_; }
  const String& claudeExp() const      { return cl_exp_; }
  const String& claudeSub() const      { return cl_sub_; }
  const String& codexAt() const        { return cx_at_; }
  const String& codexRt() const        { return cx_rt_; }
  const String& codexAid() const       { return cx_aid_; }
  const String& codexLr() const        { return cx_lr_; }
  const String& copilotPat() const     { return co_pat_; }
  const String& minimaxKey() const     { return mm_key_; }
  int           minimaxRegion() const  { return mm_reg_; }
  const String& kimiToken() const      { return ki_tok_; }
  const String& zaiKey() const         { return za_key_; }
  const String& zaiEndpoint() const    { return za_ep_; }
  const String& claudePlatKey() const  { return cp_key_; }
  const String& claudePlatOrg() const  { return cp_org_; }
  const String& localStatsUrl() const  { return ls_url_; }

  void setLeftProvider(uint8_t v)        { leftProv_ = v; }
  void setRightProvider(uint8_t v)       { rightProv_ = v; }
  void setTz(const String& v)            { tz_ = v; }
  void setRefreshSec(uint32_t v)         { refreshSec_ = (v < 300 ? 300 : (v > 3600 ? 3600 : v)); }
  void setDeepSleep(bool v)              { deepSleep_ = v; }
  void setDarkMode(bool v)               { dark_ = v; }
  void setSecureTokens(bool v)           { secure_ = v; }
  void setClaudeAt(const String& v)      { cl_at_ = v; }
  void setClaudeRt(const String& v)      { cl_rt_ = v; }
  void setClaudeExp(const String& v)     { cl_exp_ = v; }
  void setClaudeSub(const String& v)     { cl_sub_ = v; }
  void setCodexAt(const String& v)       { cx_at_ = v; }
  void setCodexRt(const String& v)       { cx_rt_ = v; }
  void setCodexAid(const String& v)      { cx_aid_ = v; }
  void setCodexLr(const String& v)       { cx_lr_ = v; }
  void setCopilotPat(const String& v)    { co_pat_ = v; }
  void setMinimaxKey(const String& v)    { mm_key_ = v; }
  void setMinimaxRegion(int v)           { mm_reg_ = v; }
  void setKimiToken(const String& v)     { ki_tok_ = v; }
  void setZaiKey(const String& v)        { za_key_ = v; }
  void setZaiEndpoint(const String& v)   { za_ep_ = v; }
  void setClaudePlatKey(const String& v) { cp_key_ = v; }
  void setClaudePlatOrg(const String& v) { cp_org_ = v; }
  void setLocalStatsUrl(const String& v) { ls_url_ = v; }

  String toJson() const;
  bool   fromJson(const String& json);

 private:
  uint8_t leftProv_    = 0;
  uint8_t rightProv_   = 0;
  String  tz_          = "UTC0";
  uint32_t refreshSec_ = 300;
  bool    deepSleep_   = false;
  bool    dark_        = false;
  bool    secure_      = false;
  uint8_t pendingTestMask_ = 0;

  String cl_at_, cl_rt_, cl_exp_, cl_sub_;
  String cx_at_, cx_rt_, cx_aid_, cx_lr_;
  String co_pat_;
  String mm_key_;
  int    mm_reg_ = 0;
  String ki_tok_;
  String za_key_;
  String za_ep_ = "https://api.z.ai";
  String cp_key_, cp_org_;
  String ls_url_;
};

}  // namespace usage_monitor
