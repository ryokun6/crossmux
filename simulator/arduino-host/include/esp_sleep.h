#pragma once
// Host stub for ESP-IDF light-sleep. The simulator reports USB always connected
// (HalGPIO::isUsbConnected() == true), so StandbyActivity never enters Sleep mode and
// these symbols are unreachable at runtime; provide inline no-ops just for linkage.

#include <esp_system.h>  // esp_sleep_source_t + ESP_SLEEP_WAKEUP_*

#include <cstdint>

typedef int gpio_num_t;

typedef enum {
  GPIO_INTR_DISABLE = 0,
  GPIO_INTR_POSEDGE,
  GPIO_INTR_NEGEDGE,
  GPIO_INTR_ANYEDGE,
  GPIO_INTR_LOW_LEVEL,
  GPIO_INTR_HIGH_LEVEL,
} gpio_int_type_t;

inline int esp_sleep_enable_timer_wakeup(uint64_t) { return 0; }
inline int esp_sleep_enable_gpio_wakeup() { return 0; }
inline int esp_sleep_disable_wakeup_source(esp_sleep_source_t) { return 0; }
inline int esp_light_sleep_start() { return 0; }
inline int gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return 0; }
inline int gpio_wakeup_disable(gpio_num_t) { return 0; }
