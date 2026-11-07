# ESP32-S31 I²C test target

This ESP-IDF app responds at 7-bit address `0x42` with four bytes spelling `NRF!`. It tests `examples/nrf52_i2c.bmx` on a physical bus. The app runs on the ESP32-S31 Function CoreBoard-1, with SDA on GPIO0 (pin printed `0`) and SCL on GPIO1 (pin printed `1`).

With both boards unplugged, connect nRF D8 to ESP `0`, nRF D9 to ESP `1`, and nRF GND to an ESP `G` pin. Power each board by its own USB cable; do not connect 3V3 or 5V between them. The nRF example uses 100 kHz and the internal pull-ups on both boards.

For the alternate [nRF I²C route](../../../examples/nrf52_i2c_alt.overlay), connect nRF D7 to ESP `0` and nRF D19 to ESP `1`, with the same ground connection. Build the nRF example with that overlay selected.

Build with ESP-IDF 6.1 preview support for ESP32-S31:

```sh
idf.py --preview set-target esp32s31
idf.py --preview build
```

The supplied `sdkconfig.defaults` selects 16 MB flash. Before replacing any existing ESP app, back up the active application partition and confirm the partition table. Flash only the app image at the active app offset, preserving the board's bootloader, partition table, OTA selection, and stored data. The board used for bring-up had an active `ota_0` partition at `0x10000`, but another board may differ.

For the [external pull-up check](../../../examples/nrf52_i2c_external_pullups.bmx), build this responder with `idf.py --preview -D BMX_I2C_TARGET_INTERNAL_PULLUP=0 build`. That disables the ESP's internal pull-ups too. Add separate resistors from SDA and SCL to the nRF board's 3.3 V rail and pass `pullUps=False` in the nRF example. Keep the boards' grounds connected; do not connect their 3.3 V rails directly.
