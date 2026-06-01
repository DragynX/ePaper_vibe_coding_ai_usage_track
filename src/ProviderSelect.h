// ProviderSelect.h -- compile-time provider selection via build flags.
// 编译期 provider 选择:用 -D UM_ENABLE_<X> 和 -D UM_<X>_SIDE=1|2 选定左右各一个。
//
// Side convention: 1 = LEFT, 2 = RIGHT.
// Exactly one provider must be assigned to each side, enforced by #error.
//
// This header centralizes all the #if logic so UsageApp and main.cpp just read
// the resolved UM_LEFT_PROVIDER / UM_RIGHT_PROVIDER tokens.

#ifndef USAGE_MONITOR_PROVIDER_SELECT_H
#define USAGE_MONITOR_PROVIDER_SELECT_H

#define UM_SIDE_LEFT  1
#define UM_SIDE_RIGHT 2

// Provider id tokens (arbitrary, used only in this header's switch).
#define UM_PROV_CLAUDE  1
#define UM_PROV_CODEX   2
#define UM_PROV_COPILOT 3
#define UM_PROV_MINIMAX 4
#define UM_PROV_KIMI    5
#define UM_PROV_ZAI     6

// --- Resolve LEFT provider ---
#if defined(UM_ENABLE_CLAUDE) && UM_CLAUDE_SIDE == UM_SIDE_LEFT
  #define UM_LEFT_PROVIDER UM_PROV_CLAUDE
  #define UM_LEFT_NAME "Claude"
#endif
#if defined(UM_ENABLE_CODEX) && UM_CODEX_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT. Only one is allowed per side."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_CODEX
  #define UM_LEFT_NAME "Codex"
#endif
#if defined(UM_ENABLE_COPILOT) && UM_COPILOT_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT. Only one is allowed per side."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_COPILOT
  #define UM_LEFT_NAME "Copilot"
#endif
#if defined(UM_ENABLE_MINIMAX) && UM_MINIMAX_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT. Only one is allowed per side."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_MINIMAX
  #define UM_LEFT_NAME "MiniMax"
#endif
#if defined(UM_ENABLE_KIMI) && UM_KIMI_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT. Only one is allowed per side."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_KIMI
  #define UM_LEFT_NAME "Kimi"
#endif
#if defined(UM_ENABLE_ZAI) && UM_ZAI_SIDE == UM_SIDE_LEFT
  #ifdef UM_LEFT_PROVIDER
    #error "Multiple providers assigned to LEFT. Only one is allowed per side."
  #endif
  #define UM_LEFT_PROVIDER UM_PROV_ZAI
  #define UM_LEFT_NAME "Zai"
#endif
#ifndef UM_LEFT_PROVIDER
  #error "No provider assigned to LEFT. Define e.g. -D UM_ENABLE_CODEX -D UM_CODEX_SIDE=1"
#endif

// --- Resolve RIGHT provider ---
#if defined(UM_ENABLE_CLAUDE) && UM_CLAUDE_SIDE == UM_SIDE_RIGHT
  #define UM_RIGHT_PROVIDER UM_PROV_CLAUDE
  #define UM_RIGHT_NAME "Claude"
#endif
#if defined(UM_ENABLE_CODEX) && UM_CODEX_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT. Only one is allowed per side."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_CODEX
  #define UM_RIGHT_NAME "Codex"
#endif
#if defined(UM_ENABLE_COPILOT) && UM_COPILOT_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT. Only one is allowed per side."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_COPILOT
  #define UM_RIGHT_NAME "Copilot"
#endif
#if defined(UM_ENABLE_MINIMAX) && UM_MINIMAX_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT. Only one is allowed per side."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_MINIMAX
  #define UM_RIGHT_NAME "MiniMax"
#endif
#if defined(UM_ENABLE_KIMI) && UM_KIMI_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT. Only one is allowed per side."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_KIMI
  #define UM_RIGHT_NAME "Kimi"
#endif
#if defined(UM_ENABLE_ZAI) && UM_ZAI_SIDE == UM_SIDE_RIGHT
  #ifdef UM_RIGHT_PROVIDER
    #error "Multiple providers assigned to RIGHT. Only one is allowed per side."
  #endif
  #define UM_RIGHT_PROVIDER UM_PROV_ZAI
  #define UM_RIGHT_NAME "Zai"
#endif
#ifndef UM_RIGHT_PROVIDER
  #error "No provider assigned to RIGHT. Define e.g. -D UM_ENABLE_CLAUDE -D UM_CLAUDE_SIDE=2"
#endif

#endif  // USAGE_MONITOR_PROVIDER_SELECT_H
