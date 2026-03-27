#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "BLE_MONITOR";

// Define a custom service and characteristic UUIDs in little endian format
static const ble_uuid128_t service_uuid =
    BLE_UUID128_INIT(0xC5, 0xB4, 0xA3, 0xF2, 0xE1, 0xD0, 0xC9, 0xB8,
                     0xA7, 0xF6, 0xE5, 0xD4, 0xC3, 0xB2, 0xE1, 0xA0);

static const ble_uuid128_t gesture_char_uuid =
    BLE_UUID128_INIT(0xC6, 0xB5, 0xA4, 0xF3, 0xE2, 0xD1, 0xC0, 0xB9,
                     0xA8, 0xF7, 0xE6, 0xD5, 0xC4, 0xB3, 0xE2, 0xA1);
// define the characteristic handle and connection handle
static uint16_t gesture_val_handle;
static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
// define flags to track connection and notification status
static bool device_connected = false;
static bool notify_enabled = false;
// buffer to hold the current gesture text
static char gesture_text[64] = "READY";
static char last_sent_text[64] = "";

// forward declarations of functions
static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

// GATT access callback function for the gesture characteristic
static int gesture_chr_access_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    // prevent unused parameter warnings, not strictly necessary
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;


    switch (ctxt->op) {
        // case for read operation on the characteristic, return the current gesture text
        case BLE_GATT_ACCESS_OP_READ_CHR:
            // ctxt -> om is the outgoing message buffer, that gets appended with gesture_text, strlen() is just length of text
            if (os_mbuf_append(ctxt->om, gesture_text, strlen(gesture_text)) != 0) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            // return success
            return 0;

        default:
            // return generic ble error
            return BLE_ATT_ERR_UNLIKELY;
    }
}
// define the GATT service and characteristic
static const struct ble_gatt_svc_def gatt_svcs[] = {
    {   
        // defines a primary service with the specified UUID and a single characteristic for gesture text
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {   
                // defines a characteristic with the specified UUID, read and notify properties, and the access callback function
                .uuid = &gesture_char_uuid.u,
                .access_cb = gesture_chr_access_cb,
                .val_handle = &gesture_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
            },
            {0}
        },
    },
    {0}
};
// function to start advertising the BLE service with the defined UUID and characteristics
static void start_advertising(void)
{
    struct ble_hs_adv_fields adv_fields; // advertisement packet 
    struct ble_hs_adv_fields rsp_fields; // scan response packet
    struct ble_gap_adv_params adv_params; // advertising parameters
    uint8_t own_addr_type; 
    int rc; // return code for error handling

    rc = ble_hs_id_infer_auto(0, &own_addr_type); // infer the device's own address type, 0 means no privacy, will use public address
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }
    
    // set up the advertisement fields, including flags, service UUID, and device name
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.uuids128 = (ble_uuid128_t *)&service_uuid;
    adv_fields.num_uuids128 = 1;
    adv_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    // set up the scan response fields, including the device name
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    const char *name = ble_svc_gap_device_name();
    rsp_fields.name = (const uint8_t *)name;
    rsp_fields.name_len = strlen(name);
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    // set up the advertising parameters, including connection mode, discoverability, and intervals
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Advertising started");
    }
}
// callback function to handle GAP events such as connection, disconnection, subscription changes, and MTU updates
static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        // handle connection event, update connection status and log the event
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                current_conn_handle = event->connect.conn_handle;
                device_connected = true;
                notify_enabled = false;
                ESP_LOGI(TAG, "Connected, conn_handle=%d", current_conn_handle);
            } else {
                ESP_LOGW(TAG, "Connect failed, status=%d", event->connect.status);
                start_advertising();
            }
            return 0;
        
        // handle disconnection event, reset connection status and restart advertising
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
            current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
            device_connected = false;
            notify_enabled = false;
            start_advertising();
            return 0;
        
        // handle subscription changes for notifications, update the notify_enabled flag and log the new subscription status
        case BLE_GAP_EVENT_SUBSCRIBE:
            if (event->subscribe.attr_handle == gesture_val_handle) {
                notify_enabled = event->subscribe.cur_notify;
                ESP_LOGI(TAG,
                         "Subscribe changed: notify=%d indicate=%d",
                         event->subscribe.cur_notify,
                         event->subscribe.cur_indicate);
            }
            return 0;

        // handle MTU (Maximum Transmission Unit) update event, log the new MTU value for the connection 
        // MTU determines the maximum size of data that can be sent in a single BLE packet
        case BLE_GAP_EVENT_MTU:
            ESP_LOGI(TAG,
                     "MTU updated: conn_handle=%d mtu=%d",
                     event->mtu.conn_handle,
                     event->mtu.value);
            return 0;

        default:
            return 0;
    }
}

// callback function to handle stack synchronization, ensures the device has a valid address and starts advertising
static void on_stack_sync(void) {
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
        return;
    }

    start_advertising();
}

// FreeRTOS task to run the NimBLE host stack, this is required for handling BLE events and operations
static void host_task(void *param) {
    (void)param;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

// helper function to send a notification with the current gesture text to the connected client
// checks connection and notification status before sending
static void send_gesture_notification(const char *msg) {
    if (!device_connected || !notify_enabled) {
        return;
    }

    if (msg == NULL || msg[0] == '\0') {
        return;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(msg, strlen(msg));
    if (om == NULL) {
        ESP_LOGE(TAG, "Failed to allocate notification buffer");
        return;
    }

    int rc = ble_gatts_notify_custom(current_conn_handle, gesture_val_handle, om);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_notify_custom failed: %d", rc);
    } else {
        ESP_LOGI(TAG, "Notification sent: %s", msg);
    }
}

// main initialization function to set up BLE, including NVS flash, Bluetooth controller, NimBLE stack, GAP and GATT services
void ble_init(void) {
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    nimble_port_init();

    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_svc_gap_device_name_set("Gesture Recognition");
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
        return;
    }

    rc = ble_gatts_count_cfg(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
        return;
    }

    rc = ble_gatts_add_svcs(gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
        return;
    }

    ble_hs_cfg.sync_cb = on_stack_sync;

    nimble_port_freertos_init(host_task);
}
// main function
void app_main(void) {
    ble_init();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        // update the gesture_text variable with the latest recognized gesture from the gesture recognition logic here

        if (device_connected &&
            notify_enabled &&
            strncmp(gesture_text, last_sent_text, sizeof(last_sent_text)) != 0) {

            send_gesture_notification(gesture_text);

            strncpy(last_sent_text, gesture_text, sizeof(last_sent_text) - 1);
            last_sent_text[sizeof(last_sent_text) - 1] = '\0';
        }
    }
}