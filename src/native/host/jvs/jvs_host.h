// jvs_host.h - Native JVS Controller Host Driver
//
// Polls native JVS controllers and submits input events to the router.
// Supports JVS 2L12B
#ifndef JVS_HOST_H
#define JVS_HOST_H

#include <stdint.h>
#include <stdbool.h>

#include "core/input_interface.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

#define JVS_UART              uart1
#ifndef PIN_JVS_TX
#define PIN_JVS_TX            8
#endif
#ifndef PIN_JVS_RX
#define PIN_JVS_RX            9
#endif
#ifndef PIN_JVS_RE
#define PIN_JVS_RE            6
#endif
#ifndef PIN_JVS_DE
#define PIN_JVS_DE            7
#endif
#ifndef PIN_JVS_SENSE_IN_HIGH
#define PIN_JVS_SENSE_IN_HIGH 12
#endif
#ifndef PIN_JVS_SENSE_IN_LOW
#define PIN_JVS_SENSE_IN_LOW  13
#endif

#define JVS_MAX_PLAYERS       2

// Device types
#define JVSPAD_NONE           -1
#define JVSPAD_ARCADE_STICK   0

// DEBUG
#ifndef JVS_DEBUG
#define JVS_DEBUG 0
#endif

// ============================================================================
// PUBLIC API
// ============================================================================

// Initialize JVS host driver
// Sets up GPIO pins
void jvs_host_init(void);

// Poll JVS controllers and submit events to router
// Call this regularly from main loop (typically from app's task function)
void jvs_host_task(void);

// Get detected device type for a port
// Returns: -1=none, 0=ARCADE controller
int8_t jvs_host_get_device_type(uint8_t port);

// Check if any JVS I/O is connected
bool jvs_host_is_connected(void);

// ============================================================================
// INPUT INTERFACE
// ============================================================================

// JVS input interface (implements InputInterface pattern for app declaration)
extern const InputInterface jvs_input_interface;

#endif // JVS_HOST_H
