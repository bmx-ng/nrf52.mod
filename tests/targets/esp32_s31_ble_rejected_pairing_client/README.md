# ESP32-S31 rejected pairing client

This temporary peer scans for `NRF Reject Test`, accepts its numeric comparison, and observes the nRF rejecting the first attempt. It then reconnects, pairs again, reads the authenticated `FFF1` value, and reports `S31_REJECT,rejected_then_retry=pass`.

Build with `idf.py build` for `esp32s31`. Back up the active app partition before writing this temporary client at `0x10000`; preserve the ESP32 bootloader, partition table, OTA metadata, and NVS. Restore the saved app after testing. The Tenstar board test restored the app with flash hash verification and its `GC_STAGE2_STRESS` self-check passed.
