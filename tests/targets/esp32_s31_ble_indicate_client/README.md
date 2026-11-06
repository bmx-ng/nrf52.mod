# ESP32-S31 nRF indication client

This temporary ESP-IDF 6.1 NimBLE app tests `examples/nrf52_ble_indicate.bmx` on an ESP32-S31 Function CoreBoard-1. It scans for `NRF Indicate`, connects, discovers service `FFF0`, characteristic `FFF1`, and its CCC descriptor by UUID, reads the current four-byte value, enables indications, and checks that the nRF sends `ACK!`. The characteristic may still contain `ACK!` on a repeat connection. NimBLE acknowledges the indication; the nRF reports the confirmation. The ESP32 prints `NRF52_BLE_INDICATE,subscribe=pass,receive=pass`.

Build with `idf.py build` for `esp32s31`. Back up the active app partition before writing this temporary client, then restore it after the test. Preserve the bootloader, partition table, OTA metadata, and NVS. The on-board check used only the `ota_0` app slot at `0x10000`; esptool verified the restored image hash and the original `GC_STAGE2_STRESS` app resumed its `done=1` output.
