' Copyright (c) 2026 Bruce A Henderson and contributors
' SPDX-License-Identifier: Zlib

SuperStrict

Rem
bbdoc: nRF52840 System OFF power mode.
about: Successful calls do not return. A wake event resets the chip, runs the
bootloader, and starts the application again. Save any state that must survive
before calling. This API is separate from Embedded.System.Power because its
sleep calls return to the same program.
End Rem
Module NRF52.System.Power
?nrf52

ModuleInfo "Version: 0.1"
ModuleInfo "License: zlib/libpng"

Import Embedded.System.Power

Extern "C"
    Rem
    bbdoc: Enter System OFF, waking through an already configured source, reset,
    or USB VBUS detection. Does not return on success.
    End Rem
    Function SystemOff:EPowerResult() = "bmx_nrf52_system_off"

    Rem
    bbdoc: Enter System OFF and wake when the chosen GPIO reaches the active level.
    about: Configures the GPIO as an input with a pull toward its inactive level.
    Returns InvalidArgument for an unavailable pin and PreconditionNotMet if
    the pin is already active. Does not return on success. The caller must use
    a pin that is not in use by another peripheral.
    End Rem
    Function SystemOffUntilGPIO:EPowerResult(pin:UInt, activeHigh:Int) = ..
        "bmx_nrf52_system_off_until_gpio"
End Extern
?
