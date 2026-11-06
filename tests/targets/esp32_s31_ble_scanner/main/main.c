#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_BLE_SCANNER";

static int scan_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type != BLE_GAP_EVENT_DISC) return 0;
    struct ble_hs_adv_fields fields = {0};
    if (ble_hs_adv_parse_fields(&fields, event->disc.data,
            event->disc.length_data) != 0) return 0;
    for (unsigned i = 0; i < fields.num_uuids16; ++i) {
        if (fields.uuids16[i].value == 0xfff0) {
            ESP_LOGI(tag, "NRF52_BLE_ADV_SERVICE,uuid16=pass,rssi=%d",
                event->disc.rssi);
            break;
        }
    }
    if (fields.name_len == sizeof("Test beacon") - 1 &&
            memcmp(fields.name, "Test beacon", fields.name_len) == 0) {
        ESP_LOGI(tag, "NRF52_BLE,advertisement=pass,rssi=%d",
            event->disc.rssi);
    }
    return 0;
}

static void on_sync(void) {
    uint8_t address_type;
    int rc = ble_hs_id_infer_auto(0, &address_type);
    if (rc != 0) {
        ESP_LOGE(tag, "address type failed: %d", rc);
        return;
    }
    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    params.filter_duplicates = 0;
    rc = ble_gap_disc(address_type, BLE_HS_FOREVER, &params, scan_event, NULL);
    if (rc != 0) ESP_LOGE(tag, "scan failed: %d", rc);
    else ESP_LOGI(tag, "scan started");
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
}
