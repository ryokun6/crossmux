// HAL power manager backend stub for the simulator.
// The host has no real power management — battery is fixed. Deep sleep is a
// no-op (same as the emscripten build): std::exit() still runs static
// destructors and trips ActivityManager's "never destroy" assert.

#include <HalPowerManager.h>

#include "../simulator_config.h"

void HalPowerManager::begin() {}
void HalPowerManager::setPowerSaving(bool) {}
void HalPowerManager::startDeepSleep(HalGPIO&) const {}
uint16_t HalPowerManager::getBatteryPercentage() const { return SIMULATOR_BATTERY_PERCENT; }

HalPowerManager::Lock::Lock() {}
HalPowerManager::Lock::~Lock() {}

HalPowerManager powerManager;
