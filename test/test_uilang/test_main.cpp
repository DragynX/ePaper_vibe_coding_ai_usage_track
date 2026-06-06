#include <unity.h>
#include <string.h>
#include "DisplayText.h"
#include "UiLang.h"

// The string table exposes two columns (uiStrEn / uiStrZh) so this one native
// build can assert both languages regardless of UM_LANG_ZH.

void test_en_column_spot_values() {
  TEST_ASSERT_EQUAL_STRING("AI Usage Monitor", uiStrEn(UiStringId::kAppName));
  TEST_ASSERT_EQUAL_STRING("Session",          uiStrEn(UiStringId::kWinSession));
  TEST_ASSERT_EQUAL_STRING("resets",        uiStrEn(UiStringId::kResets));
  TEST_ASSERT_EQUAL_STRING("CLOUD SUMMARY", uiStrEn(UiStringId::kCloudSummary));
  TEST_ASSERT_EQUAL_STRING("SESSION LEFT",  uiStrEn(UiStringId::kSessionLeft));
}

void test_zh_column_spot_values() {
  TEST_ASSERT_EQUAL_STRING("用量监控屏", uiStrZh(UiStringId::kAppName));
  TEST_ASSERT_EQUAL_STRING("5 小时",     uiStrZh(UiStringId::kWinSession));
  TEST_ASSERT_EQUAL_STRING("重置",       uiStrZh(UiStringId::kResets));
  TEST_ASSERT_EQUAL_STRING("云端汇总",   uiStrZh(UiStringId::kCloudSummary));
  TEST_ASSERT_EQUAL_STRING("5小时剩余",  uiStrZh(UiStringId::kSessionLeft));
}

// The subset font has no CJK punctuation; every zh string must use spaces only.
void test_zh_strings_avoid_punctuation() {
  for (int i = 0; i < static_cast<int>(UiStringId::kCount); i++) {
    const char* s = uiStrZh(static_cast<UiStringId>(i));
    TEST_ASSERT_NULL(strstr(s, "，"));
    TEST_ASSERT_NULL(strstr(s, "。"));
    TEST_ASSERT_NULL(strstr(s, "："));
  }
}

void test_zh_display_text_replaces_punctuation_with_spaces() {
  const std::string text = umSanitizeDisplayTextForLang("买苹果，香蕉。OK!", true);
  TEST_ASSERT_EQUAL_STRING("买苹果 香蕉 OK", text.c_str());
}

void test_en_display_text_keeps_punctuation() {
  const std::string text = umSanitizeDisplayTextForLang("Buy apples, then go.", false);
  TEST_ASSERT_EQUAL_STRING("Buy apples, then go.", text.c_str());
}

// Every id must resolve to a non-empty string in both columns, and the two
// columns must differ (catches a copy-paste that left a cell untranslated).
void test_every_id_nonempty_and_distinct() {
  for (int i = 0; i < static_cast<int>(UiStringId::kCount); i++) {
    const UiStringId id = static_cast<UiStringId>(i);
    TEST_ASSERT_TRUE(uiStrEn(id)[0] != '\0');
    TEST_ASSERT_TRUE(uiStrZh(id)[0] != '\0');
    TEST_ASSERT_TRUE(strcmp(uiStrEn(id), uiStrZh(id)) != 0);
  }
}

void test_wipe_when_tag_differs_or_missing() {
  TEST_ASSERT_FALSE(umShouldWipeForLanguage(0, 0));
  TEST_ASSERT_FALSE(umShouldWipeForLanguage(1, 1));
  TEST_ASSERT_TRUE(umShouldWipeForLanguage(0, 1));
  TEST_ASSERT_TRUE(umShouldWipeForLanguage(1, 0));
  TEST_ASSERT_TRUE(umShouldWipeForLanguage(-1, 0));
  TEST_ASSERT_TRUE(umShouldWipeForLanguage(-1, 1));
}

int main(int argc, char** argv) {
  UNITY_BEGIN();
  RUN_TEST(test_en_column_spot_values);
  RUN_TEST(test_zh_column_spot_values);
  RUN_TEST(test_zh_strings_avoid_punctuation);
  RUN_TEST(test_zh_display_text_replaces_punctuation_with_spaces);
  RUN_TEST(test_en_display_text_keeps_punctuation);
  RUN_TEST(test_every_id_nonempty_and_distinct);
  RUN_TEST(test_wipe_when_tag_differs_or_missing);
  return UNITY_END();
}
