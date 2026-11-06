# ESP32-S31 nRF GATT client

This temporary ESP-IDF 6.1 NimBLE app tests the `NRF52.Network.BLE` example on an ESP32-S31 Function CoreBoard-1. It scans for `NRF GATT`, connects, discovers service `FFF0` and characteristic `FFF1` by UUID, reads `NRF!`, writes `PING`, reads `PING` back, and disconnects. A pass is printed as `NRF52_BLE_GATT,connect=pass,read=pass,write=pass`.

Build with `idf.py build` for `esp32s31`. Flash only the active app partition after backing it up; preserve the ESP32 bootloader, partition table, OTA metadata, and NVS. Restore the backed-up app after the test. The on-board check used only the `ota_0` app slot at `0x10000`; esptool verified the restored image hash and the original `GC_STAGE2_STRESS` app resumed its `done=1` output.
