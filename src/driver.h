#ifndef USAGE_MONITOR_DRIVER_H
#define USAGE_MONITOR_DRIVER_H

// The device target is selected by platformio.ini via a build flag:
//   -D UM_DEVICE_E1001 / E1002 / E1003
// Fall back to E1003 when built without an explicit target (e.g. Arduino IDE).
#if !defined(UM_DEVICE_E1001) && \
    !defined(UM_DEVICE_E1002) && \
    !defined(UM_DEVICE_E1003)
  #define UM_DEVICE_E1003
#endif

#define UM_SCREEN_GRAY4   1
#define UM_SCREEN_COLOR6  2
#define UM_SCREEN_GRAY16  3

#if defined(UM_DEVICE_E1001)
  #define BOARD_SCREEN_COMBO 520
  #define UM_DEVICE_NAME "reTerminal E1001"
  #define UM_SCREEN_MODE UM_SCREEN_GRAY4
  #define UM_LED_PIN 6
#elif defined(UM_DEVICE_E1002)
  #define BOARD_SCREEN_COMBO 521
  #define UM_DEVICE_NAME "reTerminal E1002"
  #define UM_SCREEN_MODE UM_SCREEN_COLOR6
  #define UM_LED_PIN 6
#elif defined(UM_DEVICE_E1003)
  #define BOARD_SCREEN_COMBO 522
  #define UM_DEVICE_NAME "reTerminal E1003"
  #define UM_SCREEN_MODE UM_SCREEN_GRAY16
  #define UM_LED_PIN 16
#else
  #error "Select UM_DEVICE_E1001, UM_DEVICE_E1002, or UM_DEVICE_E1003."
#endif

#endif  // USAGE_MONITOR_DRIVER_H
