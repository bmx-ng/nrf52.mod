' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Monotonic time and sleep operations for nRF52 Zephyr targets.
about: These calls use Zephyr uptime and kernel sleep on the managed main thread.
End Rem
Module NRF52.System.Time
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
    Function MonotonicMicroseconds:ULong() = "bmx_embedded_time_microseconds"
    Function MonotonicMilliseconds:ULong() = "bmx_embedded_time_milliseconds"
    Function SleepMilliseconds(milliseconds:UInt) = "bmx_embedded_sleep_milliseconds"
    Function SleepMicroseconds(microseconds:ULong) = "bmx_embedded_sleep_microseconds"
End Extern
?
