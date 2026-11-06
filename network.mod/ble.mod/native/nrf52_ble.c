#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_system.h"
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_types.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#define BLE_UNSUPPORTED (-100)
#define BLE_INVALID (-EINVAL)
#define BLE_EVENT_COUNT 32u
#define BLE_SCAN_EVENT_LIMIT 24u
#define BLE_CLIENT_SUBSCRIPTIONS 8u
#define BLE_LOCAL_SERVICES 4u
#define BLE_LOCAL_CHARACTERISTICS 8u
#define BLE_LOCAL_ATTRS_PER_SERVICE (1u + BLE_LOCAL_CHARACTERISTICS * 3u)
#define BLE_SMP_AUTH_SECURE_CONNECTIONS 0x08u

typedef struct {
    int32_t kind, status, address_type, event_type, rssi;
    int32_t data_length, connection_handle, attribute_id;
    int32_t notifications, indications, attribute_end, parent_attribute, properties;
    uint8_t address[6];
    uint8_t data[512];
} BLEEvent;

static BLEEvent events[BLE_EVENT_COUNT];
static uint32_t event_put, event_get, dropped_events;
static uint8_t initialized, advertising, advertising_auto_restart;
static uint8_t security_configured, security_bonding, security_authentication;
static uint8_t security_secure_only;
static uint8_t scanning, scan_callback_registered;
static int64_t scan_deadline, advertising_deadline;
static uint8_t restart_advertising;
static int32_t advertised_service_id, advertised_connectable;
static uint32_t advertised_duration;
static struct bt_conn *connection, *pending_connection;
static uint8_t connection_role = 1;
static struct bt_gatt_discover_params client_discovery;
static struct bt_gatt_read_params client_read;
static struct bt_gatt_write_params client_write;
static struct bt_gatt_exchange_params client_mtu_exchange;
typedef struct {
    struct bt_gatt_subscribe_params params;
    uint16_t ccc_handle, value_handle;
    uint32_t connection_epoch;
    uint8_t active;
} BLEClientSubscription;
static BLEClientSubscription client_subscriptions[BLE_CLIENT_SUBSCRIPTIONS];
static BLEClientSubscription *pending_subscription;
static uint32_t client_connection_epoch;
static uint8_t discovery_active, read_active, write_active;
static uint8_t client_disconnect_requested;
static uint8_t client_unsubscribe_pending;
static uint8_t client_mtu_exchange_active;
static int32_t discovery_parent, read_attribute, write_attribute;
static uint8_t read_value[512], write_value[512];
static uint16_t read_length;

typedef struct {
    union {
        struct bt_uuid_16 u16;
        struct bt_uuid_128 u128;
    } uuid;
    struct bt_gatt_attr attributes[BLE_LOCAL_ATTRS_PER_SERVICE];
    struct bt_gatt_service gatt;
    uint8_t attribute_count;
    uint8_t present;
    uint8_t registered;
} BLEService;

typedef struct {
    union {
        struct bt_uuid_16 u16;
        struct bt_uuid_128 u128;
    } uuid;
    struct bt_gatt_chrc declaration;
    uint8_t value[512];
    uint16_t length, capacity;
    uint32_t flags;
    int32_t identifier, service_id;
    struct bt_gatt_ccc_managed_user_data ccc_data;
    const struct bt_gatt_attr *value_attribute, *ccc_attribute;
    uint8_t present;
} BLECharacteristic;

static BLEService local_services[BLE_LOCAL_SERVICES];
static BLECharacteristic local_characteristics[BLE_LOCAL_CHARACTERISTICS];
static uint8_t local_service_count, local_characteristic_count;
static const struct bt_uuid_16 primary_declaration_uuid =
    BT_UUID_INIT_16(BT_UUID_GATT_PRIMARY_VAL);
static const struct bt_uuid_16 characteristic_declaration_uuid =
    BT_UUID_INIT_16(BT_UUID_GATT_CHRC_VAL);
static const struct bt_uuid_16 ccc_declaration_uuid =
    BT_UUID_INIT_16(BT_UUID_GATT_CCC_VAL);
static uint8_t notification_value[512], notification_pending;
static int32_t notification_characteristic_id;
static uint8_t indication_value[512], indication_pending;
static int32_t indication_characteristic_id;
static struct bt_gatt_indicate_params indication_params;

int32_t bmx_embedded_ble_gatt_notify(int32_t characteristic_id,
    int32_t handle, int32_t indication);
int32_t bmx_embedded_ble_stop_scan(void);

static const uint8_t advertising_flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
static const struct bt_data advertising_data[] = {
    BT_DATA(BT_DATA_FLAGS, &advertising_flags, 1),
};

static BLEService *local_service(int32_t identifier) {
    if (identifier <= 0 || identifier > local_service_count) return NULL;
    return &local_services[identifier - 1];
}

static BLECharacteristic *local_characteristic(int32_t identifier) {
    if (identifier <= 0 || identifier > local_characteristic_count) return NULL;
    return &local_characteristics[identifier - 1];
}

static const struct bt_uuid *service_uuid(const BLEService *item) {
    return item->uuid.u16.uuid.type == BT_UUID_TYPE_16
        ? &item->uuid.u16.uuid : &item->uuid.u128.uuid;
}

static const struct bt_uuid *characteristic_uuid(const BLECharacteristic *item) {
    return item->uuid.u16.uuid.type == BT_UUID_TYPE_16
        ? &item->uuid.u16.uuid : &item->uuid.u128.uuid;
}

static int parse_uuid(const uint8_t *text, uint32_t length,
        struct bt_uuid_16 *u16, struct bt_uuid_128 *u128) {
    if (!text) return 0;
    uint8_t parsed[16];
    uint32_t count = 0;
    int high = -1;
    for (uint32_t i = 0; i < length; ++i) {
        uint8_t ch = text[i];
        if (ch == '-') continue;
        int nibble;
        if (ch >= '0' && ch <= '9') nibble = ch - '0';
        else if (ch >= 'a' && ch <= 'f') nibble = ch - 'a' + 10;
        else if (ch >= 'A' && ch <= 'F') nibble = ch - 'A' + 10;
        else return 0;
        if (high < 0) high = nibble;
        else {
            if (count >= sizeof(parsed)) return 0;
            parsed[count++] = (uint8_t)((high << 4) | nibble);
            high = -1;
        }
    }
    if (high >= 0) return 0;
    if (count == 2) {
        u16->uuid.type = BT_UUID_TYPE_16;
        u16->val = ((uint16_t)parsed[0] << 8) | parsed[1];
        return 1;
    }
    if (count == 16) {
        u128->uuid.type = BT_UUID_TYPE_128;
        for (uint32_t i = 0; i < 16; ++i) u128->val[i] = parsed[15 - i];
        return 1;
    }
    return 0;
}

static void queue_event(const BLEEvent *event) {
    unsigned int key = irq_lock();
    /* Leave room for connection, security, and completion events during scans. */
    if (event->kind == 2 && event_put - event_get >= BLE_SCAN_EVENT_LIMIT) {
        if (dropped_events != UINT32_MAX) ++dropped_events;
        irq_unlock(key);
        bmx_embedded_system_wake();
        return;
    }
    if (event_put - event_get == BLE_EVENT_COUNT && event->kind != 2) {
        ++event_get;
        if (dropped_events != UINT32_MAX) ++dropped_events;
    }
    if (event_put - event_get < BLE_EVENT_COUNT) {
        events[event_put++ & (BLE_EVENT_COUNT - 1u)] = *event;
    } else {
        if (dropped_events != UINT32_MAX) ++dropped_events;
    }
    irq_unlock(key);
    bmx_embedded_system_wake();
}

static void scan_received(const struct bt_le_scan_recv_info *info,
        struct net_buf_simple *buf) {
    if (!scanning || !info || !info->addr || !buf) return;
    BLEEvent event = { .kind = 2, .address_type = info->addr->type,
        .event_type = info->adv_type, .rssi = info->rssi };
    memcpy(event.address, info->addr->a.val, sizeof(event.address));
    event.data_length = buf->len < sizeof(event.data)
        ? buf->len : sizeof(event.data);
    memcpy(event.data, buf->data, event.data_length);
    queue_event(&event);
}

static void scan_timed_out(void) {
    if (!scanning) return;
    scanning = 0;
    scan_deadline = 0;
    BLEEvent event = { .kind = 3 };
    queue_event(&event);
}

static struct bt_le_scan_cb scan_callbacks = {
    .recv = scan_received,
    .timeout = scan_timed_out,
};

static int connection_handle(const struct bt_conn *conn);

static void copy_uuid(BLEEvent *event, const struct bt_uuid *uuid) {
    char text[BT_UUID_STR_LEN];
    bt_uuid_to_str(uuid, text, sizeof(text));
    event->data_length = (int32_t)strlen(text);
    memcpy(event->data, text, event->data_length);
}

static BLEClientSubscription *subscription_for_ccc(uint16_t ccc_handle) {
    for (uint32_t i = 0; i < BLE_CLIENT_SUBSCRIPTIONS; ++i)
        if (client_subscriptions[i].connection_epoch == client_connection_epoch &&
                client_subscriptions[i].ccc_handle == ccc_handle)
            return &client_subscriptions[i];
    return NULL;
}

static BLEClientSubscription *subscription_for_params(
        const struct bt_gatt_subscribe_params *params) {
    for (uint32_t i = 0; i < BLE_CLIENT_SUBSCRIPTIONS; ++i)
        if (&client_subscriptions[i].params == params)
            return &client_subscriptions[i];
    return NULL;
}

static uint8_t client_discovered(struct bt_conn *conn,
        const struct bt_gatt_attr *attr,
        struct bt_gatt_discover_params *params) {
    if (!discovery_active) return BT_GATT_ITER_STOP;
    BLEEvent event = { .connection_handle = connection_handle(conn),
        .parent_attribute = discovery_parent };
    int32_t result_kind = params->type == BT_GATT_DISCOVER_PRIMARY ? 10 :
        params->type == BT_GATT_DISCOVER_CHARACTERISTIC ? 12 : 14;
    if (!attr) {
        discovery_active = 0;
        event.kind = result_kind + 1;
        event.status = connection == conn ? 0 : -ENOTCONN;
        queue_event(&event);
        return BT_GATT_ITER_STOP;
    }
    event.kind = result_kind;
    event.attribute_id = attr->handle;
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        const struct bt_gatt_service_val *value = attr->user_data;
        if (!value || !value->uuid) return BT_GATT_ITER_CONTINUE;
        event.attribute_end = value->end_handle;
        copy_uuid(&event, value->uuid);
    } else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        const struct bt_gatt_chrc *value = attr->user_data;
        if (!value || !value->uuid) return BT_GATT_ITER_CONTINUE;
        event.attribute_end = value->value_handle;
        event.properties = value->properties;
        copy_uuid(&event, value->uuid);
    } else {
        if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) == 0) {
            BLEClientSubscription *slot = subscription_for_ccc(attr->handle);
            if (!slot) {
                for (uint32_t i = 0; i < BLE_CLIENT_SUBSCRIPTIONS; ++i) {
                    if (!client_subscriptions[i].active &&
                            !client_subscriptions[i].ccc_handle) {
                        slot = &client_subscriptions[i];
                        break;
                    }
                }
            }
            if (slot) {
                slot->ccc_handle = attr->handle;
                slot->value_handle = (uint16_t)discovery_parent;
                slot->connection_epoch = client_connection_epoch;
            } else {
                discovery_active = 0;
                event.kind = result_kind + 1;
                event.status = -ENOSPC;
                queue_event(&event);
                return BT_GATT_ITER_STOP;
            }
        }
        copy_uuid(&event, attr->uuid);
    }
    queue_event(&event);
    return BT_GATT_ITER_CONTINUE;
}

static uint8_t client_read_done(struct bt_conn *conn, uint8_t error,
        struct bt_gatt_read_params *params, const void *data, uint16_t length) {
    (void)params;
    if (!read_active) return BT_GATT_ITER_STOP;
    if (!error && data && read_length + length <= sizeof(read_value)) {
        memcpy(read_value + read_length, data, length);
        read_length += length;
        return BT_GATT_ITER_CONTINUE;
    }
    BLEEvent event = { .kind = 16,
        .status = client_disconnect_requested ? -ENOTCONN : error,
        .connection_handle = connection_handle(conn),
        .attribute_id = read_attribute, .data_length = read_length };
    if (error && event.status != -ENOTCONN) {
        struct bt_conn_info info;
        if (connection != conn || bt_conn_get_info(conn, &info) != 0 ||
                info.state != BT_CONN_STATE_CONNECTED)
            event.status = -ENOTCONN;
    }
    if (!error && data) event.status = -ENOBUFS;
    else if (!error && connection != conn) event.status = -ENOTCONN;
    memcpy(event.data, read_value, read_length);
    read_active = 0;
    queue_event(&event);
    return BT_GATT_ITER_STOP;
}

static void client_write_done(struct bt_conn *conn, uint8_t error,
        struct bt_gatt_write_params *params) {
    (void)params;
    if (!write_active) return;
    BLEEvent event = { .kind = 17,
        .status = client_disconnect_requested ? -ENOTCONN : error,
        .connection_handle = connection_handle(conn),
        .attribute_id = write_attribute };
    if (!error && connection != conn) event.status = -ENOTCONN;
    write_active = 0;
    queue_event(&event);
}

static void client_write_without_response_done(struct bt_conn *conn,
        void *user_data) {
    (void)user_data;
    if (!write_active) return;
    BLEEvent event = { .kind = 17,
        .status = connection == conn ? 0 : -ENOTCONN,
        .connection_handle = connection_handle(conn),
        .attribute_id = write_attribute };
    write_active = 0;
    queue_event(&event);
}

static void client_mtu_exchanged(struct bt_conn *conn, uint8_t error,
        struct bt_gatt_exchange_params *params) {
    (void)params;
    if (!client_mtu_exchange_active) return;
    client_mtu_exchange_active = 0;
    BLEEvent event = { .kind = 19, .status = error,
        .connection_handle = connection_handle(conn),
        .attribute_end = error ? -(int32_t)error : bt_gatt_get_mtu(conn) };
    queue_event(&event);
}

static uint8_t client_notification(struct bt_conn *conn,
        struct bt_gatt_subscribe_params *params,
        const void *data, uint16_t length) {
    BLEClientSubscription *slot = subscription_for_params(params);
    if (!slot) return BT_GATT_ITER_STOP;
    if (!data) {
        if (client_unsubscribe_pending && write_active &&
                pending_subscription == slot) {
            BLEEvent event = { .kind = 17,
                .status = connection == conn ? 0 : -ENOTCONN,
                .connection_handle = connection_handle(conn),
                .attribute_id = write_attribute };
            write_active = 0;
            pending_subscription = NULL;
            client_unsubscribe_pending = 0;
            queue_event(&event);
        }
        slot->active = 0;
        return BT_GATT_ITER_STOP;
    }
    BLEEvent event = { .kind = 18,
        .connection_handle = connection_handle(conn),
        .attribute_id = params->value_handle,
        .indications = (params->value & BT_GATT_CCC_INDICATE) != 0,
        .data_length = length < sizeof(event.data) ? length : sizeof(event.data) };
    memcpy(event.data, data, event.data_length);
    queue_event(&event);
    return BT_GATT_ITER_CONTINUE;
}

static void client_subscribed(struct bt_conn *conn, uint8_t error,
        struct bt_gatt_subscribe_params *params) {
    BLEClientSubscription *slot = subscription_for_params(params);
    if (!slot) return;
    if (pending_subscription != slot) {
        if (error) slot->active = 0;
        return;
    }
    if (client_unsubscribe_pending) {
        if (error && write_active) {
            BLEEvent event = { .kind = 17, .status = error,
                .connection_handle = connection_handle(conn),
                .attribute_id = write_attribute };
            write_active = 0;
            pending_subscription = NULL;
            client_unsubscribe_pending = 0;
            queue_event(&event);
        }
        return;
    }
    if (!write_active) return;
    BLEEvent event = { .kind = 17, .status = error,
        .connection_handle = connection_handle(conn),
        .attribute_id = write_attribute };
    if (!error && connection != conn) event.status = -ENOTCONN;
    write_active = 0;
    pending_subscription = NULL;
    client_unsubscribe_pending = 0;
    if (error) slot->active = 0;
    queue_event(&event);
}

static int connection_handle(const struct bt_conn *conn) {
    return (int)bt_conn_index(conn) + 1;
}

static void security_event(struct bt_conn *conn, int32_t status) {
    bt_security_t level = bt_conn_get_security(conn);
    const bt_addr_le_t *peer = bt_conn_get_dst(conn);
    BLEEvent event = { .kind = 21, .status = status,
        .connection_handle = connection_handle(conn),
        .notifications = level >= BT_SECURITY_L2,
        .indications = level >= BT_SECURITY_L3,
        .properties = peer && bt_le_bond_exists(BT_ID_DEFAULT, peer),
        .attribute_end = bt_conn_enc_key_size(conn) };
    queue_event(&event);
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
        enum bt_security_err error) {
    if (!error && security_secure_only && level >= BT_SECURITY_L2) {
        struct bt_conn_info info;
        if (bt_conn_get_info(conn, &info) != 0 ||
                !(info.security.flags & BT_SECURITY_FLAG_SC)) {
            security_event(conn, BT_SECURITY_ERR_AUTH_REQUIREMENT);
            (void)bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
            return;
        }
    }
    security_event(conn, error);
}

static enum bt_security_err pairing_accept(struct bt_conn *conn,
        const struct bt_conn_pairing_feat *const features) {
    (void)conn;
    if (security_secure_only &&
            !(features->auth_req & BLE_SMP_AUTH_SECURE_CONNECTIONS))
        return BT_SECURITY_ERR_AUTH_REQUIREMENT;
    return BT_SECURITY_ERR_SUCCESS;
}

static void pairing_complete(struct bt_conn *conn, bool bonded) {
    (void)bonded;
    security_event(conn, 0);
}

static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason) {
    security_event(conn, reason);
}

static struct bt_conn_auth_info_cb security_info_callbacks = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

static void passkey_event(struct bt_conn *conn, int32_t action,
        int32_t passkey) {
    BLEEvent event = { .kind = 22, .connection_handle = connection_handle(conn),
        .event_type = action, .status = passkey };
    queue_event(&event);
}

static void passkey_display(struct bt_conn *conn, unsigned int passkey) {
    passkey_event(conn, 3, (int32_t)passkey);
}

static void passkey_entry(struct bt_conn *conn) {
    passkey_event(conn, 2, 0);
}

static void passkey_confirm(struct bt_conn *conn, unsigned int passkey) {
    passkey_event(conn, 4, (int32_t)passkey);
}

static void passkey_cancel(struct bt_conn *conn) {
    passkey_event(conn, 0, 0);
}

static struct bt_conn_auth_cb security_auth_callbacks;

static void connection_event(struct bt_conn *conn, uint8_t error) {
    uint8_t central = pending_connection == conn;
    BLEEvent event = { .kind = 5, .status = error,
        .connection_handle = connection_handle(conn), .properties = central ? 0 : 1 };
    const bt_addr_le_t *address = bt_conn_get_dst(conn);
    if (address) {
        event.address_type = address->type;
        memcpy(event.address, address->a.val, 6);
    }
    if (central) {
        pending_connection = NULL;
        if (!error) connection = conn; /* Keep the creation reference. */
        else bt_conn_unref(conn);
    } else if (!error) {
        connection = bt_conn_ref(conn);
    }
    if (!error) {
        connection_role = central ? 0 : 1;
        client_disconnect_requested = 0;
        advertising = 0;
        advertising_deadline = 0;
        if (central) {
            ++client_connection_epoch;
            for (uint32_t i = 0; i < BLE_CLIENT_SUBSCRIPTIONS; ++i) {
                if (!client_subscriptions[i].active) {
                    client_subscriptions[i].ccc_handle = 0;
                    client_subscriptions[i].value_handle = 0;
                }
            }
        }
    }
    queue_event(&event);
}

static void disconnection_event(struct bt_conn *conn, uint8_t reason) {
    BLEEvent event = {0};
    int32_t handle = connection_handle(conn);
    bt_addr_le_t address = {0};
    const bt_addr_le_t *peer = bt_conn_get_dst(conn);
    uint8_t has_peer = peer != NULL;
    if (peer) address = *peer;
    if (connection == conn) {
        if (connection_role == 0) {
            if (discovery_active) {
                event.kind = client_discovery.type == BT_GATT_DISCOVER_PRIMARY ? 11 :
                    client_discovery.type == BT_GATT_DISCOVER_CHARACTERISTIC ? 13 : 15;
                event.status = -ENOTCONN;
                event.connection_handle = handle;
                event.parent_attribute = discovery_parent;
                discovery_active = 0;
                queue_event(&event);
                memset(&event, 0, sizeof(event));
            }
            if (read_active) {
                event.kind = 16;
                event.status = -ENOTCONN;
                event.connection_handle = handle;
                event.attribute_id = read_attribute;
                read_active = 0;
                queue_event(&event);
                memset(&event, 0, sizeof(event));
            }
            if (write_active) {
                event.kind = 17;
                event.status = -ENOTCONN;
                event.connection_handle = handle;
                event.attribute_id = write_attribute;
                write_active = 0;
                pending_subscription = NULL;
                client_unsubscribe_pending = 0;
                queue_event(&event);
                memset(&event, 0, sizeof(event));
            }
            if (client_mtu_exchange_active) {
                event.kind = 19;
                event.status = -ENOTCONN;
                event.connection_handle = handle;
                client_mtu_exchange_active = 0;
                queue_event(&event);
                memset(&event, 0, sizeof(event));
            }
        }
        bt_conn_unref(connection);
        connection = NULL;
    }
    if (connection_role == 1 && advertising_auto_restart) restart_advertising = 1;
    client_disconnect_requested = 0;
    event.kind = 6;
    event.status = reason;
    event.connection_handle = handle;
    event.properties = connection_role;
    if (has_peer) {
        event.address_type = address.type;
        memcpy(event.address, address.a.val, 6);
    }
    queue_event(&event);
}

static void connection_parameters_updated(struct bt_conn *conn,
        uint16_t interval, uint16_t latency, uint16_t timeout) {
    BLEEvent event = { .kind = 23,
        .connection_handle = connection_handle(conn),
        .attribute_id = interval, .attribute_end = latency,
        .parent_attribute = timeout };
    queue_event(&event);
}

static void connection_parameters_rejected(struct bt_conn *conn,
        uint8_t reason) {
    BLEEvent event = { .kind = 23, .status = reason,
        .connection_handle = connection_handle(conn) };
    queue_event(&event);
}

static void connection_phy_updated(struct bt_conn *conn,
        struct bt_conn_le_phy_info *phy) {
    BLEEvent event = { .kind = 24,
        .connection_handle = connection_handle(conn),
        .event_type = phy->tx_phy, .properties = phy->rx_phy };
    queue_event(&event);
}

BT_CONN_CB_DEFINE(nrf52_ble_callbacks) = {
    .connected = connection_event,
    .disconnected = disconnection_event,
    .le_param_updated = connection_parameters_updated,
    .le_param_update_rejected = connection_parameters_rejected,
    .le_phy_updated = connection_phy_updated,
    .security_changed = security_changed,
};

static ssize_t read_characteristic(struct bt_conn *conn,
        const struct bt_gatt_attr *attr, void *buffer, uint16_t length,
        uint16_t offset) {
    BLECharacteristic *item = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buffer, length, offset,
        item->value, item->length);
}

static ssize_t write_characteristic(struct bt_conn *conn,
        const struct bt_gatt_attr *attr, const void *buffer, uint16_t length,
        uint16_t offset, uint8_t flags) {
    BLECharacteristic *item = attr->user_data;
    if ((uint32_t)offset + length > item->capacity)
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    if (flags & BT_GATT_WRITE_FLAG_PREPARE) return 0;
    if (offset != 0)
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    unsigned int key = irq_lock();
    memcpy(item->value, buffer, length);
    item->length = length;
    irq_unlock(key);
    BLEEvent event = { .kind = 7, .connection_handle = connection_handle(conn),
        .attribute_id = item->identifier, .data_length = length };
    memcpy(event.data, buffer, length);
    queue_event(&event);
    return length;
}

static void subscription_changed(const struct bt_gatt_attr *attr,
        uint16_t value) {
    BLECharacteristic *item = NULL;
    for (uint32_t i = 0; i < local_characteristic_count; ++i) {
        if (local_characteristics[i].ccc_attribute == attr) {
            item = &local_characteristics[i];
            break;
        }
    }
    if (!item) return;
    BLEEvent event = { .kind = 8, .attribute_id = item->identifier,
        .connection_handle = connection ? connection_handle(connection) : 0,
        .notifications = (value & BT_GATT_CCC_NOTIFY) != 0,
        .indications = (value & BT_GATT_CCC_INDICATE) != 0 };
    queue_event(&event);
}

static void notification_complete(struct bt_conn *conn, void *user_data) {
    (void)user_data;
    unsigned int key = irq_lock();
    notification_pending = 0;
    int32_t identifier = notification_characteristic_id;
    irq_unlock(key);
    BLEEvent event = { .kind = 20, .attribute_id = identifier,
        .connection_handle = connection_handle(conn) };
    queue_event(&event);
}

static void indication_complete(struct bt_conn *conn,
        struct bt_gatt_indicate_params *params, uint8_t error) {
    (void)params;
    BLEEvent event = { .kind = 20,
        .attribute_id = indication_characteristic_id,
        .connection_handle = connection_handle(conn), .status = error,
        .indications = 1, .properties = error == 0 };
    queue_event(&event);
}

static void indication_destroy(struct bt_gatt_indicate_params *params) {
    (void)params;
    unsigned int key = irq_lock();
    indication_pending = 0;
    irq_unlock(key);
}

static int register_services(void) {
    for (uint32_t service_index = 0; service_index < local_service_count;
            ++service_index) {
        BLEService *entry = &local_services[service_index];
        if (entry->registered) continue;
        memset(entry->attributes, 0, sizeof(entry->attributes));
        struct bt_gatt_attr *declaration = &entry->attributes[0];
        declaration->uuid = &primary_declaration_uuid.uuid;
        declaration->perm = BT_GATT_PERM_READ;
        declaration->read = bt_gatt_attr_read_service;
        declaration->user_data = (void *)service_uuid(entry);
        uint32_t count = 1;
        for (uint32_t i = 0; i < local_characteristic_count; ++i) {
            BLECharacteristic *item = &local_characteristics[i];
            if (item->service_id != (int32_t)(service_index + 1u)) continue;
            struct bt_gatt_attr *definition = &entry->attributes[count++];
            definition->uuid = &characteristic_declaration_uuid.uuid;
            definition->perm = BT_GATT_PERM_READ;
            definition->read = bt_gatt_attr_read_chrc;
            item->declaration.uuid = characteristic_uuid(item);
            item->declaration.properties = 0;
            if (item->flags & 1u) item->declaration.properties |= BT_GATT_CHRC_READ;
            if (item->flags & 2u) item->declaration.properties |= BT_GATT_CHRC_WRITE;
            if (item->flags & 4u) item->declaration.properties |= BT_GATT_CHRC_NOTIFY;
            if (item->flags & 8u) item->declaration.properties |= BT_GATT_CHRC_INDICATE;
            if (item->flags & 16u) item->declaration.properties |= BT_GATT_CHRC_WRITE_WITHOUT_RESP;
            definition->user_data = &item->declaration;
            struct bt_gatt_attr *value = &entry->attributes[count++];
            value->uuid = characteristic_uuid(item);
            if (item->flags & 0x40u) value->perm |= BT_GATT_PERM_READ_AUTHEN;
            else if (item->flags & 0x20u) value->perm |= BT_GATT_PERM_READ_ENCRYPT;
            else if (item->flags & 1u) value->perm |= BT_GATT_PERM_READ;
            if (item->flags & 0x100u) value->perm |= BT_GATT_PERM_WRITE_AUTHEN;
            else if (item->flags & 0x80u) value->perm |= BT_GATT_PERM_WRITE_ENCRYPT;
            else if (item->flags & (2u | 16u)) value->perm |= BT_GATT_PERM_WRITE;
            if (item->flags & (2u | 16u)) value->perm |= BT_GATT_PERM_PREPARE_WRITE;
            value->read = read_characteristic;
            value->write = write_characteristic;
            value->user_data = item;
            item->value_attribute = value;
            if (item->flags & (4u | 8u)) {
                struct bt_gatt_attr *ccc = &entry->attributes[count++];
                item->ccc_data.cfg_changed = subscription_changed;
                ccc->uuid = &ccc_declaration_uuid.uuid;
                ccc->perm = BT_GATT_PERM_READ;
                if (item->flags & 0x400u) ccc->perm |= BT_GATT_PERM_WRITE_AUTHEN;
                else if (item->flags & 0x200u) ccc->perm |= BT_GATT_PERM_WRITE_ENCRYPT;
                else ccc->perm |= BT_GATT_PERM_WRITE;
                ccc->read = bt_gatt_attr_read_ccc;
                ccc->write = bt_gatt_attr_write_ccc;
                ccc->user_data = &item->ccc_data;
                item->ccc_attribute = ccc;
            }
        }
        entry->attribute_count = (uint8_t)count;
        entry->gatt.attrs = entry->attributes;
        entry->gatt.attr_count = count;
        int error = bt_gatt_service_register(&entry->gatt);
        if (error) return error;
        entry->registered = 1;
    }
    return 0;
}

int32_t bmx_embedded_ble_gatt_reset(void) {
    if (initialized) return -EBUSY;
    for (uint32_t i = 0; i < local_service_count; ++i) {
        if (local_services[i].registered) {
            int error = bt_gatt_service_unregister(&local_services[i].gatt);
            if (error) return error;
        }
    }
    memset(local_services, 0, sizeof(local_services));
    memset(local_characteristics, 0, sizeof(local_characteristics));
    local_service_count = local_characteristic_count = 0;
    return 0;
}

int32_t bmx_embedded_ble_gatt_add_service(const uint8_t *uuid,
        uint32_t length, int32_t *service_id) {
    if (initialized || !service_id) return BLE_INVALID;
    if (local_service_count == BLE_LOCAL_SERVICES) return -ENOSPC;
    BLEService *entry = &local_services[local_service_count];
    if (!parse_uuid(uuid, length, &entry->uuid.u16,
            &entry->uuid.u128)) return BLE_INVALID;
    entry->present = 1;
    *service_id = ++local_service_count;
    return 0;
}

int32_t bmx_embedded_ble_gatt_add_characteristic(int32_t service_id,
        const uint8_t *uuid, uint32_t uuid_length, uint32_t flags,
        const uint8_t *value, uint32_t value_length, uint32_t capacity,
        int32_t *characteristic_id) {
    BLEService *entry = local_service(service_id);
    if (initialized || !entry || !entry->present ||
            !characteristic_id || capacity == 0 ||
            capacity > sizeof(local_characteristics[0].value) || value_length > capacity ||
            (value_length && !value) || (flags & ~0x7ffu) ||
            ((flags & (0x20u | 0x40u)) && !(flags & 1u)) ||
            ((flags & (0x80u | 0x100u)) && !(flags & (2u | 16u))) ||
            ((flags & (0x200u | 0x400u)) && !(flags & (4u | 8u))))
        return BLE_INVALID;
    if (local_characteristic_count == BLE_LOCAL_CHARACTERISTICS) return -ENOSPC;
    BLECharacteristic *item = &local_characteristics[local_characteristic_count];
    if (!parse_uuid(uuid, uuid_length, &item->uuid.u16,
            &item->uuid.u128)) return BLE_INVALID;
    item->flags = flags;
    item->capacity = capacity;
    item->length = value_length;
    if (value_length) memcpy(item->value, value, value_length);
    item->service_id = service_id;
    item->identifier = ++local_characteristic_count;
    item->present = 1;
    *characteristic_id = item->identifier;
    return 0;
}

int32_t bmx_embedded_ble_gatt_set_value(int32_t characteristic_id,
        const uint8_t *value, uint32_t length, int32_t notify_subscribers) {
    BLECharacteristic *item = local_characteristic(characteristic_id);
    if (!item || !item->present || length > item->capacity ||
            (length && !value)) return BLE_INVALID;
    unsigned int key = irq_lock();
    if (length) memcpy(item->value, value, length);
    item->length = length;
    irq_unlock(key);
    if (notify_subscribers) {
        if (item->flags & 4u)
            return bmx_embedded_ble_gatt_notify(characteristic_id, -1, 0);
        if (item->flags & 8u) {
            if (!connection) return 0;
            return bmx_embedded_ble_gatt_notify(characteristic_id,
                connection_handle(connection), 1);
        }
        return BLE_UNSUPPORTED;
    }
    return 0;
}

int32_t bmx_embedded_ble_gatt_get_value(int32_t characteristic_id,
        uint8_t *value, uint32_t capacity, int32_t *value_length) {
    BLECharacteristic *item = local_characteristic(characteristic_id);
    if (!item || !item->present || !value_length)
        return BLE_INVALID;
    unsigned int key = irq_lock();
    *value_length = item->length;
    int result = 0;
    if (value) {
        if (capacity < item->length) result = -ENOBUFS;
        else memcpy(value, item->value, item->length);
    }
    irq_unlock(key);
    return result;
}

int32_t bmx_embedded_ble_initialize(const uint8_t *name, uint32_t name_length) {
    if (initialized) return 0;
    if (name_length >= 30u || (name_length && !name)) return BLE_INVALID;
    int error = register_services();
    if (error) return error;
    error = bt_enable(NULL);
    if (error) return error;
    error = settings_load();
    if (error) return error;
    if (!scan_callback_registered) {
        error = bt_le_scan_cb_register(&scan_callbacks);
        if (error) return error;
        scan_callback_registered = 1;
    }
    if (name_length) {
        char device_name[30];
        memcpy(device_name, name, name_length);
        device_name[name_length] = 0;
        error = bt_set_name(device_name);
        if (error) return error;
    }
    initialized = 1;
    BLEEvent event = { .kind = 1 };
    queue_event(&event);
    return 0;
}

int32_t bmx_embedded_ble_deinitialize(void) {
    if (!initialized) return 0;
    if (connection || pending_connection) return -EBUSY;
    if (scanning) {
        int scan_error = bt_le_scan_stop();
        if (scan_error) return scan_error;
        scanning = 0;
        scan_deadline = 0;
    }
    if (advertising) (void)bt_le_adv_stop();
    int error = bt_disable();
    if (error) return error;
    initialized = advertising = advertising_auto_restart = 0;
    advertising_deadline = 0;
    return 0;
}

int32_t bmx_embedded_ble_initialized(void) { return initialized; }
int32_t bmx_embedded_ble_ready(void) { return initialized; }

int32_t bmx_embedded_ble_start_advertising(int32_t service_id,
        uint32_t duration, int32_t connectable, int32_t auto_restart) {
    BLEService *advertised = service_id ? local_service(service_id) : NULL;
    if (!initialized || advertising || pending_connection ||
            (service_id && (!advertised || !advertised->registered)) ||
            duration > INT32_MAX) return BLE_INVALID;
    const struct bt_le_adv_param *params = connectable
        ? BT_LE_ADV_CONN_FAST_1 : BT_LE_ADV_NCONN;
    const char *name = bt_get_name();
    struct bt_data response = BT_DATA(BT_DATA_NAME_COMPLETE, name,
        (uint8_t)strlen(name));
    struct bt_data data[2] = { advertising_data[0] };
    size_t data_count = 1;
    uint8_t uuid16[2];
    if (advertised) {
        const struct bt_uuid *uuid = service_uuid(advertised);
        if (uuid->type == BT_UUID_TYPE_16) {
            uint16_t value = advertised->uuid.u16.val;
            uuid16[0] = (uint8_t)value;
            uuid16[1] = (uint8_t)(value >> 8);
            data[1] = (struct bt_data) {
                .type = BT_DATA_UUID16_ALL, .data_len = sizeof(uuid16),
                .data = uuid16 };
        } else {
            data[1] = (struct bt_data) {
                .type = BT_DATA_UUID128_ALL,
                .data_len = sizeof(advertised->uuid.u128.val),
                .data = advertised->uuid.u128.val };
        }
        data_count = 2;
    }
    int error = bt_le_adv_start(params, data, data_count, &response, 1);
    if (!error) {
        advertising = 1;
        advertising_deadline = duration ? k_uptime_get() + duration : 0;
        advertising_auto_restart = auto_restart != 0;
        advertised_service_id = service_id;
        advertised_connectable = connectable != 0;
        advertised_duration = duration;
    }
    return error;
}

int32_t bmx_embedded_ble_stop_advertising(void) {
    if (!initialized) return BLE_INVALID;
    advertising_auto_restart = restart_advertising = 0;
    advertising_deadline = 0;
    if (!advertising) return 0;
    int error = bt_le_adv_stop();
    if (!error) advertising = 0;
    return error;
}

int32_t bmx_embedded_ble_advertising_active(void) { return advertising; }

int32_t bmx_embedded_ble_disconnect(int32_t handle) {
    if (!connection || handle != connection_handle(connection)) return BLE_INVALID;
    if (connection_role == 0) client_disconnect_requested = 1;
    int error = bt_conn_disconnect(connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    if (error) client_disconnect_requested = 0;
    return error;
}

int32_t bmx_embedded_ble_connection_count(void) { return connection != NULL; }

int32_t bmx_embedded_ble_connection_handle(int32_t index, int32_t *handle) {
    if (!connection || index != 0 || !handle) return BLE_INVALID;
    *handle = connection_handle(connection);
    return 0;
}

int32_t bmx_embedded_ble_connection_info(int32_t handle, int32_t *role,
        int32_t *address_type, uint8_t *address, int32_t *interval,
        int32_t *latency, int32_t *timeout) {
    if (!connection || handle != connection_handle(connection) || !role ||
            !address_type || !address || !interval || !latency || !timeout)
        return BLE_INVALID;
    struct bt_conn_info info;
    int error = bt_conn_get_info(connection, &info);
    if (error) return error;
    *role = info.role == BT_CONN_ROLE_PERIPHERAL ? 1 : 0;
    *address_type = info.le.dst->type;
    memcpy(address, info.le.dst->a.val, 6);
    *interval = (int32_t)(info.le.interval_us / 1250u);
    *latency = info.le.latency;
    *timeout = info.le.timeout;
    return 0;
}

int32_t bmx_embedded_ble_connection_mtu(int32_t handle) {
    if (!connection || handle != connection_handle(connection)) return BLE_INVALID;
    return bt_gatt_get_mtu(connection);
}

int32_t bmx_embedded_ble_take_event(int32_t *kind, int32_t *status,
        int32_t *address_type, uint8_t *address, int32_t *event_type,
        int32_t *rssi, uint8_t *data, int32_t *data_length,
        int32_t *handle, int32_t *attribute_id, int32_t *notifications,
        int32_t *indications, int32_t *attribute_end, int32_t *parent_attribute,
        int32_t *properties) {
    if (!kind || !status || !address_type || !address || !event_type || !rssi ||
            !data || !data_length || !handle || !attribute_id || !notifications ||
            !indications || !attribute_end || !parent_attribute || !properties)
        return 0;
    if (scanning && scan_deadline && k_uptime_get() >= scan_deadline)
        (void)bmx_embedded_ble_stop_scan();
    if (advertising && advertising_deadline &&
            k_uptime_get() >= advertising_deadline) {
        int result = bt_le_adv_stop();
        advertising_deadline = 0;
        advertising_auto_restart = 0;
        if (!result) advertising = 0;
        BLEEvent complete = { .kind = 9, .status = result };
        queue_event(&complete);
    }
    if (restart_advertising && !connection) {
        advertising = 0;
        int result = bmx_embedded_ble_start_advertising(advertised_service_id,
            advertised_duration, advertised_connectable, 1);
        if (!result || result == -EALREADY) {
            advertising = 1;
            restart_advertising = 0;
        }
    }
    unsigned int key = irq_lock();
    if (event_get == event_put) {
        irq_unlock(key);
        return 0;
    }
    BLEEvent event = events[event_get++ & (BLE_EVENT_COUNT - 1u)];
    irq_unlock(key);
    *kind = event.kind;
    *status = event.status;
    *address_type = event.address_type;
    memcpy(address, event.address, 6);
    *event_type = event.event_type;
    *rssi = event.rssi;
    *data_length = event.data_length;
    memcpy(data, event.data, event.data_length);
    *handle = event.connection_handle;
    *attribute_id = event.attribute_id;
    *notifications = event.notifications;
    *indications = event.indications;
    *attribute_end = event.attribute_end;
    *parent_attribute = event.parent_attribute;
    *properties = event.properties;
    return 1;
}

uint32_t bmx_embedded_ble_dropped_events(void) { return dropped_events; }

int32_t bmx_embedded_ble_start_scan(uint32_t duration, int32_t active,
        int32_t filter_duplicates) {
    if (!initialized) return -EAGAIN;
    if (scanning) return -EALREADY;
    if (advertising || connection) return -EBUSY;
    if (duration > INT32_MAX) return BLE_INVALID;
    struct bt_le_scan_param params = BT_LE_SCAN_PARAM_INIT(
        active ? BT_LE_SCAN_TYPE_ACTIVE : BT_LE_SCAN_TYPE_PASSIVE,
        filter_duplicates ? BT_LE_SCAN_OPT_FILTER_DUPLICATE : 0,
        BT_GAP_SCAN_FAST_INTERVAL, BT_GAP_SCAN_FAST_WINDOW);
    params.timeout = 0;
    int error = bt_le_scan_start(&params, NULL);
    if (!error) {
        scanning = 1;
        scan_deadline = duration ? k_uptime_get() + duration : 0;
    }
    return error;
}

int32_t bmx_embedded_ble_stop_scan(void) {
    if (!initialized) return -EAGAIN;
    if (!scanning) return 0;
    int error = bt_le_scan_stop();
    if (!error) {
        scanning = 0;
        scan_deadline = 0;
        BLEEvent event = { .kind = 3 };
        queue_event(&event);
    }
    return error;
}

int32_t bmx_embedded_ble_scan_active(void) { return scanning; }
int32_t bmx_embedded_ble_connect(int32_t address_type, const uint8_t *address,
        uint32_t timeout) {
    if (!initialized) return -EAGAIN;
    if (!address || address_type < 0 || address_type > 3 ||
            timeout > UINT16_MAX * 10u) return BLE_INVALID;
    if (scanning || advertising || connection || pending_connection) return -EBUSY;
    bt_addr_le_t peer = { .type = (uint8_t)address_type };
    memcpy(peer.a.val, address, sizeof(peer.a.val));
    struct bt_conn_le_create_param create = BT_CONN_LE_CREATE_PARAM_INIT(
        BT_CONN_LE_OPT_NONE, BT_GAP_SCAN_FAST_INTERVAL,
        BT_GAP_SCAN_FAST_INTERVAL);
    create.timeout = timeout ? (uint16_t)((timeout + 9u) / 10u) : 0;
    return bt_conn_le_create(&peer, &create, BT_LE_CONN_PARAM_DEFAULT,
        &pending_connection);
}

int32_t bmx_embedded_ble_cancel_connect(void) {
    if (!initialized) return -EAGAIN;
    if (!pending_connection) return 0;
    return bt_conn_disconnect(pending_connection, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}
int32_t bmx_embedded_ble_exchange_mtu(int32_t handle) {
    if (!connection || handle != connection_handle(connection)) return BLE_INVALID;
    if (client_mtu_exchange_active) return -EBUSY;
    client_mtu_exchange.func = client_mtu_exchanged;
    client_mtu_exchange_active = 1;
    int error = bt_gatt_exchange_mtu(connection, &client_mtu_exchange);
    if (error) client_mtu_exchange_active = 0;
    return error;
}
int32_t bmx_embedded_ble_connection_rssi(int32_t handle, int32_t *rssi) {
    if (!connection || handle != connection_handle(connection) || !rssi)
        return BLE_INVALID;
    uint16_t hci_handle;
    int error = bt_hci_get_conn_handle(connection, &hci_handle);
    if (error) return error;
    struct net_buf *command = bt_hci_cmd_alloc(K_NO_WAIT);
    if (!command) return -ENOMEM;
    net_buf_add_le16(command, hci_handle);
    struct net_buf *response = NULL;
    error = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, command, &response);
    if (error) return error;
    if (!response || response->len < sizeof(struct bt_hci_rp_read_rssi)) {
        if (response) net_buf_unref(response);
        return -EIO;
    }
    const struct bt_hci_rp_read_rssi *result =
        (const struct bt_hci_rp_read_rssi *)response->data;
    *rssi = result->rssi;
    net_buf_unref(response);
    return 0;
}
int32_t bmx_embedded_ble_connection_phy(int32_t handle, int32_t *tx,
        int32_t *rx) {
    if (!connection || handle != connection_handle(connection) || !tx || !rx)
        return BLE_INVALID;
    struct bt_conn_info info;
    int error = bt_conn_get_info(connection, &info);
    if (error) return error;
    if (!info.le.phy) return -EAGAIN;
    *tx = info.le.phy->tx_phy;
    *rx = info.le.phy->rx_phy;
    return 0;
}

int32_t bmx_embedded_ble_update_connection_parameters(int32_t handle,
        int32_t min, int32_t max, int32_t latency, int32_t timeout) {
    if (!connection || handle != connection_handle(connection) ||
            min < 7500 || max > 4000000 || min > max ||
            min % 1250 || max % 1250 || latency < 0 || latency > 499 ||
            timeout < 100 || timeout > 32000 || timeout % 10)
        return BLE_INVALID;
    struct bt_le_conn_param parameters = {
        .interval_min = min / 1250, .interval_max = max / 1250,
        .latency = latency, .timeout = timeout / 10 };
    return bt_conn_le_param_update(connection, &parameters);
}

int32_t bmx_embedded_ble_set_preferred_phy(int32_t handle, int32_t tx,
        int32_t rx, int32_t coded) {
    if (!connection || handle != connection_handle(connection) ||
            tx <= 0 || rx <= 0 || (tx & ~7) || (rx & ~7) ||
            coded < 0 || coded > 2) return BLE_INVALID;
    struct bt_conn_le_phy_param parameters = {
        .pref_tx_phy = tx, .pref_rx_phy = rx,
        .options = coded == 1 ? BT_CONN_LE_PHY_OPT_CODED_S2 :
            coded == 2 ? BT_CONN_LE_PHY_OPT_CODED_S8 : BT_CONN_LE_PHY_OPT_NONE };
    return bt_conn_le_phy_update(connection, &parameters);
}
int32_t bmx_embedded_ble_configure_security(int32_t io, int32_t bonding,
        int32_t auth, int32_t secure, int32_t secure_only) {
    if (io < 0 || io > 4 || (auth && io == 3) ||
            (secure_only && !secure)) return BLE_INVALID;
    if (initialized || connection || pending_connection) return -EBUSY;
    /* Zephyr always offers SC; disabling it requires a separate build. */
    if (!secure) return BLE_UNSUPPORTED;
    if (security_configured) {
        int error = bt_conn_auth_cb_register(NULL);
        if (error) return error;
    }
    memset(&security_auth_callbacks, 0, sizeof(security_auth_callbacks));
    security_auth_callbacks.pairing_accept = pairing_accept;
    if (io == 0 || io == 1 || io == 4)
        security_auth_callbacks.passkey_display = passkey_display;
    if (io == 2 || io == 4)
        security_auth_callbacks.passkey_entry = passkey_entry;
    if (io == 1 || io == 4)
        security_auth_callbacks.passkey_confirm = passkey_confirm;
    if (io != 3) security_auth_callbacks.cancel = passkey_cancel;
    int error = bt_conn_auth_cb_register(&security_auth_callbacks);
    if (error) return error;
    if (!security_configured) {
        error = bt_conn_auth_info_cb_register(&security_info_callbacks);
        if (error) return error;
    }
    security_configured = 1;
    security_secure_only = secure_only != 0;
    security_bonding = bonding != 0;
    security_authentication = auth != 0;
    bt_set_bondable(security_bonding);
    return 0;
}

int32_t bmx_embedded_ble_secure_connection(int32_t handle) {
    if (!connection || handle != connection_handle(connection)) return BLE_INVALID;
    bt_security_t level = security_authentication ? BT_SECURITY_L3 : BT_SECURITY_L2;
    return bt_conn_set_security(connection, level);
}

int32_t bmx_embedded_ble_security_state(int32_t handle,
        int32_t *encrypted, int32_t *authenticated, int32_t *bonded,
        int32_t *key_size) {
    if (!connection || handle != connection_handle(connection) || !encrypted ||
            !authenticated || !bonded || !key_size) return BLE_INVALID;
    bt_security_t level = bt_conn_get_security(connection);
    *encrypted = level >= BT_SECURITY_L2;
    *authenticated = level >= BT_SECURITY_L3;
    *bonded = bt_le_bond_exists(BT_ID_DEFAULT, bt_conn_get_dst(connection));
    *key_size = bt_conn_enc_key_size(connection);
    return 0;
}

int32_t bmx_embedded_ble_provide_passkey(int32_t handle, int32_t action,
        int32_t passkey) {
    if (!connection || handle != connection_handle(connection) ||
            action != 2 || passkey < 0 || passkey > 999999) return BLE_INVALID;
    return bt_conn_auth_passkey_entry(connection, (unsigned int)passkey);
}

int32_t bmx_embedded_ble_confirm_passkey(int32_t handle, int32_t accept) {
    if (!connection || handle != connection_handle(connection)) return BLE_INVALID;
    if (!accept) return bt_conn_auth_cancel(connection);
    return bt_conn_auth_passkey_confirm(connection);
}

typedef struct { int32_t count, index; bt_addr_le_t address; } BLEBondSearch;

static void visit_bond(const struct bt_bond_info *info, void *user_data) {
    BLEBondSearch *search = user_data;
    if (search->count == search->index) search->address = info->addr;
    ++search->count;
}

int32_t bmx_embedded_ble_bond_count(void) {
    if (!initialized) return -EAGAIN;
    BLEBondSearch search = { .index = -1 };
    bt_foreach_bond(BT_ID_DEFAULT, visit_bond, &search);
    return search.count;
}

int32_t bmx_embedded_ble_bond(int32_t index, int32_t *address_type,
        uint8_t *address) {
    if (!initialized) return -EAGAIN;
    if (index < 0 || !address_type || !address) return BLE_INVALID;
    BLEBondSearch search = { .index = index };
    bt_foreach_bond(BT_ID_DEFAULT, visit_bond, &search);
    if (index >= search.count) return BLE_INVALID;
    *address_type = search.address.type;
    memcpy(address, search.address.a.val, 6);
    return 0;
}

int32_t bmx_embedded_ble_forget_bond(int32_t address_type,
        const uint8_t *address) {
    if (!initialized) return -EAGAIN;
    if (address_type < 0 || address_type > 3 || !address) return BLE_INVALID;
    bt_addr_le_t peer = { .type = (uint8_t)address_type };
    memcpy(peer.a.val, address, 6);
    return bt_unpair(BT_ID_DEFAULT, &peer);
}

int32_t bmx_embedded_ble_forget_all_bonds(void) {
    if (!initialized) return -EAGAIN;
    return bt_unpair(BT_ID_DEFAULT, NULL);
}
static int start_discovery(int32_t handle, int32_t start, int32_t end,
        uint8_t type, int32_t parent) {
    if (!connection || handle != connection_handle(connection) ||
            connection_role != 0 || start <= 0 || end < start ||
            end > UINT16_MAX) return BLE_INVALID;
    if (discovery_active) return -EBUSY;
    memset(&client_discovery, 0, sizeof(client_discovery));
    client_discovery.func = client_discovered;
    client_discovery.start_handle = (uint16_t)start;
    client_discovery.end_handle = (uint16_t)end;
    client_discovery.type = type;
    discovery_parent = parent;
    discovery_active = 1;
    int error = bt_gatt_discover(connection, &client_discovery);
    if (error) discovery_active = 0;
    return error;
}

int32_t bmx_embedded_ble_discover_services(int32_t handle) {
    return start_discovery(handle, BT_ATT_FIRST_ATTRIBUTE_HANDLE,
        BT_ATT_LAST_ATTRIBUTE_HANDLE, BT_GATT_DISCOVER_PRIMARY, 0);
}

int32_t bmx_embedded_ble_discover_characteristics(int32_t handle,
        int32_t start, int32_t end) {
    return start_discovery(handle, start, end,
        BT_GATT_DISCOVER_CHARACTERISTIC, start);
}

int32_t bmx_embedded_ble_discover_descriptors(int32_t handle,
        int32_t value_handle, int32_t end) {
    return start_discovery(handle, value_handle + 1, end,
        BT_GATT_DISCOVER_DESCRIPTOR, value_handle);
}

int32_t bmx_embedded_ble_client_read(int32_t handle, int32_t attribute,
        uint32_t offset) {
    if (!connection || handle != connection_handle(connection) ||
            connection_role != 0 || attribute <= 0 || attribute > UINT16_MAX ||
            offset > UINT16_MAX) return BLE_INVALID;
    if (read_active) return -EBUSY;
    memset(&client_read, 0, sizeof(client_read));
    client_read.func = client_read_done;
    client_read.handle_count = 1;
    client_read.single.handle = (uint16_t)attribute;
    client_read.single.offset = (uint16_t)offset;
    read_attribute = attribute;
    read_length = 0;
    read_active = 1;
    int error = bt_gatt_read(connection, &client_read);
    if (error) read_active = 0;
    return error;
}

int32_t bmx_embedded_ble_client_write(int32_t handle, int32_t attribute,
        const uint8_t *value, uint32_t length, int32_t response) {
    if (!connection || handle != connection_handle(connection) ||
            connection_role != 0 || attribute <= 0 || attribute > UINT16_MAX ||
            length > sizeof(write_value) || (length && !value)) return BLE_INVALID;
    if (write_active) return -EBUSY;
    uint16_t mtu = bt_gatt_get_mtu(connection);
    if (mtu < 3) return -EMSGSIZE;
    if (!response) {
        if (length > mtu - 3u) return -EMSGSIZE;
        if (length) memcpy(write_value, value, length);
        write_attribute = attribute;
        write_active = 1;
        int error = bt_gatt_write_without_response_cb(connection,
            (uint16_t)attribute, write_value, (uint16_t)length, false,
            client_write_without_response_done, NULL);
        if (error) write_active = 0;
        return error;
    }
    BLEClientSubscription *slot = subscription_for_ccc((uint16_t)attribute);
    if (slot && length == 2 && slot->value_handle) {
        uint16_t ccc_value = (uint16_t)value[0] | ((uint16_t)value[1] << 8);
        if (slot->active) {
            if (ccc_value != 0) return -EALREADY;
            write_attribute = attribute;
            write_active = 1;
            pending_subscription = slot;
            client_unsubscribe_pending = 1;
            int error = bt_gatt_unsubscribe(connection, &slot->params);
            if (error) {
                write_active = 0;
                pending_subscription = NULL;
                client_unsubscribe_pending = 0;
            }
            return error;
        }
        if (ccc_value == BT_GATT_CCC_NOTIFY ||
                ccc_value == BT_GATT_CCC_INDICATE) {
            memset(&slot->params, 0, sizeof(slot->params));
            slot->params.notify = client_notification;
            slot->params.subscribe = client_subscribed;
            slot->params.value_handle = slot->value_handle;
            slot->params.ccc_handle = slot->ccc_handle;
            slot->params.value = ccc_value;
            atomic_set_bit(slot->params.flags, BT_GATT_SUBSCRIBE_FLAG_VOLATILE);
            write_attribute = attribute;
            write_active = 1;
            pending_subscription = slot;
            slot->active = 1;
            int error = bt_gatt_subscribe(connection, &slot->params);
            if (error) {
                write_active = 0;
                pending_subscription = NULL;
                slot->active = 0;
            }
            return error;
        }
    }
    if (length) memcpy(write_value, value, length);
    memset(&client_write, 0, sizeof(client_write));
    client_write.func = client_write_done;
    client_write.handle = (uint16_t)attribute;
    client_write.data = write_value;
    client_write.length = (uint16_t)length;
    write_attribute = attribute;
    write_active = 1;
    int error = bt_gatt_write(connection, &client_write);
    if (error) write_active = 0;
    return error;
}
int32_t bmx_embedded_ble_gatt_notify(int32_t characteristic_id,
        int32_t handle, int32_t indication) {
    BLECharacteristic *item = local_characteristic(characteristic_id);
    if (!item || !item->present ||
            !(item->flags & (indication ? 8u : 4u))) return BLE_INVALID;
    BLEService *entry = local_service(item->service_id);
    if (!initialized || !entry->registered) return -EAGAIN;
    if (indication && handle < 0) return BLE_INVALID;
    if (!connection) return handle < 0 ? 0 : -ENOTCONN;
    if (handle >= 0 && handle != connection_handle(connection)) return BLE_INVALID;
    if (!bt_gatt_is_subscribed(connection, item->value_attribute,
            indication ? BT_GATT_CCC_INDICATE : BT_GATT_CCC_NOTIFY))
        return handle < 0 ? 0 : -EACCES;
    if (item->length > bt_gatt_get_mtu(connection) - 3u)
        return -EMSGSIZE;
    if (indication) {
        unsigned int key = irq_lock();
        if (indication_pending) {
            irq_unlock(key);
            return -EBUSY;
        }
        indication_pending = 1;
        indication_characteristic_id = characteristic_id;
        memcpy(indication_value, item->value, item->length);
        uint16_t length = item->length;
        irq_unlock(key);
        memset(&indication_params, 0, sizeof(indication_params));
        indication_params.attr = item->value_attribute;
        indication_params.data = indication_value;
        indication_params.len = length;
        indication_params.func = indication_complete;
        indication_params.destroy = indication_destroy;
        int error = bt_gatt_indicate(connection, &indication_params);
        if (error) {
            key = irq_lock();
            indication_pending = 0;
            irq_unlock(key);
        }
        return error;
    }
    unsigned int key = irq_lock();
    if (notification_pending) {
        irq_unlock(key);
        return -EBUSY;
    }
    notification_pending = 1;
    notification_characteristic_id = characteristic_id;
    memcpy(notification_value, item->value, item->length);
    uint16_t length = item->length;
    irq_unlock(key);
    struct bt_gatt_notify_params params = {
        .attr = item->value_attribute, .data = notification_value, .len = length,
        .func = notification_complete,
    };
    int error = bt_gatt_notify_cb(connection, &params);
    if (error) {
        key = irq_lock();
        notification_pending = 0;
        irq_unlock(key);
    }
    return error;
}
