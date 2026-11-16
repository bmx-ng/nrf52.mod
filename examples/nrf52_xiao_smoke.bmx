SuperStrict

Framework BRL.StandardIO
Import Embedded.Hardware.GPIO
Import NRF52.System.Time

Const RedLED:UInt = 26 ' RGB red, P0.26, active low

Local ready:Int = GPIOIsValid(RedLED) And GPIOInit(RedLED) And GPIOSetOutput(RedLED)
While True
    If ready Then
        GPIOPut(RedLED, 0)
        SleepMilliseconds(100)
        GPIOPut(RedLED, 1)
        Print "NRF52_XIAO,usb=pass,red_led=pass"
    Else
        Print "NRF52_XIAO,red_led=fail"
    End If
    SleepMilliseconds(900)
Wend
