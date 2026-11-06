# ESP32-S31 BLE bulk target

Temporary NimBLE peer for `examples/nrf52_ble_bulk_client.bmx`. It advertises as `ESP Bulk Target` and exposes a 300-byte readable and writable characteristic at service `FFF0`, characteristic `FFF1`. The initial value is the sequence 0–299 modulo 256. It supports a long write and a short write without response so the nRF client can verify transfer completion and readback. Restore the ESP32's original application after the board test.
