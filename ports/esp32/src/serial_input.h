/**
 * serial_input.h
 *
 * Generic, protocol-agnostic line parser. It has no idea what "the
 * device" actually is, and no idea whether bytes come from a UART, a
 * TCP socket, or a unit test - it just assembles bytes into lines and
 * hands each complete line to bacnet_translator_update(). This is what
 * lets you test the whole pipeline today by typing numbers, and swap in
 * the real device later with a one-line config change
 * (config.h: CFG_SERIAL_INPUT_SOURCE) plus whatever byte source the
 * host project wires up.
 *
 * Wire protocol (deliberately dead simple, so it's easy to fake by hand
 * while testing, and easy to generate from whatever the real device
 * turns out to send):
 *
 *   PRESSURE1:54.3\n   -> updates the point named "PRESSURE1"
 *   ALARM1:1\n         -> updates the point named "ALARM1"
 *   54.3\n             -> no name given: updates the FIRST point in
 *                         config.h's CFG_POINTS table (fastest way to
 *                         test with a single value).
 *   # anything\n       -> ignored (comment/log line)
 *   (blank line)       -> ignored
 *
 * Plain C, no Arduino dependency: the host project is responsible for
 * actually reading bytes from hardware (a UART, a socket, etc.) and
 * calling serial_input_feed_byte() once per byte received. That is the
 * only integration point.
 */
#ifndef SERIAL_INPUT_H
#define SERIAL_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

/** Reset the internal line-assembly buffer. Call once at startup. */
void serial_input_init(void);

/**
 * Feed one raw byte received from whatever transport is in use.
 * Assembles complete lines and, for each one, calls
 * bacnet_translator_update() with the parsed point name and value.
 */
void serial_input_feed_byte(char c);

/**
 * Optional: register a callback invoked whenever a line names a point
 * that isn't in config.h's CFG_POINTS table. Useful for host-specific
 * logging (e.g. printing to Serial on Arduino) without hard-coding any
 * particular logging mechanism into this portable module. Pass NULL to
 * disable (the default).
 */
typedef void (*serial_input_unknown_point_cb)(const char *point_name);
void serial_input_set_unknown_point_callback(serial_input_unknown_point_cb cb);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_INPUT_H */
