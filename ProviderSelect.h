// ProviderSelect.h -- provider ID constants and (legacy) compile-time selection.
// When UM_ALL_PROVIDERS is defined (runtime selection mode), only the UM_PROV_*
// constants are exported and all compile-time enforcement is skipped.

#ifndef USAGE_MONITOR_PROVIDER_SELECT_H
#define USAGE_MONITOR_PROVIDER_SELECT_H

#include "ProjectConfig.h"

#define UM_SIDE_LEFT  1
#define UM_SIDE_RIGHT 2

// Provider ID tokens (used both at compile time and as runtime uint8_t values).
#define UM_PROV_CLAUDE     1
#define UM_PROV_CODEX      2
#define UM_PROV_COPILOT    3
#define UM_PROV_MINIMAX    4
#define UM_PROV_KIMI       5
#define UM_PROV_ZAI        6
#define UM_PROV_CLAUDEPLAT 7

#ifndef UM_ALL_PROVIDERS

// --- Legacy compile-time selection (kept for backward compat) ---

// --- Resolve LEFT provider ---
#if defined(UM_ENABLE_CLAUDE) && UM_CLAUDE_SIDE == UM_SIDE_LEFT
  #define UM_LEFT_PROVIDER UM_PROV_CLAUDE
  #define UM_LEFT_NAME "Claude"
  #define UM_LEFT_KEY "claude"
#endif
#if defined(UM_ENABLE_CODEX) && UM_CODEX_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_CODEX
  #define UM_LEFT_NAME "Codex"
  #define UM_LEFT_KEY "codex"
#endif
#if defined(UM_ENABLE_COPILOT) && UM_COPILOT_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_COPILOT
  #define UM_LEFT_NAME "Copilot"
  #define UM_LEFT_KEY "copilot"
#endif
#if defined(UM_ENABLE_MINIMAX) && UM_MINIMAX_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_MINIMAX
  #define UM_LEFT_NAME "MiniMax"
  #define UM_LEFT_KEY "minimax"
#endif
#if defined(UM_ENABLE_KIMI) && UM_KIMI_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_KIMI
  #define UM_LEFT_NAME "Kimi"
  #define UM_LEFT_KEY "kimi"
#endif
#if defined(UM_ENABLE_ZAI) && UM_ZAI_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_ZAI
  #define UM_LEFT_NAME "Zai"
  #define UM_LEFT_KEY "zai"
#endif
#ifndef UM_LEFT_PROVIDER
  #error "No provider assigned to LEFT."
#endif

// --- Resolve RIGHT provider ---
#if defined(UM_ENABLE_CLAUDE) && UM_CLAUDE_SIDE == UM_SIDE_RIGHT
  #define UM_RIGHT_PROVIDER UM_PROV_CLAUDE
  #define UM_RIGHT_NAME "Claude"
  #define UM_RIGHT_KEY "claude"
#endif
#if defined(UM_ENABLE_CODEX) && UM_CODEX_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_CODEX
  #define UM_RIGHT_NAME "Codex"
  #define UM_RIGHT_KEY "codex"
#endif
#if defined(UM_ENABLE_COPILOT) && UM_COPILOT_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_COPILOT
  #define UM_RIGHT_NAME "Copilot"
  #define UM_RIGHT_KEY "copilot"
#endif
#if defined(UM_ENABLE_MINIMAX) && UM_MINIMAX_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_MINIMAX
  #define UM_RIGHT_NAME "MiniMax"
  #define UM_RIGHT_KEY "minimax"
#endif
#if defined(UM_ENABLE_KIMI) && UM_KIMI_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_KIMI
  #define UM_RIGHT_NAME "Kimi"
  #define UM_RIGHT_KEY "kimi"
#endif
#if defined(UM_ENABLE_ZAI) && UM_ZAI_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_ZAI
  #define UM_RIGHT_NAME "Zai"
  #define UM_RIGHT_KEY "zai"
#endif
#ifndef UM_RIGHT_PROVIDER
  #error "No provider assigned to RIGHT."
#endif

#endif  // !UM_ALL_PROVIDERS

#endif  // USAGE_MONITOR_PROVIDER_SELECT_H
