#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"

void ble_store_config_init(void);

static const char *tag = "S31_PASSKEY";
static uint8_t own_address_type;
static uint16_t value_handle;
static unsigned int connection_count;

static int gap_event(struct ble_gap_event *event, void *arg);

static void scan(void) {
    struct ble_gap_disc_params params = {0};
    int rc = ble_gap_disc(own_address_type, BLE_HS_FOREVER, &params,
        gap_event, NULL);
    if (rc) ESP_LOGE(tag, "scan failed: %d", rc);
    else ESP_LOGI(tag, "S31_PASSKEY,scanning=pass");
}

static int read_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)arg;
    char value[3];
    if (error->status || !attr || OS_MBUF_PKTLEN(attr->om) != 3 ||
            os_mbuf_copydata(attr->om, 0, 3, value) != 0 ||
            memcmp(value, "SEC", 3) != 0) {
        ESP_LOGE(tag, "protected read failed: %d", error->status);
        return 0;
    }
    ESP_LOGI(tag, "S31_PASSKEY,authenticated_read=pass,connection=%u", connection_count);
    ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
}

static int characteristic_found(uint16_t conn,
        const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
        void *arg) {
    (void)arg;
    if (error->status == 0 && chr &&
            ble_uuid_cmp(&chr->uuid.u, BLE_UUID16_DECLARE(0xfff1)) == 0)
        value_handle = chr->val_handle;
    else if (error->status == BLE_HS_EDONE) {
        if (!value_handle) ESP_LOGE(tag, "protected characteristic missing");
        else {
            int rc = ble_gattc_read(conn, value_handle, read_done, NULL);
            if (rc) ESP_LOGE(tag, "read start failed: %d", rc);
        }
    }
    return 0;
}

static int service_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_svc *service, void *arg) {
    (void)arg;
    if (error->status == 0 && service &&
            ble_uuid_cmp(&service->uuid.u, BLE_UUID16_DECLARE(0xfff0)) == 0) {
        int rc = ble_gattc_disc_all_chrs(conn, service->start_handle,
            service->end_handle, characteristic_found, NULL);
        if (rc) ESP_LOGE(tag, "characteristic discovery failed: %d", rc);
    }
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_hs_adv_fields fields = {0};
            if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                    event->disc.length_data) != 0 ||
                    fields.name_len != sizeof("NRF Passkey") - 1 ||
                    memcmp(fields.name, "NRF Passkey", fields.name_len) != 0)
                return 0;
            ble_addr_t address = event->disc.addr;
            (void)ble_store_util_delete_peer(&address);
            int rc = ble_gap_disc_cancel();
            if (!rc) rc = ble_gap_connect(own_address_type, &address, 30000,
                NULL, gap_event, NULL);
            if (rc) ESP_LOGE(tag, "connect failed: %d", rc);
            return 0;
        }
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status) {
                ESP_LOGE(tag, "connect status: %d", event->connect.status);
                scan();
                return 0;
            }
            ++connection_count;
            value_handle = 0;
            ESP_LOGI(tag, "S31_PASSKEY,connected=%u", connection_count);
            int rc = ble_gap_security_initiate(event->connect.conn_handle);
            if (rc) ESP_LOGE(tag, "security start failed: %d", rc);
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.status) {
                ESP_LOGE(tag, "encryption failed: %d", event->enc_change.status);
                return 0;
            }
            struct ble_gap_conn_desc description;
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &description);
            if (rc || !description.sec_state.authenticated ||
                    !description.sec_state.encrypted) {
                ESP_LOGE(tag, "authentication state failed: %d", rc);
                return 0;
            }
            ESP_LOGI(tag, "S31_PASSKEY,authenticated=pass");
            rc = ble_gattc_disc_all_svcs(event->enc_change.conn_handle,
                service_found, NULL);
            if (rc) ESP_LOGE(tag, "service discovery failed: %d", rc);
            return 0;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            if (event->passkey.params.action != BLE_SM_IOACT_DISP) {
                ESP_LOGE(tag, "unexpected passkey action: %d",
                    event->passkey.params.action);
                return 0;
            }
            /* Fixed test passkey shared with the nRF peer. */
            struct ble_sm_io response = { .action = BLE_SM_IOACT_DISP,
                .passkey = 123456 };
            rc = ble_sm_inject_io(event->passkey.conn_handle, &response);
            if (rc) ESP_LOGE(tag, "display passkey failed: %d", rc);
            else ESP_LOGI(tag, "S31_PASSKEY,display=pass");
            return 0;
        }
        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(tag, "S31_PASSKEY,disconnected=%u", connection_count);
            return 0;
    }
    return 0;
}

static void on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &own_address_type);
    if (rc) ESP_LOGE(tag, "address type failed: %d", rc);
    else scan();
}

static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nimble_port_init());
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sync_cb = on_sync;
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}
