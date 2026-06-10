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
  int  uiFont() const           { return ui_font_; }
  bool uiAa() const             { return ui_aa_; }
  int  uiSharp() const          { return ui_sharp_; }
  uint16_t battFullMv() const   { return battFull_; }

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
  const String& claudePlatPrepaid() const { return cp_prepaid_; }
  const String& claudePlatTopup() const   { return cp_topup_; }
  const String& claudePlatMode() const     { return cp_mode_; }      // "prepaid" | "spend"
  const String& claudePlatSpendWin() const { return cp_spendwin_; }  // "7" | "14" | "30"
  const String& localStatsUrl() const  { return ls_url_; }

  void setLeftProvider(uint8_t v)        { leftProv_ = v; }
  void setRightProvider(uint8_t v)       { rightProv_ = v; }
  void setTz(const String& v)            { tz_ = v; }
  void setRefreshSec(uint32_t v)         { refreshSec_ = (v < 300 ? 300 : (v > 3600 ? 3600 : v)); }
  void setDeepSleep(bool v)              { deepSleep_ = v; }
  void setDarkMode(bool v)               { dark_ = v; }
  void setSecureTokens(bool v)           { secure_ = v; }
  void setUiFont(int v)                  { ui_font_ = v; }
  void setUiAa(bool v)                   { ui_aa_ = v; }
  void setUiSharp(int v)                 { ui_sharp_ = v; }
  void setBattFullMv(uint16_t v)         { battFull_ = v; }
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
  void setClaudePlatPrepaid(const String& v) { cp_prepaid_ = v; }
  void setClaudePlatTopup(const String& v)   { cp_topup_ = v; }
  void setClaudePlatMode(const String& v)     { cp_mode_ = v; }
  void setClaudePlatSpendWin(const String& v) { cp_spendwin_ = v; }
  void setLocalStatsUrl(const String& v) { ls_url_ = v; }

  String toJson() const;
  bool   fromJson(const String& json);

  // Erase all credential fields for one provider (id 1..7). Caller saves.
  void clearProvider(uint8_t prov);

 private:
  uint8_t leftProv_    = 0;
  uint8_t rightProv_   = 0;
  String  tz_          = "UTC0";
  uint32_t refreshSec_ = 300;
  bool    deepSleep_   = false;
  bool    dark_        = false;
  int     ui_font_     = 0;       // selected device typeface index (0..N-1)
  bool    ui_aa_       = true;    // grayscale anti-aliasing (smooth) vs crisp 1-bit
  int     ui_sharp_    = 50;      // AA edge contrast 0..100 (higher = sharper)
  bool    secure_      = false;
  uint16_t battFull_   = 4200;
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
  String cp_prepaid_, cp_topup_;
  String cp_mode_     = "prepaid";   // device shows prepaid-remaining vs spend-window
  String cp_spendwin_ = "30";        // 7 | 14 | 30 day cost when in spend mode
  String ls_url_;
};

}  // namespace usage_monitor
