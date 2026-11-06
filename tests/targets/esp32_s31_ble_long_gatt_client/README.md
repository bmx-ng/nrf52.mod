# ESP32-S31 long GATT client

Temporary over-the-air peer for `examples/nrf52_ble_long_gatt_server.bmx`. It finds the nRF service, negotiates MTU 247, writes a 300-byte pattern through prepare/execute writes, reads it back, and disconnects. Restore the ESP32's original application after the test.
