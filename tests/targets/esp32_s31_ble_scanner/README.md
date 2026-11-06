# ESP32-S31 BLE scanner test target

This temporary ESP-IDF app scans for the `Test beacon` name advertised by
`examples/nrf52_ble_probe.bmx`. It prints
`NRF52_BLE,advertisement=pass,rssi=...` when it sees the nRF board.
It also reports `NRF52_BLE_ADV_SERVICE,uuid16=pass` when a primary
advertisement includes the `FFF0` service UUID from
`examples/nrf52_ble_advertising_service.bmx`.

Build with ESP-IDF 6.1 preview support for `esp32s31` and the supplied
`sdkconfig.defaults`. The image is an application only. Before flashing,
identify the active app partition and back it up. On the Function CoreBoard-1
used for bring-up, `ota_0` was active at `0x10000` and had a 4 MiB capacity;
the scanner image was 803,376 bytes. Only that app slot was overwritten. Its
original bytes were restored and verified after the scan.

The two boards need USB power but no wire between them for this test.
