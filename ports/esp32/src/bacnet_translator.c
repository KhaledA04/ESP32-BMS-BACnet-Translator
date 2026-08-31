/**
 * bacnet_translator.c
 *
 * Plain C - no Arduino, no WiFi.h, no Serial. This file wires config.h's
 * CFG_POINTS table into real BACnet objects using the BACnet Stack
 * project's public object APIs
 * (bacnet-stack/src/bacnet/basic/object/{av,bv,device}.*) and the
 * official ESP32 BACnet/IP transport
 * (bacnet-stack/ports/esp32/src/{bip.c,bip_init.c,bip_socket.cpp,bvlc.c}).
 *
 * The object table pattern, init sequence, and task loop below mirror
 * bacnet-stack/ports/esp32/src/bacnet_app.c line for line in structure -
 * that file is itself plain C, and is the one the bacnet-stack
 * maintainers have already tested end-to-end with YABE - only the object
 * TYPES (Analog/Binary VALUE instead of INPUT/OUTPUT) and the fact that
 * the point list comes from config.h instead of being hard-coded are
 * different here.
 *
 * Because this file has zero Arduino/C++ dependency, it compiles as a
 * normal C translation unit and can be linked into any host project that
 * already has bacnet-stack's core sources and an ESP32 (or other) BACnet
 * /IP datalink available - Arduino is not required by this file itself.
 */
#include <string.h>

#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/av.h"
#include "bacnet/basic/object/bv.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/npdu.h"
#include "bip.h"

#include "bacnet_translator.h"
#include "config.h"

static uint8_t PDUBuffer[BIP_MPDU_MAX];

/* This table tells the BACnet Stack's Device object which object types
 * this server supports and which functions implement each one. It is
 * the same object_functions_t pattern used by every bacnet-stack
 * application (see ports/esp32/src/bacnet_app.c for the reference
 * version of this table using Analog/Binary INPUT/OUTPUT objects
 * instead). */
static object_functions_t My_Object_Table[] = {
    { OBJECT_DEVICE, NULL, Device_Count, Device_Index_To_Instance,
      Device_Valid_Object_Instance_Number, Device_Object_Name,
      Device_Read_Property_Local, Device_Write_Property_Local,
      Device_Property_Lists, DeviceGetRRInfo, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, Device_Writable_Property_List },

    { OBJECT_ANALOG_VALUE, Analog_Value_Init, Analog_Value_Count,
      Analog_Value_Index_To_Instance, Analog_Value_Valid_Instance,
      Analog_Value_Object_Name, Analog_Value_Read_Property,
      Analog_Value_Write_Property, Analog_Value_Property_Lists, NULL, NULL,
      Analog_Value_Encode_Value_List, Analog_Value_Change_Of_Value,
      Analog_Value_Change_Of_Value_Clear, Analog_Value_Intrinsic_Reporting,
      NULL, NULL, Analog_Value_Create, Analog_Value_Delete, NULL,
      Analog_Value_Writable_Property_List },

    { OBJECT_BINARY_VALUE, Binary_Value_Init, Binary_Value_Count,
      Binary_Value_Index_To_Instance, Binary_Value_Valid_Instance,
      Binary_Value_Object_Name, Binary_Value_Read_Property,
      Binary_Value_Write_Property, Binary_Value_Property_Lists, NULL, NULL,
      Binary_Value_Encode_Value_List, Binary_Value_Change_Of_Value,
      Binary_Value_Change_Of_Value_Clear, Binary_Value_Intrinsic_Reporting,
      NULL, NULL, Binary_Value_Create, Binary_Value_Delete, NULL,
      Binary_Value_Writable_Property_List },

    { MAX_BACNET_OBJECT_TYPE, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
      NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/**
 * Create one BACnet object per row of config.h's CFG_POINTS table.
 * This is the generalized replacement for hand-writing one object per
 * sensor: add a row to CFG_POINTS and it shows up here automatically.
 */
static void init_server_objects(void)
{
    unsigned i;

    Device_Init(My_Object_Table);
    (void)Device_Set_Object_Instance_Number(CFG_BACNET_DEVICE_INSTANCE);
    (void)Device_Object_Name_ANSI_Init(CFG_BACNET_DEVICE_NAME);

    for (i = 0; i < CFG_POINT_COUNT; i++) {
        const bacnet_point_config_t *p = &CFG_POINTS[i];
        uint32_t inst;

        if (p->kind == OBJ_ANALOG_VALUE) {
            inst = Analog_Value_Create(p->instance);
            (void)Analog_Value_Name_Set(inst, p->name);
            (void)Analog_Value_Description_Set(inst, p->description);
            (void)Analog_Value_Present_Value_Set(
                inst, p->initial_value, BACNET_MAX_PRIORITY);
        } else {
            inst = Binary_Value_Create(p->instance);
            (void)Binary_Value_Name_Set(inst, p->name);
            (void)Binary_Value_Description_Set(inst, p->description);
            (void)Binary_Value_Present_Value_Set(
                inst, (p->initial_value != 0.0f) ? BINARY_ACTIVE
                                                  : BINARY_INACTIVE);
        }
    }
}

bool bacnet_translator_init(void)
{
    address_init();
    init_server_objects();

    if (!bip_init(CFG_BACNET_UDP_PORT)) {
        return false;
    }

    apdu_set_unrecognized_service_handler_handler(
        handler_unrecognized_service);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);

    Send_I_Am(&Handler_Transmit_Buffer[0]);

    return true;
}

void bacnet_translator_task(void)
{
    BACNET_ADDRESS src = { 0 };
    uint16_t pdu_len = bip_receive(&src, PDUBuffer, sizeof(PDUBuffer), 0);

    if (pdu_len) {
        npdu_handler(&src, PDUBuffer, pdu_len);
    }

    tsm_timer_milliseconds(1);
}

bool bacnet_translator_update(const char *point_name, float value)
{
    unsigned i;

    for (i = 0; i < CFG_POINT_COUNT; i++) {
        if (strcmp(CFG_POINTS[i].name, point_name) == 0) {
            uint32_t inst = CFG_POINTS[i].instance;

            if (CFG_POINTS[i].kind == OBJ_ANALOG_VALUE) {
                return Analog_Value_Present_Value_Set(
                    inst, value, BACNET_MAX_PRIORITY);
            } else {
                return Binary_Value_Present_Value_Set(
                    inst, (value != 0.0f) ? BINARY_ACTIVE : BINARY_INACTIVE);
            }
        }
    }
    return false;
}
