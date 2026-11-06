# ESP32-S31 multi-service GATT client

Peer for `examples/nrf52_ble_multi_gatt.bmx`. It discovers both services and all three characteristics, reads each initial value, writes the second characteristic, verifies the readback, and disconnects. Restore the ESP32's original application after the test.
