/**
 * config.h
 *
 * THIS is the file engineers touch. Nothing else in this project needs to
 * change to add a data point, move to a real Wi-Fi network, or point the
 * "device" input at a different UART.
 *
 * Plain C (no Arduino/C++ features) so it can be #included from both the
 * portable .c modules (bacnet_translator.c, serial_input.c) and the
 * Arduino-specific .cpp glue (main.cpp) without any restrictions.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* ===================================================================
 * Wi-Fi
 * =================================================================== */
#define CFG_WIFI_SSID          "SSYMK"
#define CFG_WIFI_PASSWORD      "H0me_p@ssw0rd"

/* 0 = DHCP (recommended until the team confirms otherwise, see the
 * report's Part 10, question 8). 1 = use the static settings below. */
#define CFG_WIFI_USE_STATIC_IP   0
#define CFG_STATIC_IP           "192.168.1.50"
#define CFG_STATIC_GATEWAY      "192.168.1.1"
#define CFG_STATIC_SUBNET       "255.255.255.0"

/* ===================================================================
 * BACnet/IP (what the BMS / YABE will see)
 * =================================================================== */
/* Must be unique on the BACnet network - ask the team before deploying
 * for real (report Part 10, question 9). */
#define CFG_BACNET_DEVICE_INSTANCE   399001

#define CFG_BACNET_DEVICE_NAME       "ESP32 BMS Translator"

/* Standard BACnet/IP port. Leave this unless the network team says
 * otherwise (report Part 10, question 11). */
#define CFG_BACNET_UDP_PORT          47808

/* ===================================================================
 * Serial input - this is the "device" side of the translator
 * =================================================================== */
/* Where should raw readings be read from?
 *
 *   SERIAL_SOURCE_USB   - the SAME USB cable already used to flash/monitor
 *                          the ESP32 (the Arduino/PlatformIO "Serial
 *                          Monitor"). Use this to TEST the whole pipeline
 *                          with zero extra wiring: type a line into the
 *                          monitor and watch it become a BACnet Present
 *                          Value.
 *
 *   SERIAL_SOURCE_UART2 - a second, dedicated hardware UART (pins below).
 *                          Wire the real device here once its format is
 *                          known - or wire a cheap USB-TTL adapter here
 *                          to test with real wires/baud rate before the
 *                          real device exists (see SETUP.md, Test B).
 *
 * Flip this ONE line to switch between "test with my keyboard" and
 * "read the real device" - nothing else in the code changes. */
#define SERIAL_SOURCE_USB     0
#define SERIAL_SOURCE_UART2   1
#define CFG_SERIAL_INPUT_SOURCE   SERIAL_SOURCE_USB

/* 9600 is the most common default for BMS/field-device UARTs (it's also
 * a very common Modbus/BACnet MS/TP default). Change this once the real
 * device's baud rate is confirmed (report Part 10, question 3). */
#define CFG_SERIAL_BAUD_RATE   9600

/* Only used when CFG_SERIAL_INPUT_SOURCE == SERIAL_SOURCE_UART2.
 * Defaults below match the ESP32's usual free hardware UART2 pins. */
#define CFG_UART2_RX_PIN   16
#define CFG_UART2_TX_PIN   17

/* ===================================================================
 * Data points - THE table engineers edit to add/remove a value
 * =================================================================== */
/* kind:
 *   OBJ_ANALOG_VALUE - a number (pressure, temperature, a raw counter...)
 *   OBJ_BINARY_VALUE - on/off, 0/1, normal/alarm...
 *
 * "name" is used two places: as the tag in the serial protocol
 * (see SETUP.md - "NAME:VALUE") and as the BACnet object name shown
 * in YABE / the BMS.
 *
 * "instance" only needs to be unique among points of the SAME kind on
 * THIS device - 1, 2, 3... is fine. */
enum bacnet_point_kind { OBJ_ANALOG_VALUE, OBJ_BINARY_VALUE };

typedef struct {
    const char *name;
    enum bacnet_point_kind kind;
    uint32_t instance;
    const char *description;
    float initial_value; /* for binary points: 0 = inactive, nonzero = active */
} bacnet_point_config_t;

static const bacnet_point_config_t CFG_POINTS[] = {
    /* name          kind              instance  description                        initial */
    { "PRESSURE1", OBJ_ANALOG_VALUE, 1, "Zone 1 gas pressure (raw units)", 0.0f },
    { "ALARM1",    OBJ_BINARY_VALUE, 1, "Zone 1 alarm state",              0.0f },

    /* Add more rows here. Example:
     * { "PRESSURE2", OBJ_ANALOG_VALUE, 2, "Zone 2 gas pressure (raw units)", 0.0f },
     */
};

#define CFG_POINT_COUNT ((unsigned)(sizeof(CFG_POINTS) / sizeof(CFG_POINTS[0])))

#endif /* CONFIG_H */
