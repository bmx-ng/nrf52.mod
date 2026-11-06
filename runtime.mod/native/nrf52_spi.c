#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "blitzmax/embedded_spi.h"
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/spi.h>

/* The Pro Micro board enables one SPI bus, with fixed devicetree pinctrl. */
#define SPI_RX_PIN 39u  /* P1.07 */
#define SPI_TX_PIN 34u  /* P1.02 */
#define SPI_SCK_PIN 33u /* P1.01 */
#define SPI_ERROR (-1)
#define SPI_INVALID_ARGUMENT (-5)

static uint8_t configured;
static uint8_t initialized;
static uint8_t data_bits = 8;
static uint32_t baudrate;
static spi_operation_t operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |
    SPI_TRANSFER_MSB;
static struct spi_config configs[2];
static uint8_t last_config;
static uint8_t config_dirty = 1;

static uint32_t supported_frequency(uint32_t requested) {
    if (requested < 250000u) return 125000u;
    if (requested < 500000u) return 250000u;
    if (requested < 1000000u) return 500000u;
    if (requested < 2000000u) return 1000000u;
    if (requested < 4000000u) return 2000000u;
    if (requested < 8000000u) return 4000000u;
    return 8000000u;
}

static const struct device *spi_device(void) {
    return DEVICE_DT_GET(DT_NODELABEL(spi2));
}

static int valid_controller(int32_t controller) {
    return controller == 0 && device_is_ready(spi_device());
}

static int32_t transfer(const uint8_t *source, uint8_t *destination,
        int32_t length) {
    struct spi_buf tx_buf = {.buf = (void *)source, .len = (size_t)length};
    struct spi_buf rx_buf = {.buf = destination, .len = (size_t)length};
    struct spi_buf_set tx_set = {.buffers = &tx_buf, .count = 1};
    struct spi_buf_set rx_set = {.buffers = &rx_buf, .count = 1};
    if (length == 0) return 0;
    if (config_dirty) {
        last_config ^= 1;
        configs[last_config].frequency = baudrate;
        configs[last_config].operation = operation;
        configs[last_config].slave = 0;
        config_dirty = 0;
    }
    return spi_transceive(spi_device(), &configs[last_config],
        source ? &tx_set : NULL, destination ? &rx_set : NULL) == 0 ? length : SPI_ERROR;
}

int32_t bmx_embedded_spi_controller_count(void) { return 1; }
int32_t bmx_embedded_spi_default_controller(void) { return 0; }
uint32_t bmx_embedded_spi_default_rx_pin(void) { return SPI_RX_PIN; }
uint32_t bmx_embedded_spi_default_tx_pin(void) { return SPI_TX_PIN; }
uint32_t bmx_embedded_spi_default_sck_pin(void) { return SPI_SCK_PIN; }
uint32_t bmx_embedded_spi_default_csn_pin(void) { return UINT32_MAX; }

int32_t bmx_embedded_spi_configure_pins(int32_t controller, uint32_t rx_pin,
        uint32_t tx_pin, uint32_t sck_pin) {
    if (!valid_controller(controller) || initialized || rx_pin != SPI_RX_PIN ||
            tx_pin != SPI_TX_PIN || sck_pin != SPI_SCK_PIN) return 0;
    configured = 1;
    return 1;
}

uint32_t bmx_embedded_spi_init(int32_t controller, uint32_t frequency) {
    if (!valid_controller(controller) || !configured || initialized ||
            frequency < 125000u || frequency > 8000000u) return 0;
    baudrate = supported_frequency(frequency);
    operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_TRANSFER_MSB;
    data_bits = 8;
    initialized = 1;
    config_dirty = 1;
    return baudrate;
}

int32_t bmx_embedded_spi_deinit(int32_t controller) {
    if (!valid_controller(controller) || !initialized) return 0;
    initialized = 0;
    baudrate = 0;
    return 1;
}

uint32_t bmx_embedded_spi_set_baudrate(int32_t controller, uint32_t frequency) {
    if (!valid_controller(controller) || !initialized ||
            frequency < 125000u || frequency > 8000000u) return 0;
    baudrate = supported_frequency(frequency);
    config_dirty = 1;
    return baudrate;
}

uint32_t bmx_embedded_spi_get_baudrate(int32_t controller) {
    return valid_controller(controller) && initialized ? baudrate : 0;
}

int32_t bmx_embedded_spi_set_format(int32_t controller, uint32_t bits,
        uint32_t polarity, uint32_t phase, uint32_t bit_order) {
    if (!valid_controller(controller) || !initialized ||
            (bits != 8 && bits != 16) || polarity > 1 || phase > 1 ||
            bit_order > 1) return 0;
    /* Nordic's SPI peripheral transfers bytes. A 16-bit word is two bytes. */
    data_bits = (uint8_t)bits;
    operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |
        (polarity ? SPI_MODE_CPOL : 0) | (phase ? SPI_MODE_CPHA : 0) |
        (bit_order ? SPI_TRANSFER_MSB : SPI_TRANSFER_LSB);
    config_dirty = 1;
    return 1;
}

int32_t bmx_embedded_spi_write_read_blocking(int32_t controller, void *source,
        void *destination, int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 8 ||
            length < 0 || (length && (!source || !destination)))
        return SPI_INVALID_ARGUMENT;
    return transfer(source, destination, length);
}

int32_t bmx_embedded_spi_write_blocking(int32_t controller, void *source,
        int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 8 ||
            length < 0 || (length && !source)) return SPI_INVALID_ARGUMENT;
    return transfer(source, NULL, length);
}

int32_t bmx_embedded_spi_read_blocking(int32_t controller, uint32_t repeated_data,
        void *destination, int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 8 ||
            repeated_data > 255 || length < 0 || (length && !destination))
        return SPI_INVALID_ARGUMENT;
    uint8_t repeated[64];
    memset(repeated, (int)repeated_data, sizeof(repeated));
    uint8_t *output = destination;
    for (int32_t offset = 0; offset < length;) {
        int32_t count = length - offset;
        if (count > (int32_t)sizeof(repeated)) count = sizeof(repeated);
        if (transfer(repeated, output + offset, count) < 0) return SPI_ERROR;
        offset += count;
    }
    return length;
}

int32_t bmx_embedded_spi_write16_read16_blocking(int32_t controller,
        uint16_t *source, uint16_t *destination, int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 16 ||
            length < 0 || length > INT32_MAX / 2 ||
            (length && (!source || !destination))) return SPI_INVALID_ARGUMENT;
    int32_t result = transfer((uint8_t *)source, (uint8_t *)destination, length * 2);
    return result < 0 ? result : length;
}

int32_t bmx_embedded_spi_write16_blocking(int32_t controller,
        uint16_t *source, int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 16 ||
            length < 0 || length > INT32_MAX / 2 || (length && !source))
        return SPI_INVALID_ARGUMENT;
    int32_t result = transfer((uint8_t *)source, NULL, length * 2);
    return result < 0 ? result : length;
}

int32_t bmx_embedded_spi_read16_blocking(int32_t controller,
        uint32_t repeated_data, uint16_t *destination, int32_t length) {
    if (!valid_controller(controller) || !initialized || data_bits != 16 ||
            repeated_data > 65535 || length < 0 || (length && !destination))
        return SPI_INVALID_ARGUMENT;
    uint16_t repeated[32];
    for (unsigned i = 0; i < 32; ++i) repeated[i] = (uint16_t)repeated_data;
    for (int32_t offset = 0; offset < length;) {
        int32_t count = length - offset;
        if (count > 32) count = 32;
        if (transfer((uint8_t *)repeated, (uint8_t *)(destination + offset),
                count * 2) < 0) return SPI_ERROR;
        offset += count;
    }
    return length;
}
