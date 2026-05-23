#include <Arduino.h>

#include <chrono>
#include <cstdlib>
#include <random>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
const Clock::time_point kEpoch = Clock::now();

std::mt19937& random_engine() {
  thread_local std::mt19937 engine{std::random_device{}()};
  return engine;
}
}  // namespace

unsigned long millis() {
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - kEpoch).count());
}

unsigned long micros() {
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - kEpoch).count());
}

void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

void delayMicroseconds(unsigned long us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

void yield() { std::this_thread::yield(); }

long random(long upper) {
  if (upper <= 0) return 0;
  std::uniform_int_distribution<long> dist(0, upper - 1);
  return dist(random_engine());
}

long random(long lower, long upper) {
  if (upper <= lower) return lower;
  std::uniform_int_distribution<long> dist(lower, upper - 1);
  return dist(random_engine());
}

void randomSeed(unsigned long seed) { random_engine().seed(seed); }

// Weak GPIO/ADC defaults. Consumers can link strong overrides.
__attribute__((weak)) void pinMode(uint8_t, uint8_t) {}
__attribute__((weak)) void digitalWrite(uint8_t, uint8_t) {}
__attribute__((weak)) int digitalRead(uint8_t) { return 0; }
__attribute__((weak)) uint16_t analogRead(uint8_t) { return 0; }
__attribute__((weak)) void analogWrite(uint8_t, int) {}

bool setCpuFrequencyMhz(uint32_t) { return true; }
uint32_t getCpuFrequencyMhz() { return 160; }

// Global Arduino-core bus instances. WiFi / HTTPClient / OTA / etc are Arduino-ESP32
// ecosystem libraries — not arduino-host's responsibility; consumers that need them
// supply their own header + global.
SPIClass SPI;
TwoWire Wire;
