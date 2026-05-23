// Minimal "Arduino on host" sketch. Demonstrates that the runtime, Serial sink,
// and timing primitives are all wired correctly. Drives a virtual LED via stdout
// because there is no GPIO hardware on the host side.

#include <Arduino.h>

#include <cstdio>

namespace {
constexpr unsigned long kBlinkPeriodMs = 1000;
unsigned long g_iterations = 0;
}  // namespace

void setup() {
  arduino_host::set_serial_sink(stdout);  // Route Serial to stdout for predictable demo output.
  Serial.begin(115200);
  Serial.println("[blink_headless] arduino-host runtime ready");
  Serial.printf("[blink_headless] free heap=%u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()));
}

void loop() {
  unsigned long now = millis();
  bool ledOn = (now / kBlinkPeriodMs) % 2 == 0;
  Serial.printf("[t=%lu ms] LED=%s\n", now, ledOn ? "ON" : "OFF");
  delay(kBlinkPeriodMs);
  if (++g_iterations >= 5) {
    Serial.println("[blink_headless] done");
    std::exit(0);
  }
}

int main() {
  setup();
  while (true) {
    loop();
  }
}
