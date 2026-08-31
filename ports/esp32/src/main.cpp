/**
 * main.cpp
 *
 * This is the ONLY file in the project that touches Arduino/WiFi APIs.
 * Everything that actually matters - the BACnet object logic and the
 * serial line parser - lives in the portable bacnet_translator.c and
 * serial_input.c modules, which have no Arduino dependency at all and
 * can be lifted into a bigger/different project as-is.
 *
 * This file's job is just:
 *   1. Bring up Wi-Fi (mirrors bacnet-stack/ports/esp32/src/main_bip.cpp's
 *      wifi_connect(), reading credentials from config.h instead of
 *      PlatformIO build flags).
 *   2. Call bacnet_translator_init() once Wi-Fi is up.
 *   3. Read bytes from whichever UART config.h selects and feed them
 *      one at a time into serial_input_feed_byte().
 *   4. Call bacnet_translator_task() every loop pass.
 */
#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "bacnet_translator.h"
#include "serial_input.h"

#if CFG_SERIAL_INPUT_SOURCE == SERIAL_SOURCE_UART2
static HardwareSerial &DeviceSerial = Serial2;
#else
static HardwareSerial &DeviceSerial = Serial;
#endif

static void on_unknown_point(const char *point_name)
{
    Serial.print("# [SERIAL] unknown point name: ");
    Serial.println(point_name);
}

static void wifi_connect(void)
{
    Serial.print("# [WIFI] connecting to ");
    Serial.println(CFG_WIFI_SSID);

#if CFG_WIFI_USE_STATIC_IP
    IPAddress ip, gateway, subnet;
    ip.fromString(CFG_STATIC_IP);
    gateway.fromString(CFG_STATIC_GATEWAY);
    subnet.fromString(CFG_STATIC_SUBNET);
    WiFi.config(ip, gateway, subnet);
#endif

    WiFi.mode(WIFI_STA);
    WiFi.begin(CFG_WIFI_SSID, CFG_WIFI_PASSWORD);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000UL) {
        delay(250);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("# [WIFI] connected, IP=");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(
            "# [WIFI] FAILED to connect - check CFG_WIFI_SSID / "
            "CFG_WIFI_PASSWORD in config.h. BACnet will not be reachable "
            "until Wi-Fi is up.");
    }
}

void setup(void)
{
    // Note: while CFG_SERIAL_INPUT_SOURCE == SERIAL_SOURCE_USB, this same
    // port also carries the test data you type in - see serial_input.h.
    Serial.begin(CFG_SERIAL_BAUD_RATE);
    delay(200);
    Serial.println("# ESP32 BACnet BMS Translator - booting");

    wifi_connect();

    if (WiFi.status() == WL_CONNECTED) {
        if (bacnet_translator_init()) {
            Serial.print("# [BACNET] ready. Device Instance=");
            Serial.print(CFG_BACNET_DEVICE_INSTANCE);
            Serial.print(", UDP port=");
            Serial.print(CFG_BACNET_UDP_PORT);
            Serial.print(", points=");
            Serial.println(CFG_POINT_COUNT);
        } else {
            Serial.println(
                "# [BACNET] init FAILED (bip_init could not open the "
                "UDP socket)");
        }
    }

#if CFG_SERIAL_INPUT_SOURCE == SERIAL_SOURCE_UART2
    Serial2.begin(
        CFG_SERIAL_BAUD_RATE, SERIAL_8N1, CFG_UART2_RX_PIN, CFG_UART2_TX_PIN);
    Serial.println("# [SERIAL] reading device data from UART2");
#else
    Serial.println(
        "# [SERIAL] reading device data from USB Serial - type a line "
        "like PRESSURE1:54.3 and press Enter");
#endif

    serial_input_init();
    serial_input_set_unknown_point_callback(on_unknown_point);

    Serial.println("# [READY] waiting for readings...");
}

void loop(void)
{
    while (DeviceSerial.available()) {
        serial_input_feed_byte((char)DeviceSerial.read());
    }
    bacnet_translator_task();
}
