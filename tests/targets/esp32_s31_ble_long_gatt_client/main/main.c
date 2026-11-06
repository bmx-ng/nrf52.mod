#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_LONG_GATT";
static uint8_t own_address_type;
static uint16_t service_start, service_end, value_handle;
static uint8_t readback[300];
static uint16_t readback_length;

static int readback_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    if (error->status == 0 && attr) {
        uint16_t length = OS_MBUF_PKTLEN(attr->om);
        if (readback_length + length > sizeof(readback) ||
                os_mbuf_copydata(attr->om, 0, length,
                    readback + readback_length) != 0) {
            ESP_LOGE(tag, "readback overflow");
            return BLE_HS_EAPP;
        }
        readback_length += length;
        return 0;
    }
    if (error->status != BLE_HS_EDONE) {
        ESP_LOGE(tag, "readback failed: %d", error->status);
        return 0;
    }
    if (readback_length != sizeof(readback)) {
        ESP_LOGE(tag, "readback length=%u", readback_length);
        return 0;
    }
    for (unsigned i = 0; i < sizeof(readback); ++i) {
        if (readback[i] != (uint8_t)i) {
            ESP_LOGE(tag, "readback mismatch at %u", i);
            return 0;
        }
    }
    ESP_LOGI(tag, "S31_LONG_GATT,mtu=247,write_300=pass,read_300=pass");
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
}

static int write_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)attr; (void)arg;
    if (error->status != 0) {
        ESP_LOGE(tag, "long write failed: %d", error->status);
        return 0;
    }
    int rc = ble_gattc_read_long(conn, value_handle, 0, readback_done, NULL);
    if (rc) ESP_LOGE(tag, "long read start failed: %d", rc);
    return 0;
}

static void start_long_write(uint16_t conn) {
    uint8_t outgoing[300];
    for (unsigned i = 0; i < sizeof(outgoing); ++i) outgoing[i] = (uint8_t)i;
    struct os_mbuf *mbuf = ble_hs_mbuf_from_flat(outgoing, sizeof(outgoing));
    if (!mbuf) {
        ESP_LOGE(tag, "long write allocation failed");
        return;
    }
    int rc = ble_gattc_write_long(conn, value_handle, 0, mbuf, write_done, NULL);
    if (rc) ESP_LOGE(tag, "long write start failed: %d", rc);
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
        start_long_write(conn);
    } else if (error->status != 0) {
        ESP_LOGE(tag, "characteristic discovery failed: %d", error->status);
    }
    return 0;
}

static int service_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_svc *service, void *arg) {
    (void)arg;
    if (error->status == 0 && service) {
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
        int rc = ble_gattc_disc_chrs_by_uuid(conn, service_start, service_end,
            BLE_UUID16_DECLARE(0xfff1), characteristic_found, NULL);
        if (rc) ESP_LOGE(tag, "characteristic discovery start failed: %d", rc);
    } else if (error->status != 0) {
        ESP_LOGE(tag, "service discovery failed: %d", error->status);
    }
    return 0;
}

static int mtu_done(uint16_t conn, const struct ble_gatt_error *error,
        uint16_t mtu, void *arg) {
    (void)arg;
    if (error->status != 0 || mtu < 247) {
        ESP_LOGE(tag, "MTU exchange failed: %d, mtu=%u", error->status, mtu);
        return 0;
    }
    int rc = ble_gattc_disc_all_svcs(conn, service_found, NULL);
    if (rc) ESP_LOGE(tag, "service discovery start failed: %d", rc);
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields = {0};
        if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                event->disc.length_data) != 0) return 0;
        if (fields.name_len != sizeof("NRF Long GATT") - 1 ||
                memcmp(fields.name, "NRF Long GATT", fields.name_len) != 0)
            return 0;
        ble_addr_t address = event->disc.addr;
        int rc = ble_gap_disc_cancel();
        if (rc) { ESP_LOGE(tag, "stop scan failed: %d", rc); return 0; }
        rc = ble_gap_connect(own_address_type, &address, 30000,
            NULL, gap_event, NULL);
        if (rc) ESP_LOGE(tag, "connect start failed: %d", rc);
    } else if (event->type == BLE_GAP_EVENT_CONNECT) {
        if (event->connect.status) {
            ESP_LOGE(tag, "connect failed: %d", event->connect.status);
            return 0;
        }
        int rc = ble_gattc_exchange_mtu(event->connect.conn_handle,
            mtu_done, NULL);
        if (rc) ESP_LOGE(tag, "MTU exchange start failed: %d", rc);
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        ESP_LOGI(tag, "S31_LONG_GATT,disconnect=pass");
    }
    return 0;
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_address_type);
    if (rc) { ESP_LOGE(tag, "address type failed: %d", rc); return; }
    struct ble_gap_disc_params params = {0};
    params.passive = 0;
    rc = ble_gap_disc(own_address_type, BLE_HS_FOREVER, &params,
        gap_event, NULL);
    if (rc) ESP_LOGE(tag, "scan failed: %d", rc);
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
