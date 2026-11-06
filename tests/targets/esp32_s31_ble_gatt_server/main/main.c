#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"

static const char *tag = "S31_GATT_TARGET";
static const char target_name[] = "ESP GATT Target";
static uint8_t own_address_type;
static uint16_t value_handle;
static uint8_t value[4] = {'E', 'S', 'P', '!'};
static const ble_uuid16_t service_uuid = BLE_UUID16_INIT(0xfff0);
static const ble_uuid16_t characteristic_uuid = BLE_UUID16_INIT(0xfff1);
static const ble_uuid16_t descriptor_uuid = BLE_UUID16_INIT(0xfff2);

static int access_value(uint16_t conn, uint16_t handle,
        struct ble_gatt_access_ctxt *context, void *arg) {
    (void)conn; (void)handle; (void)arg;
    if (context->op == BLE_GATT_ACCESS_OP_READ_CHR)
        return os_mbuf_append(context->om, value, sizeof(value)) == 0
            ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    if (context->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (OS_MBUF_PKTLEN(context->om) != sizeof(value) ||
                os_mbuf_copydata(context->om, 0, sizeof(value), value) != 0)
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        ESP_LOGI(tag, "S31_GATT_SERVER,write=%c%c%c%c",
            value[0], value[1], value[2], value[3]);
        return 0;
    }
    if (context->op == BLE_GATT_ACCESS_OP_READ_DSC)
        return os_mbuf_append(context->om, "DSC", 3) == 0
            ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    return BLE_ATT_ERR_UNLIKELY;
}

static struct ble_gatt_dsc_def descriptors[] = {
    { .uuid = &descriptor_uuid.u, .att_flags = BLE_ATT_F_READ,
      .access_cb = access_value },
    { 0 },
};

static const struct ble_gatt_chr_def characteristics[] = {
    { .uuid = &characteristic_uuid.u, .access_cb = access_value,
      .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
      .val_handle = &value_handle, .descriptors = descriptors },
    { 0 },
};

static const struct ble_gatt_svc_def services[] = {
    { .type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &service_uuid.u,
      .characteristics = characteristics },
    { 0 },
};

static int gap_event(struct ble_gap_event *event, void *arg);

static void advertise(void) {
    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    int result = ble_gap_adv_start(own_address_type, NULL, BLE_HS_FOREVER,
        &params, gap_event, NULL);
    if (result) ESP_LOGE(tag, "advertising failed: %d", result);
    else ESP_LOGI(tag, "S31_GATT_SERVER,advertising=pass");
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    if (event->type == BLE_GAP_EVENT_CONNECT) {
        ESP_LOGI(tag, "S31_GATT_SERVER,connected=%d", event->connect.status == 0);
        if (event->connect.status) advertise();
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        memcpy(value, "ESP!", sizeof(value));
        ESP_LOGI(tag, "S31_GATT_SERVER,disconnected=pass");
        advertise();
    }
    return 0;
}

static void on_sync(void) {
    int result = ble_hs_id_infer_auto(0, &own_address_type);
    if (result) { ESP_LOGE(tag, "address type failed: %d", result); return; }
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    result = ble_gap_adv_set_fields(&fields);
    if (result) { ESP_LOGE(tag, "advertisement fields failed: %d", result); return; }
    struct ble_hs_adv_fields response = {0};
    response.name = (const uint8_t *)target_name;
    response.name_len = strlen(target_name);
    response.name_is_complete = 1;
    result = ble_gap_adv_rsp_set_fields(&response);
    if (result) { ESP_LOGE(tag, "scan response failed: %d", result); return; }
    advertise();
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
    ESP_ERROR_CHECK(ble_gatts_count_cfg(services));
    ESP_ERROR_CHECK(ble_gatts_add_svcs(services));
    ble_hs_cfg.sync_cb = on_sync;
    nimble_port_freertos_init(host_task);
}
