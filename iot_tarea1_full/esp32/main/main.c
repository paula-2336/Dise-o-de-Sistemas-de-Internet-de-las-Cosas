#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <endian.h>
#include <math.h>

// esp-idf apis
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_log.h>
#include <nvs_flash.h>

// nimble stack apis
#include <nimble/ble.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_npl_os.h>
#include <host/ble_hs.h>
#include <host/ble_uuid.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <services/gatt/ble_svc_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include "host/ble_att.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_id.h"
#include "host/ble_store.h"
#include "os/os_mbuf.h"
#include "portmacro.h"




#define TEMP_MIN 20.0f
#define TEMP_MAX 30.0f
#define ACCEL_RANGE_G 16.0f
#define ATTENUATION 0.70f /* ejes secundarios = 70 % del principal */

ble_uuid16_t acc_chr_uuid = BLE_UUID16_INIT(0x1801);
ble_uuid16_t acc_svc_uuid = BLE_UUID16_INIT(0x2A58);
ble_uuid16_t temp_chr_uuid = BLE_UUID16_INIT(0x2AE6);
ble_uuid16_t temp_svc_uuid = BLE_UUID16_INIT(0x1809);

uint16_t acc_char_attr_handle;
uint16_t tmp_char_attr_handle;

uint8_t store;


char device_name[] = "Tarea_1";
char device_name_short[] = "T1";

uint8_t notify_status = 0;
uint16_t notify_attr = 0;
uint16_t notify_temp = 0;
uint16_t notify_acc = 0;
uint16_t notify_conn = 0;

//--------------------------------------------------

typedef struct { float ax, ay, az; } accel_sample_t;
accel_sample_t simulate_accel(void) {
    /* Eje principal: sinusoide + ruido uniforme */
    static float phase = 0.0f;
    phase += 0.01f; /* avance de fase por muestra a 1000 Hz */
    float main_val = ACCEL_RANGE_G * sinf(phase)
        + ((float)rand() / RAND_MAX - 0.5f) * 1.0f;
    accel_sample_t s = {
        .ax = main_val,
        .ay = main_val * ATTENUATION + ((float)rand()/RAND_MAX - 0.5f)*0.5f,
        .az = main_val * ATTENUATION + ((float)rand()/RAND_MAX - 0.5f)*0.5f,
    };
    return s;
}

void acc_task() {
  for (;;) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    if (notify_status) {
      int err = ble_gatts_notify(notify_conn, notify_acc);
      if (err) {
        printf("algo");
        continue;
      }
    }
  }
}

float simulate_temperature(void) {
    static float temp = 25.0f;
    float delta = ((float)rand() / RAND_MAX - 0.5f) * 0.4f;
    temp += delta;
    if (temp < TEMP_MIN) temp = TEMP_MIN;
    if (temp > TEMP_MAX) temp = TEMP_MAX;
    return temp;
}


void temp_task() {
  for (;;) {
    vTaskDelay(15000 / portTICK_PERIOD_MS);

    if (notify_status) {
      int err = ble_gatts_notify(notify_conn, notify_temp);
      if (err) {
        printf("algo");
        continue;
      }
    }
  }
}

//--------------------------------

int chr_access(uint16_t conn_handle, uint16_t attr_handle,
  struct ble_gatt_access_ctxt *ctxt, void *arg) {

  if (attr_handle != acc_char_attr_handle && attr_handle != tmp_char_attr_handle) {
    return BLE_ATT_ERR_ATTR_NOT_FOUND;
  }

  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    os_mbuf_append(ctxt->om, &store, 1);
    printf("se leyo dslklkds\n");

  } else if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    if (ctxt->om->om_len != sizeof(store)) {
      return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    store = ctxt->om->om_data[0];
    printf("Se escrinio\n");

  } else {
    return BLE_ATT_ERR_UNLIKELY;
  }

  return 0;
}

struct ble_gatt_svc_def gatt_svc[] = {
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &temp_svc_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
        .uuid = &temp_chr_uuid.u,
        .access_cb = chr_access,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &tmp_char_attr_handle,
      },
      {0}
    }
  },
  {
    .type = BLE_GATT_SVC_TYPE_PRIMARY,
    .uuid = &acc_svc_uuid.u,
    .characteristics = (struct ble_gatt_chr_def[]) {
      {
      .uuid = &acc_chr_uuid.u,
      .access_cb = chr_access,
      .flags = BLE_GATT_CHR_F_NOTIFY,
      .val_handle = &acc_char_attr_handle,
      },
      {0}
    },
  },
  {0},
};

void start_adv();

int connection_handler(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_CONNECT) {
      printf("Cliente %d conectado con estado %d\n", event->connect.conn_handle, event->connect.status);
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
      printf("Cliente %d desconectado por motivo %d\n", event->disconnect.conn.conn_handle, event->disconnect.reason);
      start_adv();
    } else if (event->type == BLE_GAP_EVENT_SUBSCRIBE) {

      printf("Suscripción de cliente %d actualizada a %d\n", event->subscribe.conn_handle, event->subscribe.cur_notify);

      notify_status = event->subscribe.cur_notify;

      if (notify_status) {

        if (event->subscribe.attr_handle == tmp_char_attr_handle) {
          notify_temp = event->subscribe.attr_handle;
        } else if (event->subscribe.attr_handle == acc_char_attr_handle) {
          notify_acc = event->subscribe.attr_handle;
        }
        notify_conn = event->subscribe.conn_handle;
      } else {
        notify_attr = 0;
        notify_conn = 0;
      }
    }


    return 0;
}

void start_adv(void) {
  uint16_t adv_interval_ms = 200;
  uint8_t ble_addr_type = BLE_ADDR_PUBLIC;

  uint8_t addr_val[8];
  ble_hs_id_copy_addr(ble_addr_type,addr_val, NULL);

  struct ble_hs_adv_fields adv_fields = {
    .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
    .name = (uint8_t*)device_name_short,
    .name_len = strlen(device_name_short),
    .name_is_complete = 0,

    .adv_itvl = BLE_GAP_ADV_ITVL_MS(adv_interval_ms),
    .adv_itvl_is_present = 1,

    .device_addr = addr_val,
    .device_addr_type = ble_addr_type,
    .device_addr_is_present = 1,
  };

  ble_gap_adv_set_fields(&adv_fields);

  struct ble_hs_adv_fields rsp_fields = {
    .name = (uint8_t*) device_name,
    .name_len = strlen(device_name),
    .name_is_complete = 1,
  };

  ble_gap_adv_rsp_set_fields(&rsp_fields);


  struct ble_gap_adv_params adv_params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,

    .itvl_min = BLE_GAP_ADV_ITVL_MS(adv_interval_ms),
    .itvl_max = BLE_GAP_ADV_ITVL_MS(adv_interval_ms + 10),
  };

  ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, connection_handler, NULL);

  printf("Advertisment comenzado\n");
}

void on_stack_reset(int reason) {
  printf("Nimble se reinicio\n");
}

void on_stack_sync(void) {
  start_adv();
}


void app_main(void) {

  nvs_flash_init();

  nimble_port_init();

  ble_svc_gap_init();
  ble_svc_gap_device_name_set(device_name);

  ble_svc_gatt_init();
  ble_gatts_count_cfg(gatt_svc);
  ble_gatts_add_svcs(gatt_svc);


  ble_hs_cfg.reset_cb = on_stack_reset;
  ble_hs_cfg.sync_cb = on_stack_sync;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;


  xTaskCreate(acc_task, "Acceleration", 4096, NULL, 5, NULL);
  xTaskCreate(temp_task, "Temperature", 4096, NULL, 5, NULL);

  nimble_port_run();

  sleep(2);

  esp_restart();


}