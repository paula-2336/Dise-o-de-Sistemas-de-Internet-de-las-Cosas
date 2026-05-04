
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#define TAG "IOT_TASK1"

// UUIDs
#define SERVICE_ACCEL_UUID    0x1800
#define CHAR_ACCEL_UUID       0x2A58
#define SERVICE_TEMP_UUID     0x1809
#define CHAR_TEMP_UUID        0x246E

// Simulation Constants
#define ACCEL_RANGE_G 16.0f
#define ATTENUATION 0.70f
#define TEMP_MIN 20.0f
#define TEMP_MAX 30.0f

typedef struct {
    uint32_t timestamp;
    float ax, ay, az;
} accel_packet_t;

typedef struct {
    uint32_t timestamp;
    float temp;
} temp_packet_t;

static uint16_t accel_handle, temp_handle;
static bool accel_notify_enabled = false;
static bool temp_notify_enabled = false;
static uint16_t gatts_if_for_app;

accel_packet_t simulate_accel(void) {
    static float phase = 0.0f;
    phase += 0.01f;
    float main_val = ACCEL_RANGE_G * sinf(phase) + (((float)rand() / RAND_MAX) - 0.5f) * 1.0f;
    accel_packet_t p;
    p.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
    p.ax = main_val;
    p.ay = main_val * ATTENUATION + (((float)rand() / RAND_MAX) - 0.5f) * 0.5f;
    p.az = main_val * ATTENUATION + (((float)rand() / RAND_MAX) - 0.5f) * 0.5f;
    return p;
}

float simulate_temperature(void) {
    static float temp = 25.0f;
    float delta = (((float)rand() / RAND_MAX) - 0.5f) * 0.4f;
    temp += delta;
    if (temp < TEMP_MIN) temp = TEMP_MIN;
    if (temp > TEMP_MAX) temp = TEMP_MAX;
    return temp;
}

void accel_task(void *pvParameters) {
    while (1) {
        if (accel_notify_enabled) {
            accel_packet_t p = simulate_accel();
            esp_ble_gatts_send_indication(gatts_if_for_app, 0, accel_handle, sizeof(p), (uint8_t *)&p, false);
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void temp_task(void *pvParameters) {
    while (1) {
        if (temp_notify_enabled) {
            temp_packet_t p;
            p.timestamp = xTaskGetTickCount() * portTICK_PERIOD_MS;
            p.temp = simulate_temperature();
            esp_ble_gatts_send_indication(gatts_if_for_app, 0, temp_handle, sizeof(p), (uint8_t *)&p, false);
        }
        vTaskDelay(pdMS_TO_TICKS(15000));
    }
}


void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize BLE, Register Services, Start Tasks
    xTaskCreate(accel_task, "accel_task", 4096, NULL, 5, NULL);
    xTaskCreate(temp_task, "temp_task", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "ESP32 BLE Server Started");
}
