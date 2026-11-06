# ESP32-S31 BLE notification target

Temporary over-the-air peer for the nRF BLE client examples on the Tenstar Robot ProMicro NRF52840. It advertises as `ESP Notify Target`, exposes service `FFF0`, a readable, notifying and indicating `FFF1` characteristic, and a second readable and notifying `FFF3` characteristic. It sends `PUSH` after a `FFF1` notification subscription and `ACK!` after an indication subscription. Subscribing to `FFF3` sends `TWO!` from that characteristic and `BOTH` from `FFF1`, allowing a simultaneous subscription check. The ESP32's original application should be restored after board tests.

The same target is used by `examples/nrf52_ble_client_subscription.bmx`, `examples/nrf52_ble_dual_notify_client.bmx`, and `examples/nrf52_ble_connection_controls.bmx`.
