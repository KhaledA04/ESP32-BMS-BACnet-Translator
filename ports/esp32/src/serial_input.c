/**
 * serial_input.c
 *
 * Plain C - no Arduino, no HardwareSerial. See serial_input.h for the
 * wire protocol and integration contract. This file only assembles
 * bytes into lines and dispatches to bacnet_translator_update(); it has
 * no opinion about where the bytes came from.
 */
#include <string.h>
#include <stdlib.h>

#include "serial_input.h"
#include "bacnet_translator.h"
#include "config.h"

static char line_buf[128];
static size_t line_len = 0;
static serial_input_unknown_point_cb unknown_point_cb = NULL;

static void handle_line(char *line)
{
    const char *name;
    float value;
    char *colon;

    /* trim leading spaces/tabs */
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line == '\0' || *line == '#') {
        return; /* blank line or comment/log line - ignore */
    }

    colon = strchr(line, ':');
    if (colon != NULL) {
        *colon = '\0';
        name = line;
        value = (float)atof(colon + 1);
    } else {
        /* bare value, no "NAME:" prefix -> route to the first configured
         * point so a single typed number is enough to test end to end */
        name = CFG_POINTS[0].name;
        value = (float)atof(line);
    }

    if (!bacnet_translator_update(name, value)) {
        if (unknown_point_cb != NULL) {
            unknown_point_cb(name);
        }
    }
}

void serial_input_init(void)
{
    line_len = 0;
}

void serial_input_set_unknown_point_callback(serial_input_unknown_point_cb cb)
{
    unknown_point_cb = cb;
}

void serial_input_feed_byte(char c)
{
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        line_buf[line_len] = '\0';
        handle_line(line_buf);
        line_len = 0;
        return;
    }
    if (line_len < sizeof(line_buf) - 1) {
        line_buf[line_len++] = c;
    }
    /* else: line too long, silently drop extra characters */
}
