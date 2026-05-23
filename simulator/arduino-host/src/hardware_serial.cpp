#include <HardwareSerial.h>

#include <cstdio>
#include <mutex>

namespace {
std::mutex& serial_mutex() {
  static std::mutex m;
  return m;
}

std::FILE* default_sink() {
#if defined(ARDUINO_HOST_SERIAL_STDOUT)
  return stdout;
#elif defined(ARDUINO_HOST_SERIAL_FILE)
  return nullptr;  // Consumer must call setSink() with a FILE*.
#else
  return stderr;
#endif
}
}  // namespace

HardwareSerial::HardwareSerial(std::FILE* sink) : sink_(sink ? sink : default_sink()) {}

void HardwareSerial::begin(unsigned long /*baud*/) {
  // No-op: the sink is set at construction.
}
void HardwareSerial::begin(unsigned long /*baud*/, uint32_t /*config*/, int8_t /*rxPin*/, int8_t /*txPin*/) {}
void HardwareSerial::end() {}

size_t HardwareSerial::write(uint8_t b) {
  if (!sink_) return 0;
  std::lock_guard<std::mutex> lock(serial_mutex());
  std::fputc(b, sink_);
  return 1;
}

size_t HardwareSerial::write(const uint8_t* buffer, size_t size) {
  if (!sink_) return 0;
  std::lock_guard<std::mutex> lock(serial_mutex());
  return std::fwrite(buffer, 1, size, sink_);
}

void HardwareSerial::flush() {
  if (!sink_) return;
  std::lock_guard<std::mutex> lock(serial_mutex());
  std::fflush(sink_);
}

HardwareSerial Serial;

namespace arduino_host {
void set_serial_sink(std::FILE* sink) { Serial.setSink(sink); }
}  // namespace arduino_host
