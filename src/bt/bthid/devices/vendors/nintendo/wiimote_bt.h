// wiimote_bt.h - Nintendo Wiimote Bluetooth Driver

#ifndef WIIMOTE_BT_H
#define WIIMOTE_BT_H

#include "bt/bthid/bthid.h"

extern const bthid_driver_t wiimote_bt_driver;

void wiimote_bt_register(void);

#endif // WIIMOTE_BT_H
