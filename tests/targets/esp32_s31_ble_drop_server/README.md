# ESP32-S31 BLE held-read target

Temporary peer for `examples/nrf52_ble_disconnect_during_read.bmx`. It exposes a 300-byte readable characteristic and holds the first read for five seconds. Reset the ESP32 after the nRF prints `BLE_DROP,read_started=1`; the nRF should receive a disconnect while the read is pending, then reconnect and read the complete value. Restore the ESP32's original application after the test.
