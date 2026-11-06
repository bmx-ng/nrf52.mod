# ESP32-S31 GATT server test target

This temporary NimBLE app advertises as `ESP GATT Target` and exposes service `FFF0`, readable and writable characteristic `FFF1` (initial value `ESP!`), and readable descriptor `FFF2` (value `DSC`). The nRF client writes `PING` and reads it back. Flash only the ESP32-S31 application slot at `0x10000`, then restore its backed-up original application.
