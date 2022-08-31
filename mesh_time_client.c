/*
* Copyright 2016-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*/

/** @file
 *
 *
 * This file shows how to create a device which implements mesh user time client.
 */
#include "wiced_bt_ble.h"
#include "wiced_bt_gatt.h"
#include "wiced_bt_mesh_models.h"
#include "wiced_bt_trace.h"
#include "rtc.h"
#include "wiced_bt_mesh_app.h"

#ifdef HCI_CONTROL
#include "wiced_transport.h"
#include "hci_control_api.h"
#endif

#include "wiced_bt_cfg.h"
extern wiced_bt_cfg_settings_t wiced_bt_cfg_settings;

/******************************************************
 *          Constants
 ******************************************************/
#define MESH_PID                0x3023
#define MESH_VID                0x0002

/******************************************************
 *          Structures
 ******************************************************/

/******************************************************
 *          Function Prototypes
 ******************************************************/
static void mesh_app_init(wiced_bool_t is_provisioned);
static uint32_t mesh_app_proc_rx_cmd(uint16_t opcode, uint8_t *p_data, uint32_t length);
static void mesh_time_client_message_handler(uint16_t event, wiced_bt_mesh_event_t *p_event, void *p_data);
static void mesh_time_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_zone_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_zone_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_tai_utc_delta_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t data_len);
static void mesh_time_tai_utc_delta_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_role_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_role_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length);
static void mesh_time_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_state_msg_t *p_time_status);
static void mesh_time_zone_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_zone_status_t *p_time_status);
static void mesh_time_tai_utc_delta_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_tai_utc_delta_status_t *p_time_delta_status);
static void mesh_time_role_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_role_msg_t *p_role_status);


/******************************************************
 *          Variables Definitions
 ******************************************************/
uint8_t mesh_mfr_name[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MANUFACTURER_NAME] = { 'C', 'y', 'p', 'r', 'e', 's', 's', 0 };
uint8_t mesh_model_num[WICED_BT_MESH_PROPERTY_LEN_DEVICE_MODEL_NUMBER]     = { '1', '2', '3', '4', 0, 0, 0, 0 };
uint8_t mesh_system_id[8]                                                  = { 0xbb, 0xb8, 0xa1, 0x80, 0x5f, 0x9f, 0x91, 0x71 };

wiced_bt_mesh_core_config_model_t   mesh_element1_models[] =
{
    WICED_BT_MESH_DEVICE,
    WICED_BT_MESH_MODEL_TIME_CLIENT,
};
#define MESH_APP_NUM_MODELS  (sizeof(mesh_element1_models) / sizeof(wiced_bt_mesh_core_config_model_t))

#define MESH_TIME_CLIENT_ELEMENT_INDEX   0

wiced_bt_mesh_core_config_element_t mesh_elements[] =
{
    {
        .location = MESH_ELEM_LOC_MAIN,                                 // location description as defined in the GATT Bluetooth Namespace Descriptors section of the Bluetooth SIG Assigned Numbers
        .default_transition_time = MESH_DEFAULT_TRANSITION_TIME_IN_MS,  // Default transition time for models of the element in milliseconds
        .onpowerup_state = WICED_BT_MESH_ON_POWER_UP_STATE_RESTORE,     // Default element behavior on power up
        .properties_num = 4,                                            // Number of properties in the array models
        .properties = NULL,                                             // Array of properties in the element.
        .models_num = MESH_APP_NUM_MODELS,                              // Number of models in the array models
        .models = mesh_element1_models,                                 // Array of models located in that element. Model data is defined by structure wiced_bt_mesh_core_config_model_t
    },
};

uint8_t mesh_num_elements = 1;

wiced_bt_mesh_core_config_t  mesh_config =
{
    .company_id         = MESH_COMPANY_ID_CYPRESS,                  // Company identifier assigned by the Bluetooth SIG
    .product_id         = MESH_PID,                                 // Vendor-assigned product identifier
    .vendor_id          = MESH_VID,                                 // Vendor-assigned product version identifier
#if LOW_POWER_NODE
    .features           = WICED_BT_MESH_CORE_FEATURE_BIT_LOW_POWER, // A bit field indicating the device features. In Low Power mode no Relay, no Proxy and no Friend
    .friend_cfg         =                                           // Empty Configuration of the Friend Feature
    {
        .receive_window = 0,                                        // Receive Window value in milliseconds supported by the Friend node.
        .cache_buf_len  = 0,                                        // Length of the buffer for the cache
        .max_lpn_num    = 0                                         // Max number of Low Power Nodes with established friendship. Must be > 0 if Friend feature is supported.
    },
    .low_power          =                                           // Configuration of the Low Power Feature
    {
        .rssi_factor           = 2,                                 // contribution of the RSSI measured by the Friend node used in Friend Offer Delay calculations.
        .receive_window_factor = 2,                                 // contribution of the supported Receive Window used in Friend Offer Delay calculations.
        .min_cache_size_log    = 3,                                 // minimum number of messages that the Friend node can store in its Friend Cache.
        .receive_delay         = 100,                               // Receive delay in 1 ms units to be requested by the Low Power node.
        .poll_timeout          = 36000                              // Poll timeout in 100ms units to be requested by the Low Power node.
    },
#else
    .features           = WICED_BT_MESH_CORE_FEATURE_BIT_FRIEND | WICED_BT_MESH_CORE_FEATURE_BIT_RELAY,   // In Friend mode support friend, relay
    .friend_cfg         =                                           // Configuration of the Friend Feature(Receive Window in Ms, messages cache)
    {
        .receive_window        = 20,
        .cache_buf_len         = 300,                               // Length of the buffer for the cache
        .max_lpn_num           = 4                                  // Max number of Low Power Nodes with established friendship. Must be > 0 if Friend feature is supported.
    },
    .low_power          =                                           // Configuration of the Low Power Feature
    {
        .rssi_factor           = 0,                                 // contribution of the RSSI measured by the Friend node used in Friend Offer Delay calculations.
        .receive_window_factor = 0,                                 // contribution of the supported Receive Window used in Friend Offer Delay calculations.
        .min_cache_size_log    = 0,                                 // minimum number of messages that the Friend node can store in its Friend Cache.
        .receive_delay         = 0,                                 // Receive delay in 1 ms units to be requested by the Low Power node.
        .poll_timeout          = 0                                  // Poll timeout in 100ms units to be requested by the Low Power node.
    },
#endif
    .gatt_client_only          = WICED_FALSE,                       // Can connect to mesh over GATT or ADV
    .elements_num  = (uint8_t)(sizeof(mesh_elements) / sizeof(mesh_elements[0])),   // number of elements on this device
    .elements      = mesh_elements                                  // Array of elements for this device
};

/*
 * Mesh application library will call into application functions if provided by the application.
 */
wiced_bt_mesh_app_func_table_t wiced_bt_mesh_app_func_table =
{
    mesh_app_init,          // application initialization
    NULL,                   // Default SDK platform button processing
    NULL,                   // GATT connection status
    NULL,                   // attention processing
    NULL,                   // notify period set
    mesh_app_proc_rx_cmd,   // WICED HCI command
    NULL,                   // LPN sleep
    NULL                    // factory reset
};

/******************************************************
 *               Function Definitions
 ******************************************************/
void mesh_app_init(wiced_bool_t is_provisioned)
{
    wiced_bt_cfg_settings.device_name = (uint8_t *)"Time Client";
    wiced_bt_cfg_settings.gatt_cfg.appearance = APPEARANCE_GENERIC_TAG;
    // Adv Data is fixed. Spec allows to put URI, Name, Appearance and Tx Power in the Scan Response Data.
    if (!is_provisioned)
    {
        wiced_bt_ble_advert_elem_t  adv_elem[3];
        uint8_t                     buf[2];
        uint8_t                     num_elem = 0;
        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_NAME_COMPLETE;
        adv_elem[num_elem].len = (uint16_t)strlen((const char*)wiced_bt_cfg_settings.device_name);
        adv_elem[num_elem].p_data = wiced_bt_cfg_settings.device_name;
        num_elem++;

        adv_elem[num_elem].advert_type = BTM_BLE_ADVERT_TYPE_APPEARANCE;
        adv_elem[num_elem].len = 2;
        buf[0] = (uint8_t)wiced_bt_cfg_settings.gatt_cfg.appearance;
        buf[1] = (uint8_t)(wiced_bt_cfg_settings.gatt_cfg.appearance >> 8);
        adv_elem[num_elem].p_data = buf;
        num_elem++;

        wiced_bt_mesh_set_raw_scan_response_data(num_elem, adv_elem);
    }

    // register with the library to receive parsed data
    wiced_bt_mesh_model_time_client_init(mesh_time_client_message_handler, is_provisioned);
}

/*
 * Process event received from the time Server.
 */
void mesh_time_client_message_handler(uint16_t event, wiced_bt_mesh_event_t *p_event, void *p_data)
{
#if defined HCI_CONTROL
    wiced_bt_mesh_hci_event_t *p_hci_event;
#endif
    WICED_BT_TRACE("time clt msg:%d\n", event);

    switch (event)
    {
    case WICED_BT_MESH_TIME_STATUS:
#if defined HCI_CONTROL
        if ((p_hci_event = wiced_bt_mesh_create_hci_event(p_event)) != NULL)
            mesh_time_status_hci_event_send(p_hci_event, (wiced_bt_mesh_time_state_msg_t *)p_data);
#endif
        break;

    case WICED_BT_MESH_TIME_ZONE_STATUS:
#if defined HCI_CONTROL
        if ((p_hci_event = wiced_bt_mesh_create_hci_event(p_event)) != NULL)
            mesh_time_zone_status_hci_event_send(p_hci_event, (wiced_bt_mesh_time_zone_status_t *)p_data);
#endif
        break;

    case WICED_BT_MESH_TAI_UTC_DELTA_STATUS:
#if defined HCI_CONTROL
        if ((p_hci_event = wiced_bt_mesh_create_hci_event(p_event)) != NULL)
            mesh_time_tai_utc_delta_status_hci_event_send(p_hci_event, (wiced_bt_mesh_time_tai_utc_delta_status_t *)p_data);
#endif
        break;

    case WICED_BT_MESH_TIME_ROLE_STATUS:
#if defined HCI_CONTROL
        if ((p_hci_event = wiced_bt_mesh_create_hci_event(p_event)) != NULL)
            mesh_time_role_status_hci_event_send(p_hci_event, (wiced_bt_mesh_time_role_msg_t *)p_data);
#endif
        break;

    default:
        WICED_BT_TRACE("unknown\n");
        break;
    }
    wiced_bt_mesh_release_event(p_event);
}

/*
 * In 2 chip solutions MCU can send commands to change time state.
 */
uint32_t mesh_app_proc_rx_cmd(uint16_t opcode, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_event_t *p_event;

    WICED_BT_TRACE("[%s] cmd_opcode 0x%02x\n", __FUNCTION__, opcode);

    switch (opcode)
    {
    case HCI_CONTROL_MESH_COMMAND_TIME_GET:
    case HCI_CONTROL_MESH_COMMAND_TIME_SET:
    case HCI_CONTROL_MESH_COMMAND_TIME_ZONE_GET:
    case HCI_CONTROL_MESH_COMMAND_TIME_ZONE_SET:
    case HCI_CONTROL_MESH_COMMAND_TIME_TAI_UTC_DELTA_GET:
    case HCI_CONTROL_MESH_COMMAND_TIME_TAI_UTC_DELTA_SET:
    case HCI_CONTROL_MESH_COMMAND_TIME_ROLE_GET:
    case HCI_CONTROL_MESH_COMMAND_TIME_ROLE_SET:
        break;

    default:
        WICED_BT_TRACE("unknown\n");
        return WICED_FALSE;
    }
    p_event = wiced_bt_mesh_create_event_from_wiced_hci(opcode, MESH_COMPANY_ID_BT_SIG, WICED_BT_MESH_CORE_MODEL_ID_TIME_CLNT, &p_data, &length);
    if (p_event == NULL)
    {
        WICED_BT_TRACE("bad hdr\n");
        return WICED_TRUE;
    }
    switch (opcode)
    {
    case HCI_CONTROL_MESH_COMMAND_TIME_GET:
        mesh_time_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_SET:
        mesh_time_set(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_ZONE_GET:
        mesh_time_zone_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_ZONE_SET:
        mesh_time_zone_set(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_TAI_UTC_DELTA_GET:
        mesh_time_tai_utc_delta_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_TAI_UTC_DELTA_SET:
        mesh_time_tai_utc_delta_set(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_ROLE_GET:
        mesh_time_role_get(p_event, p_data, length);
        break;

    case HCI_CONTROL_MESH_COMMAND_TIME_ROLE_SET:
        mesh_time_role_set(p_event, p_data, length);
        break;
    }
    return WICED_TRUE;
}

/*
 * Send time get command
 */
void mesh_time_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    WICED_BT_TRACE("mesh_time_get\n");
    wiced_bt_mesh_model_time_client_time_get_send(p_event);
}

/*
 * Send time set command
 */
void mesh_time_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_time_state_msg_t set_data;

    WICED_BT_TRACE("mesh_time_set\n");

    STREAM_TO_UINT40(set_data.tai_seconds, p_data);
    STREAM_TO_UINT8(set_data.subsecond, p_data);
    STREAM_TO_UINT8(set_data.uncertainty, p_data);
    STREAM_TO_UINT8(set_data.time_authority, p_data);
    STREAM_TO_UINT16(set_data.tai_utc_delta_current, p_data);
    STREAM_TO_UINT8(set_data.time_zone_offset_current, p_data);

    wiced_bt_mesh_model_time_client_time_set_send(p_event, &set_data);
}

/*
 * Send time zone get command
 */
void mesh_time_zone_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    WICED_BT_TRACE("mesh_time_zone_get\n");

    wiced_bt_mesh_model_time_client_time_zone_get_send(p_event);
}

/*
 * Send time zone set command
 */
void mesh_time_zone_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_time_zone_set_t set_data;

    WICED_BT_TRACE("mesh_time_zone_set\n");

    STREAM_TO_UINT8(set_data.time_zone_offset_new, p_data);
    STREAM_TO_UINT40(set_data.tai_of_zone_change, p_data);

    wiced_bt_mesh_model_time_client_time_zone_set_send(p_event, &set_data);
}

/*
 * Send time tai_utc delta get command
 */
void mesh_time_tai_utc_delta_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t data_len)
{
    WICED_BT_TRACE("mesh_time_tai_utc_delta_get\n");

    wiced_bt_mesh_model_time_client_tai_utc_delta_get_send(p_event);
}

/*
 * Send time tai_utc delta set command
 */
void mesh_time_tai_utc_delta_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_time_tai_utc_delta_set_t set_data;

    WICED_BT_TRACE("mesh_time_tai_utc_delta_set\n");

    STREAM_TO_UINT16(set_data.tai_utc_delta_new, p_data);
    STREAM_TO_UINT40(set_data.tai_of_delta_change, p_data);

    wiced_bt_mesh_model_time_client_tai_utc_delta_set_send(p_event, &set_data);
}

/*
 * Send time role get command
 */
void mesh_time_role_get(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    WICED_BT_TRACE("mesh_time_role_get\n");

    wiced_bt_mesh_model_time_client_time_role_get_send(p_event);
}

/*
 * Send time role set command
 */
void mesh_time_role_set(wiced_bt_mesh_event_t *p_event, uint8_t *p_data, uint32_t length)
{
    wiced_bt_mesh_time_role_msg_t set_data;

    WICED_BT_TRACE("mesh_time_role_set\n");

    STREAM_TO_UINT8(set_data.role, p_data);

    wiced_bt_mesh_model_time_client_time_role_set_send(p_event, &set_data);
}

#ifdef HCI_CONTROL
/*
 * Send Time Status event over transport
 */
void mesh_time_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_state_msg_t *p_time_status)
{
    uint8_t *p = p_hci_event->data;

    WICED_BT_TRACE("%s\n", __FUNCTION__);

    UINT40_TO_STREAM(p, p_time_status->tai_seconds);
    UINT8_TO_STREAM(p, p_time_status->subsecond);
    UINT8_TO_STREAM(p, p_time_status->uncertainty);
    UINT8_TO_STREAM(p, p_time_status->time_authority);
    UINT16_TO_STREAM(p, p_time_status->tai_utc_delta_current);
    UINT8_TO_STREAM(p, p_time_status->time_zone_offset_current);

    WICED_BT_TRACE("TAI_seconds: %x\n", p_time_status->tai_seconds);
    WICED_BT_TRACE("subsecond: %x\n", p_time_status->subsecond);
    WICED_BT_TRACE("uncertainty: %x\n", p_time_status->uncertainty);
    WICED_BT_TRACE("auth: %x\n", p_time_status->time_authority);
    WICED_BT_TRACE("tai_utc_delta_current: %x\n", p_time_status->tai_utc_delta_current);
    WICED_BT_TRACE("time_zone_offset_current: %x\n",p_time_status->time_zone_offset_current);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_TIME_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}

/*
 * Send Time Zone Status event over transport
 */
void mesh_time_zone_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_zone_status_t *p_time_status)
{
    uint8_t *p = p_hci_event->data;

    WICED_BT_TRACE("%s\n", __FUNCTION__);

    UINT8_TO_STREAM(p, p_time_status->time_zone_offset_current);
    UINT8_TO_STREAM(p, p_time_status->time_zone_offset_new);
    UINT40_TO_STREAM(p, p_time_status->tai_of_zone_change);

    WICED_BT_TRACE(" TAI_of_zone_change:\n");
    WICED_BT_TRACE("TAI_of_zone_change: %x\n", p_time_status->tai_of_zone_change);
    WICED_BT_TRACE("time_zone_offset_current: %x\n", p_time_status->time_zone_offset_current);
    WICED_BT_TRACE("time_zone_offset_new: %x\n",p_time_status->time_zone_offset_new);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_TIME_ZONE_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}


/*
 * Send Time TAI UTC delta status event over transport
 */
void mesh_time_tai_utc_delta_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_tai_utc_delta_status_t *p_time_delta_status)
{
    uint8_t *p = p_hci_event->data;

    WICED_BT_TRACE("%s\n", __FUNCTION__);

    UINT16_TO_STREAM(p, p_time_delta_status->tai_utc_delta_current);
    UINT16_TO_STREAM(p, p_time_delta_status->tai_utc_delta_new);
    UINT40_TO_STREAM(p, p_time_delta_status->tai_of_delta_change);

    WICED_BT_TRACE(" tai_of_delta_change:\n");
    WICED_BT_TRACE("tai_of_delta_change: %x\n", p_time_delta_status->tai_of_delta_change);
    WICED_BT_TRACE("tai_utc_delta_current: %x\n", p_time_delta_status->tai_utc_delta_current);
    WICED_BT_TRACE("tai_utc_delta_new: %x\n",p_time_delta_status->tai_utc_delta_new);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_TIME_TAI_UTC_DELTA_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}

/*
 * Send Time role status event over transport
 */
void mesh_time_role_status_hci_event_send(wiced_bt_mesh_hci_event_t *p_hci_event, wiced_bt_mesh_time_role_msg_t *p_role_status)
{
    uint8_t *p = p_hci_event->data;

    WICED_BT_TRACE("%s role:%x\n", __FUNCTION__, p_role_status->role);

    UINT8_TO_STREAM(p, p_role_status->role);

    mesh_transport_send_data(HCI_CONTROL_MESH_EVENT_TIME_ROLE_STATUS, (uint8_t *)p_hci_event, (uint16_t)(p - (uint8_t *)p_hci_event));
}
#endif
