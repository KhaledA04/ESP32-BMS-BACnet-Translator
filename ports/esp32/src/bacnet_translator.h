/**
 * bacnet_translator.h
 *
 * The generic "does the BACnet thingy" module. It does not know or care
 * what device the data came from - it only knows the point table in
 * config.h. Built directly on top of the official, community-maintained
 * BACnet Stack ESP32/BACnet-IP port (bacnet-stack/ports/esp32), so it does
 * not reimplement any part of the BACnet protocol itself.
 *
 * Plain C, no Arduino dependency: this header/module pair is meant to be
 * dropped into a bigger project as-is. The only platform-specific glue
 * needed around it is (a) bringing up Wi-Fi/networking before calling
 * bacnet_translator_init(), and (b) calling bacnet_translator_task()
 * regularly from whatever the host project's main loop looks like.
 */
#ifndef BACNET_TRANSLATOR_H
#define BACNET_TRANSLATOR_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Bring up the BACnet/IP stack: create one BACnet object per row in
 * config.h's CFG_POINTS table, start listening on CFG_BACNET_UDP_PORT,
 * and broadcast I-Am so BMS/YABE clients can discover this device.
 *
 * Call once, AFTER the network layer (Wi-Fi/Ethernet) is already
 * connected and able to bind a UDP socket.
 *
 * @return true on success, false if the BACnet/IP UDP socket could not
 *         be opened (e.g. network not actually up yet).
 */
bool bacnet_translator_init(void);

/**
 * Pump the BACnet/IP stack: process any inbound packets (Who-Is,
 * ReadProperty, ...) and advance protocol timers.
 *
 * Call on every pass through the main loop - do not block/delay for
 * long between calls, or discovery/reads will feel sluggish to clients.
 */
void bacnet_translator_task(void);

/**
 * THE "update" function: push a new reading into the BACnet object whose
 * name (from config.h's CFG_POINTS table) matches point_name.
 *   - For an OBJ_ANALOG_VALUE point, `value` is written as-is.
 *   - For an OBJ_BINARY_VALUE point, `value != 0` means active/on.
 *
 * @param point_name  must exactly match a "name" entry in CFG_POINTS.
 * @param value       the new reading.
 * @return true if point_name was found and the object was updated,
 *         false if point_name does not match any configured point.
 */
bool bacnet_translator_update(const char *point_name, float value);

#ifdef __cplusplus
}
#endif

#endif /* BACNET_TRANSLATOR_H */
