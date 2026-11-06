#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_store.h"

void ble_store_config_init(void);

static const char *tag = "S31_PROTECTED";
static uint8_t own_address_type;
static uint16_t service_start, service_end, value_handle, ccc_handle;
static bool denied_write, denied_subscribe, wrote, subscribed, notified;

static int gap_event(struct ble_gap_event *event, void *arg);

static bool security_error(int status) {
    return status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) ||
        status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC);
}

static int subscribed_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)conn; (void)attr; (void)arg;
    if (error->status) ESP_LOGE(tag, "authenticated subscription failed: %d", error->status);
    else {
        subscribed = true;
        ESP_LOGI(tag, "S31_PROTECTED,authenticated_subscribe=pass");
    }
    return 0;
}

static int written_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)attr; (void)arg;
    if (error->status) {
        ESP_LOGE(tag, "authenticated write failed: %d", error->status);
        return 0;
    }
    wrote = true;
    ESP_LOGI(tag, "S31_PROTECTED,authenticated_write=pass");
    const uint8_t enabled[2] = {1, 0};
    int rc = ble_gattc_write_flat(conn, ccc_handle, enabled, sizeof(enabled),
        subscribed_done, NULL);
    if (rc) ESP_LOGE(tag, "authenticated subscription start failed: %d", rc);
    return 0;
}

static int pre_subscribe_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)attr; (void)arg;
    if (!security_error(error->status)) {
        ESP_LOGE(tag, "unauthenticated subscription status: %d", error->status);
        return 0;
    }
    denied_subscribe = true;
    ESP_LOGI(tag, "S31_PROTECTED,preauth_subscribe_denied=pass,status=%d",
        error->status);
    int rc = ble_gap_security_initiate(conn);
    if (rc) ESP_LOGE(tag, "security start failed: %d", rc);
    return 0;
}

static int pre_write_done(uint16_t conn, const struct ble_gatt_error *error,
        struct ble_gatt_attr *attr, void *arg) {
    (void)attr; (void)arg;
    if (!security_error(error->status)) {
        ESP_LOGE(tag, "unauthenticated write status: %d", error->status);
        return 0;
    }
    denied_write = true;
    ESP_LOGI(tag, "S31_PROTECTED,preauth_write_denied=pass,status=%d",
        error->status);
    const uint8_t enabled[2] = {1, 0};
    int rc = ble_gattc_write_flat(conn, ccc_handle, enabled, sizeof(enabled),
        pre_subscribe_done, NULL);
    if (rc) ESP_LOGE(tag, "preauth subscription start failed: %d", rc);
    return 0;
}

static int descriptor_found(uint16_t conn, const struct ble_gatt_error *error,
        uint16_t chr_val_handle, const struct ble_gatt_dsc *descriptor, void *arg) {
    (void)chr_val_handle; (void)arg;
    if (error->status == 0 && descriptor &&
            ble_uuid_cmp(&descriptor->uuid.u, BLE_UUID16_DECLARE(0x2902)) == 0)
        ccc_handle = descriptor->handle;
    else if (error->status == BLE_HS_EDONE) {
        if (!ccc_handle) { ESP_LOGE(tag, "CCC missing"); return 0; }
        const uint8_t value[3] = {'B', 'A', 'D'};
        int rc = ble_gattc_write_flat(conn, value_handle, value, sizeof(value),
            pre_write_done, NULL);
        if (rc) ESP_LOGE(tag, "preauth write start failed: %d", rc);
    } else if (error->status) ESP_LOGE(tag, "descriptor discovery failed: %d", error->status);
    return 0;
}

static int characteristic_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_chr *characteristic, void *arg) {
    (void)arg;
    if (error->status == 0 && characteristic &&
            ble_uuid_cmp(&characteristic->uuid.u, BLE_UUID16_DECLARE(0xfff1)) == 0)
        value_handle = characteristic->val_handle;
    else if (error->status == BLE_HS_EDONE) {
        if (!value_handle) { ESP_LOGE(tag, "characteristic missing"); return 0; }
        int rc = ble_gattc_disc_all_dscs(conn, value_handle, service_end,
            descriptor_found, NULL);
        if (rc) ESP_LOGE(tag, "descriptor discovery start failed: %d", rc);
    } else if (error->status) ESP_LOGE(tag, "characteristic discovery failed: %d", error->status);
    return 0;
}

static int service_found(uint16_t conn, const struct ble_gatt_error *error,
        const struct ble_gatt_svc *service, void *arg) {
    (void)arg;
    if (error->status == 0 && service &&
            ble_uuid_cmp(&service->uuid.u, BLE_UUID16_DECLARE(0xfff0)) == 0) {
        service_start = service->start_handle;
        service_end = service->end_handle;
    } else if (error->status == BLE_HS_EDONE) {
        if (!service_start) { ESP_LOGE(tag, "service missing"); return 0; }
        int rc = ble_gattc_disc_all_chrs(conn, service_start, service_end,
            characteristic_found, NULL);
        if (rc) ESP_LOGE(tag, "characteristic discovery start failed: %d", rc);
    } else if (error->status) ESP_LOGE(tag, "service discovery failed: %d", error->status);
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    int rc;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_hs_adv_fields fields = {0};
            if (ble_hs_adv_parse_fields(&fields, event->disc.data,
                    event->disc.length_data) != 0 ||
                    fields.name_len != sizeof("NRF Protected") - 1 ||
                    memcmp(fields.name, "NRF Protected", fields.name_len) != 0)
                return 0;
            ble_addr_t address = event->disc.addr;
            (void)ble_store_util_delete_peer(&address);
            rc = ble_gap_disc_cancel();
            if (!rc) rc = ble_gap_connect(own_address_type, &address, 30000,
                NULL, gap_event, NULL);
            if (rc) ESP_LOGE(tag, "connect failed: %d", rc);
            return 0;
        }
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status) {
                ESP_LOGE(tag, "connection status: %d", event->connect.status);
                return 0;
            }
            ESP_LOGI(tag, "S31_PROTECTED,connected=pass");
            rc = ble_gattc_disc_all_svcs(event->connect.conn_handle,
                service_found, NULL);
            if (rc) ESP_LOGE(tag, "service discovery start failed: %d", rc);
            return 0;
        case BLE_GAP_EVENT_PASSKEY_ACTION: {
            if (event->passkey.params.action != BLE_SM_IOACT_NUMCMP) {
                ESP_LOGE(tag, "unexpected passkey action: %d", event->passkey.params.action);
                return 0;
            }
            struct ble_sm_io response = { .action = BLE_SM_IOACT_NUMCMP,
                .numcmp_accept = 1 };
            rc = ble_sm_inject_io(event->passkey.conn_handle, &response);
            if (rc) ESP_LOGE(tag, "comparison failed: %d", rc);
            return 0;
        }
        case BLE_GAP_EVENT_ENC_CHANGE: {
            if (event->enc_change.status) {
                ESP_LOGE(tag, "encryption status: %d", event->enc_change.status);
                return 0;
            }
            struct ble_gap_conn_desc description;
            rc = ble_gap_conn_find(event->enc_change.conn_handle, &description);
            if (rc || !description.sec_state.authenticated ||
                    !description.sec_state.encrypted ||
                    !denied_write || !denied_subscribe) {
                ESP_LOGE(tag, "authentication state invalid: %d", rc);
                return 0;
            }
            ESP_LOGI(tag, "S31_PROTECTED,authenticated=pass");
            const uint8_t value[3] = {'U', 'P', '!'};
            rc = ble_gattc_write_flat(event->enc_change.conn_handle,
                value_handle, value, sizeof(value), written_done, NULL);
            if (rc) ESP_LOGE(tag, "authenticated write start failed: %d", rc);
            return 0;
        }
        case BLE_GAP_EVENT_NOTIFY_RX: {
            char value[4];
            if (event->notify_rx.indication ||
                    event->notify_rx.attr_handle != value_handle ||
                    OS_MBUF_PKTLEN(event->notify_rx.om) != 4 ||
                    os_mbuf_copydata(event->notify_rx.om, 0, 4, value) != 0 ||
                    memcmp(value, "PUSH", 4) != 0) {
                ESP_LOGE(tag, "notification mismatch");
                return 0;
            }
            notified = true;
            ESP_LOGI(tag, "S31_PROTECTED,notification=pass");
            ble_gap_terminate(event->notify_rx.conn_handle,
                BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        case BLE_GAP_EVENT_DISCONNECT:
            if (denied_write && denied_subscribe && wrote && subscribed && notified)
                ESP_LOGI(tag, "S31_PROTECTED,complete=pass");
            else ESP_LOGE(tag, "protected access incomplete");
            return 0;
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
    else ESP_LOGI(tag, "S31_PROTECTED,scanning=pass");
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
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_YESNO;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sync_cb = on_sync;
    ble_store_config_init();
    nimble_port_freertos_init(host_task);
}
