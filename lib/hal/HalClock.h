#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "HalGPIO.h"

class HalClock;
extern HalClock halClock;  // Singleton

struct UtcDateTime {
  uint16_t year = 2024;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
};

class HalClock {
  bool _available = false;
  mutable uint8_t _cachedHour = 0;
  mutable uint8_t _cachedMinute = 0;
  mutable bool _hasCachedTime = false;
  mutable unsigned long _lastPollMs = 0;

  static constexpr unsigned long CLOCK_POLL_MS = 10000;  // 10 seconds

 public:
  // Call after gpio.begin() and powerManager.begin() (I2C already initialised for X3)
  void begin();

  // True if the DS3231 RTC is present on this device (X3 only).
  bool isAvailable() const { return _available; }

  // True when wall-clock time is usable: DS3231 present or system epoch looks valid.
  bool hasValidTime() const;

  // Apply the saved fixed UTC offset (biased quarter-hours) to the C library TZ.
  void applySavedTimezone(uint8_t utcOffsetQuarterHoursBiased);

  // X3: copy the DS3231 UTC calendar into the ESP system epoch. No-op on X4 / missing RTC.
  bool hydrateSystemFromRtc();

  // Read the current UTC date/time from DS3231 (X3) or the system clock.
  bool getUtcDateTime(UtcDateTime& out) const;

  // Get current UTC hour (0-23) and minute (0-59).
  bool getTime(uint8_t& hour, uint8_t& minute) const;

  // Write UTC date/time to DS3231 when present and always update the system epoch.
  bool setUtcDateTime(const UtcDateTime& dt);

  // Format local wall time into a caller-provided buffer.
  // 24h mode produces "HH:MM" (needs >=6 bytes); 12h mode produces "H:MM AM"/"HH:MM PM" (needs >=9 bytes).
  // utcOffsetQuarterHoursBiased: biased quarter-hour offset (48 = UTC+0, 0 = UTC-12, 104 = UTC+14).
  bool formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48, bool use12Hour = false) const;

  // Format the full local date/time for settings display (YYYY-MM-DD HH:MM).
  bool formatDateTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Sync from NTP. X3 also writes the DS3231; X4 only updates the system epoch.
  // Requires WiFi to be connected. Blocks for up to ~5s while waiting for SNTP response.
  bool syncFromNTP();

  // Convert biased quarter-hour storage to POSIX offset seconds (positive = east of UTC).
  static int32_t biasedOffsetToSeconds(uint8_t utcOffsetQuarterHoursBiased);

  static time_t utcToEpoch(const UtcDateTime& dt);
  static void epochToUtc(time_t epoch, UtcDateTime& out);

 private:
  bool readRtcUtc(UtcDateTime& out) const;
  bool writeRtcUtc(const UtcDateTime& dt);
  bool writeTimeToRTC(uint8_t hour, uint8_t minute, uint8_t second);
};
