#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_SCAN_TARGET";
static uint8_t own_address_type;
static const char target_name[] = "ESP Scan Target";
static int gap_event(struct ble_gap_event *event, void *arg);

static void start_advertising(void) {
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int rc = ble_gap_adv_start(own_address_type, NULL, BLE_HS_FOREVER,
        &params, gap_event, NULL);
    if (rc) ESP_LOGE(tag, "advertising start failed: %d", rc);
    else ESP_LOGI(tag, "S31_BLE_ADVERTISER,advertising=pass");
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_CONNECT) {
        ESP_LOGI(tag, "S31_BLE_ADVERTISER,connected=%d", event->connect.status == 0);
        if (event->connect.status) start_advertising();
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        ESP_LOGI(tag, "S31_BLE_ADVERTISER,disconnected=pass");
        start_advertising();
    } else if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
        ESP_LOGE(tag, "advertising stopped: %d", event->adv_complete.reason);
    }
    return 0;
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_address_type);
    if (rc) { ESP_LOGE(tag, "address type failed: %d", rc); return; }
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    rc = ble_gap_adv_set_fields(&fields);
    if (rc) { ESP_LOGE(tag, "advertisement fields failed: %d", rc); return; }
    struct ble_hs_adv_fields response = {0};
    response.name = (const uint8_t *)target_name;
    response.name_len = strlen(target_name);
    response.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response);
    if (rc) { ESP_LOGE(tag, "scan response failed: %d", rc); return; }
    start_advertising();
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
}
