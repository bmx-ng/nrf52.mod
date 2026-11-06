' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: Read-only nRF52 device ID and reset-cause information.
about: Zephyr reads the SoC identity and captures reset flags at application
startup. The flags are then cleared in hardware so the next reset has a fresh
cause. Multiple flags from before startup can still be present in the snapshot.
End Rem
Module NRF52.System.Device
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Const ResetCausePin:UInt = 1
Const ResetCauseSoftware:UInt = 2
Const ResetCauseBrownout:UInt = 4
Const ResetCausePowerOn:UInt = 8
Const ResetCauseWatchdog:UInt = 16
Const ResetCauseDebug:UInt = 32
Const ResetCauseLowPowerWake:UInt = 128
Const ResetCauseCpuLockup:UInt = 256

Extern "C"
	Function UniqueDeviceID:String() = "bmx_embedded_unique_device_id"
	Function UniqueDeviceIDBytes:Byte[]() = "bmx_embedded_unique_device_id_bytes"
	Function ResetCauseFlags:UInt() = "bmx_nrf52_reset_cause_flags"
	Function SupportedResetCauseFlags:UInt() = "bmx_nrf52_supported_reset_cause_flags"
End Extern
?
