' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: nRF52 hardware watchdog.
about: The nRF52840 watchdog cannot be stopped once enabled. Disable returns
False while it is running; power cycling the board stops it. Reset causes are
captured and cleared at application startup so WatchdogCausedReboot reports the
current boot rather than an older reset.
End Rem
Module NRF52.Hardware.Watchdog
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.Hardware.Watchdog
?
