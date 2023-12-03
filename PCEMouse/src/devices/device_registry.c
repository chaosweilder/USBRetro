// device_registry.c
#include "device_registry.h"
#include "sony_ds4.h"
// Include other devices here

DeviceInterface* device_interfaces[MAX_DEVICE_TYPES] = {0};

void register_devices() {
    device_interfaces[0] = &sony_ds4_interface;
    // Register other devices here
    // device_interfaces[1] = &another_device_interface;
}
