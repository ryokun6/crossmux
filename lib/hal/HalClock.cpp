#include "HalClock.h"

#include <Logging.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include <time.h>

#include <cassert>
#include <cstdio>

HalClock halClock;  // Singleton instance

namespace {
constexpr uint32_t VALID_CLOCK_THRESHOLD = 1704067200UL;  // 2024-01-01 UTC

// DS3231 register layout (BCD encoded):
//   0x00: Seconds
//   0x01: Minutes
//   0x02: Hours    (bit 6 = 12/24 mode, bits 5-0 = hours in 24h mode)
//   0x03: Day of week
//   0x04: Date
//   0x05: Month / century
//   0x06: Year (00-99)

uint8_t bcdToDec(uint8_t bcd) { return ((bcd >> 4) * 10) + (bcd & 0x0F); }
uint8_t decToBcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

// Howard Hinnant days-from-civil (proleptic Gregorian).
int32_t daysFromCivil(int year, int month, int day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
  return era * 146097 + static_cast<int>(dayOfEra) - 719468;
}

void civilFromDays(int z, int& year, int& month, int& day) {
  z += 719468;
  const int era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned dayOfEra = static_cast<unsigned>(z - era * 146097);
  const unsigned yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  year = static_cast<int>(yearOfEra) + era * 400;
  const unsigned dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const unsigned monthPart = (5 * dayOfYear + 2) / 153;
  day = static_cast<int>(dayOfYear - (153 * monthPart + 2) / 5 + 1);
  month = static_cast<int>(monthPart + (monthPart < 10 ? 3 : -9));
  year += (month <= 2);
}

void buildPosixTz(const int32_t offsetSecEast, char* buf, const size_t bufSize) {
  // POSIX TZ offset is positive west of UTC; negate the east-positive offset.
  const int32_t posixSec = -offsetSecEast;
  const int32_t absSec = posixSec >= 0 ? posixSec : -posixSec;
  const int hours = static_cast<int>(absSec / 3600);
  const int mins = static_cast<int>((absSec % 3600) / 60);
  const char sign = posixSec >= 0 ? '-' : '+';
  if (mins == 0) {
    snprintf(buf, bufSize, "UTC%c%d", sign, hours);
  } else {
    snprintf(buf, bufSize, "UTC%c%d:%02d", sign, hours, mins);
  }
}

uint8_t centuryBitForYear(const uint16_t year) { return (year >= 2000) ? 0 : static_cast<uint8_t>(0x80); }

uint16_t yearFromRtcCentury(const uint8_t centuryBit, const uint8_t yearBcd) {
  const uint8_t yearTwo = bcdToDec(yearBcd);
  return static_cast<uint16_t>((centuryBit ? 1900 : 2000) + yearTwo);
}
}  // namespace

int32_t HalClock::biasedOffsetToSeconds(const uint8_t utcOffsetQuarterHoursBiased) {
  uint8_t biased = utcOffsetQuarterHoursBiased;
  if (biased > 104) biased = 48;
  return (static_cast<int32_t>(biased) - 48) * 15 * 60;
}

time_t HalClock::utcToEpoch(const UtcDateTime& dt) {
  const int32_t days = daysFromCivil(static_cast<int>(dt.year), static_cast<int>(dt.month), static_cast<int>(dt.day));
  return static_cast<time_t>(days) * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

void HalClock::epochToUtc(const time_t epoch, UtcDateTime& out) {
  if (epoch < 0) {
    out = {};
    return;
  }
  const time_t dayEpoch = epoch / 86400;
  const int32_t daySeconds = static_cast<int32_t>(epoch % 86400);
  int year = 0;
  int month = 0;
  int day = 0;
  civilFromDays(static_cast<int>(dayEpoch), year, month, day);
  out.year = static_cast<uint16_t>(year);
  out.month = static_cast<uint8_t>(month);
  out.day = static_cast<uint8_t>(day);
  out.hour = static_cast<uint8_t>(daySeconds / 3600);
  out.minute = static_cast<uint8_t>((daySeconds % 3600) / 60);
  out.second = static_cast<uint8_t>(daySeconds % 60);
}

void HalClock::begin() {
  if (!gpio.deviceIsX3()) {
    _available = false;
    return;
  }

  Wire.beginTransmission(I2C_ADDR_DS3231);
  Wire.write(DS3231_SEC_REG);
  if (Wire.endTransmission(false) != 0) {
    LOG_INF("CLK", "DS3231 RTC not found");
    _available = false;
    return;
  }
  Wire.requestFrom(I2C_ADDR_DS3231, static_cast<uint8_t>(1));
  if (Wire.available() < 1) {
    _available = false;
    return;
  }
  Wire.read();

  _available = true;
  LOG_INF("CLK", "DS3231 RTC found");

  uint8_t h = 0;
  uint8_t m = 0;
  getTime(h, m);
}

void HalClock::applySavedTimezone(const uint8_t utcOffsetQuarterHoursBiased) {
  char tz[20];
  buildPosixTz(biasedOffsetToSeconds(utcOffsetQuarterHoursBiased), tz, sizeof(tz));
  setenv("TZ", tz, 1);
  tzset();
}

bool HalClock::hasValidTime() const {
  if (_available) return true;
  return static_cast<uint32_t>(time(nullptr)) >= VALID_CLOCK_THRESHOLD;
}

bool HalClock::hydrateSystemFromRtc() {
  if (!_available) return false;

  UtcDateTime dt{};
  if (!readRtcUtc(dt)) return false;

  const time_t epoch = utcToEpoch(dt);
  if (epoch < static_cast<time_t>(VALID_CLOCK_THRESHOLD)) return false;

  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  if (settimeofday(&tv, nullptr) != 0) {
    LOG_ERR("CLK", "Failed to hydrate system time from DS3231");
    return false;
  }

  LOG_INF("CLK", "System time hydrated from DS3231: %04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day,
          dt.hour, dt.minute, dt.second);
  return true;
}

bool HalClock::readRtcUtc(UtcDateTime& out) const {
  if (!_available) return false;

  Wire.beginTransmission(I2C_ADDR_DS3231);
  Wire.write(DS3231_SEC_REG);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(I2C_ADDR_DS3231, static_cast<uint8_t>(7));
  if (Wire.available() < 7) return false;

  const uint8_t rawSec = Wire.read();
  const uint8_t rawMin = Wire.read();
  const uint8_t rawHour = Wire.read();
  Wire.read();  // day of week
  const uint8_t rawDate = Wire.read();
  const uint8_t rawMonth = Wire.read();
  const uint8_t rawYear = Wire.read();

  out.second = bcdToDec(rawSec & 0x7F);
  out.minute = bcdToDec(rawMin & 0x7F);

  if (rawHour & 0x40) {
    uint8_t h12 = bcdToDec(rawHour & 0x1F);
    const bool pm = rawHour & 0x20;
    if (h12 == 12) h12 = 0;
    out.hour = pm ? static_cast<uint8_t>(h12 + 12) : h12;
  } else {
    out.hour = bcdToDec(rawHour & 0x3F);
  }

  out.day = bcdToDec(rawDate & 0x3F);
  out.month = bcdToDec(rawMonth & 0x1F);
  out.year = yearFromRtcCentury(rawMonth & 0x80, rawYear);
  return true;
}

bool HalClock::getUtcDateTime(UtcDateTime& out) const {
  if (_available) {
    const unsigned long now = millis();
    if (_lastPollMs != 0 && (now - _lastPollMs) < CLOCK_POLL_MS && _hasCachedTime) {
      if (!readRtcUtc(out)) return false;
      out.hour = _cachedHour;
      out.minute = _cachedMinute;
      return true;
    }

    if (!readRtcUtc(out)) {
      if (!_hasCachedTime) return false;
      out.hour = _cachedHour;
      out.minute = _cachedMinute;
      return true;
    }

    _cachedHour = out.hour;
    _cachedMinute = out.minute;
    _hasCachedTime = true;
    _lastPollMs = now;
    return true;
  }

  epochToUtc(time(nullptr), out);
  return hasValidTime();
}

bool HalClock::getTime(uint8_t& hour, uint8_t& minute) const {
  UtcDateTime dt{};
  if (!getUtcDateTime(dt)) return false;
  hour = dt.hour;
  minute = dt.minute;
  return true;
}

bool HalClock::writeRtcUtc(const UtcDateTime& dt) {
  assert(dt.hour < 24);
  assert(dt.minute < 60);
  assert(dt.second < 60);
  assert(dt.day >= 1 && dt.day <= 31);
  assert(dt.month >= 1 && dt.month <= 12);

  Wire.beginTransmission(I2C_ADDR_DS3231);
  Wire.write(DS3231_SEC_REG);
  Wire.write(decToBcd(dt.second));
  Wire.write(decToBcd(dt.minute));
  Wire.write(decToBcd(dt.hour));
  Wire.write(decToBcd(1));  // day-of-week unused by firmware
  Wire.write(decToBcd(dt.day));
  Wire.write(static_cast<uint8_t>(decToBcd(dt.month) | centuryBitForYear(dt.year)));
  Wire.write(decToBcd(static_cast<uint8_t>(dt.year % 100)));
  if (Wire.endTransmission() != 0) {
    LOG_ERR("CLK", "Failed to write date/time to DS3231");
    return false;
  }

  _lastPollMs = 0;
  _cachedHour = dt.hour;
  _cachedMinute = dt.minute;
  _hasCachedTime = true;
  return true;
}

bool HalClock::setUtcDateTime(const UtcDateTime& dt) {
  const time_t epoch = utcToEpoch(dt);
  if (epoch < 0) return false;

  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  if (settimeofday(&tv, nullptr) != 0) {
    LOG_ERR("CLK", "Failed to set system time");
    return false;
  }

  if (_available && !writeRtcUtc(dt)) return false;
  return true;
}

bool HalClock::formatTime(char* buf, const size_t bufSize, uint8_t utcOffsetQuarterHoursBiased,
                          const bool use12Hour) const {
  if (bufSize < (use12Hour ? 9u : 6u)) return false;
  UtcDateTime utc{};
  if (!getUtcDateTime(utc)) return false;

  if (utcOffsetQuarterHoursBiased > 104) utcOffsetQuarterHoursBiased = 104;
  const int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  int totalMinutes = static_cast<int>(utc.hour) * 60 + static_cast<int>(utc.minute) + offsetQuarterHours * 15;
  totalMinutes = ((totalMinutes % 1440) + 1440) % 1440;

  const int hour24 = totalMinutes / 60;
  const int min = totalMinutes % 60;
  if (use12Hour) {
    const bool pm = hour24 >= 12;
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;
    snprintf(buf, bufSize, "%d:%02d %s", hour12, min, pm ? "PM" : "AM");
  } else {
    snprintf(buf, bufSize, "%02d:%02d", hour24, min);
  }
  return true;
}

bool HalClock::formatDateTime(char* buf, const size_t bufSize, uint8_t utcOffsetQuarterHoursBiased) const {
  if (bufSize < 17) return false;
  UtcDateTime utc{};
  if (!getUtcDateTime(utc)) return false;

  if (utcOffsetQuarterHoursBiased > 104) utcOffsetQuarterHoursBiased = 104;
  const int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  int64_t totalSeconds = static_cast<int64_t>(utcToEpoch(utc)) + static_cast<int64_t>(offsetQuarterHours) * 15 * 60;

  const int64_t daySeconds = ((totalSeconds % 86400) + 86400) % 86400;
  const int64_t dayOrdinal = (totalSeconds - daySeconds) / 86400;

  int year = 0;
  int month = 0;
  int day = 0;
  civilFromDays(static_cast<int>(dayOrdinal), year, month, day);

  const int hour24 = static_cast<int>(daySeconds / 3600);
  const int min = static_cast<int>((daySeconds % 3600) / 60);
  snprintf(buf, bufSize, "%04d-%02d-%02d %02d:%02d", year, month, day, hour24, min);
  return true;
}

bool HalClock::writeTimeToRTC(const uint8_t hour, const uint8_t minute, const uint8_t second) {
  UtcDateTime dt{};
  if (!readRtcUtc(dt)) {
    dt.year = 2024;
    dt.month = 1;
    dt.day = 1;
  }
  dt.hour = hour;
  dt.minute = minute;
  dt.second = second;
  return writeRtcUtc(dt);
}

bool HalClock::syncFromNTP() {
  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("CLK", "WiFi not connected, cannot sync NTP");
    return false;
  }

  LOG_INF("CLK", "Starting NTP sync...");
  configTzTime("UTC0", "pool.ntp.org", "time.nist.gov");

  constexpr int maxAttempts = 50;
  for (int i = 0; i < maxAttempts; i++) {
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      const time_t now = time(nullptr);
      UtcDateTime dt{};
      epochToUtc(now, dt);

      timeval tv = {.tv_sec = now, .tv_usec = 0};
      settimeofday(&tv, nullptr);

      if (_available) {
        if (writeRtcUtc(dt)) {
          LOG_INF("CLK", "RTC set to %04u-%02u-%02u %02u:%02u:%02u UTC", dt.year, dt.month, dt.day, dt.hour, dt.minute,
                  dt.second);
          return true;
        }
        return false;
      }

      LOG_INF("CLK", "System clock set from NTP (no persistent RTC)");
      return true;
    }
    delay(100);
  }

  LOG_ERR("CLK", "NTP sync timed out");
  return false;
}
