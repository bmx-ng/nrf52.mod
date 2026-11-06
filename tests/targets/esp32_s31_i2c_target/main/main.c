#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c_slave.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define I2C_ADDRESS 0x42
#define I2C_SDA_GPIO 0
#define I2C_SCL_GPIO 1

static const char *TAG = "s31_i2c_target";
static QueueHandle_t events;
static uint8_t received[16];
static uint32_t received_length;

static bool on_request(i2c_slave_dev_handle_t device,
        const i2c_slave_request_event_data_t *event, void *arg) {
    (void)device;
    (void)event;
    (void)arg;
    const uint8_t code = 1;
    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(events, &code, &wake);
    return wake == pdTRUE;
}

static bool on_receive(i2c_slave_dev_handle_t device,
        const i2c_slave_rx_done_event_data_t *event, void *arg) {
    (void)device;
    (void)arg;
    received_length = event->length < sizeof(received) ? event->length : sizeof(received);
    memcpy(received, event->buffer, received_length);
    const uint8_t code = 2;
    BaseType_t wake = pdFALSE;
    xQueueSendFromISR(events, &code, &wake);
    return wake == pdTRUE;
}

void app_main(void) {
    events = xQueueCreate(8, sizeof(uint8_t));
    ESP_ERROR_CHECK(events ? ESP_OK : ESP_ERR_NO_MEM);
    i2c_slave_config_t config = {
        .i2c_port = I2C_NUM_1,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .send_buf_depth = 64,
        .receive_buf_depth = 64,
        .slave_addr = I2C_ADDRESS,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7,
        .flags.enable_internal_pullup = true,
    };
    i2c_slave_dev_handle_t device;
    ESP_ERROR_CHECK(i2c_new_slave_device(&config, &device));
    i2c_slave_event_callbacks_t callbacks = {
        .on_request = on_request,
        .on_receive = on_receive,
    };
    ESP_ERROR_CHECK(i2c_slave_register_event_callbacks(device, &callbacks, NULL));
    static const uint8_t initial_reply[8] = {
        0x4e, 0x52, 0x46, 0x21, 0x4e, 0x52, 0x46, 0x21
    };
    uint32_t initial_written = 0;
    ESP_ERROR_CHECK(i2c_slave_write(device, initial_reply, sizeof(initial_reply),
        &initial_written, 100));
    ESP_LOGI(TAG, "ready at 0x42: SDA GPIO0/J2-9, SCL GPIO1/J2-12");

    uint8_t code;
    for (;;) {
        if (xQueueReceive(events, &code, portMAX_DELAY) != pdTRUE) continue;
        if (code == 1) {
            static const uint8_t reply[4] = {0x4e, 0x52, 0x46, 0x21};
            uint32_t written = 0;
            esp_err_t status = i2c_slave_write(device, reply, sizeof(reply), &written, 100);
            ESP_LOGI(TAG, "read request: status=%s queued=%lu", esp_err_to_name(status),
                (unsigned long)written);
        } else if (code == 2) {
            ESP_LOGI(TAG, "write received: count=%lu first=%02x", (unsigned long)received_length,
                received_length ? received[0] : 0);
        }
    }
}
