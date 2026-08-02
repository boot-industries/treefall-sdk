#include <orpheus/audio_driver_manager.h>

int main() {
  auto manager = orpheus::createAudioDriverManager();
  if (!manager) {
    return 1;
  }
  const auto devices = manager->enumerateDevices();
  if (devices.size() != 1 || devices.front().deviceId != "dummy" ||
      devices.front().driverType != "Dummy") {
    return 2;
  }
  if (manager->getDeviceInfo("coreaudio-disabled").has_value() ||
      manager->getDeviceInfo("wasapi:disabled").has_value()) {
    return 3;
  }
  return 0;
}
