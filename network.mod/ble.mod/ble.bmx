' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: nRF52 Bluetooth Low Energy adapter.
about: Provides the portable Embedded.Network.BLE observer, central, peripheral, and GATT
client/server
subset through Zephyr. Remaining operations report unsupported.
End Rem
Module NRF52.Network.BLE
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Network.BLE
Import "native/nrf52_ble.c"
?
