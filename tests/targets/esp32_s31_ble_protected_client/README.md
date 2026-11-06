# ESP32-S31 protected access client

This temporary peer scans for `NRF Protected` and checks that a characteristic write and CCC notification subscription are rejected before pairing. It then pairs through numeric comparison, repeats both operations, verifies the notification, and reports `S31_PROTECTED,complete=pass`.

Build with `idf.py build` for `esp32s31`. Back up the active app partition before writing this temporary client at `0x10000`; preserve the bootloader, partition table, OTA metadata, and NVS. Restore the saved app after testing. The Tenstar board test restored the original app with flash hash verification and passed its `GC_STAGE2_STRESS` self-check.
