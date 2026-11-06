# ESP32-S31 nRF notification client

This temporary ESP-IDF 6.1 NimBLE app tests `examples/nrf52_ble_notify.bmx` on an ESP32-S31 Function CoreBoard-1. It scans for `NRF Notify`, connects, discovers service `FFF0`, characteristic `FFF1`, and its CCC descriptor by UUID, reads the initial `NRF!`, enables notifications, and checks that the nRF sends `PUSH`. A pass is printed as `NRF52_BLE_NOTIFY,subscribe=pass,notify=pass`.

Build with `idf.py build` for `esp32s31`. Back up the active app partition before writing this temporary client, then restore it after the test. Preserve the bootloader, partition table, OTA metadata, and NVS. The on-board check used only the `ota_0` app slot at `0x10000`; esptool verified the restored image hash and the original `GC_STAGE2_STRESS` app resumed its `done=1` output.
