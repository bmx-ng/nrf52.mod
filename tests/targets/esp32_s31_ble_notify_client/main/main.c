#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_BLE_NOTIFY";
static uint8_t own_address_type;
static uint16_t service_start, service_end, value_handle, ccc_handle;

static int subscription_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)conn; (void)attr; (void)arg;
    if (error->status != 0) {
        ESP_LOGE(tag, "subscription failed: %d", error->status);
    } else {
        ESP_LOGI(tag, "subscription enabled");
    }
    return 0;
}

static int descriptor_found(uint16_t conn, const struct ble_gatt_error *error,
        uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg) {
    (void)chr_val_handle; (void)arg;
    if (error->status == 0 && dsc &&
            ble_uuid_cmp(&dsc->uuid.u, BLE_UUID16_DECLARE(0x2902)) == 0)
        ccc_handle = dsc->handle;
    if (error->status == BLE_HS_EDONE) {
        if (!ccc_handle) {
            ESP_LOGE(tag, "CCC descriptor not found");
            return 0;
        }
        uint8_t enabled[2] = {1, 0};
        int rc = ble_gattc_write_flat(conn, ccc_handle, enabled, sizeof(enabled),
            subscription_done, NULL);
        if (rc) ESP_LOGE(tag, "subscription start failed: %d", rc);
    } else if (error->status != 0) {
        ESP_LOGE(tag, "descriptor discovery failed: %d", error->status);
    }
    return 0;
}

static int first_read(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    uint8_t value[4];
    if (error->status != 0 || !attr || OS_MBUF_PKTLEN(attr->om) != 4 ||
            os_mbuf_copydata(attr->om, 0, 4, value) != 0 ||
            memcmp(value, "NRF!", 4) != 0) {
        ESP_LOGE(tag, "initial read failed: %d", error->status);
        return 0;
    }
    ESP_LOGI(tag, "initial read passed");
    int rc = ble_gattc_disc_all_dscs(conn, value_handle, service_end,
        descriptor_found, NULL);
    if (rc) ESP_LOGE(tag, "descriptor discovery start failed: %d", rc);
    return 0;
}

static int characteristic_found(uint16_t conn,
        const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
        void *arg) {
    (void)arg;
    if (error->status == 0 && chr) value_handle = chr->val_handle;
    if (error->status == BLE_HS_EDONE) {
        if (!value_handle) {
            ESP_LOGE(tag, "characteristic not found");
            return 0;
        }
        int rc = ble_gattc_read(conn, value_handle, first_read, NULL);
        if (rc) ESP_LOGE(tag, "read start failed: %d", rc);
    } else if (error->status != 0) {
        ESP_LOGE(tag, "characteristic discovery failed: %d", error->status);
    }
    return 0;
}

static int service_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_svc *service, void *arg) {
    (void)arg;
    if (error->status == 0 && service) {
        char uuid_text[BLE_UUID_STR_LEN];
        ble_uuid_to_str(&service->uuid.u, uuid_text);
        ESP_LOGI(tag, "service handle=%u uuid=%s", service->start_handle, uuid_text);
        if (ble_uuid_cmp(&service->uuid.u, BLE_UUID16_DECLARE(0xfff0)) != 0)
            return 0;
        service_start = service->start_handle;
        service_end = service->end_handle;
    }
    if (error->status == BLE_HS_EDONE) {
        if (!service_start) {
            ESP_LOGE(tag, "service not found");
            return 0;
        }
        int rc = ble_gattc_disc_chrs_by_uuid(conn, service_start,
            service_end, BLE_UUID16_DECLARE(0xfff1),
            characteristic_found, NULL);
        if (rc) ESP_LOGE(tag, "characteristic discovery start failed: %d", rc);
    } else if (error->status != 0) {
        ESP_LOGE(tag, "service discovery failed: %d", error->status);
    }
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields = {0};
        if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                event->disc.length_data) != 0) return 0;
        if (fields.name_len != sizeof("NRF Notify") - 1 ||
                memcmp(fields.name, "NRF Notify", fields.name_len) != 0)
            return 0;
        ble_addr_t address = event->disc.addr;
        int rc = ble_gap_disc_cancel();
        if (rc) { ESP_LOGE(tag, "stop scan failed: %d", rc); return 0; }
        rc = ble_gap_connect(own_address_type, &address, 30000,
            NULL, gap_event, NULL);
        if (rc) ESP_LOGE(tag, "connect start failed: %d", rc);
        else ESP_LOGI(tag, "connecting to nRF");
    } else if (event->type == BLE_GAP_EVENT_CONNECT) {
        if (event->connect.status) {
            ESP_LOGE(tag, "connect failed: %d", event->connect.status);
            return 0;
        }
        ESP_LOGI(tag, "connected");
        int rc = ble_gattc_disc_all_svcs(event->connect.conn_handle,
            service_found, NULL);
        if (rc) ESP_LOGE(tag, "service discovery start failed: %d", rc);
    } else if (event->type == BLE_GAP_EVENT_NOTIFY_RX) {
        uint8_t value[4];
        if (!event->notify_rx.indication &&
                event->notify_rx.attr_handle == value_handle &&
                OS_MBUF_PKTLEN(event->notify_rx.om) == 4 &&
                os_mbuf_copydata(event->notify_rx.om, 0, 4, value) == 0 &&
                memcmp(value, "PUSH", 4) == 0) {
            ESP_LOGI(tag, "NRF52_BLE_NOTIFY,subscribe=pass,notify=pass");
            ble_gap_terminate(event->notify_rx.conn_handle,
                BLE_ERR_REM_USER_CONN_TERM);
        } else {
            ESP_LOGE(tag, "notification mismatch");
        }
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        ESP_LOGI(tag, "disconnected");
    }
    return 0;
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_address_type);
    if (rc) { ESP_LOGE(tag, "address type failed: %d", rc); return; }
    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    params.filter_duplicates = 0;
    rc = ble_gap_disc(own_address_type, BLE_HS_FOREVER, &params,
        gap_event, NULL);
    if (rc) ESP_LOGE(tag, "scan failed: %d", rc);
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
