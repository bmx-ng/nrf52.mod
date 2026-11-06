' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Managed context and arena diagnostics for nRF52 Zephyr targets.
End Rem
Module NRF52.Runtime
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Extern "C"
    Function ManagedContextValid:Int() = "bmx_nrf52_context_valid"
    Function ArenaCapacity:UInt() = "bmx_embedded_arena_capacity"
    Function ArenaUsed:UInt() = "bmx_embedded_arena_used"
    Function ArenaRemaining:UInt() = "bmx_embedded_arena_remaining"
End Extern
?
