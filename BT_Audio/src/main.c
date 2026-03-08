#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_common_api.h"
#include "esp_err.h"

// Custom UUIDs for BLE service and characteristics
// Randomly generated for this project
// UUID for service 
static uint8_t SERVICE_UUID[16] = { 
    0xA0,0xE1,0xB2,0xC3,0xD4,0xE5,0xF6,0xA7,
    0xB8,0xC9,0xD0,0xE1,0xF2,0xA3,0xB4,0xC5
};
// UUID for gesture characteristic
static uint8_t GESTURE_CHAR_UUID[16] = {
    0xA1,0xE2,0xB3,0xC4,0xD5,0xE6,0xF7,0xA8,
    0xB9,0xC0,0xD1,0xE2,0xF3,0xA4,0xB5,0xC6
};

// Global variables to keep track of GATT service and characteristics
static uint16_t service_handle = 0; // Handle assigned by ESP-IDF for GATT service
static uint16_t gesture_char_handle = 0; // Handle for gesture CHARACTERISTIC
static esp_gatt_if_t gatts_if_global; // GLOBAL VARIABLE to store GATT interface for sending notifications
static uint16_t conn_id_global; // GLOBAL VARIABLE to store connection ID for sending notifications
static bool device_connected = false; // FLAG to track if a device is currently connected
static uint16_t battery_service_handle = 0; // Handle for battery service
static uint16_t battery_char_handle = 0; // Handle for battery level CHARACTERISTIC
static const char *TAG = "BLE_MONITOR"; // Set tag for logging
static uint16_t gesture_cccd_handle = 0; // Handle for Client Characteristic Configuration Descriptor (CCCD)
// Used to enable and disable notifications for the gesture characteristic
static bool gesture_notify_enabled = false; // Flag to track if notifications are enabled for the gesture characteristic

// Define buffer for gesture text data 
static char gesture_text[64] = "READY";
static char last_sent[64] = "";
// Define array of words we want to set, TESTING PURPOSES ONLY!
const char* test_gestures[] = {"READY", "SET", "LET'S", "GO", "IS","THIS", "WORKING"};
// BLE advertising parameters
static esp_ble_adv_params_t adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x40,
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// GAP event handler
static void gap_event_handler(esp_gap_ble_cb_event_t event,
                              esp_ble_gap_cb_param_t *param)
{
    switch(event) {

        // Called when advertising data is configured
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising data set complete, starting advertising...");
            esp_ble_gap_start_advertising(&adv_params);
            break;

        // Called when advertising actually starts, for logging purposes only 
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising started successfully!");
            } else {
                ESP_LOGE(TAG, "Advertising start failed, status = %d", param->adv_start_cmpl.status);
            }
            break;

        // NOT FINISHED!!!!!!!!!!!!!!!!!!
        // Called when need to update connection parameters after a connection is established, can lower power usage
        // case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        // {
        //     ESP_LOGI(TAG, "Connection parameters update event");

        //     // Adjustable parameters for low power wearable
        //     esp_ble_conn_update_params_t params;
        //     params.min_int = 0x50;   // 80 ms 
        //     params.max_int = 0x60;   // 96 ms
        //     params.latency = 0;      // don't skip connection events
        //     params.timeout = 400;    // 4 sec supervision timeout

        //     esp_ble_gap_update_conn_params(&params);

        //     ESP_LOGI(TAG, "Requested updated connection parameters: min=%d max=%d latency=%d timeout=%d",
        //              params.min_int, params.max_int, params.latency, params.timeout);
        //     break;
        // }

        default:
            // Other GAP events not handled here
            break;
    }
}

// GATT server event handler
static void gatts_event_handler(esp_gatts_cb_event_t event,              
                                esp_gatt_if_t gatts_if,
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    
    // Checks if GATT server is registered with the GATT stack
    case ESP_GATTS_REG_EVT: {
        ESP_LOGI(TAG, "GATT registered");

        esp_ble_gap_set_device_name("Gesture Recognition");

        // define advertising data
        esp_ble_adv_data_t adv_data = {0}; // Initialize advertising data structure
        adv_data.set_scan_rsp = false; // This is advertising data, not scan response 
        // Scan response can send extra data, phone must scan for it
        adv_data.include_name = true; // Include device name 
        adv_data.include_txpower = true; // Include TX power level (transmission power)
        adv_data.service_uuid_len = 16; // Length of the service UUID
        adv_data.p_service_uuid = SERVICE_UUID; // Define the service UUID
        adv_data.flag = ESP_BLE_ADV_FLAG_GEN_DISC | // General discoverable mode
                        ESP_BLE_ADV_FLAG_BREDR_NOT_SPT; // BR/EDR not supported, BLE only device

        esp_ble_gap_config_adv_data(&adv_data); 

        // define GATT service
        esp_gatt_srvc_id_t service_id = {0}; 
        service_id.is_primary = true; // Primary service
        service_id.id.inst_id = 0; 
        service_id.id.uuid.len = ESP_UUID_LEN_128; // Length of the service UUID
        memcpy(service_id.id.uuid.uuid.uuid128, SERVICE_UUID, 16); // Set the service UUID

        esp_ble_gatts_create_service(gatts_if, &service_id, 10); // Create the GATT service with 10 handles
        // 1 service, 2 gesture characteristic, 2 for battery characteristic, 1 for client characteristic, and some extra room
        break;
    }

    // fires after main service is created, adds first characteristic for gesture data
    case ESP_GATTS_CREATE_EVT:
        service_handle = param->create.service_handle; // updates service handle 
        esp_ble_gatts_start_service(service_handle);  // starts service with the assigned handle

        {
            esp_bt_uuid_t char_uuid = {0}; // creates UUID struct for characteristic 
            char_uuid.len = ESP_UUID_LEN_128; // length of characteristic UUID
            memcpy(char_uuid.uuid.uuid128, GESTURE_CHAR_UUID, 16); // sets characteristic UUID

            // Adds characteristic to the service with the assigned handle
            esp_ble_gatts_add_char(service_handle,
                                   &char_uuid,
                                   // gives permission to read characteristic value
                                   ESP_GATT_PERM_READ,
                                    // allows both notifications and read requests for gesture data
                                   ESP_GATT_CHAR_PROP_BIT_NOTIFY | ESP_GATT_CHAR_PROP_BIT_READ, 
                                   NULL,
                                   NULL);
        }
    
        break;
    
    // NOT FINISHED!!!!!!!!!!!!!!!!!!
    // fires after a characteristic is added, adds second service for battery level data
    // case ESP_GATTS_ADD_CHAR_EVT:

    //     if (gesture_char_handle == 0) {
    //         gesture_char_handle = param->add_char.attr_handle;

    //         esp_bt_uuid_t char_uuid = {0};
    //         char_uuid.len = ESP_UUID_LEN_128;
    //         memcpy(char_uuid.uuid.uuid128, INT_CHAR_UUID, 16);

    //         esp_ble_gatts_add_char(service_handle,
    //                                &char_uuid,
    //                                ESP_GATT_PERM_READ,
    //                                ESP_GATT_CHAR_PROP_BIT_NOTIFY,
    //                                NULL,
    //                                NULL);
    //     } else {
    //         intensity_char_handle = param->add_char.attr_handle;
    //     }
    //     break;

    case ESP_GATTS_ADD_CHAR_EVT:
        // temporary solution to only add one characteristic for gesture data, will add battery level service later
        ESP_LOGI(TAG, "Characteristic added, handle=%d", param->add_char.attr_handle);
        gesture_char_handle = param->add_char.attr_handle;

        // Add Client Characteristic Configuration Descriptor (CCCD) for the gesture characteristic to enable notifications
        // Define the UUID for the CCCD, standard 16-bit UUID (0x2902)
        esp_bt_uuid_t cccd_uuid = {0}; // initialize UUID struct for CCCD
        cccd_uuid.len = ESP_UUID_LEN_16; // CCCD UUID length is 16 byes
        cccd_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG; // 0x2902
        
        uint16_t cccd_value = 0x0000; // notifications disabled by default
        // Define the attribute value for the CCCD, which will hold the notification enable/disable state
        esp_attr_value_t cccd_attr = {
            .attr_max_len = sizeof(uint16_t),
            .attr_len     = sizeof(uint16_t),
            .attr_value   = (uint8_t *)&cccd_value,
        };

        // Add the CCCD to the gesture characteristic, allows client to enable notifications by writing to it
        esp_err_t err = esp_ble_gatts_add_char_descr(
            service_handle,
            &cccd_uuid,
            // allows read and write permissions for the CCCD, client needs to write to it to enable notifications
            ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
            &cccd_attr,
            NULL
        );

        // checks if adding the CCCD was successful, logs an error if it failed
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add CCCD, err=0x%x", err);
        }
        break;

    // confirms the CCCD was added and saves its handle for later use
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        ESP_LOGI(TAG, "Descriptor added, handle=%d", param->add_char_descr.attr_handle);
        gesture_cccd_handle = param->add_char_descr.attr_handle;
        break;

    // fires when client writes to the CCCD to enable or disable notifications for gesture data
    case ESP_GATTS_WRITE_EVT:
        
        // Recieve client write to CCCD to enable/disable notifications for gesture characteristic
        if (param->write.handle == gesture_cccd_handle  && param->write.len == 2) {
            // param is 2-byte long data so check both 
            uint16_t cccd = param->write.value[0] | (param->write.value[1] << 8);
            if (cccd == 0x0001) { // Notifications enabled
                ESP_LOGI(TAG, "Notifications enabled for gesture characteristic");
                gesture_notify_enabled = true;
            } else if (cccd == 0x0000) { // Notifications disabled
                ESP_LOGI(TAG, "Notifications disabled for gesture characteristic");
                gesture_notify_enabled = false;
            } 
            // Do not need / support indications (required ack from client), ingnore 0x02 value 
        }

        // writes back to client to confirm write request if client requests it 
        if (param->write.need_rsp) {
        esp_ble_gatts_send_response(gatts_if, param->write.conn_id,
                                param->write.trans_id, ESP_GATT_OK, NULL);
        }
        break;

    // fires when a client device connects
    case ESP_GATTS_CONNECT_EVT:
        ESP_LOGI(TAG, "Device connected");
        device_connected = true; // Set flag to indicate a device is connected
        gatts_if_global = gatts_if; // Update GATT interface for sending notifications later
        conn_id_global = param->connect.conn_id; // Update connection ID for sending notifications later
        break;
    
    // fires when a client device disconnects, restarts advertising to allow new connections
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(TAG, "Device disconnected");
        device_connected = false; // Clear flag to indicate no device is connected
        esp_ble_gap_start_advertising(&adv_params); // Restart advertising to allow new connections
        break;
    
    default:
        break;
    }
}

// Send notificaiton 
static void send_gesture_notify(const char* msg)
{   
    // check if device is connected, notifications are enabled, and characteristic handle is valid before sending notification
    if (!device_connected || !gesture_notify_enabled || gesture_char_handle == 0) return;
    if (msg == NULL || *msg == 0) return; // Return if notification is empty

    // clamp length of message 
    const size_t MAX_PAYLOAD = 20;
    size_t len = strlen(msg);
    if (len > MAX_PAYLOAD) len = MAX_PAYLOAD;

    uint8_t message[64] = {0}; // Initialize data buffer with zeros
    memcpy(message, msg, len); // Copy msg into data buffer

    // Update the characteristic value in the GATT database 
    esp_ble_gatts_set_attr_value(gesture_char_handle, len, message); 

    // Send notification with update characteristic value 
    esp_err_t err = esp_ble_gatts_send_indicate(
        gatts_if_global,
        conn_id_global,
        gesture_char_handle,
        len,
        message,
        false  // false = notification (no confirmation). true = indication (needs ACK)
    );
 
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "send_indicate failed: 0x%x", err);
    }
}

// BLE initialization
void ble_init(void)
{   
    // initialize NVS(non-volatile storage) for storing BLE parameters, required by ESP-IDF
    esp_err_t ret = nvs_flash_init();

    // error check, if no free pages or new version found, erase NVS and initialize again
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    // check if NVS initialization was successful
    ESP_ERROR_CHECK(ret);

    // release memory for classic bluetooth 
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // initialize and enable Bluetooth controller in BLE mode, then initialize and enable Bluedroid stack
    esp_bt_controller_config_t bt_cfg =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    // error check Bluetooth initialization 
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg)); // initialize Bluetooth controller 
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE)); // enable Bluetooth controller in BLE mode
    ESP_ERROR_CHECK(esp_bluedroid_init()); // initialize bluedriod stack
    ESP_ERROR_CHECK(esp_bluedroid_enable()); // enable bluedroid stack 

    esp_ble_gap_register_callback(gap_event_handler); // intialize gap event 
    esp_ble_gatts_register_callback(gatts_event_handler); // initialize gatt event 
    
    esp_ble_gatts_app_register(0);  // registers the gatt device 
}

// Main

void app_main(void)
{
    ble_init();
    
    while (1) {
        ESP_LOGI(TAG, "This totally works!");
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // currently uses polling to send gestures. can change to using ISR in future 
        // check if device connected, gesture notification enabled, gesture is different 
        if (device_connected && gesture_notify_enabled ) {
        /*&& strncmp(gesture_text, last_sent, sizeof(last_sent)) != 0 */ 
            
            // then send the text through 
            send_gesture_notify(gesture_text);

            // remember what we sent
            strncpy(last_sent, gesture_text, sizeof(last_sent) - 1); // copy new gesture text to last_sent
            last_sent[sizeof(last_sent) - 1] = '\0'; // ensure last_sent null-terminates 

            // copy random gesture text from test_gestures to gesture_text to test notifications
            snprintf(gesture_text, sizeof(gesture_text), "%s",
            test_gestures[rand() % (sizeof(test_gestures) / sizeof(test_gestures[0]))]);
        }
    }
}
