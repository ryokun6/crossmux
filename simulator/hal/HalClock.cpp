// HAL clock implementation for the desktop simulator. Uses the host system
// clock as the UTC source so date/time settings can be exercised without a DS3231.

#include <HalClock.h>

#include <cstdio>
#include <sys/time.h>
#include <time.h>

HalClock halClock;

namespace {
constexpr uint32_t VALID_CLOCK_THRESHOLD = 1704067200UL;  // 2024-01-01 UTC

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

time_t utcToEpoch(const UtcDateTime& dt) {
  const int32_t days = daysFromCivil(static_cast<int>(dt.year), static_cast<int>(dt.month), static_cast<int>(dt.day));
  return static_cast<time_t>(days) * 86400 + dt.hour * 3600 + dt.minute * 60 + dt.second;
}

void epochToUtc(const time_t epoch, UtcDateTime& out) {
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
}  // namespace

void HalClock::begin() {
  // Host has no DS3231; report unavailable so manual-set UI stays X3-gated.
  _available = false;
}

void HalClock::applySavedTimezone(const uint8_t utcOffsetQuarterHoursBiased) {
  const int32_t offsetSec = biasedOffsetToSeconds(utcOffsetQuarterHoursBiased);
  const int32_t posixSec = -offsetSec;
  char tz[20];
  const int32_t absSec = posixSec >= 0 ? posixSec : -posixSec;
  const int hours = static_cast<int>(absSec / 3600);
  const int mins = static_cast<int>((absSec % 3600) / 60);
  const char sign = posixSec >= 0 ? '-' : '+';
  if (mins == 0) {
    snprintf(tz, sizeof(tz), "UTC%c%d", sign, hours);
  } else {
    snprintf(tz, sizeof(tz), "UTC%c%d:%02d", sign, hours, mins);
  }
  setenv("TZ", tz, 1);
  tzset();
}

bool HalClock::hasValidTime() const { return static_cast<uint32_t>(time(nullptr)) >= VALID_CLOCK_THRESHOLD; }

bool HalClock::hydrateSystemFromRtc() { return false; }

bool HalClock::getUtcDateTime(UtcDateTime& out) const {
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

bool HalClock::setUtcDateTime(const UtcDateTime& dt) {
  const time_t epoch = utcToEpoch(dt);
  timeval tv = {.tv_sec = epoch, .tv_usec = 0};
  return settimeofday(&tv, nullptr) == 0;
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
  const int64_t totalSeconds = static_cast<int64_t>(utcToEpoch(utc)) + static_cast<int64_t>(offsetQuarterHours) * 15 * 60;
  const time_t localEpoch = static_cast<time_t>(totalSeconds);
  tm local{};
  gmtime_r(&localEpoch, &local);
  snprintf(buf, bufSize, "%04d-%02d-%02d %02d:%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
           local.tm_min);
  return true;
}

bool HalClock::syncFromNTP() { return false; }
