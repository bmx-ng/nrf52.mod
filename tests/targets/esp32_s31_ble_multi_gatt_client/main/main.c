#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_MULTI_GATT";
static uint8_t own_address_type;
static uint16_t service_start[2], service_end[2], value_handle[3];
static uint8_t discovery_stage, read_stage, complete;

static int read_value(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg);

static int readback(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    uint8_t value[3];
    if (error->status || !attr || OS_MBUF_PKTLEN(attr->om) != 3 ||
            os_mbuf_copydata(attr->om, 0, 3, value) != 0 ||
            memcmp(value, "UP!", 3) != 0) {
        ESP_LOGE(tag, "readback failed: %d", error->status);
        return 0;
    }
    complete = 1;
    ESP_LOGI(tag, "S31_MULTI_GATT,services=2,characteristics=3,reads=pass,write_second=pass");
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
}

static int write_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)attr; (void)arg;
    if (error->status) {
        ESP_LOGE(tag, "write failed: %d", error->status);
        return 0;
    }
    int rc = ble_gattc_read(conn, value_handle[1], readback, NULL);
    if (rc) ESP_LOGE(tag, "readback start failed: %d", rc);
    return 0;
}

static int read_value(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    static const char *expected[] = { "ONE", "TWO", "THR" };
    uint8_t value[3];
    if (read_stage >= 3 || error->status || !attr ||
            OS_MBUF_PKTLEN(attr->om) != 3 ||
            os_mbuf_copydata(attr->om, 0, 3, value) != 0 ||
            memcmp(value, expected[read_stage], 3) != 0) {
        ESP_LOGE(tag, "read stage %u failed: %d", read_stage, error->status);
        return 0;
    }
    ++read_stage;
    int rc;
    if (read_stage < 3) {
        rc = ble_gattc_read(conn, value_handle[read_stage], read_value, NULL);
    } else {
        rc = ble_gattc_write_flat(conn, value_handle[1], "UP!", 3,
            write_done, NULL);
    }
    if (rc) ESP_LOGE(tag, "next request failed: %d", rc);
    return 0;
}

static int characteristic_found(uint16_t conn,
        const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
        void *arg) {
    (void)arg;
    if (error->status == 0 && chr) {
        if (ble_uuid_cmp(&chr->uuid.u, BLE_UUID16_DECLARE(0xfff1)) == 0)
            value_handle[0] = chr->val_handle;
        else if (ble_uuid_cmp(&chr->uuid.u, BLE_UUID16_DECLARE(0xfff2)) == 0)
            value_handle[1] = chr->val_handle;
        else if (ble_uuid_cmp(&chr->uuid.u, BLE_UUID16_DECLARE(0xffe1)) == 0)
            value_handle[2] = chr->val_handle;
    } else if (error->status == BLE_HS_EDONE) {
        if (discovery_stage == 0) {
            if (!value_handle[0] || !value_handle[1]) {
                ESP_LOGE(tag, "first service characteristics missing");
                return 0;
            }
            discovery_stage = 1;
            int rc = ble_gattc_disc_all_chrs(conn, service_start[1],
                service_end[1], characteristic_found, NULL);
            if (rc) ESP_LOGE(tag, "second characteristic discovery failed: %d", rc);
        } else {
            if (!value_handle[2]) {
                ESP_LOGE(tag, "second service characteristic missing");
                return 0;
            }
            int rc = ble_gattc_read(conn, value_handle[0], read_value, NULL);
            if (rc) ESP_LOGE(tag, "first read start failed: %d", rc);
        }
    } else if (error->status) {
        ESP_LOGE(tag, "characteristic discovery failed: %d", error->status);
    }
    return 0;
}

static int service_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_svc *service, void *arg) {
    (void)arg;
    if (error->status == 0 && service) {
        int index = -1;
        if (ble_uuid_cmp(&service->uuid.u, BLE_UUID16_DECLARE(0xfff0)) == 0)
            index = 0;
        else if (ble_uuid_cmp(&service->uuid.u, BLE_UUID16_DECLARE(0xffe0)) == 0)
            index = 1;
        if (index >= 0) {
            service_start[index] = service->start_handle;
            service_end[index] = service->end_handle;
        }
    } else if (error->status == BLE_HS_EDONE) {
        if (!service_start[0] || !service_start[1] ||
                service_start[0] == service_start[1]) {
            ESP_LOGE(tag, "services missing or overlapping");
            return 0;
        }
        int rc = ble_gattc_disc_all_chrs(conn, service_start[0],
            service_end[0], characteristic_found, NULL);
        if (rc) ESP_LOGE(tag, "first characteristic discovery failed: %d", rc);
    } else if (error->status) {
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
        if (fields.name_len != sizeof("NRF Multi GATT") - 1 ||
                memcmp(fields.name, "NRF Multi GATT", fields.name_len) != 0)
            return 0;
        ble_addr_t address = event->disc.addr;
        int rc = ble_gap_disc_cancel();
        if (rc) { ESP_LOGE(tag, "stop scan failed: %d", rc); return 0; }
        rc = ble_gap_connect(own_address_type, &address, 30000,
            NULL, gap_event, NULL);
        if (rc) ESP_LOGE(tag, "connect failed: %d", rc);
    } else if (event->type == BLE_GAP_EVENT_CONNECT) {
        if (event->connect.status) {
            ESP_LOGE(tag, "connect status: %d", event->connect.status);
            return 0;
        }
        int rc = ble_gattc_disc_all_svcs(event->connect.conn_handle,
            service_found, NULL);
        if (rc) ESP_LOGE(tag, "service discovery start failed: %d", rc);
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        ESP_LOGI(tag, "S31_MULTI_GATT,disconnect=%s", complete ? "pass" : "incomplete");
    }
    return 0;
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_address_type);
    if (rc) { ESP_LOGE(tag, "address type failed: %d", rc); return; }
    struct ble_gap_disc_params params = {0};
    rc = ble_gap_disc(own_address_type, BLE_HS_FOREVER, &params,
        gap_event, NULL);
    if (rc) ESP_LOGE(tag, "scan failed: %d", rc);
    else ESP_LOGI(tag, "S31_MULTI_GATT,scanning=pass");
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
