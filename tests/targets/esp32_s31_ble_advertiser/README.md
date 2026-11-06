# ESP32-S31 BLE scan target

This temporary ESP-IDF NimBLE image advertises with its local name in the scan response, so `examples/nrf52_ble_scan.bmx` can distinguish active scanning from passive scanning. It also accepts connections from `examples/nrf52_ble_central.bmx` and resumes advertising after disconnect. Flash only the ESP32-S31 application slot at `0x10000`, then restore the backed-up original application after the test.
