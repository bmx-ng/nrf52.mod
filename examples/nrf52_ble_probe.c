#include <zephyr/bluetooth/bluetooth.h>

static const uint8_t flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
static const struct bt_data advertisement[] = {
    BT_DATA(BT_DATA_FLAGS, &flags, 1),
};
static const struct bt_data scan_response[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, "Test beacon", 11),
};

int bmx_nrf52_ble_probe_start(void) {
    int error = bt_enable(NULL);
    if (error) return error;
    return bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, advertisement,
        ARRAY_SIZE(advertisement), scan_response, ARRAY_SIZE(scan_response));
}
