' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: USB CDC console operations for nRF52 Zephyr targets.
about: BRL.StandardIO automatically uses this adapter for Print and stream I/O.
End Rem
Module NRF52.IO.StandardIO
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
    Function StandardIOInit:Int() = "bmx_nrf52_stdio_init_all"
    Function StandardIORead:Long(buffer:Byte Ptr, count:Long) = "bmx_nrf52_stdio_read"
    Function StandardIOWrite:Long(buffer:Byte Ptr, count:Long) = "bmx_nrf52_stdio_write"
    Function StandardIOFlush() = "bmx_nrf52_stdio_flush"
End Extern
?
